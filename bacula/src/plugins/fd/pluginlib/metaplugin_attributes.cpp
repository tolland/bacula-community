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
 * @file metaplugin_attributes.cpp
 * @author Radosław Korzeniewski (radoslaw@korzeniewski.net)
 * @brief This is a Backend attributes (STAT, TSTAMP) command handling subroutines for metaplugin.
 * @version 1.0.0
 * @date 2021-08-20
 *
 * @copyright Copyright (c) 2021 All rights reserved. IP transferred to Bacula Systems according to agreement.
 */

#include "metaplugin_attributes.h"

/*
 * libbac uses its own sscanf implementation which is not compatible with
 * libc implementation, unfortunately.
 * use bsscanf for Bacula sscanf flavor
 */
#ifdef sscanf
#undef sscanf
#endif

namespace metaplugin
{
namespace attributes
{
   /**
    * @brief Scans an input command and handles STAT sttributes.
    *
    * @param ctx bpContext - for Bacula debug and jobinfo messages
    * @param cmd a command buffer to scan and analyze
    * @param sp save packet to fill - output
    * @return Status Status_OK when data managed
    *                Status_Handled when data managed and TSTAMP command is not required
    *                Not_Command when it was not this command
    */
   Status read_scan_stat_command(bpContext *ctx, POOL_MEM &cmd, struct save_pkt *sp, POOL_MEM &lname)
   {
      char type;
      size_t size;
      int uid, gid;
      uint perms;
      int nlinks;

      DMSG0(ctx, DDEBUG, "read_scan_stat_command()\n");

      if (strncmp(cmd.c_str(), "STAT:/", 6) == 0)
      {
         POOL_MEM param(PM_FNAME);
         // handle stat(2) for this file
         scan_parameter_str(cmd, "STAT:", param);
         DMSG1(ctx, DDEBUG, "read_scan_stat_command():stat:%s\n", param.c_str());
         int rc = lstat(param.c_str(), &sp->statp);
         if (rc < 0)
         {
            // error
            berrno be;
            DMSG3(ctx, DERROR, "Invalid STAT path: %s Err=%s (%d)\n", param.c_str(), be.bstrerror(), be.code());
            JMSG3(ctx, M_ERROR, "Invalid STAT path: %s Err=%s (%d)\n", param.c_str(), be.bstrerror(), be.code());
            memset(&sp->statp, 0, sizeof(sp->statp));
            sp->type = FT_REG;
            return Status_Handled;
         }
         // stat is working as expected
         DMSG1(ctx, DDEBUG, "read_scan_stat_command():stat: %o\n", sp->statp.st_mode);
         switch (sp->statp.st_mode & S_IFMT)
         {
         case S_IFDIR:
            sp->type = FT_DIREND;
            sp->link = sp->fname;
            break;

         case S_IFREG:
            sp->type = FT_REG;
            break;

         case S_IFLNK:
            {
               sp->type = FT_LNK;
               ssize_t rc = readlink(param.c_str(), lname.c_str(), lname.size());
               if (rc < 0)
               {
                  berrno be;
                  // error reading link value
                  DMSG2(ctx, DERROR, "Error reading link value. Err=%s (%d)", be.bstrerror(), be.code());
                  JMSG3(ctx, M_ERROR, "Error reading link value: %s, Err=%s (%d)", param.c_str(), be.bstrerror(), be.code());
                  memset(&sp->statp, 0, sizeof(sp->statp));
                  sp->type = FT_REG;
                  return Status_Handled;
               }
               lname.c_str()[rc] = '\0';
               sp->link = lname.c_str();
               DMSG1(ctx, DDEBUG, "read_scan_stat_command():readlink:%s\n", sp->link);
            }
            break;

         case S_IFIFO:
            sp->type = FT_SPEC;
            break;

         default:
            DMSG1(ctx, DERROR, "Unsupported file type: %o\n", sp->statp.st_mode & S_IFMT);
            return Invalid_Stat_Packet;
         }
         return Status_Handled;
      }

      int32_t nfi = -1;
      int nrscan = sscanf(cmd.c_str(), "STAT:%c %ld %d %d %o %d %d",
                                       &type, &size, &uid, &gid, &perms, &nlinks, &nfi);
      DMSG1(ctx, DDEBUG, "read_scan_stat_command(nrscan): %d\n", nrscan);

      if (nrscan >= 6) {
         sp->statp.st_size = size;
         sp->statp.st_nlink = nlinks;
         sp->statp.st_uid = uid;
         sp->statp.st_gid = gid;
         sp->statp.st_mode = perms;

         switch (type)
         {
         case 'F':
            sp->type = FT_REG;
            break;
         case 'E':
            sp->type = FT_REGE;
            break;
         case 'D':
            sp->type = FT_DIREND;
            sp->link = sp->fname;
            break;
         case 'S':
            sp->type = FT_LNK;
            break;
         case 'L':
            if (nrscan > 6){
               sp->type = FT_LNKSAVED;
               sp->LinkFI = nfi;
            } else {
               DMSG1(ctx, DERROR, "Invalid stat packet: %s\n", cmd.c_str());
               return Invalid_Stat_Packet;
            }
            break;
         default:
            /* we need to signal error */
            sp->type = type;
            DMSG1(ctx, DERROR, "Invalid file type: %c\n", type);
            return Invalid_File_Type;
         }
         DMSG4(ctx, DDEBUG, "SCAN: type:%d size:%lld uid:%d gid:%d\n",
               (int)sp->type, sp->statp.st_size, sp->statp.st_uid, sp->statp.st_gid);
         DMSG3(ctx, DDEBUG, "SCAN: mode:%06o nl:%d fi:%d\n",
               sp->statp.st_mode, sp->statp.st_nlink, sp->LinkFI);

         return Status_OK;
      }
      return Not_Command;
   }

