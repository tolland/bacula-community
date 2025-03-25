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

#include "execprog.h"

/* Plugin compile time variables */
#define PLUGINPREFIX                "execprog:"


/*
 * Terminate the connection represented by BPIPE object.
 *    it shows a debug and job messages when connection close is unsuccessful
 *    and when ctx is available only.
 *
 * in:
 *    bpContext - Bacula Plugin context required for debug/job messages to show,
 *                it could be NULL in this case no messages will be shown
 * out:
 *    none
 */
int EXECPROG::terminate(bpContext *ctx, bool raise_error)
{
   // clear status
   tstatus = 0;
   pm_strcpy(m_errmsg, NULL);

   if (!is_closed())
   {
      // after close_bpipe it is no longer available
      pid_t wpid = bpipe->worker_pid;


      tstatus = close_bpipe(bpipe);
      berrno be;
      pm_strcpy(m_errmsg, be.bstrerror(tstatus));
      if (tstatus)
      {
         /* error during close */
         DMSG(ctx, DERROR, "Error closing command. Err=%s (%d)\n", m_errmsg.c_str());
         if (raise_error)
         {
            // raise it on request
            JMSG(ctx, M_ERROR, "Error closing command. Err=%s (%d)\n", m_errmsg.c_str());
         }
      }

      if (wpid)
      {
         /* terminate the process command */
         kill(wpid, SIGTERM);
      }

      bpipe = NULL;
   }

   return tstatus;
}

/*
 * Run command and prepared parameters.
 *
 * in:
 *    bpContext - for Bacula debug and jobinfo messages
 *    cmd - the command to execute
 * out:
 *    True - when command execute successfully
 *    False - when execution return error
 */
bool EXECPROG::execute_command(bpContext *ctx, const char *cmd, const char *args)
{
   POOL_MEM exe_cmd(PM_FNAME);

   if (cmd == NULL)
   {
      /* cannot execute command NULL */
      DMSG0(ctx, DERROR, "Logic error: Cannot execute NULL command!\n");
      JMSG0(ctx, M_FATAL, "Logic error: Cannot execute NULL command!\n");
      return false;
   }

   /* the format of a command line to execute is: <cmd> [<params] */
   Mmsg(exe_cmd, "%s %s", cmd, args);
   DMSG(ctx, DINFO, "Executing: %s\n", exe_cmd.c_str());
   bpipe = open_bpipe(exe_cmd.c_str(), 0, "rw");
   if (bpipe == NULL)
   {
      berrno be;
      DMSG(ctx, DERROR, "Unable to run command. Err=%s\n", be.bstrerror());
      JMSG(ctx, M_FATAL, "Unable to run command. Err=%s\n", be.bstrerror());
      return false;
   }
   DMSG(ctx, DINFO, "Command executed at PID=%d\n", get_cmd_pid());
   tstatus = 0;

   return true;
}

/*
 * Read all output from command - until eod and save it in the out buffer.
 *
 * in:
 *    bpContext - for Bacula debug jobinfo messages
 *    out - the POOL_MEM buffer we will read data
 * out:
 *    -1 - when we've got any error; the function will report it to Bacula when
 *         ctx is not NULL
 *    0 - when no more data to read - EOD
 *    <n> - the size of received message
 */
int32_t EXECPROG::read_output(bpContext *ctx, POOL_MEM &out)
{
   int status;
   int rbytes;
   bool ndone;

   if (is_closed()){
      DMSG0(ctx, DERROR, "BPIPE to command is closed, cannot get data.\n");
      JMSG0(ctx, is_fatal() ? M_FATAL : M_ERROR, "BPIPE to command is closed, cannot get data.\n");
      return -1;
   }

   /* set variables */
   rbytes = 0;
   ndone = true;
   /* read all output data */
   while (ndone){
      status = read_data(ctx, out.addr() + rbytes, out.size() - rbytes);
      if (status < 0){
         /* error */
         return -1;
      }
      rbytes += status;
      if (is_eod()){
         /* we read all data available */
         ndone = false;
         continue;
      }
      /* it seems out buffer is too small for all data */
      out.check_size(rbytes + 1024);
   }

   // we terminate the output as it would be the standard string
   out.check_size(rbytes + 1);
   out.c_str()[rbytes] = '\0';

   return rbytes;
}

/*
 * Reads a single data block from command.
 *  It reads as more data as is available on the other size and will fit into
 *  a memory buffer - buf. When EOD encountered during reading it will set
 *  f_eod flag, so checking this flag is mandatory!
 *
 * in:
 *    bpContext - for Bacula debug jobinfo messages
 *    buf - a memory buffer for data
 *    len - the length of the memory buffer - buf
 * out:
 *    -1 - when we've got any error; the function reports it to Bacula when
 *         ctx is not NULL
 *    when no more data to read - EOD
 *    <n> - the size of received data
 */
