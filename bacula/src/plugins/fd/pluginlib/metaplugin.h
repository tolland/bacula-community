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
 * @file metaplugin.h
 * @author Radosław Korzeniewski (radoslaw@korzeniewski.net)
 * @brief This is a Bacula metaplugin interface.
 * @version 3.0.0
 * @date 2021-10-07
 *
 * @copyright Copyright (c) 2021 All rights reserved.
 *            IP transferred to Bacula Systems according to agreement.
 */

#include "pluginlib.h"
#include "ptcomm.h"
#include "lib/ini.h"
#include "commctx.h"
#include "smartalist.h"


#define USE_CMD_PARSER
#include "fd_common.h"

#ifndef PLUGINLIB_METAPLUGIN_H
#define PLUGINLIB_METAPLUGIN_H


// custom checkFile() callback
typedef bRC (*checkFile_t)(bpContext *ctx, char *fname);

// metadata handling map
struct metadataTypeMap
{
   const char *command;
   metadata_type type;
};
extern const metadataTypeMap plugin_metadata_map[];

// Plugin Info definitions
extern const char *PLUGIN_LICENSE;
extern const char *PLUGIN_AUTHOR;
extern const char *PLUGIN_DATE;
extern const char *PLUGIN_VERSION;
extern const char *PLUGIN_DESCRIPTION;

// Plugin linking time variables
extern const char *PLUGINPREFIX;                /// is used for prefixing every Job and Debug messages generted by a plugin
extern const char *PLUGINNAME;                  /// should match the backend $pluginname$ used for Handshake procedure
extern const bool CUSTOMNAMESPACE;              /// defines if metaplugin should send `Namespace=...` backend plugin parameter using PLUGINNAMESPACE variable
extern const bool CUSTOMPREVJOBNAME;            /// defines if metaplugin should send `PrevJobName=...` backend plugin parameter from bacula variable
extern const char *PLUGINNAMESPACE;             /// custom backend plugin namespace used as file name prefix
extern const char *PLUGINAPI;                   /// the plugin api string which should match backend expectations
extern const char *BACKEND_CMD;                 /// a backend execution command path
extern const int32_t CUSTOMCANCELSLEEP;         /// custom wait time for backend between USR1 and terminate procedures
extern const bool ACCURATEPLUGINPARAMETER;      /// accurate parameter for plugin parameter
extern const int ADDINCLUDESTRIPOPTION;         /// setup precompiled include path strip option
extern const bool DONOTSAVE_FT_PLUGIN_CONFIG;   /// when set to `true` then Metaplugin won't save FT_PLUGIN_CONFIG as a first file during Full backup
extern const uint32_t BACKEND_TIMEOUT;          /// define a custom timeout value in seconds for data exchange (read from or write to backend)

/// defines if metaplugin should handle local filesystem restore with Bacula Core functions
/// `false` means metaplugin will redirect local restore to backend
/// `true` means Bacula Core functions will handle local restore
extern const bool CORELOCALRESTORE;

// The list of restore options saved to the RestoreObject.
extern struct ini_items plugin_items_dump[];

// the list of valid plugin options
extern const char *valid_params[];

// custom checkFile() callback
extern checkFile_t checkFile;

// a simple ro buf class
struct restore_object_class
{
   POOL_MEM plugin_name;
   POOL_MEM object_name;
   POOL_MEM data;
   int32_t length;
   bool sent;
};

/*
 * This is a main plugin API class. It manages a plugin context.
 *  All the public methods correspond to a public Bacula API calls, even if
 *  a callback is not implemented.
 */
class METAPLUGIN: public SMARTALLOC
{
public:
   enum MODE
   {
      NONE = 0,
      Backup_Full,
      BACKUP_FULL = Backup_Full,
      Backup_Incr,
      BACKUP_INCR = Backup_Incr,
      Backup_Diff,
      BACKUP_DIFF = Backup_Diff,
      Estimate_Full,
      Estimate_Incr,
      Estimate_Diff,
      // Listing,
      // QueryParams,
      RESTORE,
   };

   POOL_MEM backend_cmd;

