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
 * @file execprog.h
 * @author Radosław Korzeniewski (radoslaw@korzeniewski.net)
 * @brief This is a Bacula plugin external command execution context.
 * @version 1.2.0
 * @date 2020-01-05
 *
 * @copyright Copyright (c) 2021 All rights reserved. IP transferred to Bacula Systems according to agreement.
 */

#ifndef PLUGINLIB_EXECPROG_H
#define PLUGINLIB_EXECPROG_H

#include "pluginlib/pluginlib.h"


class EXECPROG : public SMARTALLOC
{
private:
   BPIPE *bpipe;              // this is our bpipe to communicate with external tools
   int rfd;                   // backend `stdout` to plugin file descriptor
   int wfd;                   // backend `stdin` to plugin file descriptor
   int efd;                   // backend `stderr` to plugin file descriptor
   int maxfd;                 // max file descriptors from bpipe channels
   POOL_MEM m_errmsg;         /// message buffer for error string
   int extpipe;               // set when data blast is performed using external pipe/file
   POOL_MEM extpipename;      // name of the external pipe/file for restore
   bool f_eod;                // the backend signaled EOD
   bool f_error;              // the backend signaled an error
   bool f_fatal;              // the backend signaled a fatal error
   bool f_cont;               // when we are reading next part of data packet
   bool abort_on_error;       // abort on error flag
   int32_t remaininglen;      // the number of bytes to read when `f_cont` is true
   struct timeval _timeout;   // a timeout when waiting for data to read from backend
   int tstatus;               //

public:
   EXECPROG() :
      bpipe(NULL),
      rfd(0),
      wfd(0),
      efd(0),
      maxfd(0),
      m_errmsg(PM_MESSAGE),
      extpipe(-1),
      extpipename(PM_FNAME),
      f_eod(false),
      f_error(false),
      f_fatal(false),
      f_cont(false),
      abort_on_error(false),
      remaininglen(0),
#if __cplusplus >= 201103L
      _timeout{0},
#endif
      tstatus(0)
   {
#if __cplusplus < 201103L
      _timeout.tv_sec = 0;
      _timeout.tv_usec = 0;
#endif
   }
#if __cplusplus >= 201103L
   EXECPROG(EXECPROG &) = delete;
   EXECPROG(EXECPROG &&) = delete;
   ~EXECPROG() = default;
#else
   ~EXECPROG() {};
#endif

   /**
    * @brief Checks if connection is open and we can use a bpipe object for communication.
    *
    * @return true if connection is closed and we can't use bpipe object
    * @return false if connection is available
    */
   inline bool is_open() { return bpipe != NULL; }

   /**
    * @brief Checks if connection is closed and we can't use a bpipe object for communication.
    *
    * @return true if connection is closed and we can't use bpipe object
    * @return false if connection is available
    */
   inline bool is_closed() { return bpipe == NULL; }

   /**
    * @brief Checks if backend sent us some error, backend error message is flagged on f_error.
    *
    * @return true
    * @return false
    */
   inline bool is_error() { return f_error || f_fatal; }

   /**
    * @brief Checks if backend sent us fatal error, backend error message is flagged on f_fatal.
    *
    * @return true
    * @return false
    */
   inline bool is_fatal() { return f_fatal || (f_error && abort_on_error); }

   /**
    * @brief Set the abort on error object
    *
    */
   inline void set_abort_on_error() { abort_on_error = true; }

   /**
    * @brief Clear abort_on_error flag.
    *
    */
   inline void clear_abort_on_error() { abort_on_error = false; }

   /**
    * @brief Return abort_on_error flag.
    *
    * @return true
    * @return false
    */
   inline bool is_abort_on_error() { return abort_on_error; }

   /**
    * @brief Checks if backend signaled EOD, eod from backend is flagged on f_eod.
    *
    * @return true when backend signaled EOD on last packet
    * @return false when backend did not signal EOD
    */
   inline bool is_eod() { return f_eod; }

   /**
    * @brief Clears the EOD from backend flag, f_eod.
    *  The eod flag is set when EOD message received from backend and not cleared
    *  until next recvbackend() call.
    */
   inline void clear_eod() { f_eod = false; }

   /**
    * @brief Get the cmd pid.
    *  Returns a backend PID if available.
    *
    * @return int the backend PID
    */
   inline int get_cmd_pid()
   {
      if (bpipe){
         return bpipe->worker_pid;
      }
      return -1;
   };

   inline int get_terminate_status() { return tstatus; }

   /* all you need is to simply execute the command first */
   bool execute_command(bpContext *ctx, const char *cmd, const char *args = "");

   /*
    * Run command and prepared parameters.
    */
   inline bool execute_command(bpContext *ctx, const char *cmd, const POOL_MEM &args) { return execute_command(ctx, cmd, args.c_str()); }
   inline bool execute_command(bpContext *ctx, const POOL_MEM &cmd, const POOL_MEM &args)  { return execute_command(ctx, cmd.c_str(), args.c_str()); }
   inline bool execute_command(bpContext *ctx, const POOL_MEM &cmd) { return execute_command(ctx, cmd.c_str()); }

   /* then just simply read or write data to it */
   int32_t read_data(bpContext *ctx, POOLMEM *buf, int32_t len);
   int32_t read_output(bpContext *ctx, POOL_MEM &out);
   int32_t write_data(bpContext *ctx, POOLMEM *buf, int32_t len);

   /* and finally terminate execution when finish */
   int terminate(bpContext *ctx, bool raise_error = true);

   inline POOL_MEM &get_error() { return m_errmsg; }
   inline char *get_error_str() { return m_errmsg.c_str(); }

   /* direct pipe management */
   inline int close_wpipe() { return ::close_wpipe(bpipe); }
};

#endif   // PLUGINLIB_EXECPROG_H
