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

char *run_cmd(char *command, POOLMEM **out)
{
   **out=0;
   if (run_program_full_output(command, 0, *out) != 0) {
      berrno be;
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

void *th1(void *arg)
{
   const char *ret;
   POOLMEM *q = get_pool_memory(PM_FNAME);
   run_cmd((char *)"echo toto", &q);
   if (strcmp(q, "toto") == 0) {
      ret = "ok";
   } else {
      ret = "not ok";
   }
   free_pool_memory(q);
   return (void *)ret;
}

int main(int argc, char **argv)
{
   pthread_t ids[1000];
   void *ret;
   Unittests t("bpipe_test", true, true);
   debug_level = 400;
   //t.configure(TEST_QUIET);   
   init_stack_dump();
   my_name_is(argc, argv, "bpipe-test");
   init_msg(NULL, NULL);
   daemon_start_time = time(NULL);
   setup_daemon_message_queue();

   start_watchdog();
   for (int i = 0 ; i < 990 ; i++) {
      pthread_create(&(ids[i]), NULL, th1, NULL);
      pthread_create(&(ids[i+1]), NULL, th1, NULL);
      pthread_create(&(ids[i+2]), NULL, th1, NULL);
      pthread_join(ids[i], &ret);
      ok(strcmp((char *)ret, "ok") == 0, "Checking thread output");
      pthread_join(ids[++i], &ret);
      ok(strcmp((char *)ret, "ok") == 0, "Checking thread output");
      pthread_join(ids[++i], &ret);
      ok(strcmp((char *)ret, "ok") == 0, "Checking thread output");
   }

   FILE *fp = fopen("tmp/a.pl", "w");
   if (fp) {
      fprintf(fp, "#!/usr/bin/perl -w\n"
              "use strict;\n"
              "sleep 1;\n"
              "print STDERR \"error\\n\";"
              "print \"ok\\n\";\n");
      fclose(fp);
      chmod("tmp/a.pl", 0700);

      char buf[512];
      BPIPE *p = open_bpipe((char *)"./tmp/a.pl", 0, "re");
      close_epipe(p);
      fgets(buf, sizeof(buf), p->rfd);
      int ret = close_bpipe(p);
      isnt(ret, 0, "checking bpipe output status (re)");
      is(buf, "ok\n", "checking bpipe output string (re)");

      p = open_bpipe((char *)"./tmp/a.pl", 0, "rE");
      fgets(buf, sizeof(buf), p->rfd);
      ret = close_bpipe(p);
      is(ret, 0, "checking bpipe output status (rE)");
      is(buf, "ok\n", "checking bpipe output string (rE)");
   } else {
      ok(0, "Unable to open tmp/a.sh for tests");
   }
   free_daemon_message_queue();
   stop_watchdog();
   term_msg();
   close_memory_pool();
   return report();
}
