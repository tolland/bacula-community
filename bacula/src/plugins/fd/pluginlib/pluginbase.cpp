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
/**
 * @file pluginbase.cpp
 * @author Radosław Korzeniewski (radoslaw@korzeniewski.net)
 * @brief This is a Bacula File Daemon generic plugin interface.
 * @version 1.0.0
 * @date 2021-04-08
 *
 * @copyright Copyright (c) 2021 All rights reserved. IP transferred to Bacula Systems according to agreement.
 */

#include "pluginbase.h"
#include <sys/stat.h>
#include <signal.h>
#include <sys/select.h>

/*
 * libbac uses its own sscanf implementation which is not compatible with
 * libc implementation, unfortunately.
 * use bsscanf for Bacula sscanf flavor
 */
#ifdef sscanf
#undef sscanf
#endif

#define pluginclass(ctx)     pluginlib::PLUGINBCLASS *self = (pluginlib::PLUGINBCLASS*)ctx->pContext;

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
static bRC plugincheckFile(bpContext *ctx, char *fname);
static bRC handleXACLdata(bpContext *ctx, struct xacl_pkt *xacl);
static bRC queryParameter(bpContext *ctx, struct query_pkt *qp);
static bRC metadataRestore(bpContext *ctx, struct meta_pkt *mp);

/* Pointers to Bacula functions */
bFuncs *bfuncs = NULL;
bInfo *binfo = NULL;

static pFuncs pluginFuncs =
{
   sizeof(pluginFuncs),
   FD_PLUGIN_INTERFACE_VERSION,

   /* Entry points into plugin */
   newPlugin,
   freePlugin,
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
   plugincheckFile,
   handleXACLdata,
   NULL,                        /* No restore file list */
   NULL,                        /* No checkStream */
   queryParameter,
   metadataRestore,
};

#ifdef __cplusplus
extern "C" {
#endif

/* Plugin Information structure */
static pInfo pluginInfo = {
   sizeof(pluginInfo),
   FD_PLUGIN_INTERFACE_VERSION,
   FD_PLUGIN_MAGIC,
   PLUGIN_LICENSE,
   PLUGIN_AUTHOR,
   PLUGIN_DATE,
   PLUGIN_VERSION,
   PLUGIN_DESCRIPTION,
};

/*
 * Plugin called here when it is first loaded
 */
bRC DLL_IMP_EXP loadPlugin(bInfo *lbinfo, bFuncs *lbfuncs, pInfo ** pinfo, pFuncs ** pfuncs)
{
   bfuncs = lbfuncs;               /* set Bacula function pointers */
   binfo = lbinfo;

   Dmsg4(DINFO, "%s Plugin version %s%s %s\n", PLUGINNAME, PLUGIN_VERSION, VERSIONGIT_STR, PLUGIN_DATE);

   *pinfo = &pluginInfo;           /* return pointer to our info */
   *pfuncs = &pluginFuncs;         /* return pointer to our functions */

   return bRC_OK;
}

/*
 * Plugin called here when it is unloaded, normally when Bacula is going to exit.
 */
bRC DLL_IMP_EXP unloadPlugin()
{
   return bRC_OK;
}

#ifdef __cplusplus
}
#endif


/*
 * Called here to make a new instance of the plugin -- i.e. when
 * a new Job is started.  There can be multiple instances of
 * each plugin that are running at the same time.  Your
 * plugin instance must be thread safe and keep its own
 * local data.
 */
static bRC newPlugin(bpContext *ctx)
{
   pluginlib::PLUGINBCLASS *self = (pluginlib::PLUGINBCLASS*)new_plugin_factory(ctx);

   if (!self) {
      DMSG1(ctx, DERROR, "newPlugin: Cannot build plugin %s!\n", PLUGINNAME);
      return bRC_Error;
   }

   DMSG(ctx, DINFO, "newPlugin: %s\n", PLUGINNAME);
   ctx->pContext = (void *)self;

   pthread_t mythid = pthread_self();
   DMSG2(ctx, DVDEBUG, "pContext = %p thid = %p\n", self, mythid);

   /* setup plugin */
   self->setup_plugin(ctx);

   return bRC_OK;
}

/*
 * Release everything concerning a particular instance of
 *  a plugin. Normally called when the Job terminates.
 */
static bRC freePlugin(bpContext *ctx)
{
   ASSERT_CTX;

   pluginclass(ctx);
   DMSG(ctx, D1, "freePlugin this=%p\n", self);
   if (!self){
      return bRC_Error;
   }
   delete self;

   return bRC_OK;
}

/*
 * Called by core code to get a variable from the plugin.
 *   Not currently used.
 */
static bRC getPluginValue(bpContext *ctx, pVariable var, void *value)
{
   ASSERT_CTX;

   DMSG0(ctx, D3, "getPluginValue called.\n");
   pluginclass(ctx);
   return self->getPluginValue(ctx,var, value);
}

/*
 * Called by core code to set a plugin variable.
 *  Not currently used.
 */
