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
 *   Generic base 64 input and output routines
 *
 *    Written by Kern E. Sibbald, March MM.
 *
 */


#include "bacula.h"


#ifdef TEST_MODE
#include <glob.h>
#endif


static uint8_t const base64_digits[64] =
{
  'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
  'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
  'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
  'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
  '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '+', '/'
};

static uint8_t base64_map[256] =
{
   0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
   0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
   0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
   0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
   0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
   0xff, 0xff, 0xff, 0x3e, 0xff, 0xff, 0xff, 0x3f,
   0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b,
   0x3c, 0x3d, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
   0xff, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
   0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e,
   0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16,
   0x17, 0x18, 0x19, 0xff, 0xff, 0xff, 0xff, 0xff,
   0xff, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20,
   0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,
   0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30,
   0x31, 0x32, 0x33, 0xff, 0xff, 0xff, 0xff, 0xff,
   0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
   0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
   0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
   0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
   0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
   0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
   0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
   0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
   0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
   0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
   0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
   0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
   0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
   0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
   0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
   0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
};

/* Convert a signed number into base 64, using the base64 chars
 * This is not related to MIME encoding.
 *
 * The result is stored in where, which
 * must be at least 13 characters long.
 * The string ends with a '\0'
 *
 * Returns the number of characters
 * stored including the sign if any (not including the '\0').
 */
int
to_base64(int64_t value, char *where)
{
   uint64_t val;
   int i = 0;
   int n;

   /* Handle negative values */
   if (value < 0) {
      where[i++] = '-';
      value = -value;
   }

   /* Determine output size */
   val = value;
   do {
      val >>= 6;
      i++;
   } while (val);
   n = i;

   /* Output characters */
   val = value;
   where[i] = 0;
   do {
      where[--i] = base64_digits[val & (uint64_t)0x3F];
      val >>= 6;
   } while (val);
   return n;
}

/* Convert a signed number encoded into base 64 (using the base64 chars)
 * back into an integer
 * This is not related to MIME encoding.
 * No checking is done on the validity of the characters!!
 *
 * Returns the value.
 */
int
from_base64(int64_t *value, char *where)
{
   uint64_t val = 0;
   int i, neg;

   /* Check if it is negative */
   i = neg = 0;
   if (where[i] == '-') {
      i++;
      neg = 1;
   }
   /* Construct value */
   while (where[i] != 0 && where[i] != ' ') {
      val <<= 6;
      val += base64_map[(uint8_t)where[i++]];
   }

   *value = neg ? -(int64_t)val : (int64_t)val;
   return i;
}


/*
 * Encode binary data in bin of len bytes into
 * buf as base64 characters.
 *
 * If compatible is true, the bin_to_base64 routine will be compatible
 * with what the rest of the world uses.
 *
 * This function don't does padding with '='
 *
 *  Returns: the number of characters stored not
 *           including the EOS
 */
int
bin_to_base64(char *buf, int buflen, char *bin, int binlen, int compatible)
{
   uint32_t reg, save, mask;
   int rem, i;
   int j = 0;

   reg = 0;
   rem = 0;
   buflen--;                       /* allow for storing EOS */
   for (i=0; i < binlen; ) {
      if (rem < 6) {
         reg <<= 8;
         if (compatible) {
            reg |= (uint8_t)bin[i++];
         } else {
            reg |= (int8_t)bin[i++];
         }
         rem += 8;
      }
      save = reg;
      reg >>= (rem - 6);
      if (j < buflen) {
         buf[j++] = base64_digits[reg & 0x3F];
      }
      reg = save;
      rem -= 6;
   }
   if (rem && j < buflen) {
      mask = (1 << rem) - 1;
      if (compatible) {
         buf[j++] = base64_digits[(reg & mask) << (6 - rem)];
      } else {
         buf[j++] = base64_digits[reg & mask];
      }
   }
   buf[j] = 0;
   return j;
}

int bin_to_base64_pad(char *buf, int buflen, char *bin, int binlen)
{
   int l = bin_to_base64(buf, buflen, bin, binlen, true);
   int n = 4 - l%4; /* add one or two '=' for padding */
   for (int i = 0; n < 3 && i < n; i++) {
      if (l < buflen) {
         buf[l++] = '=';
         buf[l] = '\0';
      }
   }
   return l;
}

