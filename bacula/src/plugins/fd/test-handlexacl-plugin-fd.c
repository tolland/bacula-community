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

#undef malloc
#undef free
#undef strdup

#define fi __FILE__
#define li __LINE__

#ifdef __cplusplus
extern "C" {
#endif

static const int dbglvl = 50;

#define PLUGIN_LICENSE      "AGPLv3"
#define PLUGIN_AUTHOR       "Radosław Korzeniewski"
#define PLUGIN_DATE         "Nov 2017"
#define PLUGIN_VERSION      "1"
#define PLUGIN_DESCRIPTION  "Bacula Test handleXACLdata File Daemon Plugin"

/* Forward referenced functions */
static bRC newPlugin(bpContext *ctx);
static bRC freePlugin(bpContext *ctx);
static bRC getPluginValue(bpContext *ctx, pVariable var, void *value);
static bRC setPluginValue(bpContext *ctx, pVariable var, void *value);
static bRC handlePluginEvent(bpContext *ctx, bEvent *event, void *value);
static bRC startBackupFile(bpContext *ctx, struct save_pkt *sp);
static bRC endBackupFile(bpContext *ctx);
static bRC pluginIO(bpContext *ctx, struct io_pkt *io);
static bRC startRestoreFile(bpContext *ctx, const char *cmd);
static bRC endRestoreFile(bpContext *ctx);
static bRC createFile(bpContext *ctx, struct restore_pkt *rp);
static bRC setFileAttributes(bpContext *ctx, struct restore_pkt *rp);
static bRC checkFile(bpContext *ctx, char *fname);
static bRC handleXACLdata(bpContext *ctx, struct xacl_pkt *xacl);

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
   NULL,                        /* No RestoreFileList */
   NULL                         /* No checkStream */
};

/*
 * Plugin private context
 */
struct plugin_ctx {
   boffset_t offset;
   char *cmd;                          /* plugin command line */
   char *fname;                        /* filename to "backup/restore" */
   bool acl;                           /* if acl was saved */
   bool xattr;                         /* if xattr was saved */
   POOLMEM *buf;                       /* store ConfigFile */
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
   struct plugin_ctx *pctx;

   bfuncs->DebugMessage(ctx, fi, li, dbglvl,
      "test-handlxacl-plugin-fd: newPlugin\n");

   pctx = (struct plugin_ctx *)malloc(sizeof(struct plugin_ctx));
   if (!pctx) {
      return bRC_Error;
   }

   memset(pctx, 0, sizeof(struct plugin_ctx));
   ctx->pContext = (void *)pctx;        /* set our context pointer */

   return bRC_OK;
}

/*
 * Free a plugin instance, i.e. release our private storage
 */
static bRC freePlugin(bpContext *ctx)
{
   struct plugin_ctx *pctx = (struct plugin_ctx *)ctx->pContext;
   if (!pctx) {
      return bRC_Error;
   }
   if (pctx->buf) {
      free_pool_memory(pctx->buf);
   }
   if (pctx->cmd) {
      free(pctx->cmd);                  /* free any allocated command string */
   }
   free(pctx);                          /* free our private context */
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
   struct plugin_ctx *pctx = (struct plugin_ctx *)ctx->pContext;

   if (!pctx) {
      return bRC_Error;
   }

   /*
    * Most events don't interest us so we ignore them.
    *   the debug messages are so that plugin writers can enable them to see
    *   what is really going on.
    */
   switch (event->eventType) {
   case bEventJobStart:
   case bEventJobEnd:
   case bEventEndBackupJob:
   case bEventLevel:
   case bEventSince:
   case bEventStartRestoreJob:
   case bEventEndRestoreJob:
   case bEventEndFileSet:
   case bEventStartBackupJob:
   case bEventRestoreObject:
   case bEventPluginCommand:
   case bEventVssBeforeCloseRestore:
   case bEventComponentInfo:
      bfuncs->DebugMessage(ctx, fi, li, dbglvl,
         "handlexacl: bEvent=%i\n",
         event->eventType);
      break;

   /* Plugin command e.g. plugin = <plugin-name>:<name-space>:read command:write command */
   case bEventRestoreCommand:
      /* Fall-through wanted */
   case bEventEstimateCommand:
      /* Fall-through wanted */
   case bEventBackupCommand:
   {
      char *p;
      bfuncs->DebugMessage(ctx, fi, li, dbglvl,
         "handlexacl: plugincommand cmd=%s\n",
         (char *)value);
      /* check if already handled, if so it is a new command */
      if (pctx->cmd){
         free(pctx->cmd);
         pctx->cmd = NULL;
         pctx->acl = pctx->xattr = false;
      }
      pctx->cmd = strdup((char *)value);
      p = strchr(pctx->cmd, ':');
      if (!p) {
         bfuncs->JobMessage(ctx, fi, li, M_FATAL, 0,
            "Plugin terminator not found: %s\n",
            (char *)value);
         return bRC_Error;
      }
      *p++ = 0;           /* terminate plugin */
      pctx->fname = p;
      break;
   }

   default:
      bfuncs->DebugMessage(ctx, fi, li, dbglvl,
         "handlexacl: unknown event=%i\n",
         event->eventType);
      break;
   }
   return bRC_OK;
}

/*
 * Start the backup of a specific file
 */
static bRC startBackupFile(bpContext *ctx, struct save_pkt *sp)
{
   struct plugin_ctx *pctx = (struct plugin_ctx *)ctx->pContext;

   bfuncs->DebugMessage(ctx, fi, li, dbglvl,
      "handlexacl: startBackupFile\n");

   if (!pctx) {
      return bRC_Error;
   }

   sp->fname = pctx->fname;
   time_t now = time(NULL);
   sp->statp.st_mode = 0700 | S_IFREG;
   sp->statp.st_ctime = now;
   sp->statp.st_mtime = now;
   sp->statp.st_atime = now;
   sp->statp.st_size = 0;
   sp->statp.st_blksize = 4096;
   sp->statp.st_blocks = 1;
   sp->statp.st_nlink = 1;
   sp->statp.st_uid = 0;
   sp->statp.st_gid = 0;
   sp->type = FT_REGE;

   return bRC_OK;
}

/*
 * Done with backup of this file
 */
static bRC endBackupFile(bpContext *ctx)
{
   /*
    * We would return bRC_More if we wanted startBackupFile to be
    * called again to backup another file
    */
   bfuncs->DebugMessage(ctx, fi, li, dbglvl,
      "handlexacl: endBackupFile\n");
   return bRC_OK;
}

/*
 * Bacula is calling us to do the actual I/O
 */
static bRC pluginIO(bpContext *ctx, struct io_pkt *io)
{
   struct plugin_ctx *pctx = (struct plugin_ctx *)ctx->pContext;

   bfuncs->DebugMessage(ctx, fi, li, dbglvl,
      "handlexacl: pluginIO: %i\n",
      io->func);

   if (!pctx) {
      return bRC_Error;
   }

   io->status = 0;
   io->io_errno = 0;
   return bRC_OK;
}

/*
 * Bacula is notifying us that a plugin name string was found, and
 *   passing us the plugin command, so we can prepare for a restore.
 */
static bRC startRestoreFile(bpContext *ctx, const char *cmd)
{
   bfuncs->DebugMessage(ctx, fi, li, dbglvl,
      "handlexacl: startRestoreFile cmd=%s\n",
      cmd);
   return bRC_OK;
}

/*
 * Bacula is notifying us that the plugin data has terminated, so
 *  the restore for this particular file is done.
 */
static bRC endRestoreFile(bpContext *ctx)
{
   bfuncs->DebugMessage(ctx, fi, li, dbglvl,
      "handlexacl: endRestoreFile\n");
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
   bfuncs->DebugMessage(ctx, fi, li, dbglvl,
      "handlexacl: createFile\n");
   rp->create_status = CF_EXTRACT;
   return bRC_OK;
}

/*
 * We will get here if the File is a directory after everything
 * is written in the directory.
 */
static bRC setFileAttributes(bpContext *ctx, struct restore_pkt *rp)
{
   bfuncs->DebugMessage(ctx, fi, li, dbglvl,
      "handlexacl: setFileAttributes\n");
   return bRC_OK;
}

/* When using Incremental dump, all previous dumps are necessary */
static bRC checkFile(bpContext *ctx, char *fname)
{
   struct plugin_ctx *pctx = (struct plugin_ctx *)ctx->pContext;
   bfuncs->DebugMessage(ctx, fi, li, dbglvl,
      "handlexacl: checkFile\n");
   /* it is always better to response with Seen not OK :) */
   if (pctx->fname && strcmp(pctx->fname, fname) == 0) {
      return bRC_Seen;
   } else {
      return bRC_OK;
   }
}

/*
 * New Bacula Plugin API require this
 */
static bRC handleXACLdata(bpContext *ctx, struct xacl_pkt *xacl)
{
   struct plugin_ctx *pctx = (struct plugin_ctx *)ctx->pContext;
   const char * aclstr = "user:bacula:rwx------\n";
   const char * xattrstr = "Bacula:Rulez\n";

   bfuncs->DebugMessage(ctx, fi, li, dbglvl,
      "handlexacl: handleXACLdata: %p\n",
      xacl);

   if (!pctx) {
      return bRC_Error;
   }

   switch (xacl->func){
      case BACL_BACKUP:
         bfuncs->DebugMessage(ctx, fi, li, dbglvl,
            "handlexacl: ACL backup\n");
         if (!pctx->acl){
            pctx->acl = true;
            xacl->content = (char*)aclstr;
            xacl->count = strlen(xacl->content) + 1;
         } else {
            xacl->count = 0;
         }
         break;
      case BACL_RESTORE:
         if (xacl->count > 0){
            bfuncs->DebugMessage(ctx, fi, li, dbglvl,
               "handlexacl: ACL restore\n");
            if (xacl->count != (int)(strlen(aclstr) + 1) || strcmp(xacl->content, aclstr) != 0){
               bfuncs->DebugMessage(ctx, fi, li, dbglvl,
                  "handlexacl: ACL restore ERROR\n");
               bfuncs->JobMessage(ctx,fi, li, M_FATAL, 0,
                  "handlexacl: ACL restore ERROR\n");
               return bRC_Error;
            }
         }
         break;
      case BXATTR_BACKUP:
         bfuncs->DebugMessage(ctx, fi, li, dbglvl,
            "handlexacl: XATTR backup\n");
         if (!pctx->xattr){
            pctx->xattr = true;
            xacl->content = (char*)xattrstr;
            xacl->count = strlen(xacl->content) + 1;
         } else {
            xacl->count = 0;
         }
         break;
      case BXATTR_RESTORE:
         if (xacl->count > 0){
            bfuncs->DebugMessage(ctx, fi, li, dbglvl,
               "handlexacl: XATTR restore\n");
            if (xacl->count != (int)(strlen(xattrstr) + 1) || strcmp(xacl->content, xattrstr) != 0){
               bfuncs->DebugMessage(ctx, fi, li, dbglvl,
                  "handlexacl: XATTR restore ERROR\n");
               bfuncs->JobMessage(ctx,fi, li, M_FATAL, 0,
                  "handlexacl: XATTR restore ERROR\n");
               return bRC_Error;
            }
         }
         break;
   }
   return bRC_OK;
}

#ifdef __cplusplus
}
#endif