   /**
    * @brief Scans an input command and handles TSTAMP sttributes.
    *
    * @param ctx bpContext - for Bacula debug and jobinfo messages
    * @param cmd a command buffer to scan and analyze
    * @param sp save packet to fill - output
    * @return Status Status_OK when data managed
    *                Not_Command when it was not this command
    */
   Status read_scan_tstamp_command(bpContext *ctx, POOL_MEM &cmd, struct save_pkt *sp)
   {
      time_t _atime;
      time_t _mtime;
      time_t _ctime;

      DMSG0(ctx, DDEBUG, "read_scan_tstamp_command()\n");

      int nrscan = sscanf(cmd.c_str(), "TSTAMP:%ld %ld %ld", &_atime, &_mtime, &_ctime);
      DMSG1(ctx, DDEBUG, "read_scan_tstamp_command(nrscan): %d\n", nrscan);

      if (nrscan == 3) {
         sp->statp.st_atime = _atime;
         sp->statp.st_mtime = _mtime;
         sp->statp.st_ctime = _ctime;

         DMSG3(ctx, DINFO, "SCAN: %ld(at) %ld(mt) %ld(ct)\n",
               sp->statp.st_atime, sp->statp.st_mtime, sp->statp.st_ctime);
         return Status_OK;
      }

      return Not_Command;
   }

   /**
    * @brief Prepare a STAT command based on selected data.
    *
    * @param ctx bpContext - for Bacula debug and jobinfo messages
    * @param cmd the command buffer - output
    * @param rp restore packet to use
    * @return Status Status_OK
    */
   Status make_stat_command(bpContext *ctx, POOL_MEM &cmd, const restore_pkt *rp)
   {
      /* STAT:... */
      char type;

      switch (rp->type)
      {
      case FT_REGE:
         type = 'E';
         break;
      case FT_DIREND:
         type = 'D';
         break;
      case FT_LNK:
         type = 'S';
         break;
      case FT_LNKSAVED:
         type = 'L';
         break;
      case FT_MASK:     // This is a special metaplugin protocol hack
         type = 'X';    // as the accurate code does not know this exact value
         break;
      case FT_REG:
      default:
         type = 'F';
         break;
      }
      Mmsg(cmd, "STAT:%c %lld %d %d %06o %d %d\n",
           type, rp->statp.st_size, rp->statp.st_uid, rp->statp.st_gid,
           rp->statp.st_mode, (int)rp->statp.st_nlink, rp->LinkFI);
      DMSG(ctx, DDEBUG, "make_stat_command:%s", cmd.c_str());

      return Status_OK;
   }

   /**
    * @brief Prepares a TSTAMP command based on selected data.
    *
    * @param ctx bpContext - for Bacula debug and jobinfo messages
    * @param cmd the command buffer - output
    * @param rp restore packet to use
    * @return Status Status_OK
    */
   Status make_tstamp_command(bpContext *ctx, POOL_MEM &cmd, const restore_pkt *rp)
   {
      /* TSTAMP:... */
      Mmsg(cmd, "TSTAMP:%ld %ld %ld\n", rp->statp.st_atime, rp->statp.st_mtime, rp->statp.st_ctime);
      DMSG(ctx, DDEBUG, "make_tstamp_command:%s", cmd.c_str());

      return Status_OK;
   }

   /**
    * @brief Reads a file attributes sequence (STAT, TSTAMP) used at different sections.
    *
    * @param ctx bpContext - for Bacula debug and jobinfo messages
    * @param ptcomm backend communication class - the current context
    * @param cmd the buffer for command handled
    * @param sp save packet to fill when file attributes handled
    * @return Status Status_OK when file attributes commands handled, Status_Error on any error, other depends on enum.
    */
   Status read_attributes_command(bpContext *ctx, PTCOMM *ptcomm, POOL_MEM &cmd, struct save_pkt *sp, POOL_MEM &lname)
   {
      DMSG0(ctx, DDEBUG, "read_attributes_command()\n");

      // supported sequence is `STAT` followed by `TSTAMP`
      if (ptcomm->read_command(ctx, cmd) < 0) {
         // error
         return Status_Error;
      }

      Status status = read_scan_stat_command(ctx, cmd, sp, lname);
      switch(status)
      {
      case Status_OK:
         // go with TSATMP command
         if (ptcomm->read_command(ctx, cmd) < 0) {
            // error
            return Status_Error;
         }
         status = read_scan_tstamp_command(ctx, cmd, sp);
         break;

      case Status_Handled:
         status = Status_OK;
         break;

      default:
         break;
      }

      return status;
   }

}  // namespace attributes
}  // namespace metaplugin
