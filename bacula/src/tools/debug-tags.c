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

/* Written by Eric Bollengier */

#include "bacula.h"
#include "lib/unittests.h"


void test_list(int64_t tags, int nb, const char *format)
{
      alist tlist(10, not_owned_by_alist);
      debug_get_tags_list(&tlist, tags);
      is(tlist.size(), nb, "Parse simple tags back");

      POOL_MEM tmp;
      debug_get_tags(tmp.handle(), tags);
      is(tmp.c_str(), format, "Check tags format");
}

int main(int argc, char **argv)
{
   int64_t tags=0;
   Unittests alist_test("debug-tags");
   log("Test debug tags feature");

   ok(debug_parse_tags("network,snapshot", &tags), "Parse simple tags");
   is(tags, DT_NETWORK|DT_SNAPSHOT, "Check simple parsing");
   test_list(tags, 2, "network,snapshot");

   ok(debug_parse_tags("network,!snapshot", &tags), "Parse tag deletion");
   is(tags, DT_NETWORK, "Check tag deletion");
   test_list(tags, 1, "network");
   return report();
}
