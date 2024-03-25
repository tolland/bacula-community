/*
   Bacula(R) - The Network Backup Solution

   Copyright (C) 2000-2023 Kern Sibbald

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
 * A simple Options{} plugin using rot13 encoding
 */

#define BUILD_PLUGIN

#include "bacula.h"
#include "fd_plugins.h"
#include "fd_common.h"
#include "findlib/bfile.h"          /* for bopen/bclose/bread/bwrite */


#ifdef __cplusplus
extern "C" {
#endif

static const int dbglvl = 100;
static char *working;

#define PLUGIN_LICENSE      "AGPLv3"
#define PLUGIN_AUTHOR       "Eric Bollengier"
#define PLUGIN_DATE         "August 2011"
#define PLUGIN_VERSION      "1"
#define PLUGIN_DESCRIPTION  "Bacula Rot13 Plugin"

/* Forward referenced functions */
static bRC newPlugin(bpContext *ctx);
static bRC freePlugin(bpContext *ctx);
static bRC getPluginValue(bpContext *ctx, pVariable var, void *value);
static bRC setPluginValue(bpContext *ctx, pVariable var, void *value);
static bRC handlePluginEvent(bpContext *ctx, bEvent *event, void *value);
static bRC startBackupFile(bpContext *ctx, struct save_pkt *sp);
static bRC handleBackupFile(bpContext *ctx, struct save_pkt *sp);
static bRC endBackupFile(bpContext *ctx);
static bRC pluginIO(bpContext *ctx, struct io_pkt *io);
static bRC startRestoreFile(bpContext *ctx, const char *cmd);
static bRC endRestoreFile(bpContext *ctx);
static bRC createFile(bpContext *ctx, struct restore_pkt *rp);
static bRC setFileAttributes(bpContext *ctx, struct restore_pkt *rp);
static bRC checkFile(bpContext *ctx, char *fname);

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
   checkFile
};

#define get_self(x) (((x)==NULL)?NULL:(rot13*)((x)->pContext))

/* Plugin instance class */
class rot13: public SMARTALLOC
{
private:
   bpContext *ctx;

public:
   BFILE    fd;                 /* Main file descriptor read/write */

   rot13(bpContext *bpc) { 
      binit(&fd);
   }
   ~rot13() {
   }

   /* Wait to be called to allocate memory */
   void init_mem() {
   }
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
   rot13 *self = New(rot13(ctx));
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
   if (!ctx) {
      return bRC_Error;
   }
   rot13 *self = get_self(ctx);
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
   if (!ctx) {
      return bRC_Error;
   }
   rot13 *self = get_self(ctx);

   if (!self) {
      return bRC_Error;
   }
   /*
    * Most events don't interest us so we ignore them.
    *   the printfs are so that plugin writers can enable them to see
    *   what is really going on.
    */
   switch (event->eventType) {
   case bEventHandleBackupFile:
      return handleBackupFile(ctx, (struct save_pkt *)value);
   case bEventPluginCommand:
      break;
   case bEventVssPrepareSnapshot:
      break;
   case bEventJobStart:
//    Dmsg(ctx, dbglvl, "JobStart=%s\n", (char *)value);
      break;
   case bEventJobEnd:
//    Dmsg(ctx, dbglvl, "JobEnd\n");
      break;
   case bEventStartBackupJob:
//    Dmsg(ctx, dbglvl, "StartBackupJob\n");
      break;
   case bEventEndBackupJob:
//    Dmsg(ctx, dbglvl, "EndBackupJob\n");
      break;
   case bEventLevel:
//    Dmsg(ctx, dbglvl, "JobLevel=%c %d\n", (int)value, (int)value);
      break;
   case bEventSince:
//    Dmsg(ctx, dbglvl, "since=%d\n", (int)value);
      break;

   case bEventStartRestoreJob:
//    Dmsg(ctx, dbglvl, "StartRestoreJob\n");
      break;

   case bEventEndRestoreJob:
//    Dmsg(ctx, dbglvl, "EndRestoreJob\n");
      break;

   /* Plugin command e.g. plugin = <plugin-name>:<name-space>:read command:write command */
   case bEventRestoreCommand:
//    Dmsg(ctx, dbglvl, "EventRestoreCommand cmd=%s\n", (char *)value);
      self->init_mem();
      break;
   case bEventEstimateCommand:
      /* Fall-through wanted */
   case bEventBackupCommand:
      /* Plugin = "rot13" */
      return bRC_Error;

   default:
//    Dmsg(ctx, dbglvl, "unknown event=%d\n", event->eventType);
      break;
   }
   return bRC_OK;
}

/* 
 * Start the backup of a specific file
 */

static bRC startBackupFile(bpContext *ctx, struct save_pkt *sp)
{
   return bRC_Error;
}

/*
 * Used by Options { Plugin= }
 */
