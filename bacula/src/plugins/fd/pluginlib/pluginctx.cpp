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
 * @file pluginctx.cpp
 * @author Radosław Korzeniewski (radoslaw@korzeniewski.net)
 * @brief This is a Bacula File Daemon general plugin framework. The Ccontext.
 * @version 1.0.0
 * @date 2021-04-08
 *
 * @copyright Copyright (c) 2021 All rights reserved. IP transferred to Bacula Systems according to agreement.
 */

#include "pluginctx.h"
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

namespace pluginlib
{
   /**
    * @brief Check if a parameter (param) exist in ConfigFile variables set by user.
    *          The checking ignore case of the parameter.
    *
    * @param param a parameter to search in ini parameter keys
    * @return int -1 - when a parameter param is not found in ini keys
    *             <n> - whan a parameter param is found and <n> is an index in ini->items table
    */
   int PLUGINCTX::check_ini_param(char *param)
   {
      if (ini.items){
         for (int k = 0; ini.items[k].name; k++){
            if (ini.items[k].found && strcasecmp(param, ini.items[k].name) == 0){
               return k;
            }
         }
      }

      return -1;
   }

   bRC PLUGINCTX::parse_plugin_config(bpContext *ctx, restore_object_pkt *rop)
   {
      ini.clear_items();
      if (!ini.dump_string(rop->object, rop->object_len)) {
         DMSG0(ctx, DERROR, "ini->dump_string failed\n");
         JMSG0(ctx, M_FATAL, "Unable to parse user set restore configuration.\n");
         return bRC_Error;
      }

      ini.register_items(plugin_items_dump, sizeof(struct ini_items));
      if (!ini.parse(ini.out_fname)) {
         DMSG0(ctx, DERROR, "ini->parse failed\n");
         JMSG0(ctx, M_FATAL, "Unable to parse user set restore configuration.\n");
         return bRC_Error;
      }

      for (int i = 0; ini.items[i].name; i++) {
         if (ini.items[i].found) {
            if (ini.items[i].handler == ini_store_str) {
               DMSG2(ctx, DINFO, "INI: %s = %s\n", ini.items[i].name, ini.items[i].val.strval);
            } else
            if (ini.items[i].handler == ini_store_int64) {
               DMSG2(ctx, DINFO, "INI: %s = %lld\n", ini.items[i].name, ini.items[i].val.int64val);
            } else
            if (ini.items[i].handler == ini_store_bool) {
               DMSG2(ctx, DINFO, "INI: %s = %s\n", ini.items[i].name, ini.items[i].val.boolval ? "True" : "False");
            } else {
               DMSG1(ctx, DERROR, "INI: unsupported parameter handler for: %s\n", ini.items[i].name);
               JMSG1(ctx, M_FATAL, "INI: unsupported parameter handler for: %s\n", ini.items[i].name);
               return bRC_Error;
            }
         }
      }

      return bRC_OK;
   }




#if 0
















   /**
    * @brief Search if parameter (param) is on parameter list prepared for backend.
    *          The checking ignore case of the parameter.
    *
    * @param param the parameter which we are looking for
    * @param params the list of parameters to search
    * @return true when the parameter param is found in list
    * @return false when we can't find the param on list
    */
   bool PLUGINCLASS::check_plugin_param(const char *param, alist *params)
   {
      POOLMEM *par;
      char *equal;
      bool found = false;

      foreach_alist(par, params){
         equal = strchr(par, '=');
         if (equal){
            /* temporary terminate the par at parameter name */
            *equal = '\0';
            if (strcasecmp(par, param) == 0){
               found = true;
            }
            /* restore parameter equal sign */
            *equal = '=';
         } else {
            if (strcasecmp(par, param) == 0){
               found = true;
            }
         }
      }

      return found;
   }

   /**
    * @brief Counts the number of ini->items available as it is a NULL terminated array.
    *
    * @return int the number of ini->items
    */
   int PLUGINCLASS::get_ini_count()
   {
      int count = 0;

      if (ini.items){
         for (int k = 0; ini.items[k].name; k++){
            if (ini.items[k].found){
               count++;
            }
         }
      }

      return count;
   }

   /**
    * @brief Parsing a plugin command.
    *
    * @param ctx bpContext - Bacula Plugin context structure
    * @param command plugin command string to parse
    * @param params output parsed params list
    * @return bRC bRC_OK - on success, bRC_Error - on error
    */
   bRC PLUGINCLASS::parse_plugin_command(bpContext *ctx, const char *command, smart_alist<POOL_MEM> &params)
   {
      bool found;
      int count;
      int parargc, argc;
      POOL_MEM *param;

      DMSG(ctx, DINFO, "Parse command: %s\n", command);
      if (parser.parse_cmd(command) != bRC_OK)
      {
         DMSG0(ctx, DERROR, "Unable to parse Plugin command line.\n");
         JMSG0(ctx, M_FATAL, "Unable to parse Plugin command line.\n");
         return bRC_Error;
      }

      /* count the numbers of user parameters if any */
      count = get_ini_count();

      /* the first (zero) parameter is a plugin name, we should skip it */
      argc = parser.argc - 1;
      parargc = argc + count;
      /* first parameters from plugin command saved during backup */
      for (int i = 1; i < parser.argc; i++) {
         param = new POOL_MEM(PM_FNAME);     // TODO: change to POOL_MEM
         found = false;

         int k;
         /* check if parameter overloaded by restore parameter */
         if ((k = check_ini_param(parser.argk[i])) != -1){
            found = true;
            DMSG1(ctx, DINFO, "parse_plugin_command: %s found in restore parameters\n", parser.argk[i]);
            if (render_param(ctx, *param, ini.items[k].handler, parser.argk[i], ini.items[k].val) != bRC_OK){
               delete(param);
               return bRC_Error;
            }
            params.append(param);
            parargc--;
         }

         /* check if param overloaded above */
         if (!found){
            if (parser.argv[i]){
               Mmsg(*param, "%s=%s\n", parser.argk[i], parser.argv[i]);
               params.append(param);
            } else {
               Mmsg(*param, "%s=1\n", parser.argk[i]);
               params.append(param);
            }
         }
         /* param is always ended with '\n' */
         DMSG(ctx, DINFO, "Param: %s", param);

         /* scan for abort_on_error parameter */
         if (strcasecmp(parser.argk[i], "abort_on_error") == 0){
            /* found, so check the value if provided, I only check the first char */
            if (parser.argv[i] && *parser.argv[i] == '0'){
               backend.ctx->clear_abort_on_error();
            } else {
               backend.ctx->set_abort_on_error();
            }
            DMSG1(ctx, DINFO, "abort_on_error found: %s\n", backend.ctx->is_abort_on_error() ? "True" : "False");
         }
         /* scan for listing parameter, so the estimate job should be executed as a Listing procedure */
         if (strcasecmp(parser.argk[i], "listing") == 0){
            /* found, so check the value if provided */
            if (parser.argv[i]){
               listing = Listing;
               DMSG0(ctx, DINFO, "listing procedure param found\n");
            }
         }
         /* scan for query parameter, so the estimate job should be executed as a QueryParam procedure */
         if (strcasecmp(parser.argk[i], "query") == 0){
            /* found, so check the value if provided */
            if (parser.argv[i]){
               listing = Query;
               DMSG0(ctx, DINFO, "query procedure param found\n");
            }
         }
      }
      /* check what was missing in plugin command but get from ini file */
      if (argc < parargc){
         for (int k = 0; ini.items[k].name; k++){
            if (ini.items[k].found && !check_plugin_param(ini.items[k].name, &params)){
               param = new POOL_MEM(PM_FNAME);
               DMSG1(ctx, DINFO, "parse_plugin_command: %s from restore parameters\n", ini.items[k].name);
               if (render_param(ctx, *param, ini.items[k].handler, (char*)ini.items[k].name, ini.items[k].val) != bRC_OK){
                  delete(param);
                  return bRC_Error;
               }
               params.append(param);
               /* param is always ended with '\n' */
               DMSG(ctx, DINFO, "Param: %s", param);
            }
         }
      }

      return bRC_OK;
   }

   /*
   * Parse a Restore Object saved during backup and modified by user during restore.
   *    Every RO received will generate a dedicated backend context which is used
   *    by bEventRestoreCommand to handle backend parameters for restore.
   *
   * in:
   *    bpContext - Bacula Plugin context structure
   *    rop - a restore object structure to parse
   * out:
   *    bRC_OK - on success
   *    bRC_Error - on error
   */
   bRC PLUGINCLASS::handle_plugin_restoreobj(bpContext *ctx, restore_object_pkt *rop)
   {
      if (!rop){
         return bRC_OK;    /* end of rop list */
      }

      DMSG2(ctx, DDEBUG, "handle_plugin_restoreobj: %s %d\n", rop->object_name, rop->object_type);

      // if (strcmp(rop->object_name, INI_RESTORE_OBJECT_NAME) == 0) {
      if (strcmp(rop->object_name, INI_RESTORE_OBJECT_NAME) == 0 && (rop->object_type == FT_PLUGIN_CONFIG || rop->object_type == FT_PLUGIN_CONFIG_FILLED)) {

         DMSG(ctx, DINFO, "INIcmd: %s\n", rop->plugin_name);

         ini.clear_items();
         if (!ini.dump_string(rop->object, rop->object_len))
         {
            DMSG0(ctx, DERROR, "ini->dump_string failed\n");
            JMSG0(ctx, M_FATAL, "Unable to parse user set restore configuration.\n");
            return bRC_Error;
         }

         ini.register_items(plugin_items_dump, sizeof(struct ini_items));
         if (!ini.parse(ini.out_fname))
         {
            DMSG0(ctx, DERROR, "ini->parse failed\n");
            JMSG0(ctx, M_FATAL, "Unable to parse user set restore configuration.\n");
            return bRC_Error;
         }

         for (int i = 0; ini.items[i].name; i++) {
            if (ini.items[i].found){
               if (ini.items[i].handler == ini_store_str){
                  DMSG2(ctx, DINFO, "INI: %s = %s\n", ini.items[i].name, ini.items[i].val.strval);
               } else
               if (ini.items[i].handler == ini_store_int64){
                  DMSG2(ctx, DINFO, "INI: %s = %lld\n", ini.items[i].name, ini.items[i].val.int64val);
               } else
               if (ini.items[i].handler == ini_store_bool){
                  DMSG2(ctx, DINFO, "INI: %s = %s\n", ini.items[i].name, ini.items[i].val.boolval ? "True" : "False");
               } else {
                  DMSG1(ctx, DERROR, "INI: unsupported parameter handler for: %s\n", ini.items[i].name);
                  JMSG1(ctx, M_FATAL, "INI: unsupported parameter handler for: %s\n", ini.items[i].name);
                  return bRC_Error;
               }
            }
         }

         return bRC_OK;
      }

      // handle any other RO restore
      restore_object_class *ropclass = new restore_object_class;
      ropclass->sent = false;
      pm_strcpy(ropclass->plugin_name, rop->plugin_name);
      pm_strcpy(ropclass->object_name, rop->object_name);
      ropclass->length = rop->object_len;
      pm_memcpy(ropclass->data, rop->object, rop->object_len);
      restoreobject_list.append(ropclass);
      DMSG2(ctx, DINFO, "ROclass saved for later: %s %d\n", ropclass->object_name.c_str(), ropclass->length);

      return bRC_OK;
   }

