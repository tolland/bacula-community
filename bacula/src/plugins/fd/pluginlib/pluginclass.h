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
 * @file pluginclass.h
 * @author Radosław Korzeniewski (radoslaw@korzeniewski.net)
 * @brief This is a Bacula File Daemon general plugin framework. The Class.
 * @version 1.0.0
 * @date 2021-04-08
 *
 * @copyright Copyright (c) 2021 All rights reserved. IP transferred to Bacula Systems according to agreement.
 */

#include "pluginlib.h"
#include "lib/ini.h"
#include "pluginlib/commctx.h"
#include "pluginlib/smartalist.h"

#define USE_CMD_PARSER
#include "fd_common.h"

#ifndef PLUGINLIB_PLUGINCLASS_H
#define PLUGINLIB_PLUGINCLASS_H

/// The list of restore options saved to the Plugin Config restore object.
extern struct ini_items plugin_items_dump[];

/// defines if plugin should handle local filesystem restore with Bacula Core functions
/// `false` means plugin will handle local restore itself
/// `true` means Bacula Core functions will handle local restore
extern const bool CORELOCALRESTORE;

namespace pluginlib
{
   /*
    * This is a main plugin API class. It manages a plugin context.
    *  All the public methods correspond to a public Bacula API calls, even if
    *  a callback is not implemented.
    */
   class PLUGINBCLASS: public SMARTALLOC
   {
   public:
      enum MODE
      {
         None = 0,
         BackupFull,
         BackupIncr,
         BackupDiff,
         EstimateFull,
         EstimateIncr,
         EstimateDiff,
         Listing,
         QueryParams,
         Restore,
      };
      enum OBJECT
      {
         FileObject,
         PluginObject,
         RestoreObject,
      };

      virtual bRC getPluginValue(bpContext *ctx, pVariable var, void *value) { return bRC_OK; }
      virtual bRC setPluginValue(bpContext *ctx, pVariable var, void *value) { return bRC_OK; }
      virtual bRC handlePluginEvent(bpContext *ctx, bEvent *event, void *value);
      virtual bRC startBackupFile(bpContext *ctx, struct save_pkt *sp);
      virtual bRC endBackupFile(bpContext *ctx);
      virtual bRC startRestoreFile(bpContext *ctx, const char *cmd);
      virtual bRC endRestoreFile(bpContext *ctx);
      virtual bRC pluginIO(bpContext *ctx, struct io_pkt *io);
      virtual bRC createFile(bpContext *ctx, struct restore_pkt *rp);
      virtual bRC setFileAttributes(bpContext *ctx, struct restore_pkt *rp);
      virtual bRC checkFile(bpContext *ctx, char *fname);
      virtual bRC handleXACLdata(bpContext *ctx, struct xacl_pkt *xacl);
      virtual bRC queryParameter(bpContext *ctx, struct query_pkt *qp);
      virtual bRC metadataRestore(bpContext *ctx, struct meta_pkt *mp);

      void setup_plugin(bpContext *ctx);

      PLUGINBCLASS(bpContext *bpctx) :
         mode(None),
         JobId(0),
         JobName(NULL),
         since(0),
         where(NULL),
         regexwhere(NULL),
         replace(0),
         pluginconfigsent(false),
         object(FileObject),
         fname(PM_FNAME),
         lname(PM_FNAME),
         robjbuf(PM_MESSAGE),
         parser(),
         execpath(PM_FNAME),
         workingpath(PM_FNAME),
         job_cancelled(false),
         accurate_mode(0),
         m_listing_query(PM_NAME),
         m_listing_top_nr(-1),
         m_listing_func(-1)
      {}
#if __cplusplus > 201103L
      PLUGINBCLASS() = delete;
      PLUGINBCLASS(PLUGINBCLASS&) = delete;
      PLUGINBCLASS(PLUGINBCLASS&&) = delete;
#endif
      virtual ~PLUGINBCLASS() {}

