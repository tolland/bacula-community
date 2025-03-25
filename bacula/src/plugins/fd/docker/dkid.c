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
 * @file dkid.c
 * @author Radoslaw Korzeniewski (radoslaw@korzeniewski.net)
 * @brief This is a Bacula plugin for backup/restore Docker using native tools.
 * @version 1.2.1
 * @date 2020-01-05
 *
 * @copyright Copyright (c) 2021 All rights reserved. IP transferred to Bacula Systems according to agreement.
 */

#include "dkid.h"
#include <string.h>
#include <stdlib.h>

/*
 * DKID class constructor, does default initialization.
 */
DKID::DKID() :
   ShortD(DKIDInvalid),
#if __cplusplus > 201103L
   Digest {0},
   DigestShort {0},
#endif
   shortonly(false)
{
#if ! __cplusplus > 201103L
   bmemzero(Digest, DKIDDIGESTSIZE_Len);
   bmemzero(DigestShort, DKIDDIGESTShortSIZE_Len);
#endif
};

/*
 * DKID initialization from string.
 *    as the usable area of short sha256 version used in Docker is 6bytes/48bits
 *    and we are using a 64bit (signed) integer then we have a plenty of space to mark
 *    invalid sha256 conversion with a negative ShortD value.
 */
void DKID::init(const char* data)
{
   char *dig = (char*)data;

   if (dig != NULL){
      /* check for sha256: prefix*/
      if (strstr(dig, "sha256:") == dig){
         dig += 7;
      }
      int len = strlen(dig);
      /* check for invalid input data */
      bool valid = true;
      for (int a = 0; a < (len > DKIDDIGESTShortSIZE ? DKIDDIGESTShortSIZE : len); a++){
         // we are checking for ASCII codes, a subset of UTF-8 for short digest only
         unsigned char c = (unsigned char)dig[a];
         if (c > 'f' || (c > '9' && c < 'A') || (c > 'F' && c < 'a')){
            valid = false;
            break;
         }
      }
      if (valid){
         if (len > DKIDDIGESTShortSIZE){
            /* initialize from full data */
            memcpy(Digest, dig, DKIDDIGESTSIZE);
            Digest[DKIDDIGESTSIZE] = 0;
            shortonly = false;
         } else {
            /* handle short data */
            memcpy(Digest, dig, len);
            memcpy(Digest + len, "(...)\0", 6);
            shortonly = true;
         }
         memcpy(DigestShort, dig, DKIDDIGESTShortSIZE);
         DigestShort[DKIDDIGESTShortSIZE] = 0;
         ShortD = strtol(DigestShort, NULL, 16);
      } else {
         ShortD = DKIDInvalid;
         shortonly = false;
      }
   }
};

/*
 * Basic assignment operator overloading for string.
 *
 * in:
 *    data - the null terminated string where up to 64 chars will be used
 * out:
 *    reinitialized DKID class
 */
DKID& DKID::operator= (char* data)
{
   init(data);
   return *this;
};

/*
 * Basic assignment operator overloading for POOL_MEM class.
 *
 * in:
 *    data - a reference to POOL_MEM class instance which is used as a source
 *           of null terminated string for initialization
 * out:
 *    reinitialized DKID class
 */
DKID& DKID::operator =(POOL_MEM &data)
{
   init(data.c_str());
   return *this;
}

/*
 * Basic assignment operator overloading for DKID class.
 *
 * in:
 *    other - a reference to another DKID class instance which will be used for
 *            assignment
 * out:
 *    reinitialized DKID class
 */
DKID& DKID::operator =(DKID &other)
{
   memcpy(Digest, other.Digest, DKIDDIGESTSIZE);
   memcpy(DigestShort, other.DigestShort, DKIDDIGESTShortSIZE);
   Digest[DKIDDIGESTSIZE] = 0;
   DigestShort[DKIDDIGESTShortSIZE] = 0;
   ShortD = other.ShortD;
   shortonly = other.shortonly;
   return *this;
}

/*
 * Equal to operator overloading for DKID class.
 *
 * in:
 *    other - a reference to another DKID class instance which will be used for
 *            comparison
 * out:
 *    true - if both ShortD are the same
 *    false - if ShortD variables differ or any DKID is invalid
 */
bool DKID::operator ==(DKID &other)
{
   if (ShortD >= 0 && other.ShortD >= 0 && ShortD == other.ShortD &&
         (shortonly || other.shortonly || bstrcmp(Digest, other.Digest))){
      return true;
   }
   return false;
}

/*
 * Not-Equal to operator overloading for DKID class.
 *
 * in:
 *    other - a reference to another DKID class instance which will be used for
 *            comparison
 * out:
 *    true - if ShortD variables differ and none of them are invalid
 *    false - if both ShortD are the same or any DKID is invalid
 */
bool DKID::operator !=(DKID &other)
{
   if (ShortD >= 0 && other.ShortD >= 0 && ShortD != other.ShortD){
      return true;
   }
   if (!shortonly && !other.shortonly && !bstrcmp(Digest, other.Digest)){
      return true;
   }
   return false;
}
