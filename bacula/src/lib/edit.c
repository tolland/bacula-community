/*
   Bacula(R) - The Network Backup Solution

   Copyright (C) 2000-2023 Kern Sibbald

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
 *   edit.c  edit string to ascii, and ascii to internal
 *
 *    Kern Sibbald, December MMII
 *
 */

#include "bacula.h"
#include <math.h>

/* We assume ASCII input and don't worry about overflow */
uint64_t str_to_uint64(char *str)
{
   char *p = str;
   uint64_t value = 0;

   if (!p) {
      return 0;
   }
   while (B_ISSPACE(*p)) {
      p++;
   }
   if (*p == '+') {
      p++;
   }
   if (*p == '0' && *(p+1) == 'x') {
      p = p + 2; /* skip 0x */

      while (B_ISXDIGIT(*p)) {
         if (B_ISDIGIT(*p)) {
            value = (value<<4) + (*p - '0');

         } else {
            value = (value<<4) + (tolower(*p) - 'a' + 10);
         }
         p++;
      }
   } else {
      while (B_ISDIGIT(*p)) {
         value = B_TIMES10(value) + *p - '0';
         p++;
      }
   }
   return value;
}

int64_t str_to_int64(char *str)
{
   char *p = str;
   int64_t value;
   bool negative = false;

   if (!p) {
      return 0;
   }
   while (B_ISSPACE(*p)) {
      p++;
   }
   if (*p == '+') {
      p++;
   } else if (*p == '-') {
      negative = true;
      p++;
   }
   value = str_to_uint64(p);
   if (negative) {
      value = -value;
   }
   return value;
}


/*
 * Edit an integer number with commas, the supplied buffer
 * must be at least 27 bytes long.  The incoming number
 * is always widened to 64 bits.
 */
char *edit_uint64_with_commas(uint64_t val, char *buf)
{
   edit_uint64(val, buf);
   return add_commas(buf, buf);
}

/*
 * Edit an integer into "human-readable" format with four or fewer
 * significant digits followed by a suffix that indicates the scale
 * factor.  The buf array inherits a 27 byte minimim length
 * requirement from edit_unit64_with_commas(), although the output
 * string is limited to eight characters.
 * When with_space is true (default) a space is inserted between
 * the value and the suffix
 */
char *edit_uint64_with_suffix(uint64_t val, char *buf, bool with_space)
{
  int commas = 0;
  char *c, mbuf[50];
  const char *suffix[] =
    { "", "K", "M", "G", "T", "P", "E", "Z", "Y", "FIX ME" };
  int suffixes = sizeof(suffix) / sizeof(*suffix);

  edit_uint64_with_commas(val, mbuf);

  if ((c = strchr(mbuf, ',')) != NULL) {
    commas++;
    *c++ = '.';
    while  ((c = strchr(c, ',')) != NULL) {
      commas++;
      *c++ = '\0';
    }
    mbuf[5] = '\0'; // drop this to get '123.456 TB' rather than '123.4 TB'
  }

  if (commas >= suffixes)
    commas = suffixes - 1;
  bsnprintf(buf, 27, with_space?"%s %s":"%s%s", mbuf, suffix[commas]);
  return buf;
}

/*
 * Edit an integer number, the supplied buffer
 * must be at least 27 bytes long.  The incoming number
 * is always widened to 64 bits.
 */
char *edit_uint64(uint64_t val, char *buf)
{
   /*
    * Replacement for sprintf(buf, "%" llu, val)
    */
   char mbuf[50];
   mbuf[sizeof(mbuf)-1] = 0;
   int i = sizeof(mbuf)-2;                 /* edit backward */
   if (val == 0) {
      mbuf[i--] = '0';
   } else {
      while (val != 0) {
         mbuf[i--] = "0123456789"[val%10];
         val /= 10;
      }
   }
   bstrncpy(buf, &mbuf[i+1], 27);
   return buf;
}

char *edit_int64(int64_t val, char *buf)
{
   /*
    * Replacement for sprintf(buf, "%" llu, val)
    */
   char mbuf[50];
   bool negative = false;
   mbuf[sizeof(mbuf)-1] = 0;
   int i = sizeof(mbuf)-2;                 /* edit backward */
   if (val == 0) {
      mbuf[i--] = '0';
   } else {
      if (val < 0) {
         negative = true;
         val = -val;
      }
      while (val != 0) {
         mbuf[i--] = "0123456789"[val%10];
         val /= 10;
      }
   }
   if (negative) {
      mbuf[i--] = '-';
   }
   bstrncpy(buf, &mbuf[i+1], 27);
   return buf;
}

