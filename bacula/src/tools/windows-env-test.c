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

#include "bacula.h"
#include "lib/unittests.h"


char *run_cmd(char *command, POOLMEM **out, char *env[])
{
   **out=0;
   if (run_program_full_output(command, 0, *out, env) != 0) {
      berrno be;
      Dmsg1(0, "run_program_full_output done. %s", *out);
      Jmsg(NULL, M_ERROR, 0, _("Can't run command %s. ERR=%s\n"),
           command, be.bstrerror(errno));
      if (**out) {
         Dmsg1(0, "%s", *out);
      }
      return NULL;
   }

   strip_trailing_junk(*out);

   return *out;
}

void *test1(char *env[])
{
   const char *ret;
   POOLMEM *q = get_pool_memory(PM_FNAME);

   run_cmd((char *)"set HY_ENV_VM_NAME", &q, env);
   Dmsg1(0, "test1 returns %s\n", q);
   if (strcmp(q, "HY_ENV_VM_NAME=NULL") == 0) {
      ret = "ok";
   } else {
      ret = "not ok";
   }
   free_pool_memory(q);
   return (void *)ret;
}


void *test2(char *env[])
{
   const char *ret;
   POOLMEM *q = get_pool_memory(PM_FNAME);
   
   run_cmd((char *)"set HY_ENV_JOB_NAME", &q, env);
   Dmsg1(0, "test2 returns %s\n", q);
   if (strcmp(q, "HY_ENV_JOB_NAME=hyperv-backup.2022-02-18_13.42.54_15") == 0) {
      ret = "ok";
   } else {
      ret = "not ok";
   }
   free_pool_memory(q);
   return (void *)ret;
}

void *test3(char *env[])
{
   const char *ret;
   POOLMEM *q = get_pool_memory(PM_FNAME);
   
   run_cmd((char *)"set HY_ENV_REF_JOB_NAME", &q, env);
   Dmsg1(0, "test3 returns %s\n", q);
   if (strcmp(q, "HY_ENV_REF_JOB_NAME=hyperv-backup.2022-02-17_17.58.09_21") == 0) {
      ret = "ok";
   } else {
      ret = "not ok";
   }
   free_pool_memory(q);
   return (void *)ret;
}

int main(int argc, char **argv)
{
   void *ret;
   Unittests t("windows_env_test", true, true);
   t.configure(TEST_QUIET);   
   init_stack_dump();
   my_name_is(argc, argv, "windows_env_test");
   init_msg(NULL, NULL);
   daemon_start_time = time(NULL);
   setup_daemon_message_queue();

   start_watchdog();

   debug_level = 500;

   char *_env[4];

   POOLMEM *env_item1 = get_pool_memory(PM_FNAME);
   Mmsg(env_item1, "HY_ENV_VM_NAME=NULL");
   _env[0] = env_item1;

   POOLMEM *env_item2 = get_pool_memory(PM_FNAME);
   Mmsg(env_item2, "HY_ENV_JOB_NAME=hyperv-backup.2022-02-18_13.42.54_15");
   _env[1] = env_item2;

   POOLMEM *env_item3 = get_pool_memory(PM_FNAME);
   Mmsg(env_item3, "HY_ENV_REF_JOB_NAME=hyperv-backup.2022-02-17_17.58.09_21");
   _env[2] = env_item3;

   _env[3] = NULL;

   ret = test1(_env);
   Dmsg1(0, "test1. ret = %s\n", (char *)ret);
   ok(strcmp((char *)ret, "ok") == 0, "Checking test1");

   ret = test2(_env);
   ok(strcmp((char *)ret, "ok") == 0, "Checking test2");

   ret = test3(_env);
   ok(strcmp((char *)ret, "ok") == 0, "Checking test3");

   free_pool_memory(env_item3);   
   free_pool_memory(env_item2);
   free_pool_memory(env_item1);

   free_daemon_message_queue();
   stop_watchdog();
   term_msg();
   close_memory_pool();
   return report();
}