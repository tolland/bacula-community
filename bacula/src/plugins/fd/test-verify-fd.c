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
 * A Verify Data plugin to check test-verify signature
 */

#define BUILD_PLUGIN

#include "bacula.h"
#include "fd_plugins.h"
#define USE_CMD_PARSER
#include "fd_common.h"
#include "findlib/bfile.h"          /* for bopen/bclose/bread/bwrite */


#ifdef __cplusplus
extern "C" {
#endif

static const int dbglvl = 100;
static char *working;

#define PLUGIN_LICENSE      "Bacula Systems(R) SA"
#define PLUGIN_AUTHOR       "Eric Bollengier"
#define PLUGIN_DATE         "September 2021"
#define PLUGIN_VERSION      "1"
#define PLUGIN_DESCRIPTION  "Bacula Test-Verify Plugin"

/* Forward referenced functions */
static bRC newPlugin(bpContext *ctx);
static bRC freePlugin(bpContext *ctx);
static bRC getPluginValue(bpContext *ctx, pVariable var, void *value);
static bRC setPluginValue(bpContext *ctx, pVariable var, void *value);
static bRC handlePluginEvent(bpContext *ctx, bEvent *event, void *value);
static bRC pluginIO(bpContext *ctx, struct io_pkt *io);
static bRC startVerifyFile(bpContext *ctx, struct restore_pkt *rp);
static bRC endVerifyFile(bpContext *ctx);
static bRC setFileAttributes(bpContext *ctx, struct restore_pkt *rp);

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

/* Plugin entry points for Bacula
 *
 *  A verify plugin do not have to implement specific backup or restore
 *  functions. The plugin cannot be set in the FileSet, it is loaded at
 *  startup and will register to the bEventVerifyStream event.
 */
static pFuncs pluginFuncs = {
   sizeof(pluginFuncs),
   FD_PLUGIN_INTERFACE_VERSION,

   /* Entry points into plugin */
   newPlugin,                         /* new plugin instance */
   freePlugin,                        /* free plugin instance */
   getPluginValue,
   setPluginValue,
   handlePluginEvent,
   NULL,                              /* startBackupFile */
   NULL,                              /* endBackupFile */
   NULL,                              /* startRestoreFile */
   NULL,                              /* endRestoreFile */
   pluginIO,
   NULL,                              /* createFile */
   setFileAttributes,
   NULL,                        // checkFile
   NULL,                        // handleXACLdata
   NULL,                        // restoreFileList
   NULL,                        // checkStream
   NULL,                        // queryParameter
   NULL,                        // metadataRestore
   startVerifyFile,
   endVerifyFile
};

#define get_self(x) (((x)==NULL)?NULL:(verifyplug*)((x)->pContext))

/* Plugin instance class */
class verifyplug: public cmd_parser, public SMARTALLOC
{
private:
   bpContext *ctx;

public:
   BPIPE    *pfd;                 /* Main file descriptor read/write */
   POOLMEM  *fname;
   char     *pattern;
   
   verifyplug(bpContext *bpc): cmd_parser(), ctx(bpc), pfd(NULL), fname(NULL), pattern(NULL) {
   };

   virtual ~verifyplug() {
      free_and_null_pool_memory(fname);
   };

   /* Wait to be called to allocate memory */
   void init_mem() {
      fname = get_pool_memory(PM_FNAME);
   };
};

/*
 * loadPlugin() and unloadPlugin() are entry points that are
 *  exported, so Bacula can directly call these two entry points
 *  they are common to all Bacula plugins.
 */
/*
 * External entry point called by Bacula to "load the plugin
 */
bRC DLL_IMP_EXP
loadPlugin(bInfo *lbinfo, bFuncs *lbfuncs, pInfo **pinfo, pFuncs **pfuncs)
{
   bfuncs = lbfuncs;                  /* set Bacula funct pointers */
   binfo  = lbinfo;
   *pinfo  = &pluginInfo;             /* return pointer to our info */
   *pfuncs = &pluginFuncs;            /* return pointer to our functions */
   bfuncs->getBaculaValue(NULL, bVarWorkingDir, (void *)&working);
   return bRC_OK;
}

/*
 * External entry point to unload the plugin 
 */
bRC DLL_IMP_EXP
unloadPlugin() 
{
// Dmsg(NULL, dbglvl, "Unloaded\n");
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
   verifyplug *self = New(verifyplug(ctx));
   if (!self) {
      return bRC_Error;
   }
   ctx->pContext = (void *)self;        /* set our context pointer */
   return bRC_OK;
}

/*
 * Free a plugin instance, i.e. release our private storage
 */
static bRC freePlugin(bpContext *ctx)
{
   verifyplug *self = get_self(ctx);
   if (!self) {
      return bRC_Error;
   }
   delete self;
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
   int i;
   verifyplug *self = get_self(ctx);

   if (!self) {
      return bRC_Error;
   }
   /*
    * Most events don't interest us so we ignore them.
    *   the printfs are so that plugin writers can enable them to see
    *   what is really going on.
    */
   switch (event->eventType) {

   case bEventPluginOptions:
      Jmsg(ctx, M_INFO, "Got plugin command = %s\n", (char *)value);
      self->parse_cmd((char *)value);
      bfuncs->registerBaculaEvents(ctx, bEventVerifyStream);
      if ((i = self->find_arg_with_value("pattern")) >= 1) {
         self->pattern = self->argv[i];
      } else {
         self->pattern = (char *)"notfound";
      }
      Dmsg0(0, "Register event bEventVerifyStream\n");
      break;
   case bEventVssPrepareSnapshot:
      break;
   case bEventJobStart:
//    Dmsg(ctx, dbglvl, "JobStart=%s\n", (char *)value);
      break;
   case bEventJobEnd:
//    Dmsg(ctx, dbglvl, "JobEnd\n");
      break;
   case bEventLevel:
      break;
   case bEventSince:
//    Dmsg(ctx, dbglvl, "since=%d\n", (int)value);
      break;
   case bEventStartVerifyJob:
      self->init_mem();
      break;
   case bEventHandleBackupFile:
      /* Fall-through wanted */
   case bEventStartBackupJob:
      /* Fall-through wanted */
   case bEventEndBackupJob:
      /* Fall-through wanted */
   case bEventStartRestoreJob:
      /* Fall-through wanted */
   case bEventEndRestoreJob:
      /* Fall-through wanted */
   case bEventRestoreCommand:
      /* Fall-through wanted */
   case bEventEstimateCommand:
      /* Fall-through wanted */
   case bEventBackupCommand:
      /* Plugin = "verifyplug" */
      return bRC_Error;

   default:
//    Dmsg(ctx, dbglvl, "unknown event=%d\n", event->eventType);
      break;
   }
   return bRC_OK;
}

static bRC startVerifyFile(bpContext *ctx, struct restore_pkt *rp)
{
   verifyplug *self = get_self(ctx);
   pm_strcpy(self->fname, rp->ofname);
   return bRC_OK;
}

/*
 * Done with verify of this file
 */
static bRC endVerifyFile(bpContext *ctx)
{
   return bRC_OK;
}

/*
 * Bacula is calling us to do the actual I/O
 */
static bRC pluginIO(bpContext *ctx, struct io_pkt *io)
{
   char buf[512];
   verifyplug *self = get_self(ctx);
   ssize_t nb;
   if (!self) {
      return bRC_Error;
   }
    
   io->status = 0;
   io->io_errno = 0;
   switch(io->func) {
   case IO_OPEN:
      Dmsg(ctx, dbglvl, "verifyplug: Initialize verifyplug\n");
      // Works only on GNU
      bsnprintf(buf, sizeof(buf)-1, "grep -q '%s'", self->pattern);
      if ((self->pfd = open_bpipe(buf, 0, "rw", NULL)) == NULL) {
         io->io_errno = errno;
         io->status = -1;
         Jmsg(ctx, M_ERROR, "Open failed: fname=%s ERR=%s\n", io->fname, strerror(errno));
         return bRC_Error;
      }
      io->status = fileno(self->pfd->wfd);
      break;

   case IO_READ:
      return bRC_Error;

   case IO_WRITE:
      Dmsg(ctx, dbglvl, "verifyplug: writing\n");
      nb = write(fileno(self->pfd->wfd), io->buf, io->count);
      
      if (nb != io->count) {
         Jmsg(ctx, M_ERROR, 
              "Write error\n");
         return bRC_Error;
      }
      break;

   /* Cleanup things during close */
   case IO_CLOSE:
      if (close_bpipe(self->pfd) == 0) {
         Jmsg(ctx, M_INFO, _("File %s matching pattern\n"), self->fname);
      }
      break;

   case IO_SEEK:
      /* Seek not needed for this plugin, we don't use real sparse file */
      break;
   }
   return bRC_OK;
}

/*
 * We will get here if the File is a directory after everything
 * is written in the directory.
 */
static bRC setFileAttributes(bpContext *ctx, struct restore_pkt *rp)
{
// Dmsg(ctx, dbglvl, "setFileAttributes\n");
   return bRC_Error;
}

#ifdef __cplusplus
}
#endif