/*
 * Edit an integer number with commas, the supplied buffer
 * must be at least 27 bytes long.  The incoming number
 * is always widened to 64 bits.
 */
char *edit_int64_with_commas(int64_t val, char *buf)
{
   edit_int64(val, buf);
   return add_commas(buf, buf);
}

/*
 * Given a string "str", separate the numeric part into
 *   str, and the modifier into mod.
 */
static bool get_modifier(char *str, char *num, int num_len, char *mod, int mod_len)
{
   int i, len, num_begin, num_end, mod_begin, mod_end;

   strip_trailing_junk(str);
   len = strlen(str);

   for (i=0; i<len; i++) {
      if (!B_ISSPACE(str[i])) {
         break;
      }
   }
   num_begin = i;

   /* Walk through integer part */
   for ( ; i<len; i++) {
      if (!B_ISDIGIT(str[i]) && str[i] != '.') {
         break;
      }
   }
   num_end = i;
   if (num_len > (num_end - num_begin + 1)) {
      num_len = num_end - num_begin + 1;
   }
   if (num_len == 0) {
      return false;
   }
   /* Eat any spaces in front of modifier */
   for ( ; i<len; i++) {
      if (!B_ISSPACE(str[i])) {
         break;
      }
   }
   mod_begin = i;
   for ( ; i<len; i++) {
      if (!B_ISALPHA(str[i])) {
         break;
      }
   }
   mod_end = i;
   if (mod_len > (mod_end - mod_begin + 1)) {
      mod_len = mod_end - mod_begin + 1;
   }
   Dmsg5(900, "str=%s: num_beg=%d num_end=%d mod_beg=%d mod_end=%d\n",
      str, num_begin, num_end, mod_begin, mod_end);
   bstrncpy(num, &str[num_begin], num_len);
   bstrncpy(mod, &str[mod_begin], mod_len);
   if (!is_a_number(num)) {
      return false;
   }
   bstrncpy(str, &str[mod_end], len);
   Dmsg2(900, "num=%s mod=%s\n", num, mod);

   return true;
}

/*
 * Convert a string duration to utime_t (64 bit seconds)
 * Returns false: if error
 *          true:  if OK, and value stored in value
 *
 * *** The str argument is modified by this function ***
 */
bool duration_to_utime(char *str, utime_t *value)
{
   int i, mod_len;
   double val, total = 0.0;
   char mod_str[20];
   char num_str[50];
   /*
    * The "n" = mins and months appears before minutes so that m maps
    *   to months. These "kludges" make it compatible with pre 1.31
    *   Baculas.
    */
   static const char *mod[] = {"n", "seconds", "months", "minutes", "mins",
                  "hours", "days", "weeks",   "quarters",   "years", NULL};
   static const int32_t mult[] = {60,   1, 60*60*24*30, 60, 60,
                  3600, 3600*24, 3600*24*7, 3600*24*91, 3600*24*365};

   while (*str) {
      if (!get_modifier(str, num_str, sizeof(num_str), mod_str, sizeof(mod_str))) {
         return false;
      }
      /* Now find the multiplier corresponding to the modifier */
      mod_len = strlen(mod_str);
      if (mod_len == 0) {
         i = 1;                          /* default to seconds */
      } else {
         for (i=0; mod[i]; i++) {
            if (strncasecmp(mod_str, mod[i], mod_len) == 0) {
               break;
            }
         }
         if (mod[i] == NULL) {
            return false;
         }
      }
      Dmsg2(900, "str=%s: mult=%d\n", num_str, mult[i]);
      errno = 0;
      val = strtod(num_str, NULL);
      if (errno != 0 || val < 0) {
         return false;
      }
      total += val * mult[i];
   }
   *value = (utime_t)total;
   return true;
}

/*
 * Edit a utime "duration" into ASCII
 */
char *edit_utime(utime_t val, char *buf, int buf_len)
{
   char mybuf[200];
   static const int32_t mult[] = {60*60*24*365, 60*60*24*30, 60*60*24, 60*60, 60};
   static const char *mod[]  = {"year",  "month",  "day", "hour", "min"};
   int i;
   uint32_t times;

   *buf = 0;
   for (i=0; i<5; i++) {
      times = (uint32_t)(val / mult[i]);
      if (times > 0) {
         val = val - (utime_t)times * mult[i];
         bsnprintf(mybuf, sizeof(mybuf), "%d %s%s ", times, mod[i], times>1?"s":"");
         bstrncat(buf, mybuf, buf_len);
      }
   }
   if (val == 0 && strlen(buf) == 0) {
      bstrncat(buf, "0 secs", buf_len);
   } else if (val != 0) {
      bsnprintf(mybuf, sizeof(mybuf), "%d sec%s", (uint32_t)val, val>1?"s":"");
      bstrncat(buf, mybuf, buf_len);
   }
   return buf;
}

