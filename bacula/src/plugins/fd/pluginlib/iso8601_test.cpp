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
 * @file iso8601_test.cpp
 * @author Radosław Korzeniewski (radoslaw@korzeniewski.net)
 * @brief This is a Bacula plugin ISO8601 parsing library - unittest.
 * @version 1.2.0
 * @date 2020-01-05
 *
 * @copyright Copyright (c) 2021 All rights reserved. IP transferred to Bacula Systems according to agreement.
 */

#include "bacula.h"
#include "unittests.h"
#include "iso8601.h"

struct testvect
{
   const char *teststr;
   const utime_t test_time;
};

static testvect tests[] =
{
   {"20000101T23:01:02Z", 946767662},
   {"20200514T08:35:43Z", 1589445343},
   {"2020-05-14T08:35:43Z", 1589445343},
   {"20200514T07:27:34Z", 1589441254},
   {"2020-05-14T07:27:34Z", 1589441254},
   {"2021-01-13T09:44:38Z", 1610531078},
   // {"20210113T09:44:38 +400", 1610531078},
   // {"2021-01-13T09:44:38+400", 1610531078},
   {NULL, 0},
};

int main()
{
   Unittests iso8601_test("iso8601_test");

   ISO8601DateTime dt;


   const char *teststr = tests[0].teststr;
   utime_t test_time = tests[0].test_time;
   for (int a = 1; teststr != NULL; a++)
   {
      utime_t t = dt.parse_data(teststr);
      char test_descr[64];
      snprintf(test_descr, 64, "Test %d", a);
      ok(t == test_time, test_descr);
      teststr = tests[a].teststr;
      test_time = tests[a].test_time;
   }

   return report();
}