   /*
   * Run external backend script/application using BACKEND_CMD compile variable.
   *    It will run the backend in current backend context (backendctx) and should
   *    be called when a new backend is really required only.
   *
   * in:
   *    bpContext - for Bacula debug and jobinfo messages
   * out:
   *    bRC_OK - when backend spawned successfully
   *    bRC_Error - when Plugin cannot run backend
   */
   bRC PLUGINCLASS::run_backend(bpContext *ctx)
   {
      BPIPE *bp;

      if (access(backend_cmd.c_str(), X_OK) < 0){
         berrno be;
         DMSG2(ctx, DERROR, "Unable to access backend: %s Err=%s\n", backend_cmd.c_str(), be.bstrerror());
         JMSG2(ctx, M_FATAL, "Unable to access backend: %s Err=%s\n", backend_cmd.c_str(), be.bstrerror());
         return bRC_Error;
      }
      DMSG(ctx, DINFO, "Executing: %s\n", backend_cmd.c_str());
      bp = open_bpipe(backend_cmd.c_str(), 0, "rwe");
      if (bp == NULL){
         berrno be;
         DMSG(ctx, DERROR, "Unable to run backend. Err=%s\n", be.bstrerror());
         JMSG(ctx, M_FATAL, "Unable to run backend. Err=%s\n", be.bstrerror());
         return bRC_Error;
      }
      /* setup communication channel */
      backend.ctx->set_bpipe(bp);
      DMSG(ctx, DINFO, "Backend executed at PID=%i\n", bp->worker_pid);
      return bRC_OK;
   }

   bRC backendctx_finish_func(PTCOMM *ptcomm, void *cp)
   {
      bpContext * ctx = (bpContext*)cp;
      bRC status = bRC_OK;
      POOL_MEM cmd(PM_FNAME);
      pm_strcpy(cmd, "FINISH\n");

      if (!ptcomm->write_command(ctx, cmd.addr())){
         status = bRC_Error;
      }
      if (!ptcomm->read_ack(ctx)){
         status = bRC_Error;
      }

      return status;
   }

   /*
   * Sends a "FINISH" command to all executed backends indicating the end of
   * "Restore loop".
   *
   * in:
   *    bpContext - for Bacula debug and jobinfo messages
   * out:
   *    bRC_OK - when operation was successful
   *    bRC_Error - on any error
   */
   bRC PLUGINCLASS::signal_finish_all_backends(bpContext *ctx)
   {
      return backend.foreach_command_status(backendctx_finish_func, ctx);
   }

   /*
   * Send end job command to backend.
   *    It terminates the backend when command sending was unsuccessful, as it is
   *    the very last procedure in protocol.
   *
   * in:
   *    bpContext - for Bacula debug and jobinfo messages
   *    ptcomm - backend context
   * out:
   *    bRC_OK - when send command was successful
   *    bRC_Error - on any error
   */
   bRC send_endjob(bpContext *ctx, PTCOMM *ptcomm)
   {
      bRC status = bRC_OK;
      POOL_MEM cmd(PM_FNAME);
      pm_strcpy(cmd, "END\n");

      if (!ptcomm->write_command(ctx, cmd.c_str())){
         /* error */
         status = bRC_Error;
      } else {
         if (!ptcomm->read_ack(ctx)){
            DMSG0(ctx, DERROR, "Wrong backend response to JobEnd command.\n");
            JMSG0(ctx, ptcomm->jmsg_err_level(), "Wrong backend response to JobEnd command.\n");
            status = bRC_Error;
         }
         ptcomm->signal_term(ctx);
      }
      return status;
   }

   /*
   * Terminates the current backend pointed by ptcomm context.
   *    The termination sequence consist of "End Job" protocol procedure and real
   *    backend process termination including communication channel close.
   *    When we'll get an error during "End Job" procedure then we inform the user
   *    and terminate the backend as usual without unnecessary formalities. :)
   *
   * in:
   *    bpContext - for Bacula debug and jobinfo messages
   * out:
   *    bRC_OK - when backend termination was successful, i.e. no error in
   *             "End Job" procedure
   *    bRC_Error - when backend termination encountered an error.
   */
   bRC backendctx_jobend_func(PTCOMM *ptcomm, void *cp)
   {
      bpContext *ctx = (bpContext *)cp;
      bRC status = bRC_OK;

      if (send_endjob(ctx, ptcomm) != bRC_OK){
         /* error in end job */
         DMSG0(ctx, DERROR, "Error in EndJob.\n");
         status = bRC_Error;
      }
      int pid = ptcomm->get_backend_pid();
      DMSG(ctx, DINFO, "Terminate backend at PID=%d\n", pid)
      ptcomm->terminate(ctx);

      return status;
   }

   /*
   * Terminate all executed backends.
   *    Check PLUGINCLASS::terminate_current_backend for more info.
   *
   * in:
   *    bpContext - for Bacula debug and jobinfo messages
   * out:
   *    bRC_OK - when all backends termination was successful
   *    bRC_Error - when any backend termination encountered an error
   */
   bRC PLUGINCLASS::terminate_all_backends(bpContext *ctx)
   {
      return backend.foreach_command_status(backendctx_jobend_func, ctx);
   }

   /**
    * @brief Callback used for sending a `cancel event` to the selected backend
    *
    * @param ptcomm the backend communication object
    * @param cp a bpContext - for Bacula debug and jobinfo messages
    * @return bRC bRC_OK when success
    */
   bRC backendctx_cancel_func(PTCOMM *ptcomm, void *cp)
   {
      bpContext * ctx = (bpContext*)cp;

      // cancel procedure
      // 1. get backend pid
      // 2. send SIGUSR1 to backend pid

      pid_t pid = ptcomm->get_backend_pid();
      DMSG(ctx, DINFO, "Inform backend about Cancel at PID=%d ...\n", pid)
      kill(pid, SIGUSR1);

      return bRC_OK;
   }

   /**
    * @brief Send `cancel event` to every backend and terminate it.
    *
    * @param ctx bpContext - for Bacula debug and jobinfo messages
    * @return bRC bRC_OK when success, bRC_Error if not
    */
   bRC PLUGINCLASS::cancel_all_backends(bpContext *ctx)
   {
      PLUGINCLASS *pctx = (PLUGINCLASS *)ctx->pContext;
      // the cancel procedure: for all backends execute cancel func
      return pctx->backend.foreach_command_status(backendctx_cancel_func, ctx);
   }

   /*
   * Send a "Job Info" protocol procedure parameters.
   *
   * in:
   *    bpContext - for Bacula debug and jobinfo messages
   *    type - a char compliant with the protocol indicating what jobtype we run
   * out:
   *    bRC_OK - when send job info was successful
   *    bRC_Error - on any error
   */
   bRC PLUGINCLASS::send_jobinfo(bpContext *ctx, char type)
   {
      int32_t rc;
      POOL_MEM cmd;
      char lvl;

      /* we will be sending Job Info data */
      pm_strcpy(cmd, "Job\n");
      rc = backend.ctx->write_command(ctx, cmd);
      if (rc < 0){
         /* error */
         return bRC_Error;
      }
      /* required parameters */
      Mmsg(cmd, "Name=%s\n", JobName);
      rc = backend.ctx->write_command(ctx, cmd);
      if (rc < 0){
         /* error */
         return bRC_Error;
      }
      Mmsg(cmd, "JobID=%i\n", JobId);
      rc = backend.ctx->write_command(ctx, cmd);
      if (rc < 0){
         /* error */
         return bRC_Error;
      }
      Mmsg(cmd, "Type=%c\n", type);
      rc = backend.ctx->write_command(ctx, cmd);
      if (rc < 0){
         /* error */
         return bRC_Error;
      }
      /* optional parameters */
      if (mode != RESTORE){
         switch (mode){
            case BACKUP_FULL:
               lvl = 'F';
               break;
            case BACKUP_DIFF:
               lvl = 'D';
               break;
            case BACKUP_INCR:
               lvl = 'I';
               break;
            default:
               lvl = 0;
         }
         if (lvl){
            Mmsg(cmd, "Level=%c\n", lvl);
            rc = backend.ctx->write_command(ctx, cmd);
            if (rc < 0){
               /* error */
            return bRC_Error;
            }
         }
      }
      if (since){
         Mmsg(cmd, "Since=%ld\n", since);
         rc = backend.ctx->write_command(ctx, cmd);
         if (rc < 0){
            /* error */
            return bRC_Error;
         }
      }
      if (where){
         Mmsg(cmd, "Where=%s\n", where);
         rc = backend.ctx->write_command(ctx, cmd);
         if (rc < 0){
            /* error */
            return bRC_Error;
         }
      }
      if (regexwhere){
         Mmsg(cmd, "RegexWhere=%s\n", regexwhere);
         rc = backend.ctx->write_command(ctx, cmd);
         if (rc < 0){
            /* error */
            return bRC_Error;
         }
      }
      if (replace){
         Mmsg(cmd, "Replace=%c\n", replace);
         rc = backend.ctx->write_command(ctx, cmd);
         if (rc < 0){
            /* error */
            return bRC_Error;
         }
      }

      if (CUSTOMNAMESPACE){
         Mmsg(cmd, "Namespace=%s\n", PLUGINNAMESPACE);
         rc = backend.ctx->write_command(ctx, cmd);
         if (rc < 0){
            /* error */
            return bRC_Error;
         }
      }

      if (CUSTOMPREVJOBNAME && prevjobname){
         Mmsg(cmd, "PrevJobName=%s\n", prevjobname);
         rc = backend.ctx->write_command(ctx, cmd);
         if (rc < 0){
            /* error */
            return bRC_Error;
         }
      }

      backend.ctx->signal_eod(ctx);

      if (!backend.ctx->read_ack(ctx)){
         DMSG0(ctx, DERROR, "Wrong backend response to Job command.\n");
         JMSG0(ctx, backend.ctx->jmsg_err_level(), "Wrong backend response to Job command.\n");
         return bRC_Error;
      }

      return bRC_OK;
   }

