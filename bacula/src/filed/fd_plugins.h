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
 * Application Programming Interface (API) definition for Bacula Plugins
 *
 * Kern Sibbald, October 2007
 *
 */

#ifndef __FD_PLUGINS_H
#define __FD_PLUGINS_H

#ifndef _BACULA_H
#ifdef __cplusplus
/* Workaround for SGI IRIX 6.5 */
#define _LANGUAGE_C_PLUS_PLUS 1
#endif
#define _REENTRANT    1
#define _THREAD_SAFE  1
#define _POSIX_PTHREAD_SEMANTICS 1
#define _FILE_OFFSET_BITS 64
#define _LARGEFILE_SOURCE 1
#define _LARGE_FILES 1
#endif  /* ! _BACULA_H */

#include <sys/types.h>

#if defined(HAVE_WIN32)
#if defined(HAVE_MINGW)
#include "mingwconfig.h"
#else
#include "winconfig.h"
#endif
#else  /* !HAVE_WIN32 */
#ifndef __CONFIG_H
#include "config.h"
#define __CONFIG_H
#endif
#endif

#include "../src/version.h"
#include "bc_types.h"
#include "lib/plugins.h"
#include <sys/stat.h>
#ifdef HAVE_WIN32
#include "../win32/filed/vss.h"
#endif

/*
 * This packet is used for the restore objects
 *  It is passed to the plugin when restoring
 *  the object.
 */
struct restore_object_pkt {
   int32_t pkt_size;                  /* size of this packet */
   char *object_name;                 /* Object name */
   char *object;                      /* restore object data to save */
   char *plugin_name;                 /* Plugin name */
   int32_t object_type;               /* FT_xx for this file */
   int32_t object_len;                /* restore object length */
   int32_t object_full_len;           /* restore object uncompressed length */
   int32_t object_index;              /* restore object index */
   int32_t object_compression;        /* set to compression type */
   int32_t stream;                    /* attribute stream id */
   uint32_t JobId;                    /* JobId object came from */
   int32_t pkt_end;                   /* end packet sentinel */
};

/*
 * Packet to allow plugin to set stream
 */
struct stream_pkt {
   int32_t pkt_size;                  /* size of this packet */
   int32_t stream;                    /* Stream */
   int32_t pkt_end;                   /* end packet sentinel */
};

struct restore_object {
   char *object_name;                 /* Object name to create */
   char *object;                      /* restore object data to save */
   int32_t object_len;                /* restore object length */
   int32_t index;                     /* restore object index */
   int32_t object_compression;        /* set to compression type */
};

/* Types of plugin objects */
//TODO probably more specific db types needed
#define PLUGIN_OBJECT_DATABASE "database" /* For all database plugins */

/* Possible statuses of plugin objects. Based on Job status */
#define PLUG_OBJ_STATUS_UNSET       'U' /* Status was not set at all by the plugin */
#define PLUG_OBJ_STATUS_TERMINATED  'T' /* Plugin object is OK */
#define PLUG_OBJ_STATUS_WARNING     'W' /* Plugin object OK, with some non-fatal warning */
#define PLUG_OBJ_STATUS_ERROR       'e' /* Non-fatal error */
#define PLUG_OBJ_STATUS_FATAL       'f' /* Fatal error occured */

struct plugin_object {
   char *path;
   char *plugin_name;
   char *object_category;
   char *object_type;
   char *object_name;
   char *object_source;
   char *object_uuid;
   uint64_t object_size;
   int32_t status;
   uint32_t count;
};

enum metadata_type {
   plugin_meta_blob = 0,
   plugin_meta_catalog_email,
   plugin_meta_invalid = -1,
};

/*
 * This class is used to store single plugin's metadata packet along with providing some
 * helper methods (e.g. for serialization of the data so that it can be send to the sd).
*/
struct meta_pkt: public SMARTALLOC {
   private:
      bool decoded;              /* Was metadata packed decoded from serialized stream or not */