/*
 * Decode base64 encoded data
 *
 * - ignore spaces in the input (space, tab, newline)
 * - the input can be multi-line as often in e-mails
 * - stop when a '\0', a '=' (padding) or srclen depending what is hit first
 * - test for invalid char
 * - return 0 when the output string is too short or invalid char are found in the input
 * - dest_size don't need to be bigger, the exact length is ok
 * - the output string is not ended with '\0', has it is supposed to be binary data
 *  Returns: the number of characters stored in 'dest' or 0 if dest_size was too
 *           small or invalid chars where found in the input
 */
int base64_to_bin(char *dest, int dest_size, const char *src, int srclen)
{
   uint8_t *ui8_dest = (uint8_t*) dest;
   uint8_t *bufout = (uint8_t*) dest;
   const uint8_t *bufin0 = (const uint8_t *) src;

   bool err = false;
   /* Bacula base64 strings are not always padded with = */
   while (1) {
      while (*bufin0 != 0 && *bufin0 != '=' && isspace(*bufin0)) {
         bufin0++;
      }
      if (*bufin0 == 0 || *bufin0 == '=' || (err=(base64_map[*bufin0] == 0xff))) {
         break;
      }
      const uint8_t *bufin1 = bufin0 + 1;
      while (*bufin1 != 0 && *bufin1 != '=' && isspace(*bufin1)) {
         bufin1++;
      }
      if (*bufin1 == 0 || *bufin1 == '=' ||
          (err=(base64_map[*bufin1] == 0xff)) ||
          (err=((bufout-ui8_dest) > dest_size)))
      {
         break;
      }
      *(bufout++) = (base64_map[*bufin0] << 2 | base64_map[*bufin1] >> 4);
      const uint8_t *bufin2 = bufin1 + 1;
      while (*bufin2 != 0 && *bufin2 != '=' && isspace(*bufin2)) {
         bufin2++;
      }
      if (*bufin2 == 0 || *bufin2 == '=' ||
          (err=(base64_map[*bufin2] == 0xff)) ||
          (err=(bufout-ui8_dest > dest_size)))
      {
         break;
      }
      *(bufout++) = (base64_map[*bufin1] << 4 | base64_map[*bufin2] >> 2);
      const uint8_t *bufin3 = bufin2 + 1;
      while (*bufin3 != 0 && *bufin3 != '=' && isspace(*bufin3)) {
         bufin3++;
      }
      if (*bufin3 == 0 || *bufin3 == '=' ||
          (err=(base64_map[*bufin3] == 0xff)) ||
          (err=(bufout-ui8_dest > dest_size))) {
         break;
      }
      *(bufout++) = (base64_map[*bufin2] << 6 | base64_map[*bufin3]);
      bufin0 = bufin3 + 1;
   }
   /* *bufout = 0; // don't end binary with 0 */
   return err?0:(bufout - ui8_dest);
}
#ifdef BIN_TEST
int main(int argc, char *argv[])
{
   int len;
   char buf[100];
   char junk[100];
   int i;

#ifdef xxxx
   int xx = 0;
   for (i=0; i < 1000; i++) {
      bin_to_base64(buf, sizeof(buf), (char *)&xx, 4, true);
      printf("xx=%s\n", buf);
      xx++;
   }
#endif
   junk[0] = 0xFF;
   for (i=1; i<100; i++) {
      junk[i] = junk[i-1]-1;
   }
   len = bin_to_base64(buf, sizeof(buf), junk, 16, true);
   printf("len=%d junk=%s\n", len, buf);

   strcpy(junk, "This is a sample stringa");
   len = bin_to_base64(buf, sizeof(buf), junk, strlen(junk), true);
   buf[len] = 0;
   base64_to_bin(junk, sizeof(junk), buf, len);
   printf("buf=<%s>\n", junk);
   return 0;
}
#endif

#ifdef TEST_MODE
static int errfunc(const char *epath, int eernoo)
{
  printf("in errfunc\n");
  return 1;
}


/*
 * Test the base64 routines by encoding and decoding
 * lstat() packets.
 */