   /*
   * Send a "Plugin Parameters" protocol procedure data.
   *    It parse plugin command and ini parameters before sending it to backend.
   *
   * in:
   *    bpContext - for Bacula debug and jobinfo messages
   *    command - a Plugin command for a job
   * out:
   *    bRC_OK - when send parameters was successful
   *    bRC_Error - on any error
   */
   bRC PLUGINCLASS::send_parameters(bpContext *ctx, char *command)
   {
      int32_t rc;
      bRC status = bRC_OK;
      POOL_MEM cmd(PM_FNAME);
      // alist params(16, not_owned_by_alist);
      smart_alist<POOL_MEM> params;
      POOL_MEM *param;
      bool found;

   #ifdef DEVELOPER
      static const char *regress_valid_params[] =
      {
         // add special test_backend commands to handle regression tests
         "regress_error_plugin_params",
         "regress_error_start_job",
         "regress_error_backup_no_files",
         "regress_error_backup_stderr",
         "regress_error_estimate_stderr",
         "regress_error_listing_stderr",
         "regress_error_restore_stderr",
         "regress_backup_plugin_objects",
         "regress_backup_other_file",
         "regress_error_backup_abort",
         "regress_metadata_support",
         "regress_standard_error_backup",
         "regress_cancel_backup",
         "regress_cancel_restore",
         NULL,
      };
   #endif

      /* parse and prepare final backend plugin params */
      status = parse_plugin_command(ctx, command, params);
      if (status != bRC_OK){
         /* error */
         return status;
      }

      /* send backend info that parameters are coming */
      pm_strcpy(cmd, "Params\n");
      rc = backend.ctx->write_command(ctx, cmd);
      if (rc < 0){
         /* error */
         return bRC_Error;
      }
      /* send all prepared parameters */
      foreach_alist(param, &params){
         // check valid parameter list
         found = false;
         for (int a = 0; valid_params[a] != NULL; a++ )
         {
            DMSG3(ctx, DVDEBUG, "=> '%s' vs '%s' [%d]\n", param, valid_params[a], strlen(valid_params[a]));
            if (strncasecmp(param->c_str(), valid_params[a], strlen(valid_params[a])) == 0){
               found = true;
               break;
            }
         }

   #ifdef DEVELOPER
         if (!found){
            // now handle regression tests commands
            for (int a = 0; regress_valid_params[a] != NULL; a++ ){
               DMSG3(ctx, DVDEBUG, "regress=> '%s' vs '%s' [%d]\n", param, regress_valid_params[a], strlen(regress_valid_params[a]));
               if (strncasecmp(param->c_str(), regress_valid_params[a], strlen(regress_valid_params[a])) == 0){
                  found = true;
                  break;
               }
            }
         }
   #endif

         // signal error if required
         if (!found) {
            pm_strcpy(cmd, param->c_str());
            strip_trailing_junk(cmd.c_str());
            DMSG1(ctx, DERROR, "Unknown parameter %s in Plugin command.\n", cmd.c_str());
            JMSG1(ctx, M_ERROR, "Unknown parameter %s in Plugin command.\n", cmd.c_str());
         }

         rc = backend.ctx->write_command(ctx, *param);
         if (rc < 0) {
            /* error */
            return bRC_Error;
         }
      }

      // now send accurate parameter if requested and available
      if (ACCURATEPLUGINPARAMETER && accurate_mode) {
         pm_strcpy(cmd, "Accurate=1\n");
         rc = backend.ctx->write_command(ctx, cmd);
         if (rc < 0) {
            /* error */
            return bRC_Error;
         }
      }

      // signal end of parameters block
      backend.ctx->signal_eod(ctx);
      /* ack Params command */
      if (!backend.ctx->read_ack(ctx)){
         DMSG0(ctx, DERROR, "Wrong backend response to Params command.\n");
         JMSG0(ctx, backend.ctx->jmsg_err_level(), "Wrong backend response to Params command.\n");
         return bRC_Error;
      }

      return bRC_OK;
   }

   /*
   * Send start job command pointed by command variable.
   *
   * in:
   *    bpContext - for Bacula debug and jobinfo messages
   *    command - the command string to send
   * out:
   *    bRC_OK - when send command was successful
   *    bRC_Error - on any error
   */
   bRC PLUGINCLASS::send_startjob(bpContext *ctx, const char *command)
   {
      POOL_MEM cmd;

      pm_strcpy(cmd, command);
      if (backend.ctx->write_command(ctx, cmd) < 0){
         /* error */
         return bRC_Error;
      }

      if (!backend.ctx->read_ack(ctx)){
         strip_trailing_newline(cmd.c_str());
         DMSG(ctx, DERROR, "Wrong backend response to %s command.\n", cmd.c_str());
         JMSG(ctx, backend.ctx->jmsg_err_level(), "Wrong backend response to %s command.\n", cmd.c_str());
         return bRC_Error;
      }

      return bRC_OK;
   }

   /*
   * Send "BackupStart" protocol command.
   *    more info at PLUGINCLASS::send_startjob
   *
   * in:
   *    bpContext - for Bacula debug and jobinfo messages
   * out:
   *    bRC_OK - when send command was successful
   *    bRC_Error - on any error
   */
   bRC PLUGINCLASS::send_startbackup(bpContext *ctx)
   {
      return send_startjob(ctx, "BackupStart\n");
   }

   /*
   * Send "EstimateStart" protocol command.
   *    more info at PLUGINCLASS::send_startjob
   *
   * in:
   *    bpContext - for Bacula debug and jobinfo messages
   * out:
   *    bRC_OK - when send command was successful
   *    bRC_Error - on any error
   */
   bRC PLUGINCLASS::send_startestimate(bpContext *ctx)
   {
      return send_startjob(ctx, "EstimateStart\n");
   }

   /*
   * Send "ListingStart" protocol command.
   *    more info at PLUGINCLASS::send_startjob
   *
   * in:
   *    bpContext - for Bacula debug and jobinfo messages
   * out:
   *    bRC_OK - when send command was successful
   *    bRC_Error - on any error
   */
   bRC PLUGINCLASS::send_startlisting(bpContext *ctx)
   {
      return send_startjob(ctx, "ListingStart\n");
   }

   /*
   * Send "QueryStart" protocol command.
   *    more info at PLUGIN::send_startjob
   *
   * in:
   *    bpContext - for Bacula debug and jobinfo messages
   * out:
   *    bRC_OK - when send command was successful
   *    bRC_Error - on any error
   */
   bRC PLUGINCLASS::send_startquery(bpContext *ctx)
   {
      return send_startjob(ctx, "QueryStart\n");
   }

   /*
   * Send "RestoreStart" protocol command.
   *    more info at PLUGINCLASS::send_startjob
   *
   * in:
   *    bpContext - for Bacula debug and jobinfo messages
   * out:
   *    bRC_OK - when send command was successful
   *    bRC_Error - on any error
   */
   bRC PLUGINCLASS::send_startrestore(bpContext *ctx)
   {
      int32_t rc;
      POOL_MEM cmd(PM_FNAME);
      const char * command = "RestoreStart\n";
      POOL_MEM extpipename(PM_FNAME);

      pm_strcpy(cmd, command);
      rc = backend.ctx->write_command(ctx, cmd);
      if (rc < 0){
         /* error */
         return bRC_Error;
      }

      if (backend.ctx->read_command(ctx, cmd) < 0){
         DMSG(ctx, DERROR, "Wrong backend response to %s command.\n", command);
         JMSG(ctx, backend.ctx->jmsg_err_level(), "Wrong backend response to %s command.\n", command);
         return bRC_Error;
      }
      if (backend.ctx->is_eod()){
         /* got EOD so the backend is ready for restore */
         return bRC_OK;
      }

      /* here we expect a PIPE: command only */
      if (scan_parameter_str(cmd, "PIPE:", extpipename)){
         /* got PIPE: */
         DMSG(ctx, DINFO, "PIPE:%s\n", extpipename.c_str());
         backend.ctx->set_extpipename(extpipename.c_str());
         /* TODO: decide if plugin should verify if extpipe is available */
         pm_strcpy(cmd, "OK\n");
         rc = backend.ctx->write_command(ctx, cmd);
         if (rc < 0){
            /* error */
            return bRC_Error;
         }
         return bRC_OK;
      }
      return bRC_Error;
   }

   /*
   * Switches current backend context or executes new one when required.
   *    The backend (application path) to execute is set in BACKEND_CMD compile
   *    variable and handled by PLUGINCLASS::run_backend() method. Just before new
   *    backend execution the method search for already spawned backends which
   *    handles the same Plugin command and when found the current backend context
   *    is switched to already available on list.
   *
   * in:
   *    bpContext - for Bacula debug and jobinfo messages
   *    command - the Plugin command for which we execute a backend
   * out:
   *    bRC_OK - success and backendctx has a current backend context and Plugin
   *             should send an initialization procedure
   *    bRC_Max - when was already prepared and initialized, so no
   *              reinitialization required
   *    bRC_Error - error in switching or running the backend
   */
   bRC PLUGINCLASS::switch_or_run_backend(bpContext *ctx, char *command)
   {
      DMSG0(ctx, DINFO, "Switch or run Backend.\n");
      backend.switch_command(command);

      /* check if we have the backend with the same command already */
      if (backend.ctx->is_open())
      {
         /* and its open, so skip running the new */
         DMSG0(ctx, DINFO, "Backend already prepared.\n");
         return bRC_Max;
      }

      // now execute a backend
      if (run_backend(ctx) != bRC_OK){
         return bRC_Error;
      }

      return bRC_OK;
   }


   /*
   * This is the main method for handling events generated by Bacula.
   *    The behavior of the method depends on event type generated, but there are
   *    some events which does nothing, just return with bRC_OK. Every event is
   *    tracked in debug trace file to verify the event flow during development.
   *
   * in:
   *    bpContext - for Bacula debug and jobinfo messages
   *    event - a Bacula event structure
   *    value - optional event value
   * out:
   *    bRC_OK - in most cases signal success/no error
   *    bRC_Error - in most cases signal error
   *    <other> - depend on Bacula Plugin API if applied
   */
   bRC PLUGINCLASS::handlePluginEvent(bpContext *ctx, bEvent *event, void *value)
   {
      // extract original plugin context, basically it should be `this`
      PLUGINCLASS *pctx = (PLUGINCLASS *)ctx->pContext;
      // this ensures that handlePluginEvent is thread safe for extracted pContext
      // lock_guard lg(&pctx->mutex); - removed on request

      if (job_cancelled) {
         return bRC_Error;
      }

      switch (event->eventType)
      {
      case bEventJobStart:
         DMSG(ctx, D3, "bEventJobStart value=%s\n", NPRT((char *)value));
         getBaculaVar(bVarJobId, (void *)&JobId);
         getBaculaVar(bVarJobName, (void *)&JobName);
         if (CUSTOMPREVJOBNAME){
            getBaculaVar(bVarPrevJobName, (void *)&prevjobname);
         }
         break;

      case bEventJobEnd:
         DMSG(ctx, D3, "bEventJobEnd value=%s\n", NPRT((char *)value));
         return terminate_all_backends(ctx);

      case bEventLevel:
         char lvl;
         lvl = (char)((intptr_t) value & 0xff);
         DMSG(ctx, D2, "bEventLevel='%c'\n", lvl);
         switch (lvl) {
            case 'F':
               DMSG0(ctx, D2, "backup level = Full\n");
               mode = BACKUP_FULL;
               break;
            case 'I':
               DMSG0(ctx, D2, "backup level = Incr\n");
               mode = BACKUP_INCR;
               break;
            case 'D':
               DMSG0(ctx, D2, "backup level = Diff\n");
               mode = BACKUP_DIFF;
               break;
            default:
               DMSG0(ctx, D2, "unsupported backup level!\n");
               return bRC_Error;
         }
         break;

      case bEventSince:
         since = (time_t) value;
         DMSG(ctx, D2, "bEventSince=%ld\n", (intptr_t) since);
         break;

      case bEventStartBackupJob:
         DMSG(ctx, D3, "bEventStartBackupJob value=%s\n", NPRT((char *)value));
         break;

      case bEventEndBackupJob:
         DMSG(ctx, D2, "bEventEndBackupJob value=%s\n", NPRT((char *)value));
         break;

      case bEventStartRestoreJob:
         DMSG(ctx, DINFO, "StartRestoreJob value=%s\n", NPRT((char *)value));
         getBaculaVar(bVarWhere, &where);
         DMSG(ctx, DINFO, "Where=%s\n", NPRT(where));
         getBaculaVar(bVarRegexWhere, &regexwhere);
         DMSG(ctx, DINFO, "RegexWhere=%s\n", NPRT(regexwhere));
         getBaculaVar(bVarReplace, &replace);
         DMSG(ctx, DINFO, "Replace=%c\n", replace);
         mode = RESTORE;
         break;

      case bEventEndRestoreJob:
         DMSG(ctx, DINFO, "bEventEndRestoreJob value=%s\n", NPRT((char *)value));
         return signal_finish_all_backends(ctx);

      /* Plugin command e.g. plugin = <plugin-name>:parameters */
      case bEventEstimateCommand:
         DMSG(ctx, D1, "bEventEstimateCommand value=%s\n", NPRT((char *)value));
         estimate = true;
         return prepare_backend(ctx, BACKEND_JOB_INFO_ESTIMATE, (char*)value);

      /* Plugin command e.g. plugin = <plugin-name>:parameters */
      case bEventBackupCommand:
         DMSG(ctx, D2, "bEventBackupCommand value=%s\n", NPRT((char *)value));
         pluginconfigsent = false;
         return prepare_backend(ctx, BACKEND_JOB_INFO_BACKUP, (char*)value);

      /* Plugin command e.g. plugin = <plugin-name>:parameters */
      case bEventRestoreCommand:
         DMSG(ctx, D2, "bEventRestoreCommand value=%s\n", NPRT((char *)value));
         return prepare_backend(ctx, BACKEND_JOB_INFO_RESTORE, (char*)value);

      /* Plugin command e.g. plugin = <plugin-name>:parameters */
      case bEventPluginCommand:
         DMSG(ctx, D2, "bEventPluginCommand value=%s\n", NPRT((char *)value));
         getBaculaVar(bVarAccurate, (void *)&accurate_mode);
         if (isourplugincommand(PLUGINPREFIX, (char*)value) && !backend_available)
         {
            DMSG2(ctx, DERROR, "Unable to use backend: %s Err=%s\n", backend_cmd.c_str(), backend_error.c_str());
            JMSG2(ctx, M_FATAL, "Unable to use backend: %s Err=%s\n", backend_cmd.c_str(), backend_error.c_str());
            return bRC_Error;
         }
         break;

      case bEventOptionPlugin:
      case bEventHandleBackupFile:
         if (isourplugincommand(PLUGINPREFIX, (char*)value)){
            DMSG0(ctx, DERROR, "Invalid handle Option Plugin called!\n");
            JMSG2(ctx, M_FATAL,
                  "The %s plugin doesn't support the Option Plugin configuration.\n"
                  "Please review your FileSet and move the Plugin=%s"
                  "... command into the Include {} block.\n",
                  PLUGINNAME, PLUGINPREFIX);
            return bRC_Error;
         }
         break;

      case bEventEndFileSet:
         DMSG(ctx, D3, "bEventEndFileSet value=%s\n", NPRT((char *)value));
         break;

      case bEventRestoreObject:
         /* Restore Object handle - a plugin configuration for restore and user supplied parameters */
         if (!value){
            DMSG0(ctx, DINFO, "End restore objects\n");
            break;
         }
         DMSG(ctx, D2, "bEventRestoreObject value=%p\n", value);
         return handle_plugin_restoreobj(ctx, (restore_object_pkt *) value);

      case bEventCancelCommand:
         DMSG2(ctx, D3, "bEventCancelCommand self = %p pctx = %p\n", this, pctx);
         // TODO: PETITION: Our plugin (RHV WhiteBearSolutions) search the packet E CANCEL.
         // TODO: If you modify this behaviour, please you notify us.
         // TODO: RPK[20210623]: The information about a new procedure was sent to Eric
         pctx->job_cancelled = true;
         return cancel_all_backends(ctx);

      default:
         // enabled only for Debug
         DMSG2(ctx, D2, "Unknown event: %s (%d) \n", eventtype2str(event), event->eventType);
      }

      return bRC_OK;
   }

