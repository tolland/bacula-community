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
 *  Kern Sibbald, January  MMXII
 *
 *  Selection list. A string of integers separated by commas
 *   representing items selected. Ranges of the form nn-mm
 *   are also permitted.
 */

#include "bacula.h"

/*
 * Returns next item
 *   error if returns -1 and errmsg set
 *   end of items if returns -1 and errmsg NULL
 */
int64_t sellist::next()
{
   errmsg = NULL;
   if (beg <= end) {     /* scan done? */
      //printf("Return %lld\n", beg);
      return beg++;
   }
   if (e == NULL) {
      goto bail_out;      /* nothing to scan */
   }
   /*
    * As we walk the list, we set EOF in
    *   the end of the next item to ease scanning,
    *   but save and then restore the character.
    */
   for (p=e; p && *p; p=e) {
      esave = hsave = 0;
      /* Check for list */
      e = strpbrk(p, ", ");
      if (e) {                       /* have list */
         esave = *e;
         *e++ = 0;
      }
      /* Check for range */
      h = strchr(p, '-');             /* range? */
      if (h == p) {
         errmsg = _("Negative numbers not permitted.\n");
         goto bail_out;
      }
      if (h) {                        /* have range */
         hsave = *h;
         *h++ = 0;
         if (!is_an_integer(h)) {
            errmsg = _("Range end is not integer.\n");
            goto bail_out;
         }
         skip_spaces(&p);
         if (!is_an_integer(p)) {
            errmsg = _("Range start is not an integer.\n");
            goto bail_out;
         }
         beg = str_to_int64(p);
         end = str_to_int64(h);
         //printf("beg=%lld end=%lld\n", beg, end);
         if (end <= beg) {
            errmsg = _("Range end not bigger than start.\n");
            goto bail_out;
         }
      } else {                           /* not list, not range */
         skip_spaces(&p);
         /* Check for abort (.) */
         if (*p == '.') {
            errmsg = _("User cancel requested.\n");
            goto bail_out;
         }
         /* Check for all keyword */
         if (strncasecmp(p, "all", 3) == 0) {
            all = true;
            errmsg = NULL;
            //printf("Return 0 i.e. all\n");
            return 0;
         }
         if (!is_an_integer(p)) {
            errmsg = _("Input value is not an integer.\n");
            goto bail_out;
         }
         beg = end = str_to_int64(p);
      }
      if (esave) {
         *(e-1) = esave;
      }
      if (hsave) {
         *(h-1) = hsave;
      }
      if (beg <= 0 || end <= 0) {
         errmsg = _("Selection items must be be greater than zero.\n");
         goto bail_out;
      }
      if (beg <= end) {
         //printf("Return %lld\n", beg);
         return beg++;
      }
   }
   //printf("Rtn=-1. End of items\n");
   /* End of items */
   begin();
   e = NULL;
   return -1;          /* No error */

bail_out:
   if (errmsg) {
      //printf("Bail out rtn=-1. p=%c err=%s\n", *p, errmsg);
   } else {
      //printf("Rtn=-1. End of items\n");
   }
   e = NULL;
   return -1;          /* Error, errmsg set */
}


/*
 * Set selection string and optionally scan it
 *   returns false on error in string
 *   returns true if OK
 */
bool sellist::set_string(const char *string, bool scan=true)
{
   bool ok = true;
   /*
    * Copy string, because we write into it,
    *  then scan through it once to find any
    *  errors.
    */
   if (expanded) {
      free(expanded);
      expanded = NULL;
   }
   if (str) {
      free(str);
   }
   str = bstrdup(string);
   begin();
   num_items = 0;
   if (scan) {
      while (next() >= 0) {
         num_items++;
      }
      ok = get_errmsg() == NULL;
   }
   if (ok) {
      begin();
   } else {
      e = NULL;
   }
   return ok;
}

/*
 * Get the expanded list of values separated by commas,
 * useful for SQL queries
 */