   public:
      uint32_t total_size;       /* Total size of metadata stream (consiting of packets) */
      uint16_t total_count;      /* Total count of metadata packets in the stream */
      enum metadata_type type;   /* Type of metadata (binary, email, ...) */
      uint16_t index;            /* Index of the packet in metadata stream (starting from 0, goes up to [total_count-1] */
      uint32_t buf_len;          /* Length of buffer */
      void *buf;                 /* Can be either passed by the user or allocated for deserialization */

      /* Constructor, allows to assign most of needed fields in place */
      meta_pkt(metadata_type type=plugin_meta_invalid, uint32_t len=0, void *buf=NULL, uint16_t idx=0)  {
         this->buf_len = len;
         this->type = type;
         this->buf = buf;
         this->index = idx;
         this->total_size = 0;
         this->total_count = 0;

         decoded = false;
      };

      /* Constructor, class object can be either created from stream (which will be deserialized)
       * or just initialized with some init values. */
      meta_pkt(void *stream) {
         if (stream) {
            unser_declare;
            unser_begin(stream, 0);
            unser_uint32(total_size);
            unser_uint16(total_count);
            unser_uint32((uint32_t&) type);
            unser_uint16(index);
            unser_uint32(buf_len);
            buf = bmalloc(buf_len+1);
            unser_bytes(buf, buf_len);
            ((char*)buf)[buf_len]=0; /* useful for debug */
         } else {
            total_size = 0;
            total_count = 0;
            buf_len = 0;
            type = plugin_meta_invalid;
            index = 0;
            buf = NULL;
         }

         /* Mark if we need to free buf later or not */
         decoded = stream ? true : false;
      };

      ~meta_pkt() {
         if (decoded) {
            bfree(buf); /* Metadata packet contains buffer allocated by this class, needs freeing */
         }
      };

      /* Size of single metadata packet (struct's size + plugin's metadata buffer) */
      uint32_t size() {
         return sizeof(meta_pkt) + buf_len - sizeof(buf);
      }

      /* Serialize metadata packed into specified buffer */
      void serialize(void *ser_buf) {
         ser_declare;

         ser_begin(ser_buf, size());
         ser_uint32(total_size);
         ser_uint16(total_count);
         ser_uint32(type);
         ser_uint16(index);
         ser_uint32(buf_len);
         ser_bytes(buf, buf_len);
      }

};

/*
 * This class is used for transferring plugin's file metadata between the plugin and the fd.
 * It's a helper class to make metadata packets easy to manage for the plugins.
 *
 * Example usage (in form of pseudocode) of adding two packets:
 *
 * meta_mgr = New(plugin_metadata);
 * meta_mgr->add_packet(plugin_meta_blog, buf1, buf1_len);
 * meta_mgr->add_packet(plugin_meta_catalog_email, buf2, buf2_len);
 *
 * Then just simply return meta_mgr as an 'plug_meta' field in save_packet structure.
 * Bacula will then take care of all of the packets added to the list and store it
 * onto the volume one by one.
 */
class plugin_metadata: public SMARTALLOC {
   private:
      uint32_t total_size;       /* Total size of metadata stream (consiting of many packets) */
      uint16_t total_count;      /* Total count of metadata packets in the stream */
      mutable alist packets;     /* List of packets in the stream */

   public:
      plugin_metadata() {
         packets.init(5, false);
         total_size = 0;
         total_count = 0;
      };

      ~plugin_metadata() {
         reset();
      };

      /* Create packet with specified attributes, add it to the list */
      void add_packet(metadata_type type, uint32_t len, void *buf) {
         meta_pkt *mp = New(meta_pkt(type, len, buf, total_count++));
         total_size+=mp->size();
         mp->total_size = total_size;
         mp->total_count = total_count;

         packets.push(mp);

         /* Update all packets with new total size and count */
         foreach_alist(mp, &packets) {
            mp->total_size = total_size;
            mp->total_count = total_count;
         }

      };

