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
 * @file metaplugin_accurate.h
 * @author Radosław Korzeniewski (radoslaw@korzeniewski.net)
 * @brief This is an Accurate Mode handling subroutines for metaplugin.
 * @version 1.0.0
 * @date 2021-09-20
 *
 * @copyright Copyright (c) 2021 All rights reserved. IP transferred to Bacula Systems according to agreement.
 */

#ifndef _METAPLUGIN_ACCURATE_H_
#define _METAPLUGIN_ACCURATE_H_

#include "pluginlib.h"
#include "ptcomm.h"


namespace metaplugin
{
namespace accurate
{
   bRC perform_accurate_check(bpContext *ctx, PTCOMM *ptcomm, POOL_MEM &fname, POOL_MEM &lname, bool accurate_mode, bool &accurate_mode_err);
   bRC perform_accurate_check_get(bpContext *ctx, PTCOMM *ptcomm, POOL_MEM &fname, POOL_MEM &lname, bool accurate_mode, bool &accurate_mode_err);

}  // namespace accurate
}  // namespace metaplugin

#endif   // _METAPLUGIN_ACCURATE_H_
