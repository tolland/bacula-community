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
 * @file commctx.h
 * @author Radosław Korzeniewski (radoslaw@korzeniewski.net)
 * @brief This is a Bacula plugin command context switcher template.
 * @version 1.3.0
 * @date 2020-09-13
 *
 * @copyright Copyright (c) 2021 All rights reserved.
 *            IP transferred to Bacula Systems according to agreement.
 */

#ifndef PLUGINLIB_COMMCTX_H
#define PLUGINLIB_COMMCTX_H

#include "pluginlib.h"
#include "smartalist.h"


template<typename T>
class COMMCTX : public SMARTALLOC
{
   struct CMD : public SMARTALLOC
   {
      POOL_MEM cmd;
      T * ptr;
      CMD(const char * command) : cmd(PM_FNAME), ptr(New(T(command))) { pm_strcpy(cmd, command); };
#if __cplusplus > 201103L
      CMD() = delete;
      CMD(CMD &) = delete;
      CMD(CMD &&) = delete;
#endif
      ~CMD() { delete ptr; };
   };

   /** The command context list for multiple config execution for a single job */
   smart_alist<CMD> _command_list;

public:
   T * ctx;

   COMMCTX() : ctx(NULL) {};
#if __cplusplus > 201103L
   COMMCTX(COMMCTX &) = delete;
   COMMCTX(COMMCTX &&) = delete;
   ~COMMCTX() = default;
#else
   ~COMMCTX() {};
#endif

   T * switch_command(const char *command);
   bool check_command(const char *command);
   void foreach_command(void (*func)(T *, void *), void *param);
   bRC foreach_command_status(bRC (*func)(T *, void *), void *param);
   bool is_ctx_null() { return ctx == NULL ; }

   /* Shortcut that will blow up if ctx is not yet defined, use carefully
    * The gdb backtrace is misleading, we see pluginctx->xxx while we
    * do pluginctx.ctx->xxx
    */
   T * operator->() { return ctx; }
};


/**
 * @brief Switches current command context to requested.
 *
 * @tparam T is command context type.
 * @param command is command context string to switch into.
 * @return T* is command context pointer.
 */
template<typename T>
T * COMMCTX<T>::switch_command(const char * command)
{
   CMD * cmdctx;
   foreach_alist(cmdctx, &_command_list)
   {
      if (bstrcmp(cmdctx->cmd.c_str(), command))
      {
         ctx = cmdctx->ptr;
         return ctx;
      }
   }

   cmdctx = New(CMD(command));
   _command_list.append(cmdctx);
   ctx = cmdctx->ptr;

   return ctx;
}

/**
 * @brief Checks if command context is already defined on list.
 *
 * @tparam T is command context type.
 * @param command is command context string to check.
 * @return true when command context already exist.
 * @return false when command context does not exist yet.
 */
template<typename T>
bool COMMCTX<T>::check_command(const char *command)
{
   CMD * cmdctx;
   foreach_alist(cmdctx, &_command_list)
   {
      if (bstrcmp(cmdctx->cmd.c_str(), command))
         return true;
   }

   return false;
}

/**
 * @brief Iterate on all command context to execute function.
 *
 * @tparam T is command context type.
 * @param param is the execution function param.
 */
template<typename T>
void COMMCTX<T>::foreach_command(void(*func)(T*, void*), void* param)
{
   CMD * cmdctx;
   foreach_alist(cmdctx, &_command_list)
   {
      ctx = cmdctx->ptr;
      func(ctx, param);
   }
}

/**
 * @brief Iterate on all command context to execute function and return status.
 *
 * @tparam T is command context type.
 * @param param is the execution function param.
 */
template<typename T>
bRC COMMCTX<T>::foreach_command_status(bRC(*func)(T*, void*), void* param)
{
   CMD * cmdctx;
   bRC status = bRC_OK;

   foreach_alist(cmdctx, &_command_list)
   {
      ctx = cmdctx->ptr;
      bRC rc = func(ctx, param);
      if (rc != bRC_OK)
         status = rc;
   }

   return status;
}

#endif   // PLUGINLIB_COMMCTX_H
