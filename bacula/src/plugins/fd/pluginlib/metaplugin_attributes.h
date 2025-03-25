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
 * @file metaplugin_attributes.h
 * @author Radosław Korzeniewski (radoslaw@korzeniewski.net)
 * @brief This is a Backend `STAT` command handling subroutines for metaplugin.
 * @version 1.0.0
 * @date 2021-08-20
 *
 * @copyright Copyright (c) 2021 All rights reserved. IP transferred to Bacula Systems according to agreement.
 */

#ifndef _METAPLUGIN_ATTRIBUTES_H_
#define _METAPLUGIN_ATTRIBUTES_H_

#include "pluginlib.h"
#include "ptcomm.h"


namespace metaplugin
{
namespace attributes
{
   typedef enum
   {
      Status_OK,
      Invalid_Stat_Packet,
      Invalid_File_Type,
      Status_Handled,
      Not_Command,
      Status_Error,
   } Status;

   Status read_scan_stat_command(bpContext *ctx, POOL_MEM &cmd, struct save_pkt *sp, POOL_MEM &lname);
   Status make_stat_command(bpContext *ctx, POOL_MEM &cmd, const restore_pkt *rp);
   Status read_scan_tstamp_command(bpContext *ctx, POOL_MEM &cmd, struct save_pkt *sp);
   Status make_tstamp_command(bpContext *ctx, POOL_MEM &cmd, const restore_pkt *rp);
   Status read_attributes_command(bpContext *ctx, PTCOMM *ptcomm, POOL_MEM &cmd, struct save_pkt *sp, POOL_MEM &lname);
}  // attributes
}  // metaplugin

#endif   // _METAPLUGIN_ATTRIBUTES_H_