static bool strunit_to_uint64(char *str, int str_len, uint64_t *value,
                              const char **mod)
{
   int i, mod_len;
   double val;
   char mod_str[20];
   char num_str[50];
   const int64_t mult[] = {1,                 /* byte */
                           1024,              /* kilobyte */
                           1000,              /* kb kilobyte */
                           1024,              /* kibibyte */
                           1048576,           /* megabyte */
                           1000000,           /* mb megabyte */
                           1048576,           /* mebibyte */
                           1073741824,        /* gigabyte */
                           1000000000,        /* gb gigabyte */
                           1073741824,        /* gibibyte */
                           1099511627776LL,   /* terabyte */
                           1000000000000LL,   /* tb terabyte */
                           1099511627776LL};  /* tebibyte */

   if (!get_modifier(str, num_str, sizeof(num_str), mod_str, sizeof(mod_str))) {
      return 0;
   }
   /* Now find the multiplier corresponding to the modifier */
   mod_len = strlen(mod_str);
   if (mod_len == 0) {
      i = 0;                          /* default with no modifier = 1 */
   } else {
      for (i=0; mod[i]; i++) {
         if (strncasecmp(mod_str, mod[i], mod_len) == 0) {
            break;
         }
      }
      if (mod[i] == NULL) {
         return false;
      }
   }
   Dmsg2(900, "str=%s: mult=%d\n", str, mult[i]);
   errno = 0;
   val = strtod(num_str, NULL);
   if (errno != 0 || val < 0) {
      return false;
   }
   *value = (utime_t)(val * mult[i]);
   return true;
}

/*
 * Convert a size in bytes to uint64_t
 * Returns false: if error
           true:  if OK, and value stored in value
 */
bool size_to_uint64(char *str, int str_len, uint64_t *value)
{
   /* first item * not used */
   static const char *mod[]  = {"*", "k", "kb", "kib",
                                     "m", "mb", "mib",
                                     "g", "gb", "gib",
                                     "t", "tb", "tib",
                                     NULL};
   return strunit_to_uint64(str, str_len, value, mod);
}

/*
 * Convert a speed in bytes/s to uint64_t
 * Returns false: if error
           true:  if OK, and value stored in value
 */
bool speed_to_uint64(char *str, int str_len, uint64_t *value)
{
   /* first item * not used */
   static const char *mod[]  = {"*", "k/s", "kb/s", "kib/s",
                                     "m/s", "mb/s", "mib/s",
                                     NULL};
   return strunit_to_uint64(str, str_len, value, mod);
}

/*
 * Check if specified string is a number or not.
 *  Taken from SQLite, cool, thanks.
 */
bool is_a_number(const char *n)
{
   bool digit_seen = false;

   if (n == NULL) {
      return false;
   }

   if( *n == '-' || *n == '+' ) {
      n++;
   }
   while (B_ISDIGIT(*n)) {
      digit_seen = true;
      n++;
   }
   if (digit_seen && *n == '.') {
      n++;
      while (B_ISDIGIT(*n)) { n++; }
   }
   if (digit_seen && (*n == 'e' || *n == 'E')
       && (B_ISDIGIT(n[1]) || ((n[1]=='-' || n[1] == '+') && B_ISDIGIT(n[2])))) {
      n += 2;                         /* skip e- or e+ or e digit */
      while (B_ISDIGIT(*n)) { n++; }
   }
   return digit_seen && *n==0;
}

/*
 * Check if specified string is a list of numbers or not
 */
bool is_a_number_list(const char *n)
{
   bool previous_digit = false;
   bool digit_seen = false;
   if (n == NULL) {
      return false;
   }
   while (*n) {
      if (B_ISDIGIT(*n)) {
         previous_digit=true;
         digit_seen = true;
      } else if (*n == ',' && previous_digit) {
         previous_digit = false;
      } else {
         return false;
      }
      n++;
   }
   return digit_seen && *n==0;
}

/*
 * Check if the specified string is an integer
 */
bool is_an_integer(const char *n)
{
   bool digit_seen = false;
   if (n == NULL) {
      return false;
   }
   while (B_ISDIGIT(*n)) {
      digit_seen = true;
      n++;
   }
   return digit_seen && *n==0;
}

/*
 * Check if Bacula Resoure Name is valid
 */
/*
 * Check if the Volume name has legal characters
 * If ua is non-NULL send the message
 */