int32_t EXECPROG::read_data(bpContext *ctx, POOLMEM *buf, int32_t len)
{
   int status;
   int nbytes;
   int rbytes;
   int timeout;

   if (buf == NULL || len < 1){
      /* we have no space to read data */
      DMSG0(ctx, DERROR, "No space to read data from tool.\n");
      JMSG0(ctx, is_fatal() ? M_FATAL : M_ERROR, "No space to read data from tool.\n");
      return -1;
   }

   if (is_closed()){
      DMSG0(ctx, DERROR, "BPIPE to command is closed, cannot get data.\n");
      JMSG0(ctx, is_fatal() ? M_FATAL : M_ERROR, "BPIPE to command is closed, cannot get data.\n");
      return -1;
   }

   /* we will read no more then len bytes available in the buf */
   nbytes = len;
   rbytes = 0;
   /* clear flags */
   f_eod = f_error = f_fatal = false;
   timeout = 200;          // timeout of 200ms
   while (nbytes){
      status = fread(buf + rbytes, 1, nbytes, bpipe->rfd);
      if (status == 0){
         berrno be;
         if (ferror(bpipe->rfd) != 0){
            // check if it is an interrupted system call then restart
            if (be.code() == EINTR){
               clearerr(bpipe->rfd);
               continue;
            }
            f_error = true;
            DMSG(ctx, DERROR, "BPIPE read error: ERR=%s\n", be.bstrerror());
            JMSG(ctx, is_fatal() ? M_FATAL : M_ERROR, "BPIPE read error: ERR=%s\n", be.bstrerror());
            return -1;
         }
         if (feof(bpipe->rfd) != 0){
            f_eod = true;
            return rbytes;
         }
         bmicrosleep(0, 1000);   // sleep 1mS
         if (!timeout--){
            /* reach timeout*/
            f_error = true;
            DMSG0(ctx, DERROR, "BPIPE read timeout.\n");
            JMSG0(ctx, is_fatal() ? M_FATAL : M_ERROR, "BPIPE read timeout.\n");
            return -1;
         }
      } else {
         timeout = 200;          // reset timeout
      }
      nbytes -= status;
      rbytes += status;
   }
   return rbytes;
}

/*
 * Sends a raw data block to xe tool.
 *
 * in:
 *    bpContext - for Bacula debug and jobinfo messages
 *    buf - a message buffer contains data to send
 *    len - the length of the data to send
 * out:
 *    -1 - when encountered any error
 *    <n> - the number of bytes sent, success
 */
int32_t EXECPROG::write_data(bpContext *ctx, POOLMEM *buf, int32_t len)
{
   int status;
   int nbytes;
   int wbytes;
   int timeout;

   if (buf == NULL){
      /* we have no data to write */
      DMSG0(ctx, DERROR, "No data to send to command.\n");
      JMSG0(ctx, is_fatal() ? M_FATAL : M_ERROR, "No data to send to command.\n");
      return -1;
   }

   if (is_closed()){
      DMSG0(ctx, DERROR, "BPIPE to command is closed, cannot send data.\n");
      JMSG0(ctx, is_fatal() ? M_FATAL : M_ERROR, "BPIPE to command is closed, cannot send data.\n");
      return -1;
   }

   /* we will write len bytes available in the buf */
   nbytes = len;
   wbytes = 0;
   /* clear flags */
   f_eod = f_error = f_fatal = false;
   timeout = 200;          // timeout of 200ms
   while (nbytes){
      status = fwrite(buf + wbytes, 1, nbytes, bpipe->wfd);
      if (status == 0){
         berrno be;
         if (ferror(bpipe->wfd) != 0){
            f_error = true;
            DMSG(ctx, DERROR, "BPIPE write error: ERR=%s\n", be.bstrerror());
            JMSG(ctx, is_fatal() ? M_FATAL : M_ERROR, "BPIPE write error: ERR=%s\n", be.bstrerror());
            return -1;
         }
         bmicrosleep(0, 1000);   // sleep 1mS
         if (!timeout--){
            /* reached timeout*/
            f_error = true;
            DMSG0(ctx, DERROR, "BPIPE write timeout.\n");
            JMSG0(ctx, is_fatal() ? M_FATAL : M_ERROR, "BPIPE write timeout.\n");
            return -1;
         }
      } else {
         timeout = 200;          // reset timeout
      }
      nbytes -= status;
      wbytes += status;
   }
   return wbytes;
}
