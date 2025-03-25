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
/*
 * Commands defined for the Storage Manager, for communication between the DIR and SD.
 */

#ifndef STORE_MNGR_CMDS_H
#define STORE_MNGR_CMDS_H 1

static const char store_query[] = "store_mngr query\n";
static const char store_query_freespace[] = "store_mngr freespace device=%s\n";
static char OK_store_query[] = "3000 OK query store\n";
static char OK_store_size[]   = "3000 OK size=%llu\n";

#ifdef STORAGE_DAEMON
static char store_query_unsupp_policy[] = "3200 unsupported policy\n";
#endif

#endif // STORE_MNGR_CMDS_H
