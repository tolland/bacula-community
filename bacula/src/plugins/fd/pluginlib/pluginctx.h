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
 * @file pluginctx.h
 * @author Radosław Korzeniewski (radoslaw@korzeniewski.net)
 * @brief This is a Bacula File Daemon general plugin framework. The Context.
 * @version 1.0.0
 * @date 2021-04-08
 *
 * @copyright Copyright (c) 2021 All rights reserved. IP transferred to Bacula Systems according to agreement.
 */

#include "pluginlib.h"
#include "lib/bregex.h"

#ifndef PLUGINLIB_PLUGINCTX_H
#define PLUGINLIB_PLUGINCTX_H

// The list of restore options saved to the RestoreObject.
extern struct ini_items plugin_items_dump[];

namespace pluginlib
{
   /*
    * This is a main plugin API class. It manages a plugin context.
    *  All the public methods correspond to a public Bacula API calls, even if
    *  a callback is not implemented.
    */
   class PLUGINCTX : public SMARTALLOC
   {
   public:
      PLUGINCTX(const char * command) :
         cmd(PM_FNAME),
         f_error(false),
         f_fatal(false),
         abort_on_error(false),
         f_debug(false),
         m_parameters_parsed(false),
         ini(),
         preg()
      { pm_strcpy(cmd, command); }
#if __cplusplus > 201103L
      PLUGINCTX() = delete;
      PLUGINCTX(PLUGINCTX&) = delete;
      PLUGINCTX(PLUGINCTX&&) = delete;
#endif
      virtual ~PLUGINCTX() {}

      virtual bRC parse_parameter(bpContext *ctx, const char *argk, const char *argv) = 0;
      virtual bRC parse_parameter(bpContext *ctx, ini_items &item) = 0;
      virtual bRC parse_plugin_config(bpContext *ctx, restore_object_pkt *rop);
      virtual bRC handle_restore_object(bpContext *ctx, restore_object_pkt *rop) { return bRC_OK; }

      /**
       * @brief Checks if plugin context operation is flagged on f_error.
       *
       * @return true when is error
       * @return false when no error
       */
      inline bool is_error() const { return f_error || f_fatal; }

      /**
       * @brief Checks if plugin context operation is flagged on f_fatal.
       *
       * @return true when is fatal error
       * @return false when no fatal error
       */
      inline bool is_fatal() const { return f_fatal || (f_error && abort_on_error); }

      /**
       * @brief Return a Job Message error level based on context
       *
       * @return int
       */
      inline int jmsg_err_level() const { return is_fatal() ? M_FATAL : M_ERROR; }

      /**
       * @brief Set the abort on error flag
       */
      inline void set_abort_on_error() { abort_on_error = true; }

      /**
       * @brief Clears the abort on error flag.
       */
      inline void clear_abort_on_error() { abort_on_error = false; }

      /**
       * @brief return abort on error flag status
       *
       * @return true if flag is set
       * @return false  if flag is not set
       */
      inline bool is_abort_on_error() const { return abort_on_error; }

      inline bool are_parameters_parsed() const { return m_parameters_parsed; }
      inline void set_parameters_parsed() { m_parameters_parsed = true; }

   protected:
      POOL_MEM cmd;                 /// plugin command for this context
      bool f_error;                 /// the plugin signaled an error
      bool f_fatal;                 /// the plugin signaled a fatal error
      bool abort_on_error;          /// abort on error flag
      bool f_debug;                 /// when additional debugging required
      bool m_parameters_parsed;
      ConfigFile ini;               /// Restore ini file handler
      regex_t preg;                 /// this is a regex context for include/exclude

      virtual int check_ini_param(char *param);
      virtual bool check_plugin_param(const char *param, alist *params) { return false; }
      virtual int get_ini_count() { return 0; }
   };
}  // namespace pluginlib

#endif   // PLUGINLIB_PLUGINBASE_H