int main(int argc, char *argv[])
{
   char where[500];
   int i;
   glob_t my_glob;
   char *fname;
   struct stat statp;
   struct stat statn;
   int debug_level = 0;
   char *p;
   int32_t j;
   time_t t = 1028712799;

   if (argc > 1 && strcmp(argv[1], "-v") == 0)
      debug_level++;

   base64_init();

   my_glob.gl_offs = 0;
   glob("/etc/grub.conf", GLOB_MARK, errfunc, &my_glob);

   for (i=0; my_glob.gl_pathv[i]; i++) {
      fname = my_glob.gl_pathv[i];
      if (lstat(fname, &statp) < 0) {
         berrno be;
         printf("Cannot stat %s: %s\n", fname, be.bstrerror(errno));
         continue;
      }
      encode_stat(where, &statp, sizeof(statp), 0, 0);

      printf("Encoded stat=%s\n", where);

#ifdef xxx
      p = where;
      p += to_base64((int64_t)(statp.st_atime), p);
      *p++ = ' ';
      p += to_base64((int64_t)t, p);
      printf("%s %s\n", fname, where);

      printf("%s %lld\n", "st_dev", (int64_t)statp.st_dev);
      printf("%s %lld\n", "st_ino", (int64_t)statp.st_ino);
      printf("%s %lld\n", "st_mode", (int64_t)statp.st_mode);
      printf("%s %lld\n", "st_nlink", (int64_t)statp.st_nlink);
      printf("%s %lld\n", "st_uid", (int64_t)statp.st_uid);
      printf("%s %lld\n", "st_gid", (int64_t)statp.st_gid);
      printf("%s %lld\n", "st_rdev", (int64_t)statp.st_rdev);
      printf("%s %lld\n", "st_size", (int64_t)statp.st_size);
      printf("%s %lld\n", "st_blksize", (int64_t)statp.st_blksize);
      printf("%s %lld\n", "st_blocks", (int64_t)statp.st_blocks);
      printf("%s %lld\n", "st_atime", (int64_t)statp.st_atime);
      printf("%s %lld\n", "st_mtime", (int64_t)statp.st_mtime);
      printf("%s %lld\n", "st_ctime", (int64_t)statp.st_ctime);
#endif

      if (debug_level)
         printf("%s: len=%d val=%s\n", fname, strlen(where), where);

      decode_stat(where, &statn, sizeof(statn), &j);

      if (statp.st_dev != statn.st_dev ||
          statp.st_ino != statn.st_ino ||
          statp.st_mode != statn.st_mode ||
          statp.st_nlink != statn.st_nlink ||
          statp.st_uid != statn.st_uid ||
          statp.st_gid != statn.st_gid ||
          statp.st_rdev != statn.st_rdev ||
          statp.st_size != statn.st_size ||
          statp.st_blksize != statn.st_blksize ||
          statp.st_blocks != statn.st_blocks ||
          statp.st_atime != statn.st_atime ||
          statp.st_mtime != statn.st_mtime ||
          statp.st_ctime != statn.st_ctime) {

         printf("%s: %s\n", fname, where);
         encode_stat(where, &statn, sizeof(statn), 0, 0);
         printf("%s: %s\n", fname, where);
         printf("NOT EQAL\n");
      }

   }
   globfree(&my_glob);

   printf("%d files examined\n", i);

   to_base64(UINT32_MAX, where);
   printf("UINT32_MAX=%s\n", where);

   return 0;
}
#endif

#ifndef TEST_PROGRAM
#define TEST_PROGRAM_A
#endif

#ifdef TEST_PROGRAM
#include "unittests.h"

static const unsigned char rnddata[16] = {
   0xa5, 0x7d, 0xa3, 0xc4, 0x2c, 0xa0, 0x08, 0xe9, 0x32, 0xb9, 0xc7, 0x84, 0xf6, 0xd3, 0xdf, 0x4f
};
static const char *resb16 = "pX2jxCygCOkyuceE9tPfTw";
static const char *resb8 = "q83v7c";
#define VARREF    0xABCDEFEDC

