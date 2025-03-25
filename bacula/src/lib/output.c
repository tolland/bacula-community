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
 *  Written by: Eric Bollengier, December MMXIII
 */

#define OUTPUT_C                /* control dll export in output.h */
#include "output.h"
#include "plugins.h"

/* use new output (lowercase, no special char) */
#define OF_USE_NEW_OUTPUT  1
#define OF_USE_QUOTES      2
#define OF_USE_JSON        4

void OutputWriter::parse_options(const char *options)
{
   int nb=0;
   const char *p = options;
   if (!p) {
      return;
   }
   while (*p) {
      nb=0;

      switch(*p) {
      case 'q':
         flags |= OF_USE_QUOTES;
         break;

      case 'j':
         flags |= OF_USE_QUOTES; // q
         flags |= OF_USE_JSON;   // j
         flags |= OF_USE_NEW_OUTPUT; /* o lowercase and only isalpha */
         set_separator(',');         // S44
         set_equal(':');             // e123
         set_label(":");             // l
         set_hash("{", "}");
         set_table("[", "]");
         set_object_separator('{', '}');
         break;

      case 'C':
         flags = 0;
         set_time_format(OW_DEFAULT_TIMEFORMAT);
         set_separator(OW_DEFAULT_SEPARATOR);
         set_equal(OW_DEFAULT_EQUAL);
         set_hash(OW_DEFAULT_OPEN_HASH, OW_DEFAULT_CLOSE_HASH);
         set_table(OW_DEFAULT_OPEN_TABLE, OW_DEFAULT_CLOSE_TABLE);
         set_label(OW_DEFAULT_LABEL);
         set_object_separator(0, 0);
         break;

      case 'S':                 /* object separator */
         while(isdigit(*(p+1))) {
            nb = nb*10 + (*(++p) - '0');
         }
         if (isascii(nb)) {
            set_object_separator((char) nb, (char)nb);
         }
         break;

      case 'o':
         flags |= OF_USE_NEW_OUTPUT; /* lowercase and only isalpha */
         break;

      case 't':                 /* Time format */
         if (isdigit(*(p+1))) {
            nb = (*(++p) - '0');
            set_time_format((OutputTimeType) nb);
         }
         break;

      case 's':                 /* Separator */
         while(isdigit(*(p+1))) {
            nb = nb*10 + (*(++p) - '0');
         }
         if (isascii(nb)) {
            set_separator((char) nb);
         }
         break;
      case 'e':
         while(isdigit(*(p+1))) {
            nb = nb*10 + (*(++p) - '0');
         }
         if (isascii(nb)) {
            set_equal((char) nb);
         }
         break;
      case 'l':
         while(isdigit(*(p+1))) {
            nb = nb*10 + (*(++p) - '0');
         }
         if (isascii(nb)) {
            char ed1[2];
            ed1[0] = (char) nb;
            ed1[1] = 0;
            set_label(ed1);
         }
         break;
      default:
         break;
      }
      p++;
   }
}

const char *OutputWriter::ow_quote_string(const char *str, POOLMEM *&b)
{
   if (flags & OF_USE_QUOTES) {
      b = quote_string(b, str);
      return b;
   } else {
      return str;
   }
}

const char *OutputWriter::ow_quote_string(const char *str)
{
   return ow_quote_string(str, quote_buf);
}

const char *OutputWriter::ow_quote_string2(const char *str)
{
   return ow_quote_string(str, quote_buf2);
}

char *OutputWriter::get_options(char *dest, int len)
{
   char ed1[50];
   *dest = *ed1 = 0;
   if (separator != OW_DEFAULT_SEPARATOR) {
      snprintf(dest, 50, "s%d", (int)separator);
   }
   if (object_separator[0]) {
      snprintf(ed1, sizeof(ed1), "S%d", (int) object_separator[0]);
      bstrncat(dest, ed1, len);
   }
   if (timeformat != OW_DEFAULT_TIMEFORMAT) {
      snprintf(ed1, sizeof(ed1), "t%d", (int) timeformat);
      bstrncat(dest, ed1, len);
   }
   if (equal != OW_DEFAULT_EQUAL) {
      snprintf(ed1, sizeof(ed1), "e%d", (int) equal);
      bstrncat(dest, ed1, len);
   }
   if (strcmp(label, OW_DEFAULT_LABEL) != 0) {
      snprintf(ed1, sizeof(ed1), "l%d", (int)label[0]);
      bstrncat(dest, ed1, len);
   }
   if (flags & OF_USE_NEW_OUTPUT) {
      bstrncat(dest, "o", len);
   }
   if (flags & OF_USE_QUOTES) {
      bstrncat(dest, "q", len);
   }
   if (flags & OF_USE_JSON) {
      bstrncat(dest, "j", len);
   }
   return dest;
}