      uint32_t size() const {
         return total_size;
      };

      uint16_t count() const {
         return total_count;
      };

      meta_pkt *get(int index) const {
         return (meta_pkt *)packets.get(index);
      };

      void reset() {
         /* Remove packets from list, delete each of them */
         while (!packets.empty()) {
            meta_pkt *mp = (meta_pkt *)packets.pop(); // remove from list
            delete mp;
         }

         total_size = 0;
         total_count = 0;
      }
};

/*
 * This packet is used for file save info transfer.
 */
struct save_pkt {
   int32_t pkt_size;                  /* size of this packet */
   char *fname;                       /* Full path and filename */
   char *snap_fname;                  /* Name inside the Snapshot */
   bplugin_mode_t plugin_mode;        /* Not used anymore, don't remove */
   char *link;                        /* Link name if any */
   struct stat statp;                 /* System stat() packet for file */
   int32_t type;                      /* FT_xx for this file */
   uint64_t flags;                     /* Bacula internal flags */
   bool no_read;                      /* During the save, the file should not be saved */
   bool portable;                     /* set if data format is portable */
   bool accurate_found;               /* Found in accurate list (valid after check_changes()) */
   bool do_dedup;                     /* True if we deal with a dedup storage system */
   bool stat_update;                  /* True if only file's metada needds to be updated */
   char *cmd;                         /* command */
   struct restore_object restore_obj; /* Info about restore object */
   struct plugin_object plugin_obj;   /* Plugin Object */
   const plugin_metadata *plug_meta;  /* Metadata packet provided by plugin */
   uint32_t delta_seq;                /* Delta sequence number */
   int32_t LinkFI;                    /* LinkFI if LINKSAVED */
   int32_t pkt_end;                   /* end packet sentinel */
};

/*
 * This packet is used for file restore info transfer.
 */
struct restore_pkt {
   int32_t pkt_size;                  /* size of this packet */
   bplugin_mode_t plugin_mode;        /* Not used anymore, don't remove */
   int32_t stream;                    /* attribute stream id */
   int32_t data_stream;               /* id of data stream to follow */
   int32_t type;                      /* file type FT */
   int32_t file_index;                /* file index */
   int32_t LinkFI;                    /* file index to data if hard link */
   uid_t uid;                         /* userid */
   struct stat statp;                 /* decoded stat packet */
   const char *attrEx;                /* extended attributes if any */
   const char *ofname;                /* output filename */
   const char *olname;                /* output link name */
   const char *where;                 /* where */
   const char *RegexWhere;            /* regex where */
   int replace;                       /* replace flag */
   int create_status;                 /* status from createFile() */
   uint32_t delta_seq;                /* Delta sequence number */
   int32_t pkt_end;                   /* end packet sentinel */
};

/*
 * This packet is used for file restore info transfer.
 */
struct restore_filelist_pkt {
   int32_t pkt_size;                  /* size of this packet */
   bplugin_mode_t plugin_mode;        /* Not used anymore, don't remove */
   int32_t file_index;                /* file index */
   int32_t LinkFI;                    /* file index to data if hard link */
   struct stat statp;                 /* decoded stat packet */
   const char *attrEx;                /* extended attributes if any */
   const char *ofname;                /* output filename */
   uint32_t delta_seq;                /* Delta sequence number */
   const char *chksum;                /* Checksum if available */
   int32_t pkt_end;                   /* end packet sentinel */
};

enum {
   IO_OPEN = 1,
   IO_READ = 2,
   IO_WRITE = 3,
   IO_CLOSE = 4,
   IO_SEEK = 5
};

struct io_pkt {
   int32_t pkt_size;                  /* Size of this packet */
   int32_t func;                      /* Function code */
   int32_t count;                     /* read/write count */
   int32_t flags;                     /* Open flags */
   mode_t mode;                       /* permissions for created files */
   char *buf;                         /* read/write buffer */
   const char *fname;                 /* open filename */
   int32_t status;                    /* return status */
   int32_t io_errno;                  /* errno code */
   int32_t lerror;                    /* Win32 error code */
   int32_t whence;                    /* lseek argument */
   boffset_t offset;                  /* lseek argument or in bread current offset*/
   bool win32;                        /* Win32 GetLastError returned */
   int32_t pkt_end;                   /* end packet sentinel */
};