   /*
   * Make a real data read from the backend and checks for EOD.
   *
   * in:
   *    bpContext - for Bacula debug and jobinfo messages
   *    io - Bacula Plugin API I/O structure for I/O operations
   * out:
   *    bRC_OK - when successful
   *    bRC_Error - on any error
   */
   bRC PLUGINCLASS::perform_read_data(bpContext *ctx, struct io_pkt *io)
   {
      int rc;

      if (nodata){
         io->status = 0;
         return bRC_OK;
      }
      rc = backend.ctx->read_data_fixed(ctx, io->buf, io->count);
      if (rc < 0){
         io->status = rc;
         io->io_errno = EIO;
         return bRC_Error;
      }
      io->status = rc;
      if (backend.ctx->is_eod()){
         // TODO: we signal EOD as rc=0, so no need to explicity check for EOD, right?
         io->status = 0;
      }
      return bRC_OK;
   }

   /*
   * Make a real write data to the backend.
   *
   * in:
   *    bpContext - for Bacula debug and jobinfo messages
   *    io - Bacula Plugin API I/O structure for I/O operations
   * out:
   *    bRC_OK - when successful
   *    bRC_Error - on any error
   */
   bRC PLUGINCLASS::perform_write_data(bpContext *ctx, struct io_pkt *io)
   {
      int rc;
      POOL_MEM cmd(PM_FNAME);

      /* check if DATA was sent */
      if (nodata){
         pm_strcpy(cmd, "DATA\n");
         rc = backend.ctx->write_command(ctx, cmd.c_str());
         if (rc < 0){
            /* error */
            io->status = rc;
            io->io_errno = rc;
            return bRC_Error;
         }
         /* DATA command sent */
         nodata = false;
      }
      DMSG1(ctx, DVDEBUG, "perform_write_data: %d\n", io->count);
      rc = backend.ctx->write_data(ctx, io->buf, io->count);
      io->status = rc;
      if (rc < 0){
         io->io_errno = rc;
         return bRC_Error;
      }
      nodata = false;
      return bRC_OK;
   }

   /*
   * Handle protocol "DATA" command from backend during backup loop.
   *    It handles a "no data" flag when saved file contains no data to
   *    backup (empty file).
   *
   * in:
   *    bpContext - for Bacula debug and jobinfo messages
   *    io - Bacula Plugin API I/O structure for I/O operations
   * out:
   *    bRC_OK - when successful
   *    bRC_Error - on any error
   *    io->status, io->io_errno - set to error on any error
   */
   bRC PLUGINCLASS::perform_backup_open(bpContext *ctx, struct io_pkt *io)
   {
      int rc;
      POOL_MEM cmd(PM_FNAME);

      /* expecting DATA command */
      nodata = false;
      rc = backend.ctx->read_command(ctx, cmd);
      if (backend.ctx->is_eod()){
         /* no data for file */
         nodata = true;
      } else
      // expect no error and 'DATA' starting packet
      if (rc < 0 || !bstrcmp(cmd.c_str(), "DATA")){
         io->status = rc;
         io->io_errno = EIO;
         openerror = backend.ctx->is_fatal() ? false : true;
         return bRC_Error;
      }

      return bRC_OK;
   }

   /*
   * Signal the end of data to restore and verify acknowledge from backend.
   *
   * in:
   *    bpContext - for Bacula debug and jobinfo messages
   *    io - Bacula Plugin API I/O structure for I/O operations
   * out:
   *    bRC_OK - when successful
   *    bRC_Error - on any error
   */
   bRC PLUGINCLASS::perform_write_end(bpContext *ctx, struct io_pkt *io)
   {
      if (!nodata){
         /* signal end of data to restore and get ack */
         if (!backend.ctx->send_ack(ctx)){
            io->status = -1;
            io->io_errno = EPIPE;
            return bRC_Error;
         }
      }

      if (last_type == FT_DIREND) {
         struct xacl_pkt xacl;

         if (acldatalen > 0) {
            xacl.count = acldatalen;
            xacl.content = acldata.c_str();
            bRC status = perform_write_acl(ctx, &xacl);
            if (status != bRC_OK){
               return status;
            }
         }
         if (xattrdatalen > 0) {
            xacl.count = xattrdatalen;
            xacl.content = xattrdata.c_str();
            bRC status = perform_write_xattr(ctx, &xacl);
            if (status != bRC_OK){
               return status;
            }
         }
      }

      return bRC_OK;
   }

   /*
   * Reads ACL data from backend during backup. Save it for handleXACLdata from
   * Bacula. As we do not know if a backend will send the ACL data or not, we
   * cannot wait to read this data until handleXACLdata will be called.
   * TODO: The method has a limitation and accept up to PM_BSOCK acl data to save
   *       which is about 64kB. It should be sufficient for virtually all acl data
   *       we can imagine. But when a backend developer could expect a larger data
   *       to save he should rewrite perform_read_acl() method.
   *
   * in:
   *    bpContext - for Bacula debug and jobinfo messages
   * out:
   *    bRC_OK - when ACL data was read successfully and this.readacl set to true
   *    bRC_Error - on any error during acl data read
   */
   bRC PLUGINCLASS::perform_read_acl(bpContext *ctx)
   {
      DMSG0(ctx, DINFO, "perform_read_acl\n");
      acldatalen = backend.ctx->read_data(ctx, acldata);
      if (acldatalen < 0){
         DMSG0(ctx, DERROR, "Cannot read ACL data from backend.\n");
         return bRC_Error;
      }

      DMSG1(ctx, DINFO, "readACL: %i\n", acldatalen);
      if (!backend.ctx->read_ack(ctx)){
         /* should get EOD */
         DMSG0(ctx, DERROR, "Protocol error, should get EOD.\n");
         return bRC_Error;
      }

      readacl = true;

      return bRC_OK;
   }

   /*
   * Reads XATTR data from backend during backup. Save it for handleXACLdata from
   * Bacula. As we do not know if a backend will send the XATTR data or not, we
   * cannot wait to read this data until handleXACLdata will be called.
   * TODO: The method has a limitation and accept up to PM_BSOCK xattr data to save
   *       which is about 64kB. It should be sufficient for virtually all xattr data
   *       we can imagine. But when a backend developer could expect a larger data
   *       to save he should rewrite perform_read_xattr() method.
   *
   * in:
   *    bpContext - for Bacula debug and jobinfo messages
   * out:
   *    bRC_OK - when XATTR data was read successfully and this.readxattr set
   *             to true
   *    bRC_Error - on any error during acl data read
   */
   bRC PLUGINCLASS::perform_read_xattr(bpContext *ctx)
   {
      DMSG0(ctx, DINFO, "perform_read_xattr\n");
      xattrdatalen = backend.ctx->read_data(ctx, xattrdata);
      if (xattrdatalen < 0){
         DMSG0(ctx, DERROR, "Cannot read XATTR data from backend.\n");
         return bRC_Error;
      }
      DMSG1(ctx, DINFO, "readXATTR: %i\n", xattrdatalen);
      if (!backend.ctx->read_ack(ctx)){
         /* should get EOD */
         DMSG0(ctx, DERROR, "Protocol error, should get EOD.\n");
         return bRC_Error;
      }
      readxattr = true;
      return bRC_OK;
   }

   /**
    * @brief Reads metadata info from backend and adds it as a metadata packet.
    *
    * @param ctx for Bacula debug and jobinfo messages
    * @param type detected Metadata type
    * @param sp save packet
    * @return bRC bRC_OK when success, bRC_Error when some error
    */
   bRC PLUGINCLASS::perform_read_metadata_info(bpContext *ctx, metadata_type type, struct save_pkt *sp)
   {
      POOL_MEM data(PM_MESSAGE);

      DMSG0(ctx, DINFO, "perform_read_metadata_info\n");

      int len = backend.ctx->read_data(ctx, data);
      if (len < 0){
         DMSG1(ctx, DERROR, "Cannot read METADATA(%i) information from backend.\n", type);
         return bRC_Error;
      }

      DMSG1(ctx, DINFO, "read METADATA info len: %i\n", len);
      if (!backend.ctx->read_ack(ctx)){
         /* should get EOD */
         DMSG0(ctx, DERROR, "Protocol error, should get EOD.\n");
         return bRC_Error;
      }

      // Bacula API for metadata requires that a plugin
      // handle metadata buffer allocation
      POOLMEM *ptr = (POOLMEM *)bmalloc(len);
      memcpy(ptr, data.addr(), len);

      // add it to the list for reference to not lot it
      metadatas_list.append(ptr);
      metadatas.add_packet(type, len, ptr);
      sp->plug_meta = &metadatas;

      return bRC_OK;
   }