bool is_name_valid(const char *name, POOLMEM **msg)
{
   /* Special characters to accept */
   const char *accept = EXTRA_VALID_RESOURCE_CHAR;
   return is_name_valid(name, msg, accept);
}

/*
 * Check if Bacula Resoure Name is valid
 */
/*
 * Check if the Volume/resource name has legal characters
 * If ua is non-NULL send the message
 */
bool is_name_valid(const char *name, POOLMEM **msg, const char *accept)
{
   int len;
   const char *p;

   /* No name is invalid */
   if (!name) {
      if (msg) {
         Mmsg(msg, _("Empty name not allowed.\n"));
      }
      return false;
   }
   /* Restrict the characters permitted in the Volume name */
   for (p=name; *p; p++) {
      if (B_ISALPHA(*p) || B_ISDIGIT(*p) || strchr(accept, (int)(*p))) {
         continue;
      }
      if (msg) {
         Mmsg(msg, _("Illegal character \"%c\" in name.\n"), *p);
      }
      return false;
   }
   len = p - name;
   if (len >= MAX_NAME_LENGTH) {
      if (msg) {
         Mmsg(msg, _("Name too long.\n"));
      }
      return false;
   }
   if (len == 0) {
      if (msg) {
         Mmsg(msg,  _("Name must be at least one character long.\n"));
      }
      return false;
   }
   return true;
}

/* Small helper method, so far used only by the PostgreSQL and MySQL plugins
 * to check if name provided by the user can be used.
 * Checks if given string does not contain suspicious characters, as well as
 * informs the called if it's already double-quoted or not, to know if it needs
 * some further processing.
 * @ret false if no invalid character is found
 * @ret true if some invalid character is found ('err' is filed with message in that case)
 */
bool check_for_invalid_chars(const char *str, POOLMEM **err, bool *quote_needed)
{
   *quote_needed = true;
   int len = strlen(str);
   for (int i=0; i<len; i++) {
      switch (str[i]) {
         case '\\':
            pm_strcpy(err, "Found invalid \"\\\" character");
            return true;
         case '\'':
            pm_strcpy(err, "Found invalid \"\'\" character");
            return true;
         case '`':
            pm_strcpy(err, "Found invalid \"`\" character");
            return true;
         case '$':
            pm_strcpy(err, "Found invalid \"$\" character");
            return true;
         case '\"':
            /* Only outer double quotes are allowed */
            if (i == 0 || i == (len-1)) {
               if (i == 0 && str[len-1] != '\"') {
                  pm_strcpy(err, "Missing opening double quote");
                  return true;
               } else if (i == (len-1) && str[0] != '\"') {
                  pm_strcpy(err, "Missing closing double quote");
                  return true;
               } else {
                  *quote_needed = false;
               }
               break;
            } else {
               pm_strcpy(err, "Found invalid \'\"\' character");
               return true;
            }

            return true;
         default:
            break;
      }
   }

   /* No invalid character found */
   return false;
}

/*
 * Add commas to a string, which is presumably
 * a number.
 */
char *add_commas(char *val, char *buf)
{
   int len, nc;
   char *p, *q;
   int i;

   if (val != buf) {
      strcpy(buf, val);
   }
   len = strlen(buf);
   if (len < 1) {
      len = 1;
   }
   nc = (len - 1) / 3;
   p = buf+len;
   q = p + nc;
   *q-- = *p--;
   for ( ; nc; nc--) {
      for (i=0; i < 3; i++) {
          *q-- = *p--;
      }
      *q-- = ',';
   }
   return buf;
}

/* Parse stream of tags, return next one from the stream (it will be null terminated,
 * original buffer will be changed) */
char *get_next_tag(char **buf)
{
   char *tmp = NULL;

   if (**buf != '\0') {
      char *p = *buf;
      tmp = p;
      p = strchr(*buf, ' ');
      if (p != NULL){
         *p++ = '\0';
         *buf = p;
      } else {
         *buf += strlen(tmp);
      }
      Dmsg1(900, "Found tag: %s\n", tmp);
   } else {
      Dmsg0(900, "No tag found!\n");
   }

   return tmp;
}

// #define TEST_PROGRAM

#ifdef TEST_PROGRAM
#include "unittests.h"

void d_msg(const char*, int, int, const char*, ...)
{}

