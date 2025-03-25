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
 * @file ptcomm.h
 * @author Radosław Korzeniewski (radoslaw@korzeniewski.net)
 * @brief This is a process communication lowlevel library for Bacula plugin.
 * @version 3.0.0
 * @date 2021-08-20
 *
 * @copyright Copyright (c) 2021 All rights reserved. IP transferred to Bacula Systems according to agreement.
 */

#ifndef _PTCOMM_H_
#define _PTCOMM_H_

#include "pluginlib.h"

#define PTCOMM_DEFAULT_TIMEOUT   3600        // timeout waiting for data is 1H as some backends could spent it doing real work
                                             // TODO: I think we should move it to plugin configurable variable instead of a const

#define PTCOMM_MAX_PACKET_SIZE   999999

/*
 * The protocol packet header.
 *  Every packet exchanged between Plugin and Backend will have a special header
 *  which allow to perfectly synchronize data exchange mitigating the risk
 *  of a deadlock, where both ends will wait for a data and no one wants to
 *  send it to the other end.
 *  The protocol implements a single char packet status which could be:
 *      D - data packet
 *      C - command packet
 *      E - error packet
 *      F - EOD packet
 *      T - terminate connection
 *      W - warning message
 *      I - information message
 *      A - fatal error message (abort)
 *  The length is an ascii coded decimal trailed by a "newline" char - '\n'.
 *  So, a packet header could be rendered as: 'C000012\n'
 */
struct PTHEADER
{
   char status;
   char length[7];
};

/*
 * This is a low-level transport communication class which handles all bits and
 * bytes of the protocol.
 *  The class express a high-level methods for low-level transport protocol.
 *  It handles a communication channel (bpipe) with backend execution and
 *  termination. The external data exchange using named pipes or local files is
 *  handled by this class too.
 */
class PTCOMM : public SMARTALLOC
{
private:
   BPIPE *bpipe;              // this is our bpipe to communicate with backend */
   int rfd;                   // backend `stdout` to plugin file descriptor
   int wfd;                   // backend `stdin` to plugin file descriptor
   int efd;                   // backend `stderr` to plugin file descriptor
   int maxfd;                 // max file descriptors from bpipe channels
   POOL_MEM errmsg;           // message buffer for error string */
   int extpipe;               // set when data blast is performed using external pipe/file */
   POOL_MEM extpipename;      // name of the external pipe/file for restore */
   bool f_eod;                // the backend signaled EOD */
   bool f_error;              // the backend signaled an error */
   bool f_fatal;              // the backend signaled a fatal error */
   bool f_cont;               // when we are reading next part of data packet */
   bool abort_on_error;       // abort on error flag */
   int32_t remaininglen;      // the number of bytes to read when `f_cont` is true
   uint32_t m_timeout;        // a timeout when waiting for data to read from backend, in seconds

protected:
   bool recvbackend_data(bpContext *ctx, char *buf, int32_t nbytes);
   bool sendbackend_data(bpContext *ctx, const char *buf, int32_t nbytes);

   int32_t recvbackend_header(bpContext *ctx, char *cmd, bool any=false);
   int32_t handle_read_header(bpContext *ctx, char *cmd, bool any=false);
   int32_t handle_payload(bpContext *ctx, char *buf, int32_t nbytes);

   int32_t recvbackend(bpContext *ctx, char *cmd, POOL_MEM &buf, bool any=false);
   int32_t recvbackend_fixed(bpContext *ctx, char cmd, char *buf, int32_t bufsize);

   bool sendbackend(bpContext *ctx, char cmd, const POOLMEM *buf, int32_t len, bool _single_senddata = true);

public:
   PTCOMM(const char * command = NULL) :
      bpipe(NULL),
      rfd(0),
      wfd(0),
      efd(0),
      maxfd(0),
      errmsg(PM_MESSAGE),
      extpipe(-1),
      extpipename(PM_FNAME),
      f_eod(false),
      f_error(false),
      f_fatal(false),
      f_cont(false),
      abort_on_error(false),
      remaininglen(0),
      m_timeout(PTCOMM_DEFAULT_TIMEOUT)
   {}
#if __cplusplus > 201103L
   PTCOMM(PTCOMM &) = delete;
   PTCOMM(PTCOMM &&) = delete;
#endif
   ~PTCOMM() { terminate(NULL); }

   bool handshake(bpContext *ctx, const char *pluginname, const char *pluginapi);

   int32_t read_command(bpContext *ctx, POOL_MEM &buf);
   int32_t read_any(bpContext *ctx, char *cmd, POOL_MEM &buf);
   int32_t read_data(bpContext *ctx, POOL_MEM &buf);
   int32_t read_data_fixed(bpContext *ctx, char *buf, int32_t len);

   // we have to force non-optimized sendbackend path as origin of `*buf` is unknown
   bool write_command(bpContext *ctx, const char *buf, bool _single_senddata = false);

   bRC send_data(bpContext *ctx, const char *buf, int32_t len, bool _single_senddata = false);
   bRC send_data(bpContext *ctx, POOL_MEM &buf, int32_t len) { return send_data(ctx, buf.addr(), len, true); }
   bRC recv_data(bpContext *ctx, POOL_MEM &buf, int32_t *recv_len=NULL);