   /**
    * @brief Does metadata command scan and map to metadata types.
    *
    * @param cmd a command string read from backend
    * @return metadata_type returned from map
    */
   metadata_type PLUGINCLASS::scan_metadata_type(bpContext *ctx, const POOL_MEM &cmd)
   {
      DMSG1(ctx, DDEBUG, "scan_metadata_type checking: %s\n", cmd.c_str());
      for (int i = 0; plugin_metadata_map[i].command != NULL; i++)
      {
         if (bstrcmp(cmd.c_str(), plugin_metadata_map[i].command)){
            DMSG2(ctx, DDEBUG, "match: %s => %d\n", plugin_metadata_map[i].command, plugin_metadata_map[i].type);
            return plugin_metadata_map[i].type;
         }
      }

      return plugin_meta_invalid;
   }

   const char * PLUGINCLASS::prepare_metadata_type(metadata_type type)
   {
      for (int i = 0; plugin_metadata_map[i].command != NULL; i++){
         if (plugin_metadata_map[i].type == type){
            return plugin_metadata_map[i].command;
         }
      }

      return "METADATA_STREAM\n";
   }

   /*
   * Sends ACL data from restore stream to backend.
   * TODO: The method has a limitation and accept a single xacl_pkt call for
   *       a single file. As the restored acl stream and records are the same as
   *       was saved during backup, you can expect no more then a single PM_BSOCK
   *       and about 64kB of acl data send to backend.
   *
   * in:
   *    bpContext - for Bacula debug and jobinfo messages
   *    xacl_pkt - the restored ACL data for backend
   * out:
   *    bRC_OK - when ACL data was restored successfully
   *    bRC_Error - on any error during acl data restore
   */
   bRC PLUGINCLASS::perform_write_acl(bpContext* ctx, const xacl_pkt* xacl)
   {
      if (xacl->count > 0) {
         POOL_MEM cmd(PM_FNAME);
         /* send command ACL */
         pm_strcpy(cmd, "ACL\n");
         backend.ctx->write_command(ctx, cmd.c_str());
         /* send acls data */
         DMSG1(ctx, DINFO, "writeACL: %i\n", xacl->count);
         int rc = backend.ctx->write_data(ctx, xacl->content, xacl->count);
         if (rc < 0) {
            /* got some error */
            return bRC_Error;
         }
         /* signal end of acls data to restore and get ack */
         if (!backend.ctx->send_ack(ctx)) {
            return bRC_Error;
         }
      }

      return bRC_OK;
   }

   /*
   * Sends XATTR data from restore stream to backend.
   * TODO: The method has a limitation and accept a single xacl_pkt call for
   *       a single file. As the restored acl stream and records are the same as
   *       was saved during backup, you can expect no more then a single PM_BSOCK
   *       and about 64kB of acl data send to backend.
   *
   * in:
   *    bpContext - for Bacula debug and jobinfo messages
   *    xacl_pkt - the restored XATTR data for backend
   * out:
   *    bRC_OK - when XATTR data was restored successfully
   *    bRC_Error - on any error during acl data restore
   */
   bRC PLUGINCLASS::perform_write_xattr(bpContext* ctx, const xacl_pkt* xacl)
   {
      if (xacl->count > 0) {
         POOL_MEM cmd(PM_FNAME);
         /* send command XATTR */
         pm_strcpy(cmd, "XATTR\n");
         backend.ctx->write_command(ctx, cmd.c_str());
         /* send xattrs data */
         DMSG1(ctx, DINFO, "writeXATTR: %i\n", xacl->count);
         int rc = backend.ctx->write_data(ctx, xacl->content, xacl->count);
         if (rc < 0) {
            /* got some error */
            return bRC_Error;
         }
         /* signal end of xattrs data to restore and get ack */
         if (!backend.ctx->send_ack(ctx)) {
            return bRC_Error;
         }
      }

      return bRC_OK;
   }

   /*
   * The method works as a dispatcher for expected commands received from backend.
   *    It handles a three commands associated with file attributes/metadata:
   *    - FNAME:... - the next file to backup
   *    - ACL - next data will be acl data, so perform_read_acl()
   *    - XATTR - next data will be xattr data, so perform_read_xattr()
   *    and additionally when no more files to backup it handles EOD.
   *
   * in:
   *    bpContext - for Bacula debug and jobinfo messages
   * out:
   *    bRC_OK - when plugin read the command, dispatched a work and setup flags
   *    bRC_Error - on any error during backup
   */
   bRC PLUGINCLASS::perform_read_metacommands(bpContext *ctx)
   {
      POOL_MEM cmd(PM_FNAME);

      DMSG0(ctx, DDEBUG, "perform_read_metacommands()\n");
      // setup flags
      nextfile = readacl = readxattr = false;
      objectsent = false;
      // loop on metadata from backend or EOD which means no more files to backup
      while (true)
      {
         if (backend.ctx->read_command(ctx, cmd) > 0){
            /* yup, should read FNAME, ACL or XATTR from backend, check which one */
            DMSG(ctx, DDEBUG, "read_command(1): %s\n", cmd.c_str());
            if (scan_parameter_str(cmd, "FNAME:", fname)){
               /* got FNAME: */
               nextfile = true;
               object = FileObject;
               return bRC_OK;
            }
            if (scan_parameter_str(cmd, "PLUGINOBJ:", fname)){
               /* got Plugin Object header */
               nextfile = true;
               object = PluginObject;
               // pluginobject = true;
               return bRC_OK;
            }
            if (scan_parameter_str(cmd, "RESTOREOBJ:", fname)){
               /* got Restore Object header */
               nextfile = true;
               object = RestoreObject;
               // restoreobject = true;
               return bRC_OK;
            }
            if (scan_parameter_str(cmd, "CHECK:", fname)){
               /* got accurate check query */
               perform_accurate_check(ctx);
               continue;
            }
            if (scan_parameter_str(cmd, "CHECKGET:", fname)){
               /* got accurate get query */
               perform_accurate_check_get(ctx);
               continue;
            }
            if (bstrcmp(cmd.c_str(), "ACL")){
               /* got ACL header */
               perform_read_acl(ctx);
               continue;
            }
            if (bstrcmp(cmd.c_str(), "XATTR")){
               /* got XATTR header */
               perform_read_xattr(ctx);
               continue;
            }
            if (bstrcmp(cmd.c_str(), "FileIndex")){
               /* got FileIndex query */
               perform_file_index_query(ctx);
               continue;
            }
            /* error in protocol */
            DMSG(ctx, DERROR, "Protocol error, got unknown command: %s\n", cmd.c_str());
            JMSG(ctx, M_FATAL, "Protocol error, got unknown command: %s\n", cmd.c_str());
            return bRC_Error;
         } else {
            if (backend.ctx->is_fatal()){
               /* raise up error from backend */
               return bRC_Error;
            }
            if (backend.ctx->is_eod()){
               /* no more files to backup */
               DMSG0(ctx, DDEBUG, "No more files to backup from backend.\n");
               return bRC_OK;
            }
         }
      }

      return bRC_Error;
   }

   /**
    * @brief Respond to the file index query command from backend.
    *
    * @param ctx bpContext - for Bacula debug and jobinfo messages
    * @return bRC bRC_OK when success, bRC_Error if not
    */
   bRC PLUGINCLASS::perform_file_index_query(bpContext *ctx)
   {
      POOL_MEM cmd(PM_FNAME);
      int32_t fileindex;

      getBaculaVar(bVarFileIndex, (void *)&fileindex);
      Mmsg(cmd, "%d\n", fileindex);
      if (backend.ctx->write_command(ctx, cmd) < 0){
         /* error */
         return bRC_Error;
      }

      return bRC_OK;
   }

   /**
    * @brief
    *
    * @param ctx bpContext - for Bacula debug and jobinfo messages
    * @return bRC bRC_OK when success, bRC_Error if not
    */
   bRC PLUGINCLASS::perform_accurate_check(bpContext *ctx)
   {
      if (strlen(fname.c_str()) == 0){
         // input variable is not valid
         return bRC_Error;
      }

      DMSG0(ctx, DDEBUG, "perform_accurate_check()\n");

      POOL_MEM cmd(PM_FNAME);
      struct save_pkt sp;
      memset(&sp, 0, sizeof(sp));

      // supported sequence is `STAT` followed by `TSTAMP`
      if (backend.ctx->read_command(ctx, cmd) < 0) {
         // error
         return bRC_Error;
      }

      metaplugin::attributes::Status status = metaplugin::attributes::read_scan_stat_command(ctx, cmd, &sp);
      if (status == metaplugin::attributes::Status_OK) {
         if (backend.ctx->read_command(ctx, cmd) < 0) {
            // error
            return bRC_Error;
         }

         status = metaplugin::attributes::read_scan_tstamp_command(ctx, cmd, &sp);
         if (status == metaplugin::attributes::Status_OK) {
            // success we can perform accurate check for stat packet
            bRC rc = bRC_OK;  // return 'OK' as a default
            if (accurate_mode) {
               sp.fname = fname.c_str();
               rc = checkChanges(&sp);
            } else {
               if (!accurate_mode_err) {
                  DMSG0(ctx, DERROR, "Backend CHECK command require accurate mode on!\n");
                  JMSG0(ctx, M_ERROR, "Backend CHECK command require accurate mode on!\n");
                  accurate_mode_err = true;
               }
            }

            POOL_MEM checkstatus(PM_NAME);
            Mmsg(checkstatus, "%s\n", rc == bRC_Seen ? "SEEN" : "OK");
            DMSG1(ctx, DINFO, "perform_accurate_check(): %s", checkstatus.c_str());

            if (!backend.ctx->write_command(ctx, checkstatus)) {
               DMSG0(ctx, DERROR, "Cannot send checkChanges() response to backend\n");
               JMSG0(ctx, backend.ctx->jmsg_err_level(), "Cannot send checkChanges() response to backend\n");
               return bRC_Error;
            }

            return bRC_OK;
         }
      } else {
         // check possible errors
         switch (status)
         {
         case metaplugin::attributes::Invalid_File_Type:
            JMSG2(ctx, M_ERROR, "Invalid file type: %c for %s\n", sp.type, fname.c_str());
            return bRC_Error;

         case metaplugin::attributes::Invalid_Stat_Packet:
            JMSG1(ctx, backend.ctx->jmsg_err_level(), "Invalid stat packet: %s\n", cmd.c_str());
            return bRC_Error;
         default:
            break;
         }
         // future extension for `ATTR` command
         // ...
      }

      return bRC_Error;
   }