// this is a test vector
// /@kubernetes/ kubernetes:^Ans=plugintest^Adebug^Averify_ssl=0 Container PVCs Kubernetes^APersistent^AVolume^AClaim   5368709120 U 5
char __po_log1[] = {
  0x2f, 0x40, 0x6b, 0x75, 0x62, 0x65, 0x72, 0x6e, 0x65, 0x74, 0x65, 0x73,
  0x2f, 0x20, 0x6b, 0x75, 0x62, 0x65, 0x72, 0x6e, 0x65, 0x74, 0x65, 0x73,
  0x3a, 0x01, 0x6e, 0x73, 0x3d, 0x70, 0x6c, 0x75, 0x67, 0x69, 0x6e, 0x74,
  0x65, 0x73, 0x74, 0x01, 0x64, 0x65, 0x62, 0x75, 0x67, 0x01, 0x76, 0x65,
  0x72, 0x69, 0x66, 0x79, 0x5f, 0x73, 0x73, 0x6c, 0x3d, 0x30, 0x20, 0x43,
  0x6f, 0x6e, 0x74, 0x61, 0x69, 0x6e, 0x65, 0x72, 0x20, 0x50, 0x56, 0x43,
  0x73, 0x20, 0x4b, 0x75, 0x62, 0x65, 0x72, 0x6e, 0x65, 0x74, 0x65, 0x73,
  0x01, 0x50, 0x65, 0x72, 0x73, 0x69, 0x73, 0x74, 0x65, 0x6e, 0x74, 0x01,
  0x56, 0x6f, 0x6c, 0x75, 0x6d, 0x65, 0x01, 0x43, 0x6c, 0x61, 0x69, 0x6d,
  0x20, 0x20, 0x20, 0x35, 0x33, 0x36, 0x38, 0x37, 0x30, 0x39, 0x31, 0x32,
  0x30, 0x20, 0x55, 0x20, 0x35, 0x00
};
unsigned int __po_log1_len = sizeof(__po_log1);
int __po_log1_nr = 10;

// ASD  123
char __po_log2[] = {0x41, 0x53, 0x44, 0x20, 0x20, 0x31, 0x32, 0x33, 0x00, 0x00};
unsigned int __po_log2_len = sizeof(__po_log2);

struct _edit_utime_vect
{
   const char *in;
   const utime_t val;
   const char *outval;
};

_edit_utime_vect __testvect1[] =
{
   { "3", 3, "3 secs"},
   { "3n", 180, "3 mins "},
   { "3 hours", 10800, "3 hours "},
   { "3.5 day", 302400, "3 days 12 hours "},
   { "3 week", 1814400, "21 days "},
   { "3 m", 7776000, "3 months "},
   { "3 q", 23587200, "9 months 3 days "},
   { "3 years", 94608000, "3 years "},
   { "23587201", 23587201, "9 months 3 days 1 sec"},
   { NULL, 0, NULL},
};

int main()
{
   Unittests unittest("text_edit_tests");

   {
      utime_t val;
      char buf[100];
      char outval[100];

      for (int i=0; __testvect1[i].in != NULL; i++) {
         strcpy(buf, __testvect1[i].in);
         POOL_MEM label;
         Mmsg(label, "duration_to_utime %s", __testvect1[i].in);
         bool status = duration_to_utime(buf, &val);
         ok(status, label.c_str());
         if (status){
            edit_utime(val, outval, sizeof(outval));
            ok(val == __testvect1[i].val, "checking val");
            ok(strcmp(outval, __testvect1[i].outval) == 0, "checking outval");
         }
         // printf("outval='%s'\n", outval);
      }
   }

   {
      char *testvect = __po_log2;
      char **obj_str = &testvect;

      char *fname = get_next_tag(obj_str);
      ok(fname != NULL, "checking first tag");
      ok(strcmp(fname, "ASD") == 0, "checking first tag value");

      char *empty = get_next_tag(obj_str);
      ok(empty != NULL, "checking empty tag");
      ok(strlen(empty) == 0, "checking empty value tag");

      char *last = get_next_tag(obj_str);
      ok(last != NULL, "checking last tag");
      ok(strcmp(last, "123") == 0, "checking last tag value");
      ok(obj_str != NULL, "checking obj_str");
      ok(*obj_str != NULL, "checking obj_str ptr");
      ok(**obj_str == 0, "checking obj_str char");

      char *afterlast = get_next_tag(obj_str);
      ok(afterlast == NULL, "checking no tags");
   }

   {
      char *testvect = __po_log1;
      char **obj_str = &testvect;

      for (int a = 0; a < __po_log1_nr; a++){
         char *tag = get_next_tag(obj_str);
         POOL_MEM label;
         Mmsg(label, "checking tag %d", a);
         ok(tag != NULL, label.c_str());
      }
      ok(get_next_tag(obj_str) == NULL, "checking the last");
   }

   return report();
}

#endif