   protected:
      MODE mode;                       /// Plugin mode of operation
      int JobId;                       /// Job ID
      char *JobName;                   /// Job name
      time_t since;                    /// Job since parameter
      char *where;                     /// the Where variable for restore job if set by user
      char *regexwhere;                /// the RegexWhere variable for restore job if set by user
      char replace;                    /// the replace variable for restore job
      bool pluginconfigsent;           /// set when Plugin Config was sent during Full backup
      OBJECT object;                   /// the object type to handdle
      POOL_MEM fname;                  /// current file name to backup (grabbed from backend)
      POOL_MEM lname;                  /// current LSTAT data if any
      POOL_MEM robjbuf;                /// the buffer for restore object data
      cmd_parser parser;               /// Plugin command parser
      POOL_MEM execpath;               /// ready to use path where bacula binaries are located
      POOL_MEM workingpath;            /// ready to use path for bacula working directory
      bool job_cancelled;              /// it signal the metaplugin that job was cancelled
      int accurate_mode;               /// if the job is accurate
      POOL_MEM m_listing_query;        ///
      int m_listing_top_nr;            ///
      int m_listing_func;              ///

      virtual bRC parse_plugin_config(bpContext *ctx, restore_object_pkt *rop);
      virtual bRC parse_plugin_command(bpContext *ctx, const char *command);

      virtual bRC parse_plugin_restore_object(bpContext *ctx, restore_object_pkt *rop);
      virtual bRC handle_plugin_config(bpContext *ctx, restore_object_pkt *rop) { return bRC_OK; }
      virtual bRC handle_plugin_restore_object(bpContext *ctx, restore_object_pkt *rop) { return bRC_OK; }

      virtual bRC prepare_estimate(bpContext *ctx, char *command) { return bRC_OK; }
      virtual bRC prepare_backup(bpContext *ctx, char *command) { return bRC_OK; }
      virtual bRC prepare_restore(bpContext *ctx, char *command) { return bRC_OK; }
      virtual bRC prepare_command(bpContext *ctx, char *command) { return bRC_OK; }

      virtual bRC perform_backup_open(bpContext *ctx, struct io_pkt *io) { return bRC_OK; }
      virtual bRC perform_restore_open(bpContext *ctx, struct io_pkt *io) { return bRC_OK; }
      virtual bRC perform_read_data(bpContext *ctx, struct io_pkt *io) { return bRC_OK; }
      virtual bRC perform_write_data(bpContext *ctx, struct io_pkt *io) { return bRC_OK; }
      virtual bRC perform_seek_write(bpContext *ctx, struct io_pkt *io) { return bRC_OK; }
      virtual bRC perform_restore_close(bpContext *ctx, struct io_pkt *io) { return bRC_OK; }
      virtual bRC perform_backup_close(bpContext *ctx, struct io_pkt *io) { return bRC_OK; }
      virtual bRC perform_restore_create_file(bpContext *ctx, struct restore_pkt *rp) { return bRC_OK; }
      virtual bRC perform_restore_set_file_attributes(bpContext *ctx, struct restore_pkt *rp) { return bRC_OK; }

      virtual bRC perform_backup_check_file(bpContext *ctx, char *fname) { return bRC_Seen; }
      virtual bRC perform_acl_backup(bpContext *ctx, struct xacl_pkt *xacl) { return bRC_OK; }
      virtual bRC perform_acl_restore(bpContext *ctx, struct xacl_pkt *xacl) { return bRC_OK; }
      virtual bRC perform_xattr_backup(bpContext *ctx, struct xacl_pkt *xacl) { return bRC_OK; }
      virtual bRC perform_xattr_restore(bpContext *ctx, struct xacl_pkt *xacl) { return bRC_OK; }

      virtual bRC perform_query_parameter(bpContext *ctx, struct query_pkt *qp) { return bRC_OK; }
      virtual bRC perform_restore_metadata(bpContext *ctx, struct meta_pkt *mp) { return bRC_OK; }