void OutputWriter::get_buf(bool append)
{
   if (!buf) {
      quote_buf = get_pool_memory(PM_MESSAGE);
      quote_buf2 = get_pool_memory(PM_MESSAGE);
      buf = get_pool_memory(PM_MESSAGE);
      *quote_buf2 = *quote_buf = *buf = 0;

   } else if (!append) {
      *buf = 0;
   }
}

char *OutputWriter::start_group(const char *name, bool append)
{
   get_buf(append);
   return get_output(OT_START_OBJ, OT_LABEL, name, OT_END);
}

char *OutputWriter::end_group(bool append)
{
   get_buf(append);
   if (limit >= 0) {
      get_output(OT_SEP, OT_INT, "limit", limit, OT_END);
   }
   if (offset >= 0) {
      get_output(OT_SEP, OT_INT, "offset", offset, OT_END);
   }
   return get_output(OT_SEP,
                     OT_INT32, "error", error,
                     OT_STRING, "errmsg", NPRTB(errmsg),
                     OT_END_OBJ,
                     OT_NL,
                     OT_END);
}

char *OutputWriter::start_object(const char *name, bool append)
{
   get_buf(append);
   return get_output(OT_LABEL, name, OT_START_OBJ, OT_END);
}

char *OutputWriter::end_object(bool append)
{
   get_buf(append);
   return get_output(OT_END_OBJ, OT_END);
}

char *OutputWriter::start_list(const char *name, bool append)
{
   get_buf(append);
   if (use_json()) {
      if (*buf) {
         int l = strlen(buf);
         if (buf[l - 1] != ',' && buf[l - 1] != ':' && buf[l - 1] != '{') {
            pm_strcat(buf, ",");
         }
      }
      pm_strcat(buf, ow_quote_string(name));
      pm_strcat(buf, ":[");
      need_separator = false;
      will_need_separator = false;

   } else {
      if (*buf) {
         int l = strlen(buf);
         if (buf[l - 1] != separator) {
            char ed1[2];
            ed1[0] = separator;
            ed1[1] = '\0';
            pm_strcat(buf, ed1);
         }
      }
      pm_strcat(buf, name);
      pm_strcat(buf, ": [\n");
   }
   return buf;
}

char *OutputWriter::end_list(bool append)
{
   get_buf(append);
   if (use_json()) {
      pm_strcat(buf, "]");

   } else {
      pm_strcat(buf, "]\n");
   }

   return buf;
}

/* Usage:
 *   get_output(
 *       OT_STRING,   "name",       "value",
 *       OT_PINT32,   "age",        10,
 *       OT_TIME,     "birth-date", 1120202002,
 *       OT_PINT64,   "weight",     100,
 *       OT_END);
 *
 *
 *  "name=value\nage=10\nbirt-date=2012-01-12 10:20:00\nweight=100\n"
 *
 */
char *OutputWriter::get_output(OutputType first, ...)
{
   char    *ret;
   va_list  arg_ptr;

   get_buf(true);               /* Append to the current string */

   va_start(arg_ptr, first);
   ret = get_output(arg_ptr, &buf, first);
   va_end(arg_ptr);

   return ret;
}

/* Usage:
 *   get_output(&out,
 *       OT_STRING,   "name",       "value",
 *       OT_PINT32,   "age",        10,
 *       OT_TIME,     "birth-date", 1120202002,
 *       OT_PINT64,   "weight",     100,
 *       OT_END);
 *
 *
 *  "name=value\nage=10\nbirt-date=2012-01-12 10:20:00\nweight=100\n"
 *
 */
char *OutputWriter::get_output(POOLMEM **out, OutputType first, ...)
{
   va_list arg_ptr;
   char *ret;

   va_start(arg_ptr, first);
   ret = get_output(arg_ptr, out, first);
   va_end(arg_ptr);

   return ret;
}

bool OutputWriter::use_json()
{
   return flags & OF_USE_JSON;
}