   /**
    * @brief Perform accurate query check and resturn accurate data to backend.
    *
    * @param ctx bpContext - for Bacula debug and jobinfo messages
    * @return bRC bRC_OK when success, bRC_Error if not
    */
   bRC PLUGINCLASS::perform_accurate_check_get(bpContext *ctx)
   {
      POOL_MEM cmd(PM_FNAME);

      if (strlen(fname.c_str()) == 0){
         // input variable is not valid
         return bRC_Error;
      }

      DMSG0(ctx, DDEBUG, "perform_accurate_check_get()\n");

      if (!accurate_mode) {
         // the job is not accurate, so no accurate data will be available at all
         pm_strcpy(cmd, "NOACCJOB\n");
         if (!backend.ctx->signal_error(ctx, cmd)) {
            DMSG0(ctx, DERROR, "Cannot send 'No Accurate Job' info to backend\n");
            JMSG0(ctx, backend.ctx->jmsg_err_level(), "Cannot send 'No Accurate Job' info to backend\n");
            return bRC_Error;
         }
         return bRC_OK;
      }

      accurate_attribs_pkt attribs;
      memset(&attribs, 0, sizeof(attribs));

      attribs.fname = fname.c_str();
      bRC rc = getAccurateAttribs(&attribs);

      struct restore_pkt rp;

      switch (rc)
      {
      case bRC_Seen:
         memcpy(&rp.statp, &attribs.statp, sizeof(rp.statp));
         rp.type = FT_MASK;   // This is a special metaplugin protocol hack
                              // because the current Bacula accurate code does
                              // not handle FileType on catalog attributes, yet.
         // STAT:...
         metaplugin::attributes::make_stat_command(ctx, cmd, &rp);
         backend.ctx->write_command(ctx, cmd);

         // TSTAMP:...
         if (metaplugin::attributes::make_tstamp_command(ctx, cmd, &rp) == metaplugin::attributes::Status_OK) {
            backend.ctx->write_command(ctx, cmd);
            DMSG(ctx, DINFO, "createFile:%s", cmd.c_str());
         }

         break;
      default:
         pm_strcpy(cmd, "UNAVAIL\n");
         if (!backend.ctx->write_command(ctx, cmd)) {
            DMSG0(ctx, DERROR, "Cannot send 'UNAVAIL' response to backend\n");
            JMSG0(ctx, backend.ctx->jmsg_err_level(), "Cannot send 'UNAVAIL' response to backend\n");
            return bRC_Error;
         }
         break;
      }

      return bRC_OK;
   }

   /**
    * @brief
    *
    * @param ctx bpContext - for Bacula debug and jobinfo messages
    * @param sp save_pkt from startBackupFile()
    * @return bRC bRC_OK when success, bRC_Error if not
    */
   bRC PLUGINCLASS::perform_read_pluginobject(bpContext *ctx, struct save_pkt *sp)
   {
      POOL_MEM cmd(PM_FNAME);

      if (strlen(fname.c_str()) == 0){
         // input variable is not valid
         return bRC_Error;
      }

      sp->plugin_obj.path = fname.c_str();
      DMSG0(ctx, DDEBUG, "perform_read_pluginobject()\n");
      // loop on plugin objects parameters from backend and EOD
      while (true){
         if (backend.ctx->read_command(ctx, cmd) > 0){
            DMSG(ctx, DDEBUG, "read_command(3): %s\n", cmd.c_str());
            if (scan_parameter_str(cmd, "PLUGINOBJ_CAT:", plugin_obj_cat)){
               DMSG1(ctx, DDEBUG, "category: %s\n", plugin_obj_cat.c_str());
               sp->plugin_obj.object_category = plugin_obj_cat.c_str();
               continue;
            }
            if (scan_parameter_str(cmd, "PLUGINOBJ_TYPE:", plugin_obj_type)){
               DMSG1(ctx, DDEBUG, "type: %s\n", plugin_obj_type.c_str());
               sp->plugin_obj.object_type = plugin_obj_type.c_str();
               continue;
            }
            if (scan_parameter_str(cmd, "PLUGINOBJ_NAME:", plugin_obj_name)){
               DMSG1(ctx, DDEBUG, "name: %s\n", plugin_obj_name.c_str());
               sp->plugin_obj.object_name = plugin_obj_name.c_str();
               continue;
            }
            if (scan_parameter_str(cmd, "PLUGINOBJ_SRC:", plugin_obj_src)){
               DMSG1(ctx, DDEBUG, "src: %s\n", plugin_obj_src.c_str());
               sp->plugin_obj.object_source = plugin_obj_src.c_str();
               continue;
            }
            if (scan_parameter_str(cmd, "PLUGINOBJ_UUID:", plugin_obj_uuid)){
               DMSG1(ctx, DDEBUG, "uuid: %s\n", plugin_obj_uuid.c_str());
               sp->plugin_obj.object_uuid = plugin_obj_uuid.c_str();
               continue;
            }
            POOL_MEM param(PM_NAME);
            if (scan_parameter_str(cmd, "PLUGINOBJ_SIZE:", param)){
               if (!size_to_uint64(param.c_str(), strlen(param.c_str()), &plugin_obj_size)){
                  // error in convert
                  DMSG1(ctx, DERROR, "Cannot convert Plugin Object Size to integer! p=%s\n", param.c_str());
                  JMSG1(ctx, M_ERROR, "Cannot convert Plugin Object Size to integer! p=%s\n", param.c_str());
                  return bRC_Error;
               }
               DMSG1(ctx, DDEBUG, "size: %llu\n", plugin_obj_size);
               sp->plugin_obj.object_size = plugin_obj_size;
               continue;
            }
            if (scan_parameter_str(cmd, "PLUGINOBJ_COUNT:", param)){
               uint32_t count = str_to_int64(param.c_str());
               DMSG1(ctx, DDEBUG, "count: %lu\n", count);
               sp->plugin_obj.count = count;
               continue;
            }
            /* error in protocol */
            DMSG(ctx, DERROR, "Protocol error, got unknown command: %s\n", cmd.c_str());
            JMSG(ctx, M_FATAL, "Protocol error, got unknown command: %s\n", cmd.c_str());
            return bRC_Error;
         } else {
            if (backend.ctx->is_fatal()){
               /* raise up error from backend */
               return bRC_Error;
            }
            if (backend.ctx->is_eod()){
               /* no more plugin object params to backup */
               DMSG0(ctx, DINFO, "No more Plugin Object params from backend.\n");
               // pluginobject = false;
               // pluginobjectsent = true;
               objectsent = true;
               return bRC_OK;
            }
         }
      }

      return bRC_Error;
   }

   /**
    * @brief Receives a Restore Object data and populates save_pkt.
    *
    * @param ctx bpContext - for Bacula debug and jobinfo messages
    * @param sp save_pkt from startBackupFile()
    * @return bRC bRC_OK when success, bRC_Error if not
    */
   bRC PLUGINCLASS::perform_read_restoreobject(bpContext *ctx, struct save_pkt *sp)
   {
      POOL_MEM cmd(PM_FNAME);

      sp->restore_obj.object = NULL;

      if (strlen(fname.c_str()) == 0){
         // input variable is not valid
         return bRC_Error;
      }

      DMSG0(ctx, DDEBUG, "perform_read_restoreobject()\n");
      // read object length required param
      if (backend.ctx->read_command(ctx, cmd) > 0) {
         DMSG(ctx, DDEBUG, "read_command(4): %s\n", cmd.c_str());
         POOL_MEM param(PM_NAME);
         uint64_t length;
         if (scan_parameter_str(cmd, "RESTOREOBJ_LEN:", param)) {
            if (!size_to_uint64(param.c_str(), strlen(param.c_str()), &length)){
               // error in convert
               DMSG1(ctx, DERROR, "Cannot convert Restore Object length to integer! p=%s\n", param.c_str());
               JMSG1(ctx, M_ERROR, "Cannot convert Restore Object length to integer! p=%s\n", param.c_str());
               return bRC_Error;
            }
            DMSG1(ctx, DDEBUG, "size: %llu\n", length);
            sp->restore_obj.object_len = length;
            robjbuf.check_size(length + 1);
         } else {
            // no required param
            DMSG0(ctx, DERROR, "Cannot read Restore Object length!\n");
            JMSG0(ctx, M_ERROR, "Cannot read Restore Object length!\n");
            return bRC_Error;
         }
      } else {
         if (backend.ctx->is_fatal()){
            /* raise up error from backend */
            return bRC_Error;
         }
      }

      int32_t recv_len = 0;

      if (backend.ctx->recv_data(ctx, robjbuf, &recv_len) != bRC_OK) {
         DMSG0(ctx, DERROR, "Cannot read data from backend!\n");
         return bRC_Error;
      }

      /* no more restore object data to backup */
      DMSG0(ctx, DINFO, "No more Restore Object data from backend.\n");
      objectsent = true;

      if (recv_len != sp->restore_obj.object_len) {
         DMSG2(ctx, DERROR, "Backend reported RO length:%ld read:%ld\n", sp->restore_obj.object_len, recv_len);
         JMSG2(ctx, M_ERROR, "Backend reported RO length:%ld read:%ld\n", sp->restore_obj.object_len, recv_len);
         sp->restore_obj.object_len = recv_len;
      }

      sp->restore_obj.object = robjbuf.c_str();

      return bRC_OK;
   }

   /*
   * Handle Bacula Plugin I/O API for backend
   *
   * in:
   *    bpContext - for Bacula debug and jobinfo messages
   *    io - Bacula Plugin API I/O structure for I/O operations
   * out:
   *    bRC_OK - when successful
   *    bRC_Error - on any error
   *    io->status, io->io_errno - correspond to a plugin io operation status
   */
   bRC PLUGINCLASS::pluginIO(bpContext *ctx, struct io_pkt *io)
   {
      static int rw = 0;      // this variable handles single debug message

      {
         // synchronie access to job_cancelled variable
         // lock_guard lg(&mutex); - removed on request
         if (job_cancelled) {
            return bRC_Error;
         }
      }

      /* assume no error from the very beginning */
      io->status = 0;
      io->io_errno = 0;
      switch (io->func) {
         case IO_OPEN:
            DMSG(ctx, D2, "IO_OPEN: (%s)\n", io->fname);
            switch (mode){
               case BACKUP_FULL:
               case BACKUP_INCR:
               case BACKUP_DIFF:
                  return perform_backup_open(ctx, io);
               case RESTORE:
                  nodata = true;
                  break;
               default:
                  return bRC_Error;
            }
            break;
         case IO_READ:
            if (!rw) {
               rw = 1;
               DMSG2(ctx, D2, "IO_READ buf=%p len=%d\n", io->buf, io->count);
            }
            switch (mode){
               case BACKUP_FULL:
               case BACKUP_INCR:
               case BACKUP_DIFF:
                  return perform_read_data(ctx, io);
               default:
                  return bRC_Error;
            }
            break;
         case IO_WRITE:
            if (!rw) {
               rw = 1;
               DMSG2(ctx, D2, "IO_WRITE buf=%p len=%d\n", io->buf, io->count);
            }
            switch (mode){
               case RESTORE:
                  return perform_write_data(ctx, io);
               default:
                  return bRC_Error;
            }
            break;
         case IO_CLOSE:
            DMSG0(ctx, D2, "IO_CLOSE\n");
            rw = 0;
            if (!backend.ctx->close_extpipe(ctx)){
               return bRC_Error;
            }
            switch (mode){
               case RESTORE:
                  return perform_write_end(ctx, io);
               case BACKUP_FULL:
               case BACKUP_INCR:
               case BACKUP_DIFF:
                  return perform_read_metacommands(ctx);
               default:
                  return bRC_Error;
            }
            break;
      }

      return bRC_OK;
   }

