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

#ifndef LIN_TAPE_H
#define LIN_TAPE_H

#include "bacula.h"

/* Interface to request information to a tape drive
 * Technical information found on IBM web site
 *   https://www.ibm.com/support/knowledgecenter/STAKKZ/dd_pr_kc/con_a89p4_lnx_sioc_reqsense.html
 * TODO: May not work SPARC CPU
 */

#if defined(HAVE_LINUX_OS)

#ifndef SIOC_REQSENSE
# define SIOC_REQSENSE _IOR('C', 0x02, lintape_request_sense)
#endif

struct lintape_request_sense {
   uint8_t  valid           :1,      /* sense data is valid             */
            err_code        :7;      /* error code                      */
   uint8_t  segnum;                  /* segment number                  */
   uint32_t fm                :1,    /* filemark detected               */
            eom               :1,    /* end of medium                   */
            ili               :1,    /* incorrect length indicator      */
            resvd1            :1,    /* reserved                        */
            key               :4;    /* sense key                       */
   int32_t  info;                    /* information bytes               */
   uint8_t  addlen;                  /* additional sense length         */
   uint32_t cmdinfo;                 /* command specific information    */
   uint8_t  asc;                     /* additional sense code           */
   uint8_t  ascq;                    /* additional sense code qualifier */
   uint8_t  fru;                     /* field replaceable unit code     */
   uint32_t sksv              :1,    /* sense key specific valid        */
            cd                :1,    /* control/data                    */
            resvd2            :2,    /* reserved                        */
            bpv               :1,    /* bit pointer valid               */
            sim               :3;    /* system information message      */
   uint8_t  field[2];                /* field pointer                   */
   uint8_t vendor[109];              /* vendor specific (padded to 127) */
};

#endif  // HAVE_LINUX_OS

#endif  // LIN_TAPE_H