   /**
    * @brief Sends a command to the backend.
    *
    * @param ctx bpContext - for Bacula debug and jobinfo messages
    * @param buf a message buffer contains command to send
    * @return int32_t
    *    -1 - when encountered any error
    *    <n> - the number of bytes sent, success
    */
   int32_t write_command(bpContext *ctx, POOL_MEM &buf) { return write_command(ctx, buf.c_str(), true); }
   int32_t write_data(bpContext *ctx, const char *buf, int32_t len, bool _single_senddata = false);

   bool read_ack(bpContext *ctx);
   bool send_ack(bpContext *ctx);

   /**
    * @brief Signals en error to the backend.
    *    The buf, when not NULL, can hold an error string sent do the backend.
    *
    * @param ctx for Bacula debug and jobinfo messages
    * @param buf when not NULL should consist of an error string
    *            when NULL, no error string sent to the backend
    * @return true success
    * @return false when encountered any error
    */
   inline bool signal_error(bpContext *ctx, const char * buf, bool _single_senddata = false)
   {
      int32_t len = buf ? strlen(buf) : 0;
      return sendbackend(ctx, 'E', buf, len, _single_senddata);
   }
   inline bool signal_error(bpContext *ctx, const POOL_MEM &buf) { return signal_error(ctx, buf.c_str(), true); }

   POOLMEM *get_error(bpContext *ctx);

   /**
    * @brief Signals EOD to backend.
    *
    * @param ctx bpContext - for Bacula debug and jobinfo messages
    * @return true success
    * @return false when encountered any error
    */
   inline bool signal_eod(bpContext *ctx) { return sendbackend(ctx, 'F', NULL, 0); }

   /**
    * @brief Signal end of communication to the backend.
    *    The backend should close the connection after receiving this packet.
    *
    * @param ctx bpContext - for Bacula debug and jobinfo messages
    * @return true success
    * @return false when encountered any error
    */
   inline bool signal_term(bpContext *ctx) { return sendbackend(ctx, 'T', NULL, 0); }

   void terminate(bpContext *ctx);

   /**
    * @brief Returns a backend PID if available.
    *    I'm using an arthrymetic way to make a conditional value return;
    *
    * @return int backend PID - when backend available; -1 - when backend is unavailable
    */
   inline pid_t get_backend_pid() { return bpipe != NULL ? bpipe->worker_pid : -1; }

   /**
    * @brief Sets a BPIPE object for our main communication channel.
    *
    * @param bp object, we do not check for NULL here
    */
   inline void set_bpipe(BPIPE *bp)
   {
      bpipe = bp;
      rfd = fileno(bpipe->rfd);
      wfd = fileno(bpipe->wfd);
      efd = fileno(bpipe->efd);
      maxfd = MAX(rfd, wfd);
      maxfd = MAX(maxfd, efd) + 1;
   }

   /**
    * @brief Sets a FILE descriptor used as external pipe during backup and restore.
    *
    * @param ep a FILE* descriptor used during
    */
   inline void set_extpipe(int ep) { extpipe = ep; }

   /**
    * @brief Sets an external pipe name for restore.
    *
    * @param epname - external pipe name
    */
   inline void set_extpipename(char *epname) { pm_strcpy(extpipename, epname); }
   bool close_extpipe(bpContext *ctx);

   /**
    * @brief Checks if connection is open and we can use a bpipe object for communication.
    *
    * @return true if connection is available
    * @return false if connection is closed and we can't use bpipe object
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
    * @return true when last packet was an error
    * @return false when no error packets was received
    */
   inline bool is_error() { return f_error || f_fatal; }

   /**
    * @brief Checks if backend sent us fatal error, backend error message is flagged on f_fatal.
    *
    * @return true when last packet was a fatal error
    * @return false when no fatal error packets was received
    */
   inline bool is_fatal() { return f_fatal || (f_error && abort_on_error); }

   inline int jmsg_err_level() { return is_fatal() ? M_FATAL : M_ERROR; }
   /**
    * @brief Checks if backend signaled EOD, eod from backend is flagged on f_eod.
    *
    * @return true when backend signaled EOD on last packet
    * @return false when backend did not signal EOD
    */
   inline bool is_eod() { return f_eod; }

   /**
    * @brief Clears the EOD from backend flag, f_eod.
    *    The eod flag is set when EOD message received from backend and not cleared
    *    until next recvbackend() call.
    */
   void clear_eod() { f_eod = false; }

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
   inline bool is_abort_on_error() { return abort_on_error; }

   /**
    * @brief Set the timeout value in seconds
    *
    * @param timeout a value in seconds
    */
   inline void set_timeout(uint32_t timeout) { m_timeout = timeout > 0 ? timeout : PTCOMM_DEFAULT_TIMEOUT; }

   /**
    * @brief Get the timeout object
    *
    * @return uint32_t a current value of the PTCOMM Timeout
    */
   inline uint32_t get_timeout() { return m_timeout; }
};

#endif   /* _PTCOMM_H_ */