char *OutputWriter::get_output(va_list ap, POOLMEM **out, OutputType first)
{
   char       ed1[MAX_TIME_LENGTH];
   int        i;
   int64_t    i64;
   uint64_t   u64;
   int32_t    i32;
   double     d;
   btime_t    bt;
   char      *s = NULL, *k = NULL;
   alist     *lst;
   Plugin    *plug;
   POOLMEM   *tmp3 = get_pool_memory(PM_FNAME);
   POOLMEM   *tmp2 = get_pool_memory(PM_FNAME);
   POOLMEM   *tmp = get_pool_memory(PM_FNAME);
   OutputType val = first;
   *tmp3 = *tmp2 = *tmp = 0;

   while (val != OT_END) {

      *tmp = 0;

      /* Some arguments are not using a keyword */
      switch (val) {
      case OT_SEP:
         need_separator = true;
         will_need_separator = false;
         break;
      case OT_NOP:
         need_separator = false;
         will_need_separator = false;
         break;
      case OT_CLEAR:
         need_separator = false;
         will_need_separator = false;
         break;
      case OT_START_OBJ:
      case OT_START_ARRAY:
      case OT_START_HASH:
         will_need_separator = false;
         break;
      case OT_NL:
      case OT_END:
      case OT_END_OBJ:
      case OT_END_ARRAY:
      case OT_END_HASH:
         need_separator = false;
         will_need_separator = true;
         break;
      default:
         will_need_separator = true;
         k = va_arg(ap, char *);          /* Get the variable name */

         /* If requested, we can put the keyword in lowercase */
         if (flags & OF_USE_NEW_OUTPUT) {
            tmp2 = check_pool_memory_size(tmp2, strlen(k)+1);
            for (i = 0; k[i] ; i++) {
               if (isalnum(k[i])) {
                  tmp2[i] = tolower(k[i]);
               } else {
                  tmp2[i] = '_';
               }
            }
            tmp2[i] = 0;
            k = tmp2;
         }
      }

      //Dmsg2(000, "%d - %s\n", val, k);

      switch (val) {
      case OT_ALIST_STR:
         lst = va_arg(ap, alist *);
         i = 0;
         Mmsg(tmp, "%s%c%s",
              ow_quote_string(k), equal,
              open_table);
         if (lst) {
            foreach_alist(s, lst) {
               if (i++ > 0) {
                  pm_strcat(tmp, ",");
               }
               pm_strcat(tmp, ow_quote_string(s));
            }
         }
         pm_strcat(tmp, close_table);
         break;
      case OT_PLUGINS:
         lst = va_arg(ap, alist *);
         i = 0;
         Mmsg(tmp, "%s%c%s",
              ow_quote_string("plugins"),
              equal,
              open_table);
         if (lst) {
            foreach_alist(plug, lst) {
               if (i++ > 0) {
                  pm_strcat(tmp, ",");
               }
               pm_strcat(tmp, ow_quote_string(plug->name));
            }
         }
         pm_strcat(tmp, close_table);
         break;
      case OT_RATIO:
         d = va_arg(ap, double);
         Mmsg(tmp, "%s%c%.2f", ow_quote_string(k), equal, d);
         break;
      case OT_BOOL:
         i = va_arg(ap, int);
         Mmsg(tmp, "%s%c%s", ow_quote_string(k), equal, i?"true":"false");
         break;
      case OT_DATE:             // Same as string for the moment
      case OT_STRING:
         s = va_arg(ap, char *);
         Mmsg(tmp, "%s%c%s", ow_quote_string(k), equal, ow_quote_string2(NPRTB(s))) ;
         break;
      case OT_INT32:
         i32 = va_arg(ap, int32_t);
         Mmsg(tmp, "%s%c%d", ow_quote_string(k), equal, i32);
         break;

      case OT_UTIME:
      case OT_BTIME:
         if (val == OT_UTIME) {
            bt = va_arg(ap, utime_t);
         } else {
            bt = va_arg(ap, btime_t);
         }
         switch (timeformat) {
         case OTT_TIME_NC: /* Formatted time for user display: dd-Mon hh:mm */
            bstrftime_ny(ed1, sizeof(ed1), bt);
            break;

         case OTT_TIME_UNIX:         /* unix timestamp */
            bsnprintf(ed1, sizeof(ed1), "%lld", bt);
            break;

         case OTT_TIME_ISO:
            /* wanted fallback */
         default:
            bstrutime(ed1, sizeof(ed1), bt);
         }
         if (flags & OF_USE_QUOTES) {
            ow_quote_string(ed1, tmp3);
            bstrncpy(ed1, tmp3, sizeof(ed1));
         }
         Mmsg(tmp3, "%s_epoch", k);
         Mmsg(tmp,
              "%s%c%lld"
              "%c"
              "%s%c%s",
              ow_quote_string(tmp3), equal, bt,
              separator,
              ow_quote_string2(k), equal, ed1);
         break;

      case OT_DURATION:
         bt = va_arg(ap, utime_t);     
         bstrutime(ed1, sizeof(ed1), bt);
         if (flags & OF_USE_QUOTES) {
            ow_quote_string(edit_utime(bt, ed1, sizeof(ed1)), tmp3);
            bstrncpy(ed1, tmp3, sizeof(ed1));
         }

         Mmsg(tmp3, "%s_str", k);
         Mmsg(tmp, "%s%c%lld"
              "%c"
              "%s%c%s",
              ow_quote_string(k), equal, bt,
              separator,
              ow_quote_string2(tmp3), equal, ed1);
         break;

      case OT_SIZE:
      case OT_INT64:
         i64 = va_arg(ap, int64_t);
         Mmsg(tmp, "%s%c%lld", ow_quote_string(k), equal, i64);
         break;

      case OT_PINT64:
         u64 = va_arg(ap, uint64_t);
         Mmsg(tmp, "%s%c%llu", ow_quote_string(k), equal, u64);
         break;

      case OT_PINT32:
         u64 = va_arg(ap, uint32_t);
         Mmsg(tmp, "%s%c%llu", ow_quote_string(k), equal, u64);
         break;

      case OT_INT:
         i64 = va_arg(ap, int);
         Mmsg(tmp, "%s%c%lld", ow_quote_string(k), equal, i64);
         break;

      case OT_JOBLEVEL:
      case OT_JOBTYPE:
      case OT_JOBSTATUS:
         i32 = va_arg(ap, int32_t);
         if (i32 == 0) {        // TODO: Check if it's possible
            Mmsg(tmp, "%s%c%s", ow_quote_string(k), equal, ow_quote_string2(""));
         } else {
            ed1[0] = (char)i32;
            ed1[1] = 0;
            Mmsg(tmp, "%s%c%s", ow_quote_string(k), equal, ow_quote_string2(ed1));
         }
         break;

      case OT_CLEAR:
         **out = 0;
         break;

      case OT_END_OBJ:
         i=0;
         if (object_separator[1]) {
            tmp[i++] = object_separator[1];
         } else {
            tmp[i++] = '\n';
         }
         tmp[i] = 0;
         break;

      case OT_LABEL:
         Mmsg(tmp, "%s%s", ow_quote_string(k), label);
         will_need_separator = false;
         break;

      case OT_START_OBJ:
         i=0;
         if (object_separator[0]) {
            tmp[i++] = object_separator[0];

         } else {
            tmp[i++] = '\n';
         }
         tmp[i] = 0;
         break;

      case OT_START_HASH:
         pm_strcpy(tmp, open_hash);
         break;

      case OT_END_HASH:
         pm_strcpy(tmp, close_hash);
         break;

      case OT_START_ARRAY:
         pm_strcpy(tmp, open_table);
         break;

      case OT_END_ARRAY:
         pm_strcpy(tmp, close_table);
         break;

      case OT_NL:
         pm_strcat(tmp, "\n");
         break;

      case OT_NOP:
         break;

      case OT_SEP:
         break;                 // Will add a separator at the next round

      case OT_END:
         /* wanted fallback */
      default:
         val = OT_END;
      }

      if (val != OT_END) {
         if (need_separator) {
            pm_strcat(out, separator_str);

         }
         need_separator = will_need_separator;
         pm_strcat(out, tmp);
         val = (OutputType) va_arg(ap, int); /* OutputType is promoted to int when using ... */
      }
   }

   free_pool_memory(tmp);
   free_pool_memory(tmp2);
   free_pool_memory(tmp3);
   //Dmsg1(000, "%s", *out);
   return *out;
}