static bRC setPluginValue(bpContext *ctx, pVariable var, void *value)
{
   ASSERT_CTX;

   DMSG0(ctx, D3, "setPluginValue called.\n");
   pluginclass(ctx);
   return self->setPluginValue(ctx, var, value);
}

/*
 * Called by Bacula when there are certain events that the
 *   plugin might want to know.  The value depends on the
 *   event.
 */
static bRC handlePluginEvent(bpContext *ctx, bEvent *event, void *value)
{
   ASSERT_CTX;
   if (!event) {
      return bRC_Error;
   }

   pthread_t mythid = pthread_self();
   pluginclass(ctx);
   DMSG3(ctx, D1, "handlePluginEvent (%i) pContext = %p thid = %p\n", event->eventType, self, mythid);
   return self->handlePluginEvent(ctx, event, value);
}

/*
 * Called when starting to backup a file. Here the plugin must
 *  return the "stat" packet for the directory/file and provide
 *  certain information so that Bacula knows what the file is.
 *  The plugin can create "Virtual" files by giving them
 *  a name that is not normally found on the file system.
 */
static bRC startBackupFile(bpContext *ctx, struct save_pkt *sp)
{
   ASSERT_CTX;
   if (!sp) {
      return bRC_Error;
   }

   DMSG0(ctx, D1, "startBackupFile.\n");
   pluginclass(ctx);
   return self->startBackupFile(ctx, sp);
}

/*
 * Done backing up a file.
 */
static bRC endBackupFile(bpContext *ctx)
{
   ASSERT_CTX;

   DMSG0(ctx, D1, "endBackupFile.\n");
   pluginclass(ctx);
   return self->endBackupFile(ctx);
}

/*
 * Called when starting restore the file, right after a createFile().
 */
static bRC startRestoreFile(bpContext *ctx, const char *cmd)
{
   ASSERT_CTX;

   DMSG1(ctx, D1, "startRestoreFile: %s\n", NPRT(cmd));
   pluginclass(ctx);
   return self->startRestoreFile(ctx, cmd);
}

/*
 * Done restore the file.
 */
static bRC endRestoreFile(bpContext *ctx)
{
   ASSERT_CTX;

   DMSG0(ctx, D1, "endRestoreFile.\n");
   pluginclass(ctx);
   return self->endRestoreFile(ctx);
}

/*
 * Do actual I/O. Bacula calls this after startBackupFile
 *   or after startRestoreFile to do the actual file
 *   input or output.
 */
static bRC pluginIO(bpContext *ctx, struct io_pkt *io)
{
   ASSERT_CTX;

   DMSG0(ctx, DVDEBUG, "pluginIO.\n");
   pluginclass(ctx);
   return self->pluginIO(ctx, io);
}

/*
 * Called here to give the plugin the information needed to
 *  re-create the file on a restore.  It basically gets the
 *  stat packet that was created during the backup phase.
 *  This data is what is needed to create the file, but does
 *  not contain actual file data.
 */
static bRC createFile(bpContext *ctx, struct restore_pkt *rp)
{
   ASSERT_CTX;

   DMSG0(ctx, D1, "createFile.\n");
   pluginclass(ctx);
   return self->createFile(ctx, rp);
}

/*
 * Called after the file has been restored. This can be used to
 *  set directory permissions, ...
 */
static bRC setFileAttributes(bpContext *ctx, struct restore_pkt *rp)
{
   ASSERT_CTX;

   DMSG0(ctx, D1, "setFileAttributes.\n");
   pluginclass(ctx);
   return self->setFileAttributes(ctx, rp);
}

/*
 * handleXACLdata used for ACL/XATTR backup and restore
 */
static bRC handleXACLdata(bpContext *ctx, struct xacl_pkt *xacl)
{
   ASSERT_CTX;

   DMSG(ctx, D1, "handleXACLdata: %i\n", xacl->func);
   pluginclass(ctx);
   return self->handleXACLdata(ctx, xacl);
}

/*
 * QueryParameter interface
 */
static bRC queryParameter(bpContext *ctx, struct query_pkt *qp)
{
   ASSERT_CTX;

   DMSG2(ctx, D1, "queryParameter: cmd:%s param:%s\n", qp->command, qp->parameter);
   pluginclass(ctx);
   return self->queryParameter(ctx, qp);
}

/*
 * Metadata Restore interface
 */
static bRC metadataRestore(bpContext *ctx, struct meta_pkt *mp)
{
   ASSERT_CTX;

   DMSG2(ctx, D1, "metadataRestore: %d %d\n", mp->total_size, mp->type);
   pluginclass(ctx);
   return self->metadataRestore(ctx, mp);
}

/*
 * checkFile used for accurate mode backup
 */
static bRC plugincheckFile(bpContext * ctx, char *fname)
{
   ASSERT_CTX;

   DMSG(ctx, D3, "checkFile for: %s\n", fname);
   pluginclass(ctx);
   return self->checkFile(ctx, fname);
}
