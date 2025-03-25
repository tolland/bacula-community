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

/* Base32 is used by various One Time Password authentication apps
 * such as google authenticator.
 *
 *
 * Base32 is described in rfc4648.
 *
 * When encoding, the output buffer must be 5/8 + 1 bigger than the
 * input
 */

#include "bacula.h"

static uint8_t const base32_digits[32] =
{
  'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
  'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
  '2', '3', '4', '5', '6', '7'
};

/* The encoding process represents 40-bit groups of input bits as output
 * strings of 8 encoded characters.  Proceeding from left to right, a
 * 40-bit input group is formed by concatenating 5 8bit input groups.
 * These 40 bits are then treated as 8 concatenated 5-bit groups, each
 * of which is translated into a single character in the base 32
 * alphabet.
 */
int bin_to_base32(const uint8_t *buf, int buflen, char *bin, int binlen)
{
  int j = 0;
  uint32_t buffer;
  int next, rem;

  if (buflen < 0) {
    return -1;
  }

  if (buflen > 0) {
     buffer = buf[0];
     next = 1;
     rem = 8;
     while (j < binlen && (rem > 0 || next < buflen)) {
        if (rem < 5) {
           if (next < buflen) {
              buffer <<= 8;
              buffer |= buf[next++] & 0xFF;
              rem += 8;

           } else {
              int pad = 5 - rem;
              buffer <<= pad;
              rem += pad;
           }
        }
        int index = 0x1F & (buffer >> (rem - 5));
        rem -= 5;
        bin[j++] = base32_digits[index];
     }
  }

  if (j < binlen) {
     bin[j] = '\0';

  } else {                      // Buffer too small
     return -1;
  }
  return j;
}

#ifdef TEST_PROGRAM

#include "unittests.h"

int main(int argc, char **argv)
{
   Unittests t("base32");
   char buf[512];
   ok(bin_to_base32((const uint8_t*)"test1", strlen("test1"), buf, sizeof(buf)) > 0, "test bin_to_base32()");
   is(buf, "ORSXG5BR", "Check result of base32 encoding");
   is(bin_to_base32((const uint8_t*)"test1", strlen("test1"), buf, 3), -1, "test bin_to_base32() with small buffer");
   return report();
}

#endif