int main()
{
   Unittests base64_test("base64_test");
   char buf[30];
   char binbuf[30];
   uint len;
   bool check_cont;
   int64_t var;

/*
   for (int a=0; a < 16; a++){
      fprintf(stderr, "%c", rnddata[a]);
   }
*/

   /* test vectors from https://www.base64encode.org/ */
   const char * testdata1 = "Man is distinguished, not only by his reason, but ...";
   const char * testvect1 = "TWFuIGlzIGRpc3Rpbmd1aXNoZWQsIG5vdCBvbmx5IGJ5IGhpcyByZWFzb24sIGJ1dCAuLi4";
   // const char * filltest1  = "a";
   // const char * fillres1   = "YQ==";
   // const char * filltest2  = "ab";
   // const char * fillres2   = "YWI=";
   // const char * filltest3  = "abc";
   // const char * fillres3   = "YWJj";
   // const char * filltest4  = "abcd";
   // const char * fillres4   = "YWJjZA==";
   // const char * filltest5  = "abcde";
   // const char * fillres5   = "YWJjZGU=";
   // const char * filltest6  = "abcdef";
   // const char * fillres6   = "YWJjZGVm";

   char * vdata1 = (char*)malloc(strlen(testdata1) + 10);
   char * vdata2 = (char*)malloc(strlen(testdata1) + 10);
   char * vvect1 = (char*)malloc(strlen(testvect1) + 10);
   char * vvect2 = (char*)malloc(strlen(testvect1) + 50);

   len = bin_to_base64(vvect1, strlen(testvect1) + 10, (char*)testdata1, strlen(testdata1), true);
   is(len, strlen(testvect1), "test bin_to_base64");
   ok(strcmp(testvect1, vvect1) == 0, "test bin_to_base64 content");

   len = base64_to_bin(vdata1, strlen(testdata1) + 10, (char*)testvect1, strlen(testvect1));
   is(len, strlen(testdata1), "test base64_to_bin");
   vdata1[len] = 0;
   ok(strcmp(testdata1, vdata1) == 0, "test base64_to_bin content");

   /* try the same "decoded" string but make it smaller and smaller */
   bool notok = false;
   memcpy(vdata2, testdata1, strlen(testdata1));
   for (unsigned int i=strlen(testdata1); i>0; i--) {
      vdata2[i] = '\0';
      memset(vvect1, '\xFF', strlen(testvect1) + 10);
      memset(vdata1, '\xFF', strlen(testdata1) + 10);
      len = bin_to_base64(vvect1, strlen(testvect1) + 10, (char*)vdata2, i, true);
      // printf("i=%d len=%d strlen(vvect1)=%d\n", i , len, strlen(vvect1));
      // printf("data=%s\n", vdata2);
      // printf("base64=%s\n", vvect1);

      len = base64_to_bin(vdata1, strlen(testdata1) + 10, (char*)vvect1, strlen(vvect1));
      // printf("i=%d len=%d\n", i , len);
      vdata1[len] = 0;
      bool o = (len == i && strcmp(vdata2, vdata1) == 0);
      if (!o) {
         char buf[256];
         notok = true;
         snprintf(buf, sizeof(buf), "test base64 encode decode mismatch for len = %d", i);
         ok(o, buf);
      }
   }
   ok(!notok, "test base64 encode decode for multiple lengths");

   /* insert some spaces into the encoded string */
   notok = false;
   memcpy(vvect1, testvect1, strlen(testvect1) + 10);
   int pos[]= { 0, 30, 30, 15, 13, 8, 6, 0 }; /* "30, 30" test two spaces in a row */
   for (unsigned int i=0; i<sizeof(pos)/sizeof(*pos); i++) {
      int p = pos[i];
      if (i==0) {
         // insert space after last char
         p = strlen(vvect1);
      }
      // printf("%d %d -%s- len=%d\n", i, p, vvect1, strlen(vvect1));
      memcpy(vvect1+p+1, vvect1+p, strlen(vvect1)-p+1);
      vvect1[p]=' ';
      // printf("%d %d +%s+ len=%d\n", i, p, vvect1, strlen(vvect1));
      len = base64_to_bin(vdata1, strlen(testdata1) + 10, (char*)vvect1, strlen(vvect1));
      vdata1[len] = 0;
      // printf("i=%d len=%d vdata1=%s len=%d\n", i , len, vdata1, strlen(vdata1));
      // printf("i=%d len=%d vdata1=%s len=%d\n", i , strlen(testdata1), testdata1, strlen(testdata1));
      bool o = (len == strlen(testdata1) && strcmp(vdata1, testdata1) == 0);
      if (!o) {
         char buf[256];
         notok = true;
         snprintf(buf, sizeof(buf), "test base64 decode base64 with space mismatch for i=%d", i);
         ok(o, buf);
      }
   }
   ok(!notok, "test base64 decode base64 with multiple spaces");
   free(vdata2);
   free(vdata1);
   free(vvect1);
   free(vvect2);

   /* test buffer too small */
   len = base64_to_bin(buf, 1, testvect1, 10);
   ok(len == 0, "test base64 buffer too small 1");
   len = base64_to_bin(buf, 0, testvect1, 10);
   ok(len == 0, "test base64 buffer too small 2");
   /* test with varying padding */
   buf[0]='X';
   len = base64_to_bin(buf, 1, "YQ==", 4);
   ok(len == 1 && buf[0] == 'a', "test base64 padding YQ==");
   buf[0]='X';
   len = base64_to_bin(buf, 1, "YQ=", 3);
   ok(len == 1 && buf[0] == 'a', "test base64 padding YQ=");
   buf[0]='X';
   len = base64_to_bin(buf, 1, "YQ", 2);
   ok(len == 1 && buf[0] == 'a', "test base64 padding YQ");
   buf[0]='X';
   len = base64_to_bin(buf, 1, "YQ==", 4);
   ok(len == 1 && buf[0] == 'a', "test base64 padding YQ==");
   buf[0]='X';
   len = base64_to_bin(buf, 1, "YQ=", 3);
   ok(len == 1 && buf[0] == 'a', "test base64 padding YQ=");
   buf[0]='X';
   len = base64_to_bin(buf, 1, "YQ", 2);
   ok(len == 1 && buf[0] == 'a', "test base64 padding YQ");
   buf[0]='X';
   len = base64_to_bin(buf, 1, "YQ==", 3);
   ok(len == 1 && buf[0] == 'a', "test base64 truncated padding YQ== 3");
   buf[0]='X';
   len = base64_to_bin(buf, 1, "YQ==", 2);
   ok(len == 1 && buf[0] == 'a', "test base64 truncated padding YQ== 2");
   /* test invalid char in the input */
   len = base64_to_bin(buf, 1, "*YQ==", 5);
   ok(len == 0, "test base64 invalid char 1");
   len = base64_to_bin(buf, 1, "Y*Q==", 5);
   ok(len == 0, "test base64 invalid char 2");
   len = base64_to_bin(buf, 1, "YQ*==", 5);
   ok(len == 0, "test base64 invalid char 3");
   buf[0]='X';
   len = base64_to_bin(buf, 1, "YQ=*=", 5);
   ok(len == 1 && buf[0] == 'a', "test base64 invalid char after the padding");

   /* encode reference binary data to base64 */
   len = bin_to_base64(buf, 30, (char*)rnddata, 16, true);
   is(len, strlen(resb16), "Checking bin_to_base64 encoded length");
   ok(strcmp(resb16, buf) == 0, "Checking bin_to_base64 encoded data");
   /* decode reference base64 data to bin*/
   len = base64_to_bin(binbuf, 30, (char*)resb16, strlen(resb16));
   ok(len == 16, "Checking base64_to_bin decoded length");
   check_cont = true;
   for (uint a = 0; a < len; a++){
      if ((unsigned char)binbuf[a] != rnddata[a]){
         check_cont = false;
      }
   }
   ok(check_cont, "Checking base64_to_bin decoded data");
   /* decode the encoded base64 data to bin */
   len = base64_to_bin(binbuf, 30, buf, strlen(buf));
   ok(len == 16, "Checking base64_to_bin decoded length - encoded");
   check_cont = true;
   for (uint a = 0; a < len; a++){
      if ((unsigned char)binbuf[a] != rnddata[a]){
         check_cont = false;
      }
   }
   ok(check_cont, "Checking base64_to_bin decoded data - encoded");
   /* encode reference variable to base64 */
   len = to_base64(VARREF, buf);
   ok(len == 6, "Checking to_base64 encode length");
   ok(strcmp(resb8, buf) == 0, "Checking to_base64 encoded data");
   /* decode reference data to bin */
   len = from_base64(&var, (char*)resb8);
   ok(var == VARREF, "Checking from_base64 decoded data");
   ok(len == 6, "Checking from_base64 decoded length");
   /* decode encoded data to bin */
   len = from_base64(&var, buf);
   ok(var == VARREF, "Checking from_base64 decoded data - encoded");
   ok(len == 6, "Checking from_base64 decoded length - encoded");


   return report();
};
#endif /* TEST_PROGRAM */
