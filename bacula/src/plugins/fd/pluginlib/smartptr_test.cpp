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
 * @file smartptr_test.cpp
 * @author Radosław Korzeniewski (radoslaw@korzeniewski.net)
 * @brief Common definitions and utility functions for Inteos plugins - unittest.
 * @version 1.2.0
 * @date 2021-04-23
 *
 * @copyright Copyright (c) 2021 All rights reserved. IP transferred to Bacula Systems according to agreement.
 */

#include "pluginlib.h"
#include "smartptr.h"
#include "unittests.h"

bFuncs *bfuncs;
bInfo *binfo;

static int referencenumber = 0;
static int referencenumber2 = 0;

struct testclass
{
   testclass() { referencenumber++; }
   ~testclass() { referencenumber--; }
};

int main()
{
   Unittests pluglib_test("smartalist_test");

   // Pmsg0(0, "Initialize tests ...\n");

   referencenumber = 0;    // ensure it is zero
   {
      smart_ptr<testclass> ptr;
      ok(ptr.get() == NULL, "check default constructor");
      ok(referencenumber == 0, "check refnr");
   }

   referencenumber = 0;    // ensure it is zero
   {
      smart_ptr<testclass> ptr(new testclass);
      ok(referencenumber == 1, "check smart allocation");
   }

   ok(referencenumber == 0, "check smart free");

   referencenumber = 0;    // ensure it is zero
   referencenumber2 = 0;   // ensure it is zero
   {
      smart_ptr<testclass> ptr(new testclass);
      ok(referencenumber == 1, "check first allocation");
      testclass * other = new testclass;
      ok(referencenumber == 2, "check other allocation");
      ptr = other;
      ok(referencenumber == 1, "check assign operator");
   }

   ok(referencenumber == 0, "check smart free");

   return report();
}