   bRC getPluginValue(bpContext *ctx, pVariable var, void *value);
   bRC setPluginValue(bpContext *ctx, pVariable var, void *value);
   bRC handlePluginEvent(bpContext *ctx, bEvent *event, void *value);
   bRC startBackupFile(bpContext *ctx, struct save_pkt *sp);
   bRC endBackupFile(bpContext *ctx);
   bRC startRestoreFile(bpContext *ctx, const char *cmd);
   bRC endRestoreFile(bpContext *ctx);
   bRC pluginIO(bpContext *ctx, struct io_pkt *io);
   bRC createFile(bpContext *ctx, struct restore_pkt *rp);
   bRC setFileAttributes(bpContext *ctx, struct restore_pkt *rp);
   bRC checkFile(bpContext *ctx, char *fname);
   bRC handleXACLdata(bpContext *ctx, struct xacl_pkt *xacl);
   bRC queryParameter(bpContext *ctx, struct query_pkt *qp);
   bRC metadataRestore(bpContext *ctx, struct meta_pkt *mp);
   void setup_backend_command(bpContext *ctx, POOL_MEM &exepath);
   void terminate_backends_oncancel(bpContext *ctx);
   METAPLUGIN() : backend_cmd(PM_FNAME),
                  job_cancelled(false),
                  backend_available(false),
                  backend_error(PM_MESSAGE),
                  mode(NONE),
                  JobId(0),
                  JobName(NULL),
                  since(0),
                  accurate_mode(0),
                  accurate_mode_err(false),
                  where(NULL),
                  regexwhere(NULL),
                  replace(0),
                  pluginconfigsent(false),
                  estimate(false),
                  listing(None),
                  nodata(false),
                  nextfile(false),
                  openerror(false),
                  object(FileObject),
                  objectsent(false),
                  readacl(false),
                  readxattr(false),
                  skipextract(false),
                  new_include_created(false),
                  strip_path_option(ADDINCLUDESTRIPOPTION),
                  last_type(0),
                  fname(PM_FNAME),
                  lname(PM_FNAME),
                  robjbuf(PM_MESSAGE),
                  plugin_obj_cat(PM_FNAME),
                  plugin_obj_type(PM_FNAME),
                  plugin_obj_name(PM_FNAME),
                  plugin_obj_src(PM_FNAME),
                  plugin_obj_uuid(PM_FNAME),
                  plugin_obj_size(PM_FNAME),
                  acldatalen(0),
                  acldata(PM_MESSAGE),
                  xattrdatalen(0),
                  xattrdata(PM_MESSAGE),
                  metadatas_list(10, owned_by_alist),
                  prevjobname(NULL),
                  restoreobject_list()
   {}
#if __cplusplus > 201103L
   METAPLUGIN(METAPLUGIN&) = delete;
   METAPLUGIN(METAPLUGIN&&) = delete;
#endif
   ~METAPLUGIN() {}

private:
   enum OBJECT
   {
      FileObject,
      PluginObject,
      RestoreObject,
   };
   enum LISTING
   {
      None,
      Listing,
      Query,
   };

   bool job_cancelled;           // it signal the metaplugin that job was cancelled
   // smart_mutex mutex;            // mutex to synchronize data access - removed on request
   bool backend_available;       // When `False` then backend program is unuseable or unavailable
   POOL_MEM backend_error;       // Holds the error string when backend program is unavailable
   MODE mode;                    // Plugin mode of operation
   int JobId;                    // Job ID
   char *JobName;                // Job name
   time_t since;                 // Job since parameter
   int accurate_mode;            // if the job is accurate
   bool accurate_mode_err;       // if true then no more errors about lack of accurate mode required
   char *where;                  // the Where variable for restore job if set by user
   char *regexwhere;             // the RegexWhere variable for restore job if set by user
   char replace;                 // the replace variable for restore job
   bool pluginconfigsent;        // set when RestoreObject/INI_RESTORE_OBJECT_NAME_FT_PLUGIN_CONFIG was sent during Full backup
   bool estimate;                // used when mode is METAPLUGIN_BACKUP_* but we are doing estimate only
   LISTING listing;              // used for a Listing procedure for estimate
   bool nodata;                  // set when backend signaled no data for backup or no data for restore
   bool nextfile;                // set when IO_CLOSE got FNAME: command
   bool openerror;               // show if "openfile" was unsuccessful
   OBJECT object;                //
   bool objectsent;              // set when startBackupFile handled object and endBackupFile has to check for nextfile
   bool readacl;                 // got ACL data from backend
   bool readxattr;               // got XATTR data from backend
   bool skipextract;             // got SKIP response from backend, so we should artificially skip it for backend
   bool new_include_created;     // when NewInclude() was executed
   int strip_path_option;        //
   int32_t last_type;            // contains the last restore file type
   COMMCTX<PTCOMM> backend;      // the backend context list for multiple backend execution for a single job
   POOL_MEM fname;               // current file name to backup (grabbed from backend)
   POOL_MEM lname;               // current LSTAT data if any
   POOL_MEM robjbuf;             // the buffer for restore object data
   POOL_MEM plugin_obj_cat;      // Plugin object Category
   POOL_MEM plugin_obj_type;     // Plugin object Type
   POOL_MEM plugin_obj_name;     // Plugin object Name
   POOL_MEM plugin_obj_src;      // Plugin object Source
   POOL_MEM plugin_obj_uuid;     // Plugin object UUID
   uint64_t plugin_obj_size;     // Plugin object Size
   int acldatalen;               // the length of the data in acl buffer
   POOL_MEM acldata;             // the buffer for ACL data received from backend
   int xattrdatalen;             // the length of the data in xattr buffer
   POOL_MEM xattrdata;           // the buffer for XATTR data received from backend
   cmd_parser parser;            // Plugin command parser
   ConfigFile ini;               // Restore ini file handler
   alist metadatas_list;         // a list if managed metadatas
   plugin_metadata metadatas;    // the private metadata class
   const char *prevjobname;      // this is a bVarPrevJobName parameter if requested
   // a list of received RO from bacula which we will use to feed into backend later
   smart_alist<restore_object_class> restoreobject_list;

