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
 * A Verify Data plugin to check antivirus signature
 */

#define BUILD_PLUGIN

#include "bacula.h"
#include "fd_plugins.h"
#define USE_CMD_PARSER
#include "fd_common.h"
#include "findlib/bfile.h"          /* for bopen/bclose/bread/bwrite */
#include "lib/smartall.h"

#ifdef __cplusplus
extern "C" {
#endif

static const int dbglvl = 100;
static char *working;

#define PLUGIN_LICENSE      "AGPLv3"
#define PLUGIN_AUTHOR       "Eric Bollengier"
#define PLUGIN_DATE         "September 2021"
#define PLUGIN_VERSION      "1"
#define PLUGIN_DESCRIPTION  "Bacula Antivirus Plugin"

/* Forward referenced functions */
static bRC newPlugin(bpContext *ctx);
static bRC freePlugin(bpContext *ctx);
static bRC getPluginValue(bpContext *ctx, pVariable var, void *value);
static bRC setPluginValue(bpContext *ctx, pVariable var, void *value);
static bRC handlePluginEvent(bpContext *ctx, bEvent *event, void *value);
// static bRC startBackupFile(bpContext *ctx, struct save_pkt *sp);
// static bRC endBackupFile(bpContext *ctx);
static bRC pluginIO(bpContext *ctx, struct io_pkt *io);
// static bRC startRestoreFile(bpContext *ctx, const char *cmd);
// static bRC endRestoreFile(bpContext *ctx);
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
   NULL,                        // startBackupFile
   NULL,                        // endBackupFile
   NULL,                        // startRestoreFile
   NULL,                        // endRestoreFile
   pluginIO,
   NULL,                        // createFile
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

#define get_self(x) (((x)==NULL)?NULL:(antivirus*)((x)->pContext))

/* Plugin instance class */
class antivirus: public cmd_parser, public SMARTALLOC
{
private:
   bpContext *ctx;

public:
   BSOCKCORE    *bs;
   POOLMEM  *fname;
   char     *hostname;
   int      port;
   antivirus(bpContext *bpc): cmd_parser(), ctx(bpc), bs(NULL), fname(NULL), hostname(NULL), port(3310) {
   };

   virtual ~antivirus() {
      free_and_null_pool_memory(fname);
   };

   /* Wait to be called to allocate memory */
   void init_mem() {
      fname = get_pool_memory(PM_FNAME);
   };
   void report_virus(const char *fname, const char *msg);
};

void antivirus::report_virus(const char *name, const char *msg)
{
   fileevent_pkt event;
   Jmsg(ctx, M_ERROR, "%s Virus detected \"%s\"\n", name, msg);
   bfuncs->getBaculaValue(ctx, bVarFileIndex, &event.FileIndex);
   event.Severity = 100;
   event.Type = FILEEVENT_TYPE_ANTIVIRUS;
   bstrncpy(event.Source, "Clamav", sizeof(event.Source));
   bstrncpy(event.Description, msg, sizeof(event.Description));
   bfuncs->AddFileEvent(ctx, &event);
}

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
   antivirus *self = New(antivirus(ctx));
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
   antivirus *self = get_self(ctx);
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
   int i=0;
   if (!ctx) {
      return bRC_Error;
   }
   antivirus *self = get_self(ctx);

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
      if ((i =self->find_arg_with_value("hostname")) >= 1) {
         self->hostname = self->argv[i];
      } else {
         self->hostname = (char *)"localhost";
      }
      if ((i =self->find_arg_with_value("port")) >= 1) {
         self->port = atoi(self->argv[i]);
      } else {
         self->port = 3310;
      }
      Dmsg0(50, "Register event bEventVerifyStream\n");
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
      Dmsg0(50, "Register event bEventVerifyStream\n");
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
      /* Plugin = "antivirus" */
      return bRC_Error;

   default:
//    Dmsg(ctx, dbglvl, "unknown event=%d\n", event->eventType);
      break;
   }
   return bRC_OK;
}

