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
 * @file smartalist_test.cpp
 * @author Radosław Korzeniewski (radoslaw@korzeniewski.net)
 * @brief Common definitions and utility functions for Inteos plugins - unittest.
 * @version 1.1.0
 * @date 2020-12-23
 *
 * @copyright Copyright (c) 2020 All rights reserved. IP transferred to Bacula Systems according to agreement.
 */

#include "pluginlib.h"
#include "smartalist.h"
#include "unittests.h"
#include <utility>

bFuncs *bfuncs;
bInfo *binfo;

static int referencenumber = 0;

struct testclass : public SMARTALLOC
{
   testclass() { referencenumber++; };
   ~testclass() { referencenumber--; };
};

struct testvalue : public SMARTALLOC
{
   int value;
   testvalue(const testvalue *other) { value = other->value; }
   testvalue() : value(0) {}
};

int main()
{
   Unittests pluglib_test("smartalist_test");
   testclass *ti;

   // Pmsg0(0, "Initialize tests ...\n");

   {
      smart_alist<testclass> list;
      ti = New(testclass);
      list.append(ti);
      ok(referencenumber == 1, "check first insert");
   }

   ok(referencenumber == 0, "check first smart free");

   constexpr const int refs = 100;
   referencenumber = 0;

   {
      smart_alist<testclass> list;
      for (int a = 0; a < refs; a++)
      {
         ti = New(testclass);
         list.append(ti);
      }
      ok(referencenumber == refs, "check bulk inserts");
   }

   ok(referencenumber == 0, "check smart free");

   {
      smart_alist<testvalue> list;
      testvalue *ti = New(testvalue);
      ti->value = 0xfff1;
      list.append(ti);

      smart_alist<testvalue> copylist;

      copylist = list;

      testvalue *tv = (testvalue*)copylist.first();
      rok(tv != NULL, "test first value");
      ok(tv->value == ti->value, "test copy value");
      ok(tv != ti, "test copy pointer");
   }

// #if __cplusplus >= 201104
//    {
//       smart_alist<testvalue> list;
//       // fill it with data
//       for (int i = 0; i < 10;i++)
//       {
//          testvalue *ti = New(testvalue);
//          ti->value = 100 + i;
//          list.append(ti);
//       }

//       ok(list.size() == 10, "test prefill data size");
//    }
// #endif

   return report();
}
