/*
   Bacula(R) - The Network Backup Solution

   Copyright (C) 2000-2025 Kern Sibbald

   The original author of Bacula is Kern Sibbald, with contributions
   from many others, a complete list can be found in the file AUTHORS.

   You may use this file and others of this release according to the
   license defined in the LICENSE file, which includes the Affero General
   Public License, v3.0 ("AGPLv3") and some additional permissions and
   terms pursuant to its AGPLv3 Section 7.

   This notice must be preserved when any source code is
   conveyed and/or propagated.

   Bacula(R) is a registered trademark of Kern Sibbald.
*/
/*
 * A simple test plugin for the Bacula File Daemon derived from
 *   the bpipe plugin, but used for testing new features.
 *
 *  Kern Sibbald, October 2007
 *
 */
#include "bacula.h"
#include "fd_plugins.h"
#include "lib/ini.h"
#include "fileopts.h"
#include <wchar.h>

#define USE_CMD_PARSER
#include "fd_common.h"

#define fi __FILE__
#define li __LINE__

#ifdef __cplusplus
extern "C" {
#endif

static const int dbglvl = 000;

#define PLUGIN_LICENSE      "AGPLv3"
#define PLUGIN_AUTHOR       "Kern Sibbald"
#define PLUGIN_DATE         "September 2017"
#define PLUGIN_VERSION      "4"
#define PLUGIN_DESCRIPTION  "Bacula Test File Daemon Plugin"

/* Forward referenced functions */
static bRC newPlugin(bpContext *ctx);
static bRC freePlugin(bpContext *ctx);
static bRC getPluginValue(bpContext *ctx, pVariable var, void *value);
static bRC setPluginValue(bpContext *ctx, pVariable var, void *value);
static bRC handlePluginEvent(bpContext *ctx, bEvent *event, void *value);
static bRC startBackupFile(bpContext *ctx, struct save_pkt *sp);
static bRC metadataRestore(bpContext *ctx, meta_pkt *mp);
static bRC endBackupFile(bpContext *ctx);
static bRC pluginIO(bpContext *ctx, struct io_pkt *io);
static bRC startRestoreFile(bpContext *ctx, const char *cmd);
static bRC endRestoreFile(bpContext *ctx);
static bRC createFile(bpContext *ctx, struct restore_pkt *rp);
static bRC setFileAttributes(bpContext *ctx, struct restore_pkt *rp);
static bRC checkFile(bpContext *ctx, char *fname);
static bRC restoreFileList(bpContext *ctx, struct restore_filelist_pkt *rp);
static bRC handleXACLdata(bpContext *ctx, struct xacl_pkt *xacl);
static bRC queryParameter(bpContext *ctx, struct query_pkt *qp);

/* Pointers to Bacula functions */
static bFuncs *bfuncs = NULL;
static bInfo  *binfo = NULL;

/* Plugin Information block */
static pInfo pluginInfo = {
   sizeof(pluginInfo),
   FD_PLUGIN_INTERFACE_VERSION,
   FD_PLUGIN_MAGIC,
   PLUGIN_LICENSE,
   PLUGIN_AUTHOR,
   PLUGIN_DATE,
   PLUGIN_VERSION,
   PLUGIN_DESCRIPTION
};

/* Plugin entry points for Bacula */
static pFuncs pluginFuncs = {
   sizeof(pluginFuncs),
   FD_PLUGIN_INTERFACE_VERSION,

   /* Entry points into plugin */
   newPlugin,                         /* new plugin instance */
   freePlugin,                        /* free plugin instance */
   getPluginValue,
   setPluginValue,
   handlePluginEvent,
   startBackupFile,
   endBackupFile,
   startRestoreFile,
   endRestoreFile,
   pluginIO,
   createFile,
   setFileAttributes,
   checkFile,
   handleXACLdata,
   restoreFileList,
   NULL,                         /* No checkStream */
   queryParameter,
   metadataRestore
};

static struct ini_items test_items[] = {
   // name       handler         comment            required
   { "string1",  ini_store_str,  "Special String",    1},
   { "string2",  ini_store_str,  "2nd String",        0},
   { "ok",       ini_store_bool, "boolean",           0},
   { "req",      ini_store_bool, "boolean",           1, "yes"},
   { "list",       ini_store_alist_str, "list",       0},

// We can also use the ITEMS_DEFAULT  
// { "ok",       ini_store_bool, "boolean",           0, ITEMS_DEFAULT},
   { NULL,       NULL,           NULL,                0}
};

/*
 * Plugin private context
 */
struct plugin_ctx {
   boffset_t offset;
   FILE *fd;                          /* pipe file descriptor */
   char *cmd;                         /* plugin command line */
   char *fname;                       /* filename to "backup/restore" */
   char *reader;                      /* reader program for backup */
   char *writer;                      /* writer program for backup */
   char where[512];
   int replace;
   int nb_obj;                        /* Number of objects created */
   int nb;                            /* used in queryParameter */
   char *query_buf;                   /* buffer used to not loose memory */
   plugin_metadata *meta_mgr;         /* Metadata manager */
   int job_level;                     /* current Job level */
   POOLMEM *buf;                      /* store ConfigFile */
};

/*
 * loadPlugin() and unloadPlugin() are entry points that are
 *  exported, so Bacula can directly call these two entry points
 *  they are common to all Bacula plugins.
 */
/*
 * External entry point called by Bacula to "load the plugin
 */
bRC loadPlugin(bInfo *lbinfo, bFuncs *lbfuncs, pInfo **pinfo, pFuncs **pfuncs)
{
   bfuncs = lbfuncs;                  /* set Bacula funct pointers */
   binfo  = lbinfo;
   *pinfo  = &pluginInfo;             /* return pointer to our info */
   *pfuncs = &pluginFuncs;            /* return pointer to our functions */

   return bRC_OK;
}

/*
 * External entry point to unload the plugin 
 */
bRC unloadPlugin() 
{
// printf("test-plugin-fd: Unloaded\n");
   return bRC_OK;
}

/*
 * The following entry points are accessed through the function 
 *   pointers we supplied to Bacula. Each plugin type (dir, fd, sd)
 *   has its own set of entry points that the plugin must define.
 */
/*
 * Create a new instance of the plugin i.e. allocate our private storage
 */
static bRC newPlugin(bpContext *ctx)
{
   struct plugin_ctx *p_ctx = (struct plugin_ctx *)malloc(sizeof(struct plugin_ctx));
   if (!p_ctx) {
      return bRC_Error;
   }
   memset(p_ctx, 0, sizeof(struct plugin_ctx));
   ctx->pContext = (void *)p_ctx;        /* set our context pointer */

   /* Create metadata manager class */
   p_ctx->meta_mgr = New(plugin_metadata);

   return bRC_OK;
}

/*
 * Free a plugin instance, i.e. release our private storage
 */
static bRC freePlugin(bpContext *ctx)
{
   struct plugin_ctx *p_ctx = (struct plugin_ctx *)ctx->pContext;
   if (!p_ctx) {
      return bRC_Error;
   }
   if (p_ctx->buf) {
      free_pool_memory(p_ctx->buf);
   }
   if (p_ctx->query_buf) {
      free(p_ctx->query_buf);
   }
   if (p_ctx->cmd) {
      free(p_ctx->cmd);                  /* free any allocated command string */
   }
   if (p_ctx->meta_mgr) {
      delete p_ctx->meta_mgr;            /* delete metadata manager object */
   }
   free(p_ctx);                          /* free our private context */
   ctx->pContext = NULL;
   return bRC_OK;
}

/*
 * Return some plugin value (none defined)
 */
static bRC getPluginValue(bpContext *ctx, pVariable var, void *value) 
{
   return bRC_OK;
}

/*
 * Set a plugin value (none defined)
 */
static bRC setPluginValue(bpContext *ctx, pVariable var, void *value) 
{
   return bRC_OK;
}

/*
 * Handle an event that was generated in Bacula
 */
static bRC handlePluginEvent(bpContext *ctx, bEvent *event, void *value)
{
   struct plugin_ctx *p_ctx = (struct plugin_ctx *)ctx->pContext;
   restore_object_pkt *rop;
   if (!p_ctx) {
      return bRC_Error;
   }

// char *name;

   /*
    * Most events don't interest us so we ignore them.
    *   the printfs are so that plugin writers can enable them to see
    *   what is really going on.
    */
   switch (event->eventType) {
   case bEventJobStart:
      bfuncs->DebugMessage(ctx, fi, li, dbglvl, "test-plugin-fd: JobStart=%s\n", (char *)value);
      break;
   case bEventLevel:
      p_ctx->job_level = (intptr_t)value;
      break;
   case bEventJobEnd:
   case bEventEndBackupJob:
   case bEventSince:
   case bEventStartRestoreJob:
   case bEventEndRestoreJob:
      break;
   /* End of Dir FileSet commands, now we can add excludes */
   case bEventEndFileSet:
      bfuncs->NewOptions(ctx);
      bfuncs->AddWild(ctx, "*.c", ' ');
      bfuncs->AddWild(ctx, "*.cpp", ' ');
      bfuncs->AddOptions(ctx, "ei");         /* exclude, ignore case */
      bfuncs->AddExclude(ctx, "/home/kern/bacula/regress/README");
      break;
   case bEventStartBackupJob:
      break;
   case bEventRestoreObject:
   {
      printf("Plugin RestoreObject\n");
      if (!value) {
         bfuncs->DebugMessage(ctx, fi, li, dbglvl, "test-plugin-fd: End restore objects\n");
         break;
      }
      rop = (restore_object_pkt *)value;
      bfuncs->DebugMessage(ctx, fi, li, dbglvl, 
                           "Get RestoreObject len=%d JobId=%d oname=%s type=%d data=%.256s\n",
                           rop->object_len, rop->JobId, rop->object_name, rop->object_type,
                           rop->object);
      FILE *fp;
      POOLMEM *q;
      char *working;
      static int _nb=0;
      q = get_pool_memory(PM_FNAME);

      bfuncs->getBaculaValue(ctx, bVarWorkingDir, &working);
      Mmsg(q, "%s/restore.%d", working, _nb++);
      if ((fp = bfopen(q, "w")) != NULL) {
         fwrite(rop->object, rop->object_len, 1, fp);
         fclose(fp);
      }

      free_pool_memory(q);

      if (!strcmp(rop->object_name, INI_RESTORE_OBJECT_NAME)) {
         ConfigFile ini;
         if (!ini.dump_string(rop->object, rop->object_len)) {
            break;
         }
         ini.register_items(test_items, sizeof(struct ini_items));
         if (ini.parse(ini.out_fname)) {
            bfuncs->JobMessage(ctx, fi, li, M_INFO, 0, "string1 = %s\n", 
                               ini.items[0].val.strval);
            if (ini.items[4].found) {
               POOL_MEM tmp;
               char *elt;
               foreach_alist(elt, ini.items[4].val.alistval) {
                  pm_strcat(tmp, "[");
                  pm_strcat(tmp, elt);
                  pm_strcat(tmp, "]");
               }
               bfuncs->JobMessage(ctx, fi, li, M_INFO, 0, "list = %s\n", tmp.c_str());
            }
         } else {
            bfuncs->JobMessage(ctx, fi, li, M_ERROR, 0, "Can't parse config\n");
         }
      }

      break;
   }
   /* Plugin command e.g. plugin = <plugin-name>:<name-space>:read command:write command */
   case bEventRestoreCommand:
      /* Fall-through wanted */
   case bEventEstimateCommand:
      /* Fall-through wanted */
   case bEventBackupCommand:
   {
      char *p;
      bfuncs->DebugMessage(ctx, fi, li, dbglvl, "test-plugin-fd: pluginEvent cmd=%s\n", (char *)value);
      p_ctx->cmd = bstrdup((char *)value);
      p = strchr(p_ctx->cmd, ':');
      if (!p) {
         bfuncs->JobMessage(ctx, fi, li, M_FATAL, 0, "Plugin terminator not found: %s\n", (char *)value);
         return bRC_Error;
      }
      *p++ = 0;           /* terminate plugin */
      p_ctx->fname = p;
      p = strchr(p, ':');
      if (!p) {
         bfuncs->JobMessage(ctx, fi, li, M_FATAL, 0, "File terminator not found: %s\n", (char *)value);
         return bRC_Error;
      }
      *p++ = 0;           /* terminate file */
      p_ctx->reader = p;
      p = strchr(p, ':');
      if (!p) {
         bfuncs->JobMessage(ctx, fi, li, M_FATAL, 0, "Reader terminator not found: %s\n", (char *)value);
         return bRC_Error;
      }
      *p++ = 0;           /* terminate reader string */
      p_ctx->writer = p;
      p_ctx->nb_obj = MIN(p_ctx->nb_obj, 3);
      printf("test-plugin-fd: plugin=%s fname=%s reader=%s writer=%s\n", 
          p_ctx->cmd, p_ctx->fname, p_ctx->reader, p_ctx->writer);
      break;
   }
   case bEventPluginCommand:
      break;
   case bEventVssBeforeCloseRestore:
      break;
   case bEventComponentInfo:
      printf("plugin: Component=%s\n", NPRT((char *)value));
      break;
   case bEventFeatures:
      *((const char **)value) = PLUGIN_FEATURE_RESTORELISTFILES;
      printf("test-plugin-fd: Send plugin feature\n");
      break;
   default:
      printf("test-plugin-fd: unknown event=%d\n", event->eventType);
      break;
   }
   return bRC_OK;
}

static bRC metadataRestore(bpContext *ctx, struct meta_pkt *mp)
{
   //TODO add proper handling instead of printing (i. e. add data buffer comparison)
   Dmsg1(0, "Restoring metadata of index: %d\n", mp->index);
   /*switch (mp->type) {*/
      /*case plugin_meta_blob:*/
         /*Dmsg0(0, "Restoring metadata of 'blob' type!\n");*/
         /*Dmsg1(0, _("---- [pluginrestore] len: %lld\n"), mp->buf_len);*/
         /*Dmsg2(0, _("---- [pluginrestore] buf: %.*s\n"), mp->buf_len, mp->buf);*/
         /*break;*/
      /*case plugin_meta_catalog_email:*/
         /*Dmsg0(0, "Restoring metadata of 'email catalog' type!\n");*/
         /*break;*/
      /*default:*/
         /*Dmsg1(0, "Invalid metadata type: %d!\n", mp->type);*/
         /*break;*/
   /*}*/

   return bRC_OK;
}

/*
 * Start the backup of a specific file
 */
static bRC startBackupFile(bpContext *ctx, struct save_pkt *sp)
{
   struct plugin_ctx *p_ctx = (struct plugin_ctx *)ctx->pContext;
   static int obj_uuid = 1;
   int jobid;
   bfuncs->getBaculaValue(ctx, bVarJobId, &jobid);
   if (!p_ctx) {
      return bRC_Error;
   }
   Dmsg1(0, "nb_obj = %d\n", p_ctx->nb_obj);
   if (p_ctx->job_level != 'F') {
      switch(p_ctx->nb_obj++) {
      case 0:
         sp->restore_obj.object_name = (char *)"james.xml";
         sp->restore_obj.object = (char *)"This is test data for the restore object incr.";
         sp->restore_obj.object_len = strlen(sp->restore_obj.object)+1; 
         sp->type = FT_RESTORE_FIRST;
         break;
      case 1:
         // The file is needed to reference the plugin object
         sp->type = FT_REG;
         sp->link = sp->fname = (char *)NT_("/@testplugin/test.zero"); // Use the filename in argument
         stat(p_ctx->reader, &sp->statp);
         break;
      case 2:
         sp->plugin_obj.path = (char *)NT_("/@testplugin/test.zero");
         sp->plugin_obj.plugin_name = (char *)NT_("Test Plugin");
         sp->plugin_obj.object_category = (char *)NT_("Virtual Machine");
         sp->plugin_obj.object_type = (char *)NT_("VMWare");
         sp->plugin_obj.object_name = (char *)NT_("test vm");
         sp->plugin_obj.object_source = (char *)NT_("test plugin source");
         sp->plugin_obj.object_uuid = (char *)NT_("1234-abc-testplugin");
         sp->plugin_obj.status = PLUG_OBJ_STATUS_TERMINATED;
         sp->plugin_obj.count = 1;
         sp->plugin_obj.object_size = jobid;
         sp->type = FT_PLUGIN_OBJECT;
         bfuncs->JobMessage(ctx, fi, li, M_INFO, 0, "Generating Plugin Object size=%lu\n", sp->plugin_obj.object_size);
         break;
      case 3:
         // New version of a file present in full job
      {
         time_t now = time(NULL);
         p_ctx->nb_obj++;
         sp->type = FT_REG;
         sp->fname = (char *)"/@size_update_file@";
         sp->statp.st_mode = 0640;
         sp->statp.st_ctime = now;
         sp->statp.st_mtime = now;
         sp->statp.st_atime = now;
         sp->statp.st_size = -1; /* Size is unknown at the beginning, should be updated in the next step */
         sp->statp.st_blksize = 4096;
         Dmsg0(0, "@size_update_file@ initial step\n");
      }
      break;

      default:
         return bRC_Stop;
      }
      return bRC_OK;
   }

   if (p_ctx->nb_obj == 0) {
      sp->fname = (char *)"takeme.h";
      bfuncs->DebugMessage(ctx, fi, li, dbglvl, "AcceptFile=%s = %d\n", 
                           sp->fname, bfuncs->AcceptFile(ctx, sp));

      sp->fname = (char *)"/path/to/excludeme.o";
      bfuncs->DebugMessage(ctx, fi, li, dbglvl, "AcceptFile=%s = %d\n", 
                           sp->fname, bfuncs->AcceptFile(ctx, sp));

      sp->fname = (char *)"/path/to/excludeme.c";
      bfuncs->DebugMessage(ctx, fi, li, dbglvl, "AcceptFile=%s = %d\n", 
                           sp->fname, bfuncs->AcceptFile(ctx, sp));
   }

   if (p_ctx->nb_obj == 0) {
      sp->restore_obj.object_name = (char *)"james.xml";
      sp->restore_obj.object = (char *)"This is test data for the restore object. "
  "garbage=aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
  "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
  "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
  "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
  "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
  "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
  "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
  "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
  "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
  "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
  "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
  "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
  "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
  "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
  "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
  "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
  "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
  "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
  "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
  "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
  "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
  "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
  "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
  "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
  "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
  "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
  "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
  "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
  "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
  "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
  "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
  "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
  "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
  "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
  "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
  "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
  "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
  "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
  "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
  "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
  "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
  "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
  "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
  "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
  "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
  "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
  "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
  "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
  "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
  "\0secret";
      sp->restore_obj.object_len = strlen(sp->restore_obj.object)+1+6+1; /* str + 0 + secret + 0 */
      sp->type = FT_RESTORE_FIRST;

      static int _nb=0;
      POOLMEM *q = get_pool_memory(PM_FNAME);
      char *working;
      FILE *fp;

      bfuncs->getBaculaValue(ctx, bVarWorkingDir, &working);
      Mmsg(q, "%s/torestore.%d", working, _nb++);
      if ((fp = bfopen(q, "w")) != NULL) {
         fwrite(sp->restore_obj.object, sp->restore_obj.object_len, 1, fp);
         fclose(fp);
      }
      free_pool_memory(q);
   
   } else if (p_ctx->nb_obj == 1) {
      ConfigFile ini;
      p_ctx->buf = get_pool_memory(PM_BSOCK);
      ini.register_items(test_items, sizeof(struct ini_items));

      sp->restore_obj.object_name = (char*)INI_RESTORE_OBJECT_NAME;
      sp->restore_obj.object_len = ini.serialize(&p_ctx->buf);
      sp->restore_obj.object = p_ctx->buf;
      sp->type = FT_PLUGIN_CONFIG;

      Dmsg1(0, "RestoreOptions=<%s>\n", p_ctx->buf);

   } else if (p_ctx->nb_obj == 2) {
      sp->flags |= FO_OFFSETS;
      sp->type = FT_REG;
      sp->link = sp->fname = (char *)"/@testplugin/test.zero";
      Dmsg1(0, "Create virtual file with index information flags=%llx\n", sp->flags);

   } else if (p_ctx->nb_obj == 3) {
      sp->flags |= FO_OFFSETS;
      sp->type = FT_REG;
      sp->link = sp->fname = p_ctx->fname;
   } else if (p_ctx->nb_obj == 4) {
      sp->plugin_obj.path = (char *)NT_("/@testplugin/test.zero");
      sp->plugin_obj.plugin_name = (char *)NT_("Test Plugin");
      sp->plugin_obj.object_category = (char *)NT_("Virtual Machine");
      sp->plugin_obj.object_type = (char *)NT_("VMWare");
      sp->plugin_obj.object_name = (char *)NT_("test vm");
      sp->plugin_obj.object_source = (char *)NT_("test plugin source");
      sp->plugin_obj.object_uuid = (char *)NT_("1234-abc-testplugin");
      sp->plugin_obj.status = PLUG_OBJ_STATUS_TERMINATED;
      sp->plugin_obj.count = 2;
      sp->plugin_obj.object_size = obj_uuid++;
      sp->type = FT_PLUGIN_OBJECT;
      p_ctx->nb_obj++;
      return bRC_OK;
   } else if (p_ctx->nb_obj == 5) {
      sp->plugin_obj.path = (char *)NT_("/@testplugin/test.zero");
      sp->plugin_obj.plugin_name = (char *)NT_("Test Plugin");
      sp->plugin_obj.object_category = (char *)NT_("Database");
      sp->plugin_obj.object_type = (char *)NT_("PostgreSQL");
      sp->plugin_obj.object_name = (char *)NT_("test db");
      sp->plugin_obj.object_source = (char *)NT_("test plugin source");
      sp->plugin_obj.object_uuid = (char *)NT_("5678-abc-testplugin");
      sp->plugin_obj.status = PLUG_OBJ_STATUS_ERROR;
      sp->plugin_obj.object_size = obj_uuid++;
      sp->type = FT_PLUGIN_OBJECT;
      p_ctx->nb_obj++;
      return bRC_OK;

   } else if (p_ctx->nb_obj == 6) {
      sp->plugin_obj.path = (char *)NT_("/@testplugin/");
      sp->plugin_obj.plugin_name = (char *)NT_("Test Plugin");
      sp->plugin_obj.object_category = (char *)NT_("Database");
      sp->plugin_obj.object_type = (char *)NT_("PostgreSQL");
      sp->plugin_obj.object_name = (char *)NT_("everything");
      sp->plugin_obj.object_source = (char *)NT_("test plugin source");
      sp->plugin_obj.object_uuid = (char *)NT_("5678-abc-testplugin");
      sp->plugin_obj.status = PLUG_OBJ_STATUS_TERMINATED;
      sp->plugin_obj.object_size = obj_uuid++;
      sp->type = FT_PLUGIN_OBJECT;
      p_ctx->nb_obj++;
      return bRC_OK;

   } else if (p_ctx->nb_obj == 7) {
      p_ctx->nb_obj++;
      if (p_ctx->job_level == 'F') {
         sp->type = FT_REG;
         sp->link = sp->fname = (char *)"/@testplugin/test1.zero";
         stat("/etc/passwd", &sp->statp);

         /* Assign some metadata for the fake file */
         p_ctx->meta_mgr->reset();

         const char* m1 =
            "{\
            \"key1\": \"val1\", \
            \"key2\": \"val2\", \
            \"key3\": \"val3\"  \
         }";

         /*TODO change payload to catalog packet when it's defined*/
         const char *m2 = "{\n   \"EmailBodyPreview\" : \"pretium sollicitudin vocent vulputate te semper partiendo mi cu consectetur antiopam commodo nominavi facilis iaculis montes veniam congue vitae eam ponderum idque doctus condimentum patrioque epicurei amet nominavi possit ancillae quo bibendum definition\",\n   \"EmailCc\" : \"\",\n   \"EmailConversationId\" : \"AAQkAGZmZjBlMjI0LTMxMmEtNDFkMi1hM2YxLWEzNjI5MjY4M2JkMQAQAE9i5o-JJZ5HvgNdGUT4uEA=\",\n   \"EmailFolderName\" : \"jorgegea/users/jonis@jorgegea.onmicrosoft.com/email/REGRESS_20210915123756\",\n   \"EmailFrom\" : \"eric@bacula\",\n   \"EmailHasAttachment\" : 0,\n   \"EmailId\" : \"AAMkAGZmZjBlMjI0LTMxMmEtNDFkMi1hM2YxLWEzNjI5MjY4M2JkMQBGAAAAAAChUr1sDFmcSYm7PK3nvLVxBwB-S4yOymgVRpR5CA4-eilAAABABKyCAAB-S4yOymgVRpR5CA4-eilAAABABPgVAAA=\",\n   \"EmailImportance\" : \"NORMAL\",\n   \"EmailInternetMessageId\" : \"<AM9P190MB144311378E81EB6E8400641689DB9@AM9P190MB1443.EURP190.PROD.OUTLOOK.COM>\",\n   \"EmailIsDraft\" : 0,\n   \"EmailIsRead\" : 1,\n   \"EmailTime\" : \"2021-09-15 12:39:19\",\n   \"EmailOwner\" : \"xxxx\",\n \"EmailSize\" : 4096,\n \"EmailTenant\": \"xxxx\",\n  \"EmailSubject\" : \"Elaboraret Tellus - t\",\n   \"EmailTags\" : \"\",\n   \"EmailTo\" : \"jorge@bacula\",\n   \"Plugin\" : \"m365\",\n   \"Type\" : \"EMAIL\",\n   \"Version\" : 1\n }";

         const char *m3 = "{\n \"AttachmentOwner\" : \"xxxx\", \"AttachmentTenant\" : \"xxxx\", \"AttachmentContentType\" : \"application/octet-stream\",\n   \"AttachmentEmailId\" : \"AAMkAGZmZjBlMjI0LTMxMmEtNDFkMi1hM2YxLWEzNjI5MjY4M2JkMQBGAAAAAAChUr1sDFmcSYm7PK3nvLVxBwB-S4yOymgVRpR5CA4-eilAAABABKybAAB-S4yOymgVRpR5CA4-eilAAABABUnfAAA=\",\n   \"AttachmentId\" : \"AAMkAGZmZjBlMjI0LTMxMmEtNDFkMi1hM2YxLWEzNjI5MjY4M2JkMQBGAAAAAAChUr1sDFmcSYm7PK3nvLVxBwB-S4yOymgVRpR5CA4-eilAAABABKybAAB-S4yOymgVRpR5CA4-eilAAABABUnfAAABEgAQAKT86cEi1S9PgA8I5xS0vKA=\",\n   \"AttachmentIsInline\" : 0,\n   \"AttachmentName\" : \"Ancillae.gen\",\n  \"AttachmentSize\" : 81920,\n   \"Plugin\" : \"m365\",\n   \"Type\" : \"ATTACHMENT\",\n   \"Version\" : 1\n}";
         /* Send some huge packet to test the limits.
          * Currently max packet size is 3MB (it's size of buf and all of the meta_pkt fields being sent),
          * here we are sending 2,8MB just to be a bit below */
         uint32_t m4_size = 2800000;
         void *m4 = malloc(m4_size);

         p_ctx->meta_mgr->add_packet(plugin_meta_blob, strlen(m1)+1, (void *)m1);
         p_ctx->meta_mgr->add_packet(plugin_meta_catalog_email, strlen(m2)+1, (void *)m2);
         p_ctx->meta_mgr->add_packet(plugin_meta_catalog_email, strlen(m3)+1, (void *)m3);
         p_ctx->meta_mgr->add_packet(plugin_meta_blob, m4_size, m4);

         sp->plug_meta = p_ctx->meta_mgr;
         Dmsg0(0, "Insert metadata!!!!!!\n");
         return bRC_OK;
      }
   } else if (p_ctx->nb_obj == 8) {
      p_ctx->nb_obj++;
      sp->type = FT_REG;
      sp->link = sp->fname = (char *)"/@testplugin/test2.zero";
      stat("/etc/passwd", &sp->statp);

      /* Assign some metadata for the fake file */
      p_ctx->meta_mgr->reset();

      /*TODO change payload to catalog packet when it's defined*/
      const char *m2 = "{\n   \"EmailBodyPreview\" : \"Hello John, say hello to Veronica. Best Regards\",\n   \"EmailCc\" : \"veronica@gmail.com\",\n   \"EmailConversationId\" : \"AAQkAGZmZjBlMjI0LTMxMmEtNDFkMi1hM2YxLWEzNjI5MjY4M2JkMQAQAE9i5o-JJZ5HvgNdGUTddEA=\",\n   \"EmailFolderName\" : \"jorgegea/users/jonis@jorgegea.onmicrosoft.com/email/REGRESS_20210915123756\",\n   \"EmailFrom\" : \"eric@bacula\",\n   \"EmailHasAttachment\" : 1,\n   \"EmailId\" : \"AAMkAGZmZjBlMjI0LTMxMmEtNDFkMi1hM2Yx\",\n   \"EmailImportance\" : \"IMPORTANT\",\n   \"EmailInternetMessageId\" : \"<AM9P190MB144311378E81EB6E8400641689Ddd@AM9P190MB1443.EURP190.PROD.OUTLOOK.COM>\",\n   \"EmailIsDraft\" : 1,\n   \"EmailIsRead\" : 0,\n   \"EmailTime\" : \"2021-09-15 12:40:19\",\n   \"EmailOwner\" : \"xxxx\",\n \"EmailSize\" : 4096,\n \"EmailTenant\": \"xxxx\",\n  \"EmailSubject\" : \"Hello From regress\",\n   \"EmailTags\" : \"draft,important,work\",\n   \"EmailTo\" : \"john@bacula\",\n   \"Plugin\" : \"m365\",\n   \"Type\" : \"EMAIL\",\n   \"Version\" : 1\n }";
      const char *m3 = "{\n   \"AttachmentOwner\" : \"xxxx\", \"AttachmentTenant\" : \"xxxx\", \"AttachmentContentType\" : \"application/octet-stream\",\n   \"AttachmentEmailId\" : \"AAMkAGZmZjBlMjI0LTMxMmEtNDFkMi1hM2Yx\",\n   \"AttachmentId\" : \"AAMkAGZmZjBlMjI0LTMxMmEtNDFkMi1hM2YxLWEzNjI5MjY4M2JkMQBGAAAAAAChUr1sDFmcSYm7PK3nvLVxBwB-S4yOymgVRpR5CA4-eilAAABABKybAAB-S4yOymgVRpR5CA4-eilAAABABUnfAAABEgAQAKT86cEi1S9PgA8I5xS0vKA=\",\n   \"AttachmentIsInline\" : 0,\n   \"AttachmentName\" : \"CV.pdf\",\n  \"AttachmentSize\" : 81920,\n   \"Plugin\" : \"m365\",\n   \"Type\" : \"ATTACHMENT\",\n   \"Version\" : 1\n}";
      p_ctx->meta_mgr->add_packet(plugin_meta_catalog_email, strlen(m2)+1, (void *)m2);
      p_ctx->meta_mgr->add_packet(plugin_meta_catalog_email, strlen(m3)+1, (void *)m3);
      sp->plug_meta = p_ctx->meta_mgr;
      Dmsg0(0, "Insert metadata!!!!!! CV.pdf\n");
      return bRC_OK;
   } else if (p_ctx->nb_obj == 9) {
      time_t now = time(NULL);

      p_ctx->nb_obj++;

      sp->type = FT_REG;
      sp->fname = (char *)"/@size_update_file@";
      sp->statp.st_mode = 0640;
      sp->statp.st_ctime = now;
      sp->statp.st_mtime = now;
      sp->statp.st_atime = now;
      sp->statp.st_size = -1; /* Size is unknown at the beginning, should be updated in the next step */
      sp->statp.st_blksize = 4096;

      Dmsg0(0, "@size_update_file@ initial step\n");
      return bRC_OK;

   } else if (p_ctx->nb_obj == 10) {

      p_ctx->nb_obj++;

      sp->type = FT_REG;
      sp->fname = (char *)"/@size_update_file@";
      sp->stat_update = true;    /* File attributes should be updated */
      sp->statp.st_size = 666;   /* Update size */
      sp->statp.st_mode = 0777;  /* Update perissions */

      Dmsg0(0, "@size_update_file@ update size\n");
      return bRC_OK;
   } else {
      return bRC_Stop;
   }

   if (p_ctx->nb_obj < 2) {
      time_t now = time(NULL);
      sp->statp.st_mode = 0700 | S_IFREG;
      sp->statp.st_ctime = now;
      sp->statp.st_mtime = now;
      sp->statp.st_atime = now;
      sp->statp.st_size = sp->restore_obj.object_len;
      sp->statp.st_blksize = 4096;
      sp->statp.st_blocks = 1;
   } else {
      stat(p_ctx->reader, &sp->statp);
   }

   sp->restore_obj.index = ++p_ctx->nb_obj;

   if (sp->type != FT_REG) {
      bfuncs->DebugMessage(ctx, fi, li, dbglvl,
                           "Creating RestoreObject len=%d oname=%s data=%.256s\n", 
                           sp->restore_obj.object_len, sp->restore_obj.object_name, sp->restore_obj.object);
   }

   printf("test-plugin-fd: startBackupFile\n");
   return bRC_OK;
}

/*
 * Done with backup of this file
 */
static bRC endBackupFile(bpContext *ctx)
{
   struct plugin_ctx *p_ctx = (struct plugin_ctx *)ctx->pContext;
   if (!p_ctx) {
      return bRC_Error;
   }
   /*
    * We would return bRC_More if we wanted startBackupFile to be
    * called again to backup another file
    */
   return bRC_More;
}


/*
 * Bacula is calling us to do the actual I/O
 */
static bRC pluginIO(bpContext *ctx, struct io_pkt *io)
{
   struct plugin_ctx *p_ctx = (struct plugin_ctx *)ctx->pContext;
   if (!p_ctx) {
      return bRC_Error;
   }
   io->status = 0;
   io->io_errno = 0;
   switch(io->func) {
   case IO_OPEN:
      if (p_ctx->nb_obj == 4 && p_ctx->reader) {
         p_ctx->fd = bfopen(p_ctx->reader, "r");
         if (p_ctx->fd) {
            io->status = fileno(p_ctx->fd);
         } else {
            io->status = -1;
         }
      } else {
         io->status = 1;        /* dummy file */
      }
      p_ctx->offset = 0;
      break;

   case IO_READ:
      if (p_ctx->fd) {
         io->offset = ftell(p_ctx->fd);
         io->status = fread(io->buf, 1, io->count, p_ctx->fd);
      } else {
         if (p_ctx->offset > 3000000) {
            io->status = 0;        /* EOF */
         } else {
            memset(io->buf, 0x55, io->count);
            io->offset = p_ctx->offset;
            io->status = io->count;
            p_ctx->offset += io->count;
         }
      }
      break;

   case IO_WRITE:
      if (p_ctx->fd) {
         io->status = fwrite(io->buf, 1, io->count, p_ctx->fd);

      } else {
         io->status = -1;
      }
      break;

   /* Cleanup things during close */
   case IO_CLOSE:
      if (p_ctx->fd) {
         fclose(p_ctx->fd);
         p_ctx->fd = NULL;
      }
      io->status = 0;
      {
         struct fileevent_pkt event;
         bstrncpy(event.Description, "A good description", sizeof(event.Description));
         bstrncpy(event.Source, "A very good source", sizeof(event.Source));
         event.Type = 'c';
         event.Severity = 100;
         bfuncs->AddFileEvent(ctx, &event);
         bfuncs->JobMessage(ctx, fi, li, M_INFO, 0, "Adding FileEvent\n");
      }

      break;

   case IO_SEEK:
      if (p_ctx->nb_obj == 4) {
         io->status = fwrite(io->buf, 1, io->count, p_ctx->fd);

      } else {
         io->status = -1;
      }
      io->status = io->offset;
      break;
   }

   return bRC_OK;
}

/*
 * Bacula is notifying us that a plugin name string was found, and
 *   passing us the plugin command, so we can prepare for a restore.
 */
static bRC startRestoreFile(bpContext *ctx, const char *cmd)
{
   printf("test-plugin-fd: startRestoreFile cmd=%s\n", cmd);
   return bRC_OK;
}

/*
 * Bacula is notifying us that the plugin data has terminated, so
 *  the restore for this particular file is done.
 */
static bRC endRestoreFile(bpContext *ctx)
{
   printf("test-plugin-fd: endRestoreFile\n");
   return bRC_OK;
}

/*
 * This is called during restore to create the file (if necessary)
 * We must return in rp->create_status:
 *   
 *  CF_ERROR    -- error
 *  CF_SKIP     -- skip processing this file
 *  CF_EXTRACT  -- extract the file (i.e.call i/o routines)
 *  CF_CREATED  -- created, but no content to extract (typically directories)
 *
 */
static bRC createFile(bpContext *ctx, struct restore_pkt *rp)
{
   struct plugin_ctx *pctx=(struct plugin_ctx *)ctx->pContext;
   printf("test-plugin-fd: createFile\n");
   if (strlen(rp->where) > 512) {
      printf("Restore target dir too long. Restricting to first 512 bytes.\n");
   }
   bstrncpy(pctx->where, rp->where, sizeof(pctx->where));
   pctx->replace = rp->replace;
   rp->create_status = CF_CORE;
   return bRC_OK;
}

/*
 * We will get here if the File is a directory after everything
 * is written in the directory.
 */
static bRC setFileAttributes(bpContext *ctx, struct restore_pkt *rp)
{
   printf("test-plugin-fd: setFileAttributes\n");
   return bRC_OK;
}

/* When using Incremental dump, all previous dumps are necessary */
static bRC checkFile(bpContext *ctx, char *fname)
{
   if (strncmp(fname, "/@testplugin/", strlen("/@testplugin/")) == 0) {
      return bRC_Seen;
   } else {
      return bRC_OK;
   }
}

/* Called for each file with the RestoreFileList feature at the start of a
 * restore 
 */
static bRC restoreFileList(bpContext *ctx, struct restore_filelist_pkt *rp)
{
   if (rp) {
      Dmsg1(0, "Will restore fname=%s\n", rp->ofname);
   }
   return bRC_OK;
}

/*
 * New Bacula Plugin API require this
 */
static bRC handleXACLdata(bpContext *ctx, struct xacl_pkt *xacl)
{
   return bRC_OK;
}

/* QueryParameter interface */
static bRC queryParameter(bpContext *ctx, struct query_pkt *qp)
{
   OutputWriter ow(qp->api_opts);
   cmd_parser a;
   bRC ret = bRC_OK;
   int nb = 3;
   int sleepval = 3;
   int i;
   struct plugin_ctx *p_ctx = (struct plugin_ctx *)ctx->pContext;
   if (!p_ctx) {
      return bRC_Error;
   }
   a.parse_cmd(qp->command);
   if ((i = a.find_arg_with_value("nb")) > 0) {
      nb = str_to_int64(a.argv[i]);
   }

   if ((i = a.find_arg_with_value("sleep")) > 0) {
      sleepval = str_to_int64(a.argv[i]);
   }
   if (strcasecmp(qp->parameter, "int") == 0) {
      ow.get_output(OT_INT32, "int", 1,
                    OT_INT32, "int", 2,
                    OT_INT32, "int", 3,
                    OT_INT32, "int", 4,
                    OT_INT32, "int", 5,
                    OT_INT32, "int", 6,
                    OT_INT32, "int", 7,
                    OT_INT32, "int", 8,
                    OT_INT32, "int", 9,
                    OT_INT32, "int", 10,
                    OT_END);
      goto bail_out;
   }

   if (strcasecmp(qp->parameter, "string") == 0) {
      ow.get_output(OT_STRING, "string", "string1",
                    OT_STRING, "string", "string2",
                    OT_STRING, "string", "string3",
                    OT_STRING, "string", "string4",
                    OT_STRING, "string", "string5",
                    OT_STRING, "string", "string6",
                    OT_STRING, "string", "string7",
                    OT_STRING, "string", "string8",
                    OT_STRING, "string", "string9",
                    OT_STRING, "string", "string10",
                    OT_END);
      goto bail_out;
   }

   if (strcasecmp(qp->parameter, "withsleep") == 0) {
      if (p_ctx->nb++ < nb) {
         ow.get_output(OT_INT32, "withsleep", p_ctx->nb, OT_END);
         bmicrosleep(sleepval, 0);
      }
      if (p_ctx->nb < nb) {
         ret = bRC_More;
      }
      goto bail_out;
   }

bail_out:
   if (p_ctx->query_buf) {
      free(p_ctx->query_buf);
   }
   qp->result = p_ctx->query_buf = bstrdup(ow.get_output(OT_END));
   return ret;
}

#ifdef __cplusplus
}
#endif
