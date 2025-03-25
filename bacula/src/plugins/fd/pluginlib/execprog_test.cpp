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
 * @file execprog_test.cpp
 * @author Radosław Korzeniewski (radoslaw@korzeniewski.net)
 * @brief This is a Bacula plugin external command execution context - unittest.
 * @version 1.2.0
 * @date 2020-01-05
 *
 * @copyright Copyright (c) 2021 All rights reserved. IP transferred to Bacula Systems according to agreement.
 */

#include "bacula.h"
#include "unittests.h"
#include "execprog.h"

bFuncs *bfuncs;
bInfo *binfo;

int main()
{
   Unittests iso8601_test("execprog_test");
   EXECPROG execprog;

   nok(execprog.is_open(), "default is_open()");
   ok(execprog.is_closed(), "default is_closed()");
   nok(execprog.is_error(), "default is_error()");
   nok(execprog.is_fatal(), "default is_fatal()");
   nok(execprog.is_abort_on_error(), "default is_abort_on_error()");
   nok(execprog.is_eod(), "default is_eod()");
   nok(execprog.get_cmd_pid() > 0, "default get_cmd_pid()");

   ok(execprog.execute_command(NULL, "ls -l /etc/passwd"), "execprog()");
   POOL_MEM out(PM_MESSAGE);
   int rc = execprog.read_output(NULL, out);
   // Pmsg1(0, "out: %s\n", out.c_str());
   ok(rc > 0, "read_output()");
   ok(execprog.get_cmd_pid() > 0, "get_cmd_pid()");

   execprog.terminate(NULL);
   return report();
}