      virtual bRC perform_jobstart(bpContext *ctx) { return bRC_OK; }
      virtual bRC perform_backupjobstart(bpContext *ctx) { return bRC_OK; }
      virtual bRC perform_restorejobstart(bpContext *ctx) { return bRC_OK; }
      virtual bRC perform_jobend(bpContext *ctx) { return bRC_OK; }
      virtual bRC perform_backupjobend(bpContext *ctx) { return bRC_OK; }
      virtual bRC perform_restorejobend(bpContext *ctx) { return bRC_OK; }
      virtual bRC perform_joblevel(bpContext *ctx, char lvl) { return bRC_OK; }
      virtual bRC perform_jobsince(bpContext *ctx) { return bRC_OK; }
      virtual bRC perform_jobendfileset(bpContext *ctx) { return bRC_OK; }
      virtual bRC perform_start_backup_file(bpContext *ctx, struct save_pkt *sp) { return bRC_Max; }
      virtual bRC perform_start_restore_file(bpContext *ctx, const char *cmd) { return bRC_OK; }
      virtual bRC perform_end_backup_file(bpContext *ctx) { return bRC_OK; }
      virtual bRC perform_end_restore_file(bpContext *ctx) { return bRC_OK; }
      virtual bRC perform_cancel_command(bpContext *ctx) { return bRC_OK; }

      virtual const char **get_listing_top_struct() { return NULL; }

      virtual void pluginctx_switch_command(const char *command) {}
      virtual bool pluginctx_check_command(const char *command) { return false; }
      virtual bool pluginctx_are_parameters_parsed() const { return false; }
      virtual void pluginctx_set_parameters_parsed() {}
      virtual void pluginctx_clear_abort_on_error() {}
      virtual void pluginctx_set_abort_on_error() {}
      virtual bRC pluginctx_parse_parameter(bpContext *ctx, const char *argk, const char *argv) { return bRC_Error; }
      virtual bRC pluginctx_parse_parameter(bpContext *ctx, ini_items &item) { return bRC_Error; }
      virtual bool pluginctx_is_abort_on_error() { return false; }
      virtual int pluginctx_jmsg_err_level() { return -1; }
      virtual bRC pluginctx_parse_plugin_config(bpContext *ctx, restore_object_pkt *rop) { return bRC_Error; }
      virtual bRC pluginctx_handle_restore_object(bpContext *ctx, restore_object_pkt *rop) { return bRC_Error; }
   };

   /**
    * @brief A final template class instantinated with custom Plugin Context.
    *
    * @tparam CTX a custom Plugin Context to use
    */
   template<class CTX>
   class PLUGINCLASS : public PLUGINBCLASS
   {
   public:
      PLUGINCLASS(bpContext *bpctx) : PLUGINBCLASS(bpctx) {}
#if __cplusplus > 201103L
      PLUGINCLASS() = delete;
      PLUGINCLASS(PLUGINCLASS&) = delete;
      PLUGINCLASS(PLUGINCLASS&&) = delete;
#endif
      virtual ~PLUGINCLASS() {}

   protected:
      COMMCTX<CTX> pluginctx;          /// the current plugin execution context

      virtual void pluginctx_switch_command(const char *command) { pluginctx.switch_command(command); }
      virtual bool pluginctx_check_command(const char *command) { return pluginctx.check_command(command); }
      virtual bool pluginctx_are_parameters_parsed() { return pluginctx->are_parameters_parsed(); }
      virtual void pluginctx_set_parameters_parsed() { pluginctx->set_parameters_parsed(); }
      virtual void pluginctx_clear_abort_on_error() { pluginctx->clear_abort_on_error(); }
      virtual void pluginctx_set_abort_on_error() { pluginctx->set_abort_on_error(); }
      virtual bRC pluginctx_parse_parameter(bpContext *ctx, const char *argk, const char *argv) { return pluginctx->parse_parameter(ctx, argk, argv); }
      virtual bRC pluginctx_parse_parameter(bpContext *ctx, ini_items &item) { return pluginctx->parse_parameter(ctx, item); }
      virtual bool pluginctx_is_abort_on_error() { return pluginctx->is_abort_on_error(); }
      virtual int pluginctx_jmsg_err_level() { return pluginctx->jmsg_err_level(); }
      virtual bRC pluginctx_parse_plugin_config(bpContext *ctx, restore_object_pkt *rop) { return pluginctx->parse_plugin_config(ctx, rop); }
      virtual bRC pluginctx_handle_restore_object(bpContext *ctx, restore_object_pkt *rop) { return pluginctx->handle_restore_object(ctx, rop); }
   };
}  // namespace pluginlib

#endif   // PLUGINLIB_PLUGINCLASS_H