enum {
    BACL_BACKUP = 1,
    BACL_RESTORE = 2,
    BXATTR_BACKUP = 3,
    BXATTR_RESTORE = 4
};

struct xacl_pkt
{
   int32_t pkt_size;                // Size of this packet
   int32_t func;                    // Function code
   int32_t count;                   // read/write count
   char *content;                   // read/write buffer
   int32_t pkt_end;                 // end packet sentinel
};

struct accurate_attribs_pkt
{
   char *fname;                     // file name to lookup
   struct stat statp;               // decoded stat packet
   char *chksum;                    // file checksum
   int32_t delta_seq;               // delta seq
   bool seen;                       // if the file was seen before
};

/****************************************************************************
 *                                                                          *
 *                Bacula definitions                                        *
 *                                                                          *
 ****************************************************************************/

/* Bacula Variable Ids */
typedef enum {
  bVarJobId                 = 1,
  bVarFDName                = 2,
  bVarLevel                 = 3,
  bVarType                  = 4,
  bVarClient                = 5,
  bVarJobName               = 6,
  bVarJobStatus             = 7,
  bVarSinceTime             = 8,
  bVarAccurate              = 9,
  bVarFileSeen              = 10,
  bVarVssObject             = 11,
  bVarVssDllHandle          = 12,
  bVarWorkingDir            = 13,
  bVarPluginDir             = 14,
  bVarWhere                 = 15,
  bVarRegexWhere            = 16,
  bVarExePath               = 17,
  bVarVersion               = 18,
  bVarDistName              = 19,
  bVarPrevJobName           = 20,
  bVarPrefixLinks           = 21,
  bVarInteractiveSession    = 22,
  bVarFileIndex             = 23,
  bVarReplace               = 24,
  bVarMaxDedupBlockSize     = 25,
  bVarMinDedupBlockSize     = 26,
  bVarIsCanceled            = 27,
  bVarSysConfigPath         = 28,
  bVarEstimate              = 29,
} bVariable;

/* Events that are passed to plugin */
typedef enum {
  bEventJobStart                        = 1,
  bEventJobEnd                          = 2,
  bEventStartBackupJob                  = 3,
  bEventEndBackupJob                    = 4,
  bEventStartRestoreJob                 = 5,
  bEventEndRestoreJob                   = 6,
  bEventStartVerifyJob                  = 7,
  bEventEndVerifyJob                    = 8,
  bEventBackupCommand                   = 9,
  bEventRestoreCommand                  = 10,
  bEventEstimateCommand                 = 11,
  bEventLevel                           = 12,
  bEventSince                           = 13,
  bEventCancelCommand                   = 14, /* Executed by another thread */
  bEventVssBackupAddComponents          = 15, /* Just before bEventVssPrepareSnapshot */
  bEventVssRestoreLoadComponentMetadata = 16,
  bEventVssRestoreSetComponentsSelected = 17,
  bEventRestoreObject                   = 18,
  bEventEndFileSet                      = 19,
  bEventPluginCommand                   = 20, /* Sent during FileSet creation */
  bEventVssBeforeCloseRestore           = 21,
  /* Add drives to VSS snapshot
   *  argument: char[27] drivelist
   * You need to add them without duplicates,
   * see fd_common.h add_drive() copy_drives() to get help
   */
  bEventVssPrepareSnapshot              = 22,
  bEventOptionPlugin                    = 23,
  bEventHandleBackupFile                = 24, /* Used with Options Plugin */
  bEventComponentInfo                   = 25, /* Plugin component */
  bEventFeatures                        = 26, /* Ask for file list, ... "xxx,yyy,zzz" */
  bEventVerifyStream                    = 27, /* Register to get a copy of the data stream during verify */
  bEventPluginOptions                   = 28, /* Sent when starting the job */
} bEventType;


