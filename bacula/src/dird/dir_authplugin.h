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
 * Bacula authentication and authorization plugin framework.
 *
 * Author: Radosław Korzeniewski, MMXX
 * radoslaw@korzeniewski.net, radekk@inteos.pl
 * Inteos Sp. z o.o. http://www.inteos.pl/
 */

#ifndef __DIR_AUTHPLUGINS_H
#define __DIR_AUTHPLUGINS_H

#ifndef _BACULA_H
#ifdef __cplusplus
/* Workaround for SGI IRIX 6.5 */
#define _LANGUAGE_C_PLUS_PLUS 1
#endif
#define _REENTRANT    1
#define _THREAD_SAFE  1
#define _POSIX_PTHREAD_SEMANTICS 1
#define _FILE_OFFSET_BITS 64
#define _LARGEFILE_SOURCE 1
#define _LARGE_FILES 1
#endif

#include <sys/types.h>
#ifndef __CONFIG_H
#define __CONFIG_H
#include "config.h"
#endif
#include "bc_types.h"
#include "lib/plugins.h"

#ifdef __cplusplus
extern "C" {
#endif

/****************************************************************************
 *                                                                          *
 *                Bacula definitions                                        *
 *                                                                          *
 ****************************************************************************/

/**
 * @brief This is a list of supported auth plugin framework operations.
 */
typedef enum {
   bDirAuthenticationOperationPlugin,
   bDirAuthenticationOperationPluginAll,
   bDirAuthenticationOperationMessage,
   bDirAuthenticationOperationPlain,
   bDirAuthenticationOperationLogin = bDirAuthenticationOperationPlain,
   bDirAuthenticationOperationHidden,
   bDirAuthenticationOperationPassword = bDirAuthenticationOperationHidden,
   bDirAuthenticationOperationAuthenticate,
} bDirAuthenticationOperation;

/**
 * @brief
 */
typedef struct s_bDirAuthenticationData {
   const bDirAuthenticationOperation operation;
   const char *question;
   const uint32_t seqdata;
} bDirAuthenticationData;

typedef struct s_bDirAuthenticationRegister {
   const char *name;
   const char *welcome;
   uint32_t num;
   bDirAuthenticationData *data;
   int32_t nsTTL;
} bDirAuthenticationRegister;

typedef struct s_bDirAuthenticationValue {
   union {
      void *rawdata;                        // future extension
      bDirAuthenticationData *authdata;     // a pointer to single bDirAuthenticationData as a dynamic operation
      const char *response;
   };
   uint32_t seqdata;
} bDirAuthValue;

typedef struct s_bDirAuthorizationACLRegister {
   const char *name;
   // *TBD* - Incomplete
   const int32_t nsTTL;
} bDirAuthorizationACLRegister;

#ifdef __cplusplus
}
#endif

#endif /* __DIR_AUTHPLUGINS_H */
