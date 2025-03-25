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
 * @file commctx_test.cpp
 * @author Radosław Korzeniewski (radoslaw@korzeniewski.net)
 * @brief A Bacula plugin command context switcher template - unittest.
 * @version 1.1.0
 * @date 2020-12-23
 *
 * @copyright Copyright (c) 2020 All rights reserved. IP transferred to Bacula Systems according to agreement.
 */

#include "pluginlib.h"
#include "commctx.h"
#include "unittests.h"

bFuncs *bfuncs;
bInfo *binfo;

static int referencenumber = 0;
static int dosomethingvariable = 0;

struct testctx : public SMARTALLOC
{
   const char * cmd;
   testctx(const char *command) : cmd(command) { referencenumber++; };
   ~testctx() { referencenumber--; };
   bool meth() { return true; }
};

void do_something(testctx*, void*data)
{
   dosomethingvariable++;

   if (data != NULL)
   {
      int *var = (int *)data;
      dosomethingvariable += *var;
   }
}

bRC do_status(testctx*, void*data)
{
   if (data != NULL)
   {
      return bRC_OK;
   }

   return bRC_Error;
}

int main()
{
   Unittests commctx_test("commctx_test");

   // Pmsg0(0, "Initialize tests ...\n");

#define TEST1     "TEST1"
#define TEST2     "TEST2"

   {
      COMMCTX<testctx> ctx;

      nok(ctx.check_command(TEST1), "test empty ctx list");
      ok(referencenumber == 0, "check no allocation yet");

      auto testctx1 = ctx.switch_command(TEST1);
      ok(testctx1 != nullptr, "test first command");
      ok(referencenumber == 1, "check allocation");

      auto testctx2 = ctx.switch_command(TEST2);
      ok(testctx2 != nullptr, "test next command");
      ok(referencenumber == 2, "check allocation");

      auto currentctx = ctx.switch_command(TEST1);
      ok(currentctx != nullptr, "test switch command");
      ok(currentctx == testctx1, "test switch command to proper");
      ok(referencenumber == 2, "check allocation");

      ok(ctx.check_command(TEST2), "test check command");
   }

   ok(referencenumber == 0, "check smart free");

   {
      COMMCTX<testctx> ctx;

      auto testctx1 = ctx.switch_command(TEST1);
      ok(testctx1 != nullptr, "test switch command1");
      ok(referencenumber == 1, "check ref allocation1");

      auto testctx2 = ctx.switch_command(TEST2);
      ok(testctx2 != nullptr, "test switch command2");
      ok(referencenumber == 2, "check allocation2");

      int append = 2;
      ctx.foreach_command(do_something, &append);
      ok(dosomethingvariable == 6, "dosomethingvariable");

      dosomethingvariable = 0;
      ctx.foreach_command(do_something, NULL);
      ok(dosomethingvariable == 2, "do_something with NULL");

      auto status = ctx.foreach_command_status(do_status, &append);
      ok(status != bRC_Error, "do_status");

      status = ctx.foreach_command_status(do_status, NULL);
      ok(status == bRC_Error, "do_status with NULL");
   }

   {
      COMMCTX<testctx> ctx;
      auto testctx1 = ctx.switch_command(TEST1);
      ok(testctx1 != nullptr, "test switch command1");
      auto cmd1 = ctx->cmd;
      ok(strcmp(cmd1, TEST1) == 0, "test arrow operator variable");
      auto testctx2 = ctx.switch_command(TEST2);
      ok(testctx2 != nullptr, "test switch command2");
      auto cmd2 = ctx->meth();
      ok(cmd2, "test arrow operator method");
   }

   return report();
}
