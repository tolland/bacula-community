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

#ifndef COMPRESS_H
#define COMPRESS_H

#ifdef HAVE_LZO
const bool have_lzo = true;
#else
const bool have_lzo = false;
#endif

#ifdef HAVE_ZSTD
const bool have_zstd = true;
#else
const bool have_zstd = false;
#endif

#ifdef HAVE_LIBZ
const bool have_libz = true;
#else
const bool have_libz = false;
#endif

#endif  // COMPRESS_H