static bRC startVerifyFile(bpContext *ctx, struct restore_pkt *rp)
{
   antivirus *self = get_self(ctx);
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

#define retry_interval 1
#define max_retry_time 3
#define heart_beat 100
#define socket_name "bacula_antivirus-fd"
#define service NULL
#define max_read_size 4096

POOLMEM *pack_chunk(POOLMEM *p, uint32_t count)
{
   typedef struct {
      uint32_t size;
      char *buf;
   } chunkstruct;

   if (p) {
      chunkstruct *s = (chunkstruct*)(p - sizeof(uint32_t));
      s->size = htonl(count);
      return (POOLMEM *)s;
   }

   return NULL;
}

/*
 * Bacula is calling us to do the actual I/O
 */
static bRC pluginIO(bpContext *ctx, struct io_pkt *io)
{
   if (!ctx) {
      return bRC_Error;
   }
   antivirus *self = get_self(ctx);
   if (!self) {
      return bRC_Error;
   }
    
   io->status = 0;
   io->io_errno = 0;
   switch(io->func) {
   case IO_OPEN:
      Dmsg(ctx, dbglvl, "antivirus: Initialize antivirus\n");
      
      self->bs = New(BSOCKCORE);
      if (!self->bs->connect(get_jcr_from_tsd(), retry_interval, max_retry_time, heart_beat, socket_name, const_cast<char*>(self->hostname), service, self->port, 0)) {
         /* connect failed*/
         io->io_errno = errno;
         io->status = -1;
         Jmsg(ctx, M_ERROR, "Connection to %s:%d failed. fname=%s ERR=%s\n", self->hostname, self->port, self->fname, strerror(errno));
         delete (self->bs);
         self->bs = NULL;
         return bRC_Error;
      }
      if (self->bs && self->bs->is_open()) {
         memset((void*)self->bs->msg, 0, strlen("zINSTREAM")+1);
         self->bs->msglen = pm_strcpy(self->bs->msg, "zINSTREAM");
         self->bs->msglen++;
         if (! self->bs->send()) {
            Jmsg(ctx, M_ERROR, "INSTREAM failed: fname=%s ERR=%s\n", self->fname, strerror(errno));
            delete (self->bs);
            self->bs = NULL;
            return bRC_Error;
         }
      }
      break;

   case IO_READ:
      return bRC_Error;

   case IO_WRITE:
      Dmsg(ctx, dbglvl, "antivirus: writing\n");
      if (self->bs && self->bs->is_open()) {
         //Dmsg(ctx, dbglvl, "verifyplug: writing\n");
         POOLMEM *p = pack_chunk(io->buf, io->count);
         //Dmsg(ctx, dbglvl, "verifyplug: pack_chunk write %d %s\nOuput :%s",io->count, io->buf, p);
         if (!self->bs->send2(p, io->count+sizeof(uint32_t))) {
            Jmsg(ctx, M_ERROR, "INSTREAM WRITE failed: fname=%s ERR=%s\n", self->fname, strerror(errno));
            delete (self->bs);
            self->bs = NULL;
            return bRC_Error;
         }
      }
      break;

   /* Cleanup things during close */
   case IO_CLOSE:
      io->status = 0;
      if (self->bs && self->bs->is_open()) {
         //Dmsg(ctx, dbglvl, "verifyplug: close\n");
         memset(self->bs->msg, 0, sizeof(uint32_t));
         self->bs->msglen = 0;
         POOLMEM *p = pack_chunk(self->bs->msg, self->bs->msglen);
         //Dmsg(ctx, dbglvl, "verifyplug: pack_chunk close %d %s\nOuput :%s",io->count, io->buf, p);
         if (!self->bs->send2(p, self->bs->msglen+sizeof(uint32_t))) {
            Jmsg(ctx, M_ERROR, "INSTREAM CLOSE failed: fname=%s ERR=%s\n", self->fname, strerror(errno));
            delete (self->bs);
            self->bs = NULL;
            return bRC_Error;
         }

         /* read the reply */
         self->bs->msg[0] = 0;
         self->bs->msglen = self->bs->socketRead(self->bs->m_fd, self->bs->msg, max_read_size);
         if (self->bs->msglen > 0) {
             self->bs->msg[self->bs->msglen] = 0;
             if (strstr(self->bs->msg, "OK")) {
                Dmsg(ctx, dbglvl, "%s %s\n", self->fname, self->bs->msg);
             } else {
                self->report_virus(self->fname, self->bs->msg);
             }
         }
         self->bs->close();
         delete (self->bs);
      }
      break;

   case IO_SEEK:
      /* Seek not needed for this plugin, we don't use real sparse file */
      io->status = 0;
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
