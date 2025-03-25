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
 * Storage Daemon's part of Storage Manager - responsible for retrieving needed
 * information in response to Director's query.
 */

#ifndef STORE_MNGR_H
#define STORE_MNGR_H 1

#include "bacula.h"


/* FreeSpace policy struct to easily get rid of device duplicates in the list */
struct query_size {
   struct dlink link;
   uint64_t devno; /* Unique device number */
   uint64_t size;  /* Device's freespace in bytes */
};

/* Main FreeSpace policy handler */
bool query_store(JCR *jcr);

#endif // STORE_MNGR_H
