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
 * @file metaplugin_accurate.cpp
 * @author Radosław Korzeniewski (radoslaw@korzeniewski.net)
 * @brief This is an Accurate Mode handling subroutines for metaplugin.
 * @version 1.0.0
 * @date 2021-09-20
 *
 * @copyright Copyright (c) 2021 All rights reserved. IP transferred to Bacula Systems according to agreement.
 */

#include "metaplugin_accurate.h"
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
namespace accurate
{
   /**
    * @brief Perform a backend Accurate Mode query.
    *
    * @param ctx bpContext - for Bacula debug and jobinfo messages
    * @param ptcomm backend communication class - the current context
    * @param fname a file name to handle
    * @param accurate_mode the current state of accurate mode
    * @param accurate_mode_err when accurate mode error handled
    * @return bRC bRC_OK when success, bRC_Error if not
    */
   bRC perform_accurate_check(bpContext *ctx, PTCOMM *ptcomm, POOL_MEM &fname, POOL_MEM &lname, bool accurate_mode, bool &accurate_mode_err)
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
      if (ptcomm->read_command(ctx, cmd) < 0) {
         // error
         return bRC_Error;
      }

      metaplugin::attributes::Status status = metaplugin::attributes::read_scan_stat_command(ctx, cmd, &sp, lname);
      if (status == metaplugin::attributes::Status_OK) {
         if (ptcomm->read_command(ctx, cmd) < 0) {
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

            if (!ptcomm->write_command(ctx, checkstatus)) {
               DMSG0(ctx, DERROR, "Cannot send checkChanges() response to backend\n");
               JMSG0(ctx, ptcomm->jmsg_err_level(), "Cannot send checkChanges() response to backend\n");
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
            JMSG1(ctx, ptcomm->jmsg_err_level(), "Invalid stat packet: %s\n", cmd.c_str());
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
    * @brief Perform a backend Accurate Get Mode query.
    *
    * @param ctx bpContext - for Bacula debug and jobinfo messages
    * @param ptcomm backend communication class - the current context
    * @param fname a file name to handle
    * @param accurate_mode the current state of accurate mode
    * @param accurate_mode_err when accurate mode error handled
    * @return bRC bRC_OK when success, bRC_Error if not
    */
   bRC perform_accurate_check_get(bpContext *ctx, PTCOMM *ptcomm, POOL_MEM &fname, POOL_MEM &lname, bool accurate_mode, bool &accurate_mode_err)
   {
      POOL_MEM cmd(PM_FNAME);

      if (strlen(fname.c_str()) == 0){
         // input variable is not valid
         return bRC_Error;
      }

      DMSG0(ctx, DDEBUG, "perform_accurate_check_get()\n");

      if (!accurate_mode)
      {
         DMSG0(ctx, DERROR, "Backend CHECKGET command require accurate mode on!\n");
         JMSG0(ctx, M_ERROR, "Backend CHECKGET command require accurate mode on!\n");
         accurate_mode_err = true;

         // the job is not accurate, so no accurate data will be available at all
         pm_strcpy(cmd, "NOACCJOB\n");
         if (!ptcomm->signal_error(ctx, cmd)) {
            DMSG0(ctx, DERROR, "Cannot send 'No Accurate Job' info to backend\n");
            JMSG0(ctx, ptcomm->jmsg_err_level(), "Cannot send 'No Accurate Job' info to backend\n");
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
         ptcomm->write_command(ctx, cmd);

         // TSTAMP:...
         if (metaplugin::attributes::make_tstamp_command(ctx, cmd, &rp) == metaplugin::attributes::Status_OK) {
            ptcomm->write_command(ctx, cmd);
            DMSG(ctx, DINFO, "createFile:%s", cmd.c_str());
         }

         break;
      default:
         pm_strcpy(cmd, "UNAVAIL\n");
         if (!ptcomm->write_command(ctx, cmd)) {
            DMSG0(ctx, DERROR, "Cannot send 'UNAVAIL' response to backend\n");
            JMSG0(ctx, ptcomm->jmsg_err_level(), "Cannot send 'UNAVAIL' response to backend\n");
            return bRC_Error;
         }
         break;
      }

      return bRC_OK;
   }

}  // namespace accurate
}  // namespace metaplugin