   /*
   * Get all required information from backend to populate save_pkt for Bacula.
   *    It handles a Restore Object (FT_PLUGIN_CONFIG) for every Full backup and
   *    new Plugin Backup Command if setup in FileSet. It uses a help from
   *    endBackupFile() handling the next FNAME command for the next file to
   *    backup. The communication protocol requires some file attributes command
   *    required it raise the error when insufficient parameters received from
   *    backend. It assumes some parameters at save_pkt struct to be automatically
   *    set like: sp->portable, sp->statp.st_blksize, sp->statp.st_blocks.
   *
   * in:
   *    bpContext - for Bacula debug and jobinfo messages
   *    save_pkt - Bacula Plugin API save packet structure
   * out:
   *    bRC_OK - when save_pkt prepared successfully and we have file to backup
   *    bRC_Max - when no more files to backup
   *    bRC_Error - in any error
   */
   bRC PLUGINCLASS::startBackupFile(bpContext *ctx, struct save_pkt *sp)
   {
      POOL_MEM cmd(PM_FNAME);
      int reqparams = 2;

      if (job_cancelled) {
         return bRC_Error;
      }

      /* The first file in Full backup, is the RestoreObject */
      if (!estimate && mode == BACKUP_FULL && pluginconfigsent == false) {
         ConfigFile ini;
         ini.register_items(plugin_items_dump, sizeof(struct ini_items));
         sp->restore_obj.object_name = (char *)INI_RESTORE_OBJECT_NAME;
         sp->restore_obj.object_len = ini.serialize(robjbuf.handle());
         sp->restore_obj.object = robjbuf.c_str();
         sp->type = FT_PLUGIN_CONFIG;
         DMSG2(ctx, DINFO, "Prepared RestoreObject/%s (%d) sent.\n", INI_RESTORE_OBJECT_NAME, FT_PLUGIN_CONFIG);
         return bRC_OK;
      }

      // check if this is the first file from backend to backup
      if (!nextfile){
         // so read FNAME or EOD/Error
         if (perform_read_metacommands(ctx) != bRC_OK){
            // signal error
            return bRC_Error;
         }
         if (!nextfile){
            // got EOD, so no files to backup at all!
            // if we return a value different from bRC_OK then Bacula will finish
            // backup process, which at first call means no files to archive
            return bRC_Max;
         }
      }
      // setup required fname in save_pkt
      DMSG(ctx, DINFO, "fname:%s\n", fname.c_str());
      sp->fname = fname.c_str();

      switch (object)
      {
      case RestoreObject:
         // handle Restore Object parameters and data
         if (perform_read_restoreobject(ctx, sp) != bRC_OK) {
            // signal error
            return bRC_Error;
         }
         sp->restore_obj.object_name = fname.c_str();
         sp->type = FT_RESTORE_FIRST;
         sp->statp.st_size = sp->restore_obj.object_len;
         sp->statp.st_mode = 0700 | S_IFREG;
         {
            time_t now = time(NULL);
            sp->statp.st_ctime = now;
            sp->statp.st_mtime = now;
            sp->statp.st_atime = now;
         }
         break;
      case PluginObject:
         // handle Plugin Object parameters
         if (perform_read_pluginobject(ctx, sp) != bRC_OK) {
            // signal error
            return bRC_Error;
         }
         sp->type = FT_PLUGIN_OBJECT;
         sp->statp.st_size = sp->plugin_obj.object_size;
         break;
      default:
         // here we handle standard file metadata information
         reqparams--;

         // ensure clear state for metadatas
         sp->plug_meta = NULL;
         metadatas.reset();
         metadatas_list.destroy();

         while (backend.ctx->read_command(ctx, cmd) > 0)
         {
            DMSG(ctx, DINFO, "read_command(2): %s\n", cmd.c_str());
            metaplugin::attributes::Status status = metaplugin::attributes::read_scan_stat_command(ctx, cmd, sp);
            switch (status)
            {
            case metaplugin::attributes::Invalid_File_Type:
               JMSG2(ctx, M_ERROR, "Invalid file type: %c for %s\n", sp->type, fname.c_str());
               return bRC_Error;

            case metaplugin::attributes::Invalid_Stat_Packet:
               JMSG1(ctx, backend.ctx->jmsg_err_level(), "Invalid stat packet: %s\n", cmd.c_str());
               return bRC_Error;

            case metaplugin::attributes::Status_OK:
               if (sp->type != FT_LNK) {
                  reqparams--;
               }
               continue;
            default:
               break;
            }
            status = metaplugin::attributes::read_scan_tstamp_command(ctx, cmd, sp);
            switch (status)
            {
            case metaplugin::attributes::Status_OK:
               continue;
            default:
               break;
            }
            if (scan_parameter_str(cmd, "LSTAT:", lname) == 1) {
               sp->link = lname.c_str();
               reqparams--;
               DMSG(ctx, DINFO, "LSTAT:%s\n", lname.c_str());
               continue;
            }
            POOL_MEM tmp(PM_FNAME);
            if (scan_parameter_str(cmd, "PIPE:", tmp)) {
               /* handle PIPE command */
               DMSG(ctx, DINFO, "read pipe at: %s\n", tmp.c_str());
               int extpipe = open(tmp.c_str(), O_RDONLY);
               if (extpipe > 0) {
                  DMSG0(ctx, DINFO, "ExtPIPE file available.\n");
                  backend.ctx->set_extpipe(extpipe);
                  pm_strcpy(tmp, "OK\n");
                  backend.ctx->write_command(ctx, tmp.c_str());
               } else {
                  /* here are common error signaling */
                  berrno be;
                  DMSG(ctx, DERROR, "ExtPIPE file open error! Err=%s\n", be.bstrerror());
                  JMSG(ctx, backend.ctx->jmsg_err_level(), "ExtPIPE file open error! Err=%s\n", be.bstrerror());
                  pm_strcpy(tmp, "Err\n");
                  backend.ctx->signal_error(ctx, tmp.c_str());
                  return bRC_Error;
               }
               continue;
            }
            metadata_type mtype = scan_metadata_type(ctx, cmd);
            if (mtype != plugin_meta_invalid) {
               DMSG1(ctx, DDEBUG, "metaData handling: %d\n", mtype);
               if (perform_read_metadata_info(ctx, mtype, sp) != bRC_OK) {
                  DMSG0(ctx, DERROR, "Cannot perform_read_metadata_info!\n");
                  JMSG0(ctx, backend.ctx->jmsg_err_level(), "Cannot perform_read_metadata_info!\n");
                  return bRC_Error;
               }
               continue;
            } else {
               DMSG1(ctx, DERROR, "Invalid File Attributes command: %s\n", cmd.c_str());
               JMSG1(ctx, backend.ctx->jmsg_err_level(), "Invalid File Attributes command: %s\n", cmd.c_str());
               return bRC_Error;
            }
         }

         DMSG0(ctx, DINFO, "File attributes end.\n");
         if (reqparams > 0) {
            DMSG0(ctx, DERROR, "Protocol error, not enough file attributes from backend.\n");
            JMSG0(ctx, M_FATAL, "Protocol error, not enough file attributes from backend.\n");
            return bRC_Error;
         }

         break;
      }

      if (backend.ctx->is_error()) {
         return bRC_Error;
      }

      sp->portable = true;
      sp->statp.st_blksize = 4096;
      sp->statp.st_blocks = sp->statp.st_size / 4096 + 1;

      DMSG3(ctx, DINFO, "TSDebug: %ld(at) %ld(mt) %ld(ct)\n",
            sp->statp.st_atime, sp->statp.st_mtime, sp->statp.st_ctime);

      return bRC_OK;
   }

   /*
   * Check for a next file to backup or the end of the backup loop.
   *    The next file to backup is indicated by a FNAME command from backend and
   *    no more files to backup as EOD. It helps startBackupFile handling FNAME
   *    for next file.
   *
   * in:
   *    bpContext - for Bacula debug and jobinfo messages
   *    save_pkt - Bacula Plugin API save packet structure
   * out:
   *    bRC_OK - when no more files to backup
   *    bRC_More - when Bacula should expect a next file
   *    bRC_Error - in any error
   */
   bRC PLUGINCLASS::endBackupFile(bpContext *ctx)
   {
      POOL_MEM cmd(PM_FNAME);

      {
         // synchronie access to job_cancelled variable
         // lock_guard lg(&mutex); - removed on request
         if (job_cancelled) {
            return bRC_Error;
         }
      }

      if (!estimate){
         /* The current file was the restore object, so just ask for the next file */
         if (mode == BACKUP_FULL && pluginconfigsent == false) {
            pluginconfigsent = true;
            return bRC_More;
         }
      }

      // check for next file only when no previous error
      if (!openerror) {
         if (estimate || objectsent) {
            objectsent = false;
            if (perform_read_metacommands(ctx) != bRC_OK) {
               /* signal error */
               return bRC_Error;
            }
         }

         if (nextfile) {
            DMSG1(ctx, DINFO, "nextfile %s backup!\n", fname.c_str());
            return bRC_More;
         }
      }

      return bRC_OK;
   }

   /*
   * The PLUGIN is using this callback to handle Core restore.
   */
   bRC PLUGINCLASS::startRestoreFile(bpContext *ctx, const char *cmd)
   {
      if (restoreobject_list.size() > 0) {
         restore_object_class *ropclass;
         POOL_MEM backcmd(PM_FNAME);

         foreach_alist(ropclass, &restoreobject_list) {
            if (!ropclass->sent && strcmp(cmd, ropclass->plugin_name.c_str()) == 0) {

               Mmsg(backcmd, "RESTOREOBJ:%s\n", ropclass->object_name.c_str());
               DMSG1(ctx, DINFO, "%s", backcmd.c_str());
               ropclass->sent = true;

               if (!backend.ctx->write_command(ctx, backcmd.c_str())) {
                  DMSG0(ctx, DERROR, "Error sending RESTOREOBJ command\n");
                  return bRC_Error;
               }

               Mmsg(backcmd, "RESTOREOBJ_LEN:%d\n", ropclass->length);
               if (!backend.ctx->write_command(ctx, backcmd.c_str())) {
                  DMSG0(ctx, DERROR, "Error sending RESTOREOBJ_LEN command\n");
                  return bRC_Error;
               }

               /* send data */
               if (backend.ctx->send_data(ctx, ropclass->data, ropclass->length) != bRC_OK) {
                  DMSG0(ctx, DERROR, "Error sending RestoreObject data\n");
                  return bRC_Error;
               }
            }
         }
      }

      return bRC_OK;
   }

   /*
   * The PLUGIN is not using this callback to handle restore.
   */
   bRC PLUGINCLASS::endRestoreFile(bpContext *ctx)
   {
      return bRC_OK;
   }

   /*
   * Prepares a file to restore attributes based on data from restore_pkt.
   * It handles a response from backend to show if
   *
   * in:
   *    bpContext - bacula plugin context
   *    restore_pkt - Bacula Plugin API restore packet structure
   * out:
   *    bRC_OK - when success reported from backend
   *    rp->create_status = CF_EXTRACT - the backend will restore the file
   *                                     with pleasure
   *    rp->create_status = CF_SKIP - the backend wants to skip restoration, i.e.
   *                                  the file already exist and Replace=n was set
   *    bRC_Error, rp->create_status = CF_ERROR - in any error
   */
   bRC PLUGINCLASS::createFile(bpContext *ctx, struct restore_pkt *rp)
   {
      POOL_MEM cmd(PM_FNAME);
      // char type;

      {
         // synchronie access to job_cancelled variable
         // lock_guard lg(&mutex); - removed on request
         if (job_cancelled) {
            return bRC_Error;
         }
      }

      skipextract = false;
      acldatalen = 0;
      xattrdatalen = 0;
      if (CORELOCALRESTORE && islocalpath(where)) {
         DMSG0(ctx, DDEBUG, "createFile:Forwarding restore to Core\n");
         rp->create_status = CF_CORE;
      } else {
         // FNAME:$fname$
         Mmsg(cmd, "FNAME:%s\n", rp->ofname);
         backend.ctx->write_command(ctx, cmd);
         DMSG(ctx, DINFO, "createFile:%s", cmd.c_str());

         // STAT:...
         metaplugin::attributes::make_stat_command(ctx, cmd, rp);
         backend.ctx->write_command(ctx, cmd);
         last_type = rp->type;
         DMSG(ctx, DINFO, "createFile:%s", cmd.c_str());

         // TSTAMP:...
         if (metaplugin::attributes::make_tstamp_command(ctx, cmd, rp) == metaplugin::attributes::Status_OK) {
            backend.ctx->write_command(ctx, cmd);
            DMSG(ctx, DINFO, "createFile:%s", cmd.c_str());
         }

         // LSTAT:$link$
         if (rp->type == FT_LNK && rp->olname != NULL){
            Mmsg(cmd, "LSTAT:%s\n", rp->olname);
            backend.ctx->write_command(ctx, cmd);
            DMSG(ctx, DINFO, "createFile:%s", cmd.c_str());
         }

         backend.ctx->signal_eod(ctx);

         // check if backend accepted the file
         if (backend.ctx->read_command(ctx, cmd) > 0){
            DMSG(ctx, DINFO, "createFile:resp: %s\n", cmd.c_str());
            if (strcmp(cmd.c_str(), "OK") == 0){
               rp->create_status = CF_EXTRACT;
            } else
            if (strcmp(cmd.c_str(), "SKIP") == 0){
               rp->create_status = CF_SKIP;
               skipextract = true;
            } else
            if (strcmp(cmd.c_str(), "CORE") == 0){
               rp->create_status = CF_CORE;
            } else {
               DMSG(ctx, DERROR, "Wrong backend response to create file, got: %s\n", cmd.c_str());
               JMSG(ctx, backend.ctx->jmsg_err_level(), "Wrong backend response to create file, got: %s\n", cmd.c_str());
               rp->create_status = CF_ERROR;
               return bRC_Error;
            }
         } else {
            if (backend.ctx->is_error()){
               /* raise up error from backend */
               rp->create_status = CF_ERROR;
               return bRC_Error;
            }
         }
      }

      return bRC_OK;
   }