typedef struct s_bEvent {
   uint32_t eventType;
} bEvent;

typedef struct s_baculaInfo {
   uint32_t size;
   uint32_t version;
} bInfo;

/* Bacula Core Routines to call a plugin -- do not use within a plugin */
#ifdef FILE_DAEMON
struct BFILE;                   /* forward referenced */
struct FF_PKT;
void load_fd_plugins(const char *plugin_dir);
void new_plugins(JCR *jcr);
void free_plugins(JCR *jcr);
void generate_plugin_event(JCR *jcr, bEventType event, void *value=NULL);
bool send_plugin_name(JCR *jcr, BSOCK *sd, bool start);
bool plugin_name_stream(JCR *jcr, char *name);
int plugin_create_file(JCR *jcr, ATTR *attr, BFILE *bfd, int replace);
bool plugin_set_attributes(JCR *jcr, ATTR *attr, BFILE *ofd);
int plugin_save(JCR *jcr, FF_PKT *ff_pkt, bool top_level);
int plugin_estimate(JCR *jcr, FF_PKT *ff_pkt, bool top_level);
bool plugin_check_file(JCR *jcr, char *fname);
bRC plugin_option_handle_file(JCR *jcr, FF_PKT *ff_pkt, struct save_pkt *sp);
int plugin_get_idx(JCR *jcr, char *plugin);
bool plugin_send_restorefilelist(JCR *jcr, int plugin_index,
                                 char *path, char *lstat, char *checksum,
                                 int delta_seq);
bool is_plugin_loaded(JCR *jcr, char *cmd);
typedef struct {
   const char *plugin;
   const char *features;
} bFeature;

bool plugin_get_features(JCR *jcr, alist *list);
int plugin_backup_acl(JCR *jcr, FF_PKT *ff_pkt, char **data);
bool plugin_restore_acl(JCR *jcr, char *data, uint32_t length);
int plugin_backup_xattr(JCR *jcr, FF_PKT *ff_pkt, char **data);
bool plugin_restore_xattr(JCR *jcr, char *data, uint32_t length);
bool plugin_check_stream(JCR *jcr, int32_t &stream);
bool plugin_query_parameter(JCR *jcr, char *command, char *param, void sendit(JCR *jcr, const char *str));
bool plugin_backup_metadata(JCR *jcr, FF_PKT *ff_pkt);
bRC plugin_verify_data_close(JCR *jcr);
bRC plugin_verify_data_open(JCR *jcr,  ATTR *attr);
bRC plugin_verify_data_update(JCR *jcr,  char *buf, int len);
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Bacula interface version and function pointers --
 *  i.e. callbacks from the plugin to Bacula
 */
typedef struct s_baculaFuncs {
   uint32_t size;
   uint32_t version;
   bRC (*registerBaculaEvents)(bpContext *ctx, ...);
   bRC (*getBaculaValue)(bpContext *ctx, bVariable var, void *value);
   bRC (*setBaculaValue)(bpContext *ctx, bVariable var, void *value);
   bRC (*JobMessage)(bpContext *ctx, const char *file, int line,
       int type, utime_t mtime, const char *fmt, ...);
   bRC (*DebugMessage)(bpContext *ctx, const char *file, int line,
       int level, const char *fmt, ...);
   void *(*baculaMalloc)(bpContext *ctx, const char *file, int line,
       size_t size);
   void (*baculaFree)(bpContext *ctx, const char *file, int line, void *mem);
   bRC (*AddExclude)(bpContext *ctx, const char *file);
   bRC (*AddInclude)(bpContext *ctx, const char *file);
   bRC (*AddOptions)(bpContext *ctx, const char *opts);
   bRC (*AddRegex)(bpContext *ctx, const char *item, int type);
   bRC (*AddWild)(bpContext *ctx, const char *item, int type);
   bRC (*NewOptions)(bpContext *ctx);
   bRC (*NewInclude)(bpContext *ctx);
   bRC (*NewPreInclude)(bpContext *ctx);
   bRC (*checkChanges)(bpContext *ctx, struct save_pkt *sp);
   bRC (*AcceptFile)(bpContext *ctx, struct save_pkt *sp); /* Need fname and statp */
   bRC (*getAccurateAttribs)(bpContext *ctx, accurate_attribs_pkt *att);
   bRC (*AddPlugin)(bpContext *ctx, const char *file);
   bRC (*AddFileEvent)(bpContext *ctx, struct fileevent_pkt *ev);
} bFuncs;