char *sellist::get_expanded_list()
{
   int32_t expandedsize = 512;
   int32_t len;
   int64_t val;
   char    *p, *tmp;
   char    ed1[SIZE_EDIT_INT];

   if (!expanded) {
      p = expanded = (char *)malloc(expandedsize * sizeof(char));
      *p = 0;

      while ((val = next()) >= 0) {
         edit_int64(val, ed1);
         len = strlen(ed1);

         /* Alloc more space if needed ( '2' because of '\0' & ',')*/
         if ((p + len + 2) > (expanded + expandedsize)) {
            expandedsize = expandedsize * 2;

            tmp = (char *) realloc(expanded, expandedsize);

            /* Compute new addresses for p and expanded */
            p = tmp + (p - expanded);
            expanded = tmp;
         }

         /* If not at the begining of the string, add a "," */
         if (p != expanded) {
            strcpy(p, ",");
            p++;
         }

         strcpy(p, ed1);
         p += len;

         /* When a limit is set, we check for the buffer size, and we return if
          * we have enough bytes. The next call will get the rest of the data.
          */
         if (limit && ((p - expanded) > (limit-SIZE_EDIT_INT))) {
            return expanded;
         }
      }
   }
   return expanded;
}

#ifndef TEST_PROGRAM
#define TEST_PROGRAM_A
#endif

#ifdef TEST_PROGRAM
#include "unittests.h"

struct test {
   const int nr;
   const char *sinp;
   const char *sout;
   const bool res;
};

static struct test tests[] = {
   { 1, "1,70", "1,70", true, },
   { 2, "1", "1", true, },
   { 3, "256", "256", true, },
   { 4, "1-5", "1,2,3,4,5", true, },
   { 5, "1-5,7", "1,2,3,4,5,7", true, },
   { 6, "1 10 20 30", "1,10,20,30", true, },
   { 7, "1-5,7,20 21", "1,2,3,4,5,7,20,21", true, },
   { 8, "all", "0", true, },
   { 9, "12a", "", false, },
   {10, "12-11", "", false, },
   {11, "12-13a", "", false, },
   {12, "a123", "", false, },
   {13, "1  3", "", false, },
   {0, "dummy", "dummy", false, },
   {-2, "19,1,9,8,2,3,10,7,11,12,4,13,6,14,15,21,22,23,24,25,27,26,28,31,29,30,33,32,34,35,36,37,41,40,38,39,44,42,43,46,45,47,49,48,53,51,50,52,55,54,56,57,58,61,59,60,62,63,64,67,68,65,66,70,71,69,74,72,73,75,77,78,76,81,79,80,84,83,82,85,86,87,90,88,89,92,91,94,96,93,95,97,98,102,99,100,101,106,104,103,108,105,107,110,111,109,112,113,114,115,116,117,120,119,118,123,122,121,125,124,126,128,127,129,130,131,132,133", "", false, },
   {-1, "1-1013", "", false, },
};

#define ntests ((int)(sizeof(tests)/sizeof(struct test)))
#define MSGLEN 80

int main()
{
   Unittests sellist_test("sellist_test");
   const char *msg;
   sellist sl;
   char buf[MSGLEN];
   bool check_rc;

   for (int i = 0; i < ntests; i++) {
      if (tests[i].nr > 0){
         snprintf(buf, MSGLEN, "Checking test: %d - %s", tests[i].nr, tests[i].sinp);
         check_rc = sl.set_string(tests[i].sinp, true);
         msg = sl.get_expanded_list();
         ok(check_rc == tests[i].res && strcmp(msg, tests[i].sout) == 0, buf);
      }
   }

   sl.set_expanded_limit(200);
   sl.set_string(tests[ntests-1].sinp);
   msg = sl.get_expanded_list();
   while ((msg = sl.get_expanded_list()) && msg[0]) {
      ok(strlen(msg) >= 4, "get_expanded_list(200) > 4");
      ok(strlen(msg) < 200, "get_expanded_list(200) < 200");
      log("%s\n", msg);
      sl.free_expanded();
   }

   sl.set_expanded_limit(100);
   sl.set_string(tests[ntests-2].sinp);
   while ((msg = sl.get_expanded_list()) && msg[0]) {
      ok(strlen(msg) >= 4, "get_expanded_list(100) > 4");
      ok(strlen(msg) < 90, "get_expanded_list(100) < 90");
      log("%s\n", msg);
      sl.free_expanded();
   }
   return report();
}
#endif   /* TEST_PROGRAM */