   /*
   * Unimplemented, always return bRC_OK.
   */
   bRC PLUGINCLASS::setFileAttributes(bpContext *ctx, struct restore_pkt *rp)
   {
      return bRC_OK;
   }

   /**
    * @brief
    *
    * @param ctx
    * @param exepath
    * @return bRC
    */
   void PLUGINCLASS::setup_backend_command(bpContext *ctx, POOL_MEM &exepath)
   {
      DMSG(ctx, DINFO, "ExePath: %s\n", exepath.c_str());
      Mmsg(backend_cmd, "%s/%s", exepath.c_str(), BACKEND_CMD);
      DMSG(ctx, DINFO, "BackendPath: %s\n", backend_cmd.c_str());
      if (access(backend_cmd.c_str(), X_OK) < 0)
      {
         berrno be;
         DMSG2(ctx, DERROR, "Unable to use backend: %s Err=%s\n", backend_cmd.c_str(), be.bstrerror());
         pm_strcpy(backend_error, be.bstrerror());
         backend_available = false;
      } else {
         DMSG0(ctx, DINFO, "Backend available\n");
         backend_available = true;
      }
   }

   /**
    * @brief
    *
    * @param ctx
    * @param xacl
    * @return bRC
    */
   bRC PLUGINCLASS::handleXACLdata(bpContext *ctx, struct xacl_pkt *xacl)
   {
      {
         // synchronie access to job_cancelled variable
         // lock_guard lg(&mutex); - removed on request
         if (job_cancelled) {
            return bRC_Error;
         }
      }

      switch (xacl->func)
      {
      case BACL_BACKUP:
         if (readacl) {
            DMSG0(ctx, DINFO, "bacl_backup\n");
            xacl->count = acldatalen;
            xacl->content = acldata.c_str();
            readacl= false;
         } else {
            xacl->count = 0;
         }
         break;
      case BACL_RESTORE:
         DMSG1(ctx, DINFO, "bacl_restore: %d\n", last_type);
            if (!skipextract) {
               if (last_type != FT_DIREND) {
                  return perform_write_acl(ctx, xacl);
               } else {
                  DMSG0(ctx, DDEBUG, "delay ACL stream restore\n");
                  acldatalen = xacl->count;
                  pm_memcpy(acldata, xacl->content, acldatalen);
               }
            }
            break;
      case BXATTR_BACKUP:
         if (readxattr){
            DMSG0(ctx, DINFO, "bxattr_backup\n");
            xacl->count = xattrdatalen;
            xacl->content = xattrdata.c_str();
            readxattr= false;
         } else {
            xacl->count = 0;
         }
         break;
      case BXATTR_RESTORE:
         DMSG1(ctx, DINFO, "bxattr_restore: %d\n", last_type);
         if (!skipextract) {
            if (last_type != FT_DIREND) {
               return perform_write_xattr(ctx, xacl);
            } else {
               DMSG0(ctx, DDEBUG, "delay XATTR stream restore\n");
               xattrdatalen = xacl->count;
               pm_memcpy(xattrdata, xacl->content, xattrdatalen);
            }
         }
         break;
      }

      return bRC_OK;
   }

   /*
   * QueryParameter interface
   */
   bRC PLUGINCLASS::queryParameter(bpContext *ctx, struct query_pkt *qp)
   {
      DMSG0(ctx, D1, "PLUGINCLASS::queryParameter\n");

      // check if it is our Plugin command
      if (!isourplugincommand(PLUGINPREFIX, qp->command) != 0){
         // it is not our plugin prefix
         return bRC_OK;
      }

      {
         // synchronie access to job_cancelled variable
         // lock_guard lg(&mutex); - removed on request
         if (job_cancelled) {
            return bRC_Error;
         }
      }

      POOL_MEM cmd(PM_MESSAGE);

      if (listing == None) {
         listing = Query;
         Mmsg(cmd, "%s query=%s", qp->command, qp->parameter);
         if (prepare_backend(ctx, BACKEND_JOB_INFO_ESTIMATE, cmd.c_str()) == bRC_Error){
            return bRC_Error;
         }
      }

      /* read backend response */
      char pkt = 0;
      int32_t pktlen = backend.ctx->read_any(ctx, &pkt, cmd);
      if (pktlen < 0) {
         DMSG(ctx, DERROR, "Cannot read backend query response for %s command.\n", qp->parameter);
         JMSG(ctx, backend.ctx->jmsg_err_level(), "Cannot read backend query response for %s command.\n", qp->parameter);
         return bRC_Error;
      }

      bRC ret = bRC_More;

      /* check EOD */
      if (backend.ctx->is_eod()){
         /* got EOD so the backend finish response, so terminate the chat */
         DMSG0(ctx, D1, "PLUGINCLASS::queryParameter: got EOD\n");
         backend.ctx->signal_term(ctx);
         backend.ctx->terminate(ctx);
         qp->result = NULL;
         ret = bRC_OK;
      } else {
         switch (pkt)
         {
         case 'C':
            {
               OutputWriter ow(qp->api_opts);
               char *p, *q, *t;
               alist values(10, not_owned_by_alist);
               key_pair *kp;

               /*
               * here we have:
               *    key=value[,key2=value2[,...]]
               * parameters we should decompose
               */
               p = cmd.c_str();
               while (*p != '\0') {
                  q = strchr(p, ',');
                  if (q != NULL) {
                     *q++ = '\0';
                  }
                  // single key=value
                  DMSG(ctx, D1, "PLUGINCLASS::queryParameter:scan %s\n", p);
                  if ((t = strchr(p, '=')) != NULL) {
                     *t++ = '\0';
                  } else {
                     t = (char*)"";     // pointer to empty string
                  }
                  DMSG2(ctx, D1, "PLUGINCLASS::queryParameter:pair '%s' = '%s'\n", p, t);
                  if (strlen(p) > 0) {
                     // push values only when we have key name
                     kp = New(key_pair(p, t));
                     values.append(kp);
                  }
                  p = q != NULL ? q : (char*)"";
               }

               // if more values then one then it is a list
               if (values.size() > 1) {
                  DMSG0(ctx, D1, "PLUGINCLASS::queryParameter: will render list\n")
                  ow.start_list(qp->parameter);
               }
               // render all values
               foreach_alist(kp, &values) {
                  ow.get_output(OT_STRING, kp->key.c_str(), kp->value.c_str(), OT_END);
                  delete kp;
               }
               if (values.size() > 1) {
                  ow.end_list();
               }
               pm_strcpy(robjbuf, ow.get_output(OT_END));
               qp->result = robjbuf.c_str();
            }
            break;
         case 'D':
            pm_memcpy(robjbuf, cmd.c_str(), pktlen);
            qp->result = robjbuf.c_str();
            break;
         default:
            DMSG(ctx, DERROR, "PLUGINCLASS::queryParameter: got invalid packet: %c\n", pkt);
            JMSG(ctx, M_ERROR, "PLUGINCLASS::queryParameter: got invalid packet: %c\n", pkt);
            backend.ctx->signal_term(ctx);
            backend.ctx->terminate(ctx);
            qp->result = NULL;
            ret = bRC_Error;
            break;
         }
      }

      return ret;
   }

   /**
    * @brief Sends metadata to backend for restore.
    *
    * @param ctx for Bacula debug and jobinfo messages
    * @param mp
    * @return bRC
    */
   bRC PLUGINCLASS::metadataRestore(bpContext *ctx, struct meta_pkt *mp)
   {
      {
         // synchronie access to job_cancelled variable
         // lock_guard lg(&mutex); - removed on request
         if (job_cancelled) {
            return bRC_Error;
         }
      }

      if (!skipextract){
         POOL_MEM cmd(PM_FNAME);

         if (mp->buf != NULL && mp->buf_len > 0){
            /* send command METADATA */
            pm_strcpy(cmd, prepare_metadata_type(mp->type));
            backend.ctx->write_command(ctx, cmd.c_str());
            /* send metadata stream data */
            DMSG1(ctx, DINFO, "writeMetadata: %i\n", mp->buf_len);
            int rc = backend.ctx->write_data(ctx, (char*)mp->buf, mp->buf_len);
            if (rc < 0){
               /* got some error */
               return bRC_Error;
            }

            // signal end of metadata stream to restore and get ack
            backend.ctx->signal_eod(ctx);

            // check if backend accepted the file
            if (backend.ctx->read_command(ctx, cmd) > 0) {
               DMSG(ctx, DINFO, "metadataRestore:resp: %s\n", cmd.c_str());
               if (bstrcmp(cmd.c_str(), "SKIP")) {
                  // SKIP!
                  skipextract = true;
                  return bRC_Skip;
               }
               if (!bstrcmp(cmd.c_str(), "OK")) {
                  DMSG(ctx, DERROR, "Wrong backend response to metadataRestore, got: %s\n", cmd.c_str());
                  JMSG(ctx, backend.ctx->jmsg_err_level(), "Wrong backend response to metadataRestore, got: %s\n", cmd.c_str());
                  return bRC_Error;
               }
            } else {
               if (backend.ctx->is_error()) {
                  // raise up error from backend
                  return bRC_Error;
               }
            }
         }
      }
      return bRC_OK;
   }

   /**
    * @brief Implements default metaplugin checkFile() callback.
    *    When fname match plugin configured namespace then it return bRC_Seen by default
    *    or calls custom checkFile() callback defined by backend developer.
    *
    * @param ctx for Bacula debug and jobinfo messages
    * @param fname file name to check
    * @return bRC bRC_Seen or bRC_OK
    */
   bRC PLUGINCLASS::checkFile(bpContext * ctx, char *fname)
   {
      if ((!CUSTOMNAMESPACE && isourpluginfname(PLUGINPREFIX, fname)) || (CUSTOMNAMESPACE && isourpluginfname(PLUGINNAMESPACE, fname)))
      {
         // synchronie access to job_cancelled variable
         // lock_guard lg(&mutex); - removed on request
         if (!job_cancelled) {
            if (::checkFile != NULL) {
               return ::checkFile(ctx, fname);
            }
         }
         return bRC_Seen;
      }

      return bRC_OK;
   }

#endif

}  // namespace pluginlib
