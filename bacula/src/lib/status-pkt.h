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
 *  Status packet definition that is used in both the SD and FD. It
 *    permits Win32 to call output_status() and get the output back
 *    at the callback address line by line, and for Linux code,
 *    the output can be sent directly to a BSOCK.
 *
 *     Kern Sibbald, March MMVII
 *
 */

#ifndef STATUS_PKT_H_
#define STATUS_PKT_H_

/*
 * Packet to send to output_status()
 */
class STATUS_PKT {
public:
  BSOCK *bs;                       /* used on Unix machines */
  void *context;                   /* Win32 */
  void (*callback)(const char *msg, int len, void *context);  /* Win32 */
  int  api;                        /* set if we want API output, with api level */
  char api_opts[MAX_NAME_LENGTH];

  /* Methods */
  STATUS_PKT(): bs(NULL), callback(NULL), api(0) { 
    *api_opts = 0;
  };
  ~STATUS_PKT() { };
};

#endif