   bRC parse_plugin_command(bpContext *ctx, const char *command, smart_alist<POOL_MEM> &params);
   bRC handle_plugin_restoreobj(bpContext *ctx, restore_object_pkt *rop);
   bRC run_backend(bpContext *ctx);
   bRC send_parameters(bpContext *ctx, char *command);
   bRC send_jobinfo(bpContext *ctx, char type);
   bRC send_startjob(bpContext *ctx, const char *command);
   bRC send_startbackup(bpContext *ctx);
   bRC send_startestimate(bpContext *ctx);
   bRC send_startlisting(bpContext *ctx);
   bRC send_startquery(bpContext *ctx);
   bRC send_startrestore(bpContext *ctx);
   bRC send_endjob(bpContext *ctx);
   bRC prepare_backend(bpContext *ctx, char type, char *command);
   bRC perform_backup_open(bpContext *ctx, struct io_pkt *io);
   bRC perform_restore_open(bpContext *ctx, struct io_pkt *io);
   bRC perform_read_data(bpContext *ctx, struct io_pkt *io);
   bRC perform_write_data(bpContext *ctx, struct io_pkt *io);
   bRC perform_write_end(bpContext *ctx, struct io_pkt *io);
   bRC perform_read_metacommands(bpContext *ctx);
   bRC perform_read_pluginobject(bpContext *ctx, struct save_pkt *sp);
   bRC perform_read_restoreobject(bpContext *ctx, struct save_pkt *sp);
   bRC perform_read_acl(bpContext *ctx);
   bRC perform_write_acl(bpContext *ctx, const xacl_pkt * xacl);
   bRC perform_read_xattr(bpContext *ctx);
   bRC perform_write_xattr(bpContext *ctx, const xacl_pkt * xacl);
   bRC perform_read_metadata_info(bpContext *ctx, metadata_type type, struct save_pkt *sp);
   bRC perform_file_index_query(bpContext *ctx);
   bRC perform_accept_file(bpContext *ctx);
   bRC perform_addinclude(bpContext *ctx);
   bRC perform_change_split_option(bpContext *ctx, int nr);
   // bRC perform_write_metadata_info(bpContext *ctx, struct meta_pkt *mp);
   metadata_type scan_metadata_type(bpContext *ctx, const POOL_MEM &cmd);
   const char *prepare_metadata_type(metadata_type type);
   int check_ini_param(char *param);
   bool check_plugin_param(const char *param, alist *params);
   int get_ini_count();
   void new_backendlist(bpContext *ctx, char *command);
   bRC switch_or_run_backend(bpContext *ctx, char *command);
   bRC terminate_current_backend(bpContext *ctx);
   bRC terminate_all_backends(bpContext *ctx);
   bRC cancel_all_backends(bpContext *ctx);
   bRC signal_finish_all_backends(bpContext *ctx);
   bRC render_param(bpContext *ctx, POOL_MEM &param, INI_ITEM_HANDLER *handler, char *key, item_value val);
};

#endif   // PLUGINLIB_METAPLUGIN_H