#ifndef TEST_PROGRAM
#define TEST_PROGRAM_A
#endif

#ifdef TEST_PROGRAM
#include "unittests.h"

int main(int argc, char **argv)
{
   Unittests output_test("output_test");
   char ed1[50];
   OutputWriter wt;
   POOLMEM *tmp = get_pool_memory(PM_FNAME);
   *tmp = 0;

   int         nb   = 10000;
   const char *ptr  = "my value";
   char       *str  = bstrdup("ptr");
   int32_t     nb32 = -1;
   int64_t     nb64 = -1;
   btime_t     t    = time(NULL);

   ok(strcmp(wt.get_options(ed1, sizeof(ed1)), "") == 0, "Default options");
   Pmsg1(000, "%s", wt.start_group("test"));

   wt.get_output(&tmp, OT_CLEAR,
                 OT_STRING, "test", "my value",
                 OT_STRING, "test2", ptr,
                 OT_STRING, "test3", str,
                 OT_INT,    "nb",   nb,
                 OT_INT32,  "nb32", nb32,
                 OT_INT64,  "nb64", nb64,
                 OT_BTIME,  "now",  t,
                 OT_END);

   Pmsg1(000, "%s", tmp);

   free_pool_memory(tmp);

   Pmsg1(000, "%s",
         wt.get_output(OT_CLEAR,
                 OT_START_OBJ,
                 OT_STRING, "test", "my value",
                 OT_STRING, "test2", ptr,
                 OT_STRING, "test3", str,
                 OT_INT,    "nb",   nb,
                 OT_INT32,  "nb32", nb32,
                 OT_INT64,  "nb64", nb64,
                 OT_BTIME,  "now",  t,
                 OT_END_OBJ,
                 OT_END));

   wt.set_time_format(OTT_TIME_UNIX);
   ok(strcmp("t1", wt.get_options(ed1, sizeof(ed1))) == 0, "Check unix time format");

   Pmsg1(000, "%s",
         wt.get_output(OT_CLEAR,
                 OT_BTIME,  "now",  t,
                 OT_END));

   wt.set_time_format(OTT_TIME_NC);
   ok(strcmp("t2", wt.get_options(ed1, sizeof(ed1))) == 0, "Check NC time format");

   Pmsg1(000, "%s",
         wt.get_output(OT_CLEAR,
                 OT_BTIME,  "now",  t,
                 OT_END));

   Pmsg1(000, "%s", wt.end_group(false));

   wt.parse_options("s43t1O");
   ok(strcmp(wt.get_options(ed1, sizeof(ed1)), "s43t1") == 0, "Check options after parsing");

   char *p = wt.get_output(OT_CLEAR,
                           OT_BTIME,  "now",  t,
                           OT_STRING, "brazil", "test",
                           OT_END);
   ok(strstr(p, "+brazil=test") != NULL,
      "Check separator");
   Pmsg1(000, "===> [%s]\n", p);

   wt.parse_options("CS35");
   ok(strcmp(wt.get_options(ed1, sizeof(ed1)), "S35") == 0, "Check options after parsing");

   Pmsg1(000, "%s",
         wt.get_output(OT_CLEAR,
                 OT_START_OBJ,
                 OT_STRING, "test", "my value",
                 OT_STRING, "test2", ptr,
                 OT_END_OBJ,
                 OT_START_OBJ,
                 OT_STRING, "test", "my value",
                 OT_STRING, "test2", ptr,
                 OT_END_OBJ,
                 OT_END));


   wt.parse_options("Cs44e58q");
   ok(strcmp(wt.get_options(ed1, sizeof(ed1)), "s44e58q") == 0, "Check options after parsing");

   wt.parse_options("Cj");
   Dmsg1(0, "%s\n", wt.get_options(ed1, sizeof(ed1)));
   ok(strcmp(wt.get_options(ed1, sizeof(ed1)), "s44S123e58l58oqj") == 0, "Check options after parsing");
   
   Pmsg1(000, "%s",
         wt.get_output(OT_CLEAR,
                       OT_START_ARRAY,
                       OT_LABEL, "backup",
                 OT_START_HASH,
                 OT_STRING, "test", "my value",
                 OT_STRING, "test2", ptr,
                 OT_END_HASH,
                       OT_LABEL, "restore",
                 OT_START_HASH,
                 OT_STRING, "test", "my value",
                 OT_STRING, "test2", ptr,
                 OT_END_HASH,
                 OT_LABEL, "info",
                 OT_START_HASH,
                 OT_BTIME, "now", t,
                 OT_STRING, "test2", ptr,
                 OT_END_HASH,
                 OT_END_ARRAY,
                 OT_END));

   free(str);
   return report();
}
#endif   /* TEST_PROGRAM */