static bRC handleBackupFile(bpContext *ctx, struct save_pkt *sp)
{
   /* You can add custom filter here, the plugin command line
    * is in sp->cmd
    */
   int len = strlen(sp->fname);
   if (len > 4) {
      Dmsg(ctx, dbglvl, "rot13: fname=%s ext=%s\n", 
           sp->fname, sp->fname + len - 4);
      if (!strcmp(sp->fname + len - 4, ".txt")) {
         return bRC_OK;
      }
   }
   return bRC_Core;
}

/*
 * Done with backup of this file
 */
static bRC endBackupFile(bpContext *ctx)
{
   /* Should not be called
    */
   return bRC_Error;
}

/*
 * Bacula is calling us to do the actual I/O
 */
static bRC pluginIO(bpContext *ctx, struct io_pkt *io)
{
   if (!ctx) {
      return bRC_Error;
   }
   rot13 *self = get_self(ctx);
   if (!self) {
      return bRC_Error;
   }
    
   io->status = 0;
   io->io_errno = 0;
   switch(io->func) {
   case IO_OPEN:
      Dmsg(ctx, dbglvl, "rot13: opening the file\n");
      
      if (bopen(&self->fd, io->fname, io->flags, io->mode) < 0) {
         io->io_errno = errno;
         io->status = -1;
         Jmsg(ctx, M_ERROR, "Open failed: fname=%s ERR=%s\n", io->fname, strerror(errno));
         return bRC_Error;
      }
      io->status = self->fd.fid;
      break;

   case IO_READ:
      if (!is_bopen(&self->fd)) {
         Jmsg(ctx, M_ERROR, "Logic error: NULL read FD\n");
         return bRC_Error;
      }

      /* Read data from file */
      io->status = bread(&self->fd, io->buf, io->count);

      if (io->status == 0 && self->fd.berrno) {
         Jmsg(ctx, M_ERROR, 
              "Pipe read error: ERR=%s\n", strerror(self->fd.berrno));
         Dmsg(ctx, dbglvl, 
              "Pipe read error: ERR=%s\n", strerror(self->fd.berrno));
         return bRC_Error;
      }

      for (int i=0 ; i < io->status ; i++) {
         io->buf[i] += 13;
      }
      
      break;

   case IO_WRITE:
      Dmsg(ctx, dbglvl, "rot13: writing\n");
      if (!is_bopen(&self->fd)) {
         Jmsg(ctx, M_ERROR, "Logic error: NULL write FD\n");
         return bRC_Error;
      }

      for (int i=0 ; i < io->count ; i++) {
         io->buf[i] -= 13;
      }
      io->status = bwrite(&self->fd, io->buf, io->count);
      
      if (io->status == 0 && self->fd.berrno != 0) {
         Jmsg(ctx, M_ERROR, 
              "Write error\n");
         Dmsg(ctx, dbglvl, 
              "Write error: ERR=%s\n", strerror(self->fd.berrno));
         return bRC_Error;
      }

      break;

   /* Cleanup things during close */
   case IO_CLOSE:
      Dmsg(ctx, dbglvl, "rot13: closing\n");
      if (!is_bopen(&self->fd)) {
         //Jmsg(ctx, M_ERROR, "Logic error: NULL FD on rot13 close\n");
         return bRC_Error;
      }
      io->status = bclose(&self->fd);
      break;

   case IO_SEEK:
      if (!is_bopen(&self->fd)) {
         Jmsg(ctx, M_ERROR, "Logic error: NULL FD on rot13 seek\n");
         return bRC_Error;
      }
      /* Seek not needed for this plugin, we don't use real sparse file */
      io->status = 0;
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
// Dmsg(ctx, dbglvl, "startRestoreFile cmd=%s\n", cmd);
   return bRC_OK;
}

/*
 * Bacula is notifying us that the plugin data has terminated, so
 *  the restore for this particular file is done.
 */
static bRC endRestoreFile(bpContext *ctx)
{
// Dmsg(ctx, dbglvl, "endRestoreFile\n");
   return bRC_OK;
}

/*
 * This is called during restore to create the file (if necessary)
 * We must return in rp->create_status:
 *   
 *  CF_ERROR    -- error
 *  CF_CORE     -- let bacula's core handles the file creation
 *  CF_SKIP     -- skip processing this file
 *  CF_EXTRACT  -- extract the file (i.e.call i/o routines)
 *  CF_CREATED  -- created, but no content to extract (typically directories)
 *
 */
static bRC createFile(bpContext *ctx, struct restore_pkt *rp)
{
   rp->create_status = CF_CORE;
   return bRC_OK;
}

/*
 * We will get here if the File is a directory after everything
 * is written in the directory.
 */
static bRC setFileAttributes(bpContext *ctx, struct restore_pkt *rp)
{
// Dmsg(ctx, dbglvl, "setFileAttributes\n");
   rp->create_status = CF_CORE;
   return bRC_OK;
}

/* TODO: cleanup database file with deleted files */
static bRC checkFile(bpContext *ctx, char *fname)
{
   /* TODO: Remove working/fname.* */
   return bRC_OK;
}

#ifdef __cplusplus
}
#endif