/****************************************************************************
 *                                                                          *
 *                Plugin definitions                                        *
 *                                                                          *
 ****************************************************************************/

typedef enum {
  pVarName = 1,
  pVarDescription = 2
} pVariable;

#define FD_PLUGIN_MAGIC  "*FDPluginData*"

#define FD_PLUGIN_INTERFACE_VERSION  ( 23 + BEEF )

typedef struct s_pluginInfo {
   uint32_t size;
   uint32_t version;
   const char *plugin_magic;
   const char *plugin_license;
   const char *plugin_author;
   const char *plugin_date;
   const char *plugin_version;
   const char *plugin_description;
} pInfo;


struct query_pkt {
   int32_t pkt_size;                  /* Size of this packet */
   char *parameter;
   char *command;
   char *api_opts;
   char *result;
   int32_t pkt_end;                   /* end packet sentinel */
};

/*
 * This is a set of function pointers that Bacula can call
 *  within the plugin.
 */
typedef struct s_pluginFuncs {
   uint32_t size;
   uint32_t version;
   bRC (*newPlugin)(bpContext *ctx);
   bRC (*freePlugin)(bpContext *ctx);
   bRC (*getPluginValue)(bpContext *ctx, pVariable var, void *value);
   bRC (*setPluginValue)(bpContext *ctx, pVariable var, void *value);
   bRC (*handlePluginEvent)(bpContext *ctx, bEvent *event, void *value);
   bRC (*startBackupFile)(bpContext *ctx, struct save_pkt *sp);
   bRC (*endBackupFile)(bpContext *ctx);
   bRC (*startRestoreFile)(bpContext *ctx, const char *cmd);
   bRC (*endRestoreFile)(bpContext *ctx);
   bRC (*pluginIO)(bpContext *ctx, struct io_pkt *io);
   bRC (*createFile)(bpContext *ctx, struct restore_pkt *rp);
   bRC (*setFileAttributes)(bpContext *ctx, struct restore_pkt *rp);
   bRC (*checkFile)(bpContext *ctx, char *fname);
   bRC (*handleXACLdata)(bpContext *ctx, struct xacl_pkt *xacl);
   bRC (*restoreFileList)(bpContext *ctx, struct restore_filelist_pkt *rp);
   bRC (*checkStream)(bpContext *ctx, struct stream_pkt *sp);
   bRC (*queryParameter)(bpContext *ctx, struct query_pkt *qp);
   bRC (*metadataRestore)(bpContext *ctx, struct meta_pkt *mp);
   bRC (*startVerifyFile)(bpContext *ctx, struct restore_pkt *rp);
   bRC (*endVerifyFile)(bpContext *ctx);
} pFuncs;

#define plug_func(plugin) ((pFuncs *)(plugin->pfuncs))
#define plug_info(plugin) ((pInfo *)(plugin->pinfo))

#define FILEEVENT_TYPE_ANTIVIRUS 'a'

struct fileevent_pkt {
   int32_t FileIndex;
   char    Type;
   int     Severity;
   char    Description[MAX_NAME_LENGTH];
   char    Source[MAX_NAME_LENGTH];
};

#ifdef __cplusplus
}
#endif

#endif /* __FD_PLUGINS_H */
