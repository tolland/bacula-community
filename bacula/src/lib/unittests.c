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
 * Support routines for Unit Tests.
 */

/*
 * This is an example how to use unittests in your code:

int main()
{
   int rc;

   prolog("app_test");

   ( ... your test application goes here ...)

   rc = report();
   epilog();
   return rc;
}

 * and a C++ approach for any C++ geeks:

int main()
{
   Unittests tests("app_test");

   ( ... your test application goes here ...)

   return report();
}

 */

#include <stdio.h>
#include "bacula.h"
#include "unittests.h"

static int expected=-1;
static int err=0;
static int nb=0;
static bool lmgrinit = false;
static bool quiet = false;
static bool print_var = false;

Unittests::Unittests(const char *name, bool lmgr/*=false*/, bool motd/*=true*/)
{
   if (getenv("UNITTEST_PRINT_VAR")) {
      print_var = true;
   }
   if (getenv("UNITTEST_TEST_QUIET")) {
      quiet = true;
   }
   prolog(name, lmgr, motd);
};

/* Configure the current test with TEST_XXX flags */
void configure_test(uint64_t options)
{
   if (options & TEST_QUIET) {
      quiet = true;
   }
   if (options & TEST_PRINT_LOCAL) {
      print_var = true;
   }
}

/* Set the number of expected tests */
void unittest_set_nb_tests(int nb)
{
   expected = nb;
}

/* Get the total number of tests */
int unittest_get_nb_tests()
{
   return nb;
}

/* Get the total number of tests in error */
int unittest_get_nb_errors()
{
   return err;
}

/*
 * Test success if value is not zero.
 */
bool _ok(const char *file, int l, const char *op, int value, const char *label)
{
   nb++;
   if (!value) {
      if (err < 1000) {
         Pmsg4(-1, "ERR %.80s %s:%i on %s\n", label, file, l, op);
      } else if (err == 1000) {
         Pmsg0(-1, "ERR Too much errors\n");
      }
      if (print_var) {
         gdb_print_local(1); // make sure this line is not the last operation before the return
      }
      err++; // keep this line AFTER the gdb_print_local() to avoid "tail call" optimization
   } else if (!quiet) {
      Pmsg1(-1, "OK  %.80s\n", label);
   }
   return value;
}

/*
 * Test success if value is zero.
 */
bool _nok(const char *file, int l, const char *op, int value, const char *label)
{
   nb++;
   if (value) {
      err++;
      if (err < 1000) {
         Pmsg4(-1, "ERR %.80s %s:%i on !%s\n", label, file, l, op);
      } else if (err == 1000) {
         Pmsg0(-1, "ERR Too much errors\n");
      }
      if (print_var) {
         gdb_print_local(1);
      }
   } else if (!quiet) {
      Pmsg1(-1, "OK  %.80s\n", label);
   }
   return !value;
}

/*
 * Test success if value is different
 */
bool _is(const char *file, int l, const char *op, const char *str, const char *str2, const char *label)
{
   nb++;
   bool value = (strcmp(NPRTB(str), NPRTB(str2)) == 0);
   if (!value) {
      err++;
      if (err < 1000) {
         Pmsg6(-1, "ERR %.80s %s:%i on %s [%s] == [%s]\n", label, file, l, op, str, str2);
      } else if (err == 1000) {
         Pmsg0(-1, "ERR Too much errors\n");
      }
      if (print_var) {
         gdb_print_local(1);
      }
   } else if (!quiet) {
      Pmsg1(-1, "OK  %.80s\n", label);
   }
   return value;
}

/*
 * Test success if value is different
 */
bool _is(const char *file, int l, const char *op, int64_t v, int64_t v2, const char *label)
{
   nb++;
   bool value = (v == v2);
   if (!value) {
      err++;
      if (err < 1000) {
         Pmsg6(-1, "ERR %.80s %s:%i on %s %lld == %lld\n", label, file, l, op, v, v2);
      } else if (err == 1000) {
         Pmsg0(-1, "ERR Too much errors\n");
      }
      if (print_var) {
         gdb_print_local(1);
      }
   } else if (!quiet) {
      Pmsg1(-1, "OK  %.80s\n", label);
   }
   return value;
}

/*
 * Test success if value is different
 */
bool _isnt(const char *file, int l, const char *op, const char *str, const char *str2, const char *label)
{
   nb++;
   bool value = (strcmp(str, str2) != 0);
   if (!value) {
      err++;
      if (err < 1000) {
         Pmsg6(-1, "ERR %.80s %s:%i on %s %s == %s\n", label, file, l, op, str, str2);
      } else if (err == 1000) {
         Pmsg0(-1, "ERR Too much errors\n");
      }
      if (print_var) {
         gdb_print_local(1);
      }
   } else if (!quiet) {
      Pmsg1(-1, "OK  %.80s\n", label);
   }
   return value;
}

/*
 * Test success if value is different
 */
bool _isnt(const char *file, int l, const char *op, int64_t v, int64_t v2, const char *label)
{
   nb++;
   bool value = (v != v2);
   if (!value) {
      err++;
      if (err < 1000) {
         Pmsg6(-1, "ERR %.80s %s:%i on %s == %lld\n", label, file, l, op, v, v2);
      } else if (err == 1000) {
         Pmsg0(-1, "ERR Too much errors\n");
      }
      if (print_var) {
         gdb_print_local(1);
      }
   } else if (!quiet) {
      Pmsg1(-1, "OK  %.80s\n", label);
   }
   return value;
}

/*
 * Short report of successful/all tests.
 */
int report()
{
   /* We do not count the extra expected check in the display */
   int nb_ok = nb - err;
   int nb_total = nb;
   Pmsg0(-1, "\n==== Report ====\n");
   /* Do an extra check if the expected variable is set */
   if (expected > 0) {
      is(nb, expected, "Checking expected tests number");
   }
   Pmsg3(-1, "Result %i/%i OK%s\n", nb_ok, nb_total, (err > 0) ? "!!!":"");
   return err > 0;
}

void terminate(int sig) {};

/*
 * Initializes the application env, including lockmanager.
 */
void prolog(const char *name, bool lmgr, bool motd)
{
   if (motd) {
      Pmsg1(-1, "==== Starting %s ... ====\n", name);
   }
   my_name_is(0, NULL, name);
   init_signals(terminate);

#ifdef HAVE_WIN32
   InitWinAPIWrapper();
   WSA_Init();
#endif

   init_stack_dump();

   if (lmgr){
      lmgr_init_thread();     /* initialize the lockmanager stack */
      lmgrinit = true;
   }
};

/*
 * Finish the application, shows report about memory leakage and terminates the lockmanager.
 */
void epilog()
{
   Pmsg0(-1, "\n");
   stop_watchdog();
   if (lmgrinit) {
      lmgr_cleanup_main();
   }
   close_memory_pool();
   sm_dump(false);
   Pmsg0(-1, "==== Finish ====\n");
};

bool _stat_ok(const char *file, int l, const char *fpath)
{
   struct stat statbuf;
   return _ok(file, l, "stat_ok", stat(fpath, &statbuf) == 0, fpath);
}

bool _stat_nok(const char *file, int l, const char *fpath)
{
   struct stat statbuf;
   return _ok(file, l, "stat_nok", stat(fpath, &statbuf) != 0, fpath);
}

void fsu_rmdir(const char *dirPath)
{
#ifndef HAVE_WIN32
   POOLMEM *syscmd = get_pool_memory(PM_MESSAGE);
   Mmsg(syscmd, "rm -r \"%s\"", dirPath);
   int rc = system(syscmd);
   if (rc < 0) {
      printf("ERROR : Could not remove Directory: %s\n", dirPath);
      exit(-1);
   }
   free_and_null_pool_memory(syscmd);
#else
   POOLMEM *syscmd = get_pool_memory(PM_MESSAGE);
   Mmsg(syscmd, "rd /s /q \"%s\"", dirPath);
   int rc = system(syscmd);
   if (rc < 0) {
      printf("ERROR : Could not remove Directory: %s\n", dirPath);
      exit(-1);
   }
   free_and_null_pool_memory(syscmd);
#endif
}

void fsu_rmfile(const char *fpath)
{
   int rc = unlink(fpath);

   if (rc == -1) {
      switch(errno) {
         case EACCES:
            printf("(Unlink %s) Permission Denied\n", fpath);
            break;
         case ENOENT:
            printf("(Unlink %s) File not found\n", fpath);
            break;
         case EBUSY:
            printf("(Unlink %s) File busy\n", fpath);
            break;
         default:
            printf("(Unlink %s) Default Error\n", fpath);
            break;
      }

      exit(-1);
   }
}

void fsu_touch(const char *fpath)
{
#ifndef HAVE_WIN32
   POOLMEM *syscmd = get_pool_memory(PM_MESSAGE);
   Mmsg(syscmd, "touch \"%s\"", fpath);
   int rc = system(syscmd);
   if (rc < 0) {
      printf("ERROR : Could not touch file: %s\n", fpath);
      exit(-1);
   }
   free_and_null_pool_memory(syscmd);
#else
   POOLMEM *syscmd = get_pool_memory(PM_MESSAGE);
   Mmsg(syscmd, "echo.> \"%s\"", fpath);
   int rc = system(syscmd);
   if (rc < 0) {
      printf("ERROR : Could not touch file: %s\n", fpath);
      exit(-1);
   }
   free_and_null_pool_memory(syscmd);
#endif
}

void fsu_mvfile(char *src, char *dst)
{
#ifndef HAVE_WIN32
   POOLMEM *syscmd = get_pool_memory(PM_MESSAGE);
   Mmsg(syscmd, "mv \"%s\" \"%s\"", src, dst);
   int rc = system(syscmd);
   if (rc < 0) {
      printf("ERROR : Could not move file %s into %s. RC = %d\n", src, dst, rc);
      exit(-1);
   }
   free_and_null_pool_memory(syscmd);
#else
   char *win_src = bstrdup(src);
   unix_to_win_path(win_src);
   char *win_dst = bstrdup(dst);
   unix_to_win_path(win_dst);
   POOLMEM *syscmd = get_pool_memory(PM_MESSAGE);
   Mmsg(syscmd, "move \"%s\" \"%s\"", win_src, win_dst);
   int rc = system(syscmd);
   if (rc < 0) {
      printf("ERROR : Could not move file %s into %s. RC = %d\n", win_src, win_dst, rc);
      exit(-1);
   }
   free_and_null_pool_memory(syscmd);
   free(win_src);
   free(win_dst);
#endif
}

void fsu_cpfile(char *src, char *dst)
{
#ifndef HAVE_WIN32
   POOLMEM *syscmd = get_pool_memory(PM_MESSAGE);
   Mmsg(syscmd, "cp \"%s\" \"%s\"", src, dst);
   int rc = system(syscmd);
   if (rc < 0) {
      printf("ERROR : Could not move file %s into %s. RC = %d\n", src, dst, rc);
      exit(-1);
   }
   free_and_null_pool_memory(syscmd);
#else
   char *win_src = bstrdup(src);
   unix_to_win_path(win_src);
   char *win_dst = bstrdup(dst);
   unix_to_win_path(win_dst);
   POOLMEM *syscmd = get_pool_memory(PM_MESSAGE);
   Mmsg(syscmd, "copy \"%s\" \"%s\"", win_src, win_dst);
   int rc = system(syscmd);
   if (rc < 0) {
      printf("ERROR : Could not move file %s into %s. RC = %d\n", win_src, win_dst, rc);
      exit(-1);
   }
   free_and_null_pool_memory(syscmd);
#endif
}

void fsu_mkdir(const char *path)
{
   if (mkdir(path, 0777) != 0) {
      printf("ERROR : Could not create Directory: %s\n", path);
      exit(-1);
   }
}

void fsu_mkpath(const char *newPath)
{
#ifndef HAVE_WIN32
   POOLMEM *syscmd = get_pool_memory(PM_MESSAGE);
   Mmsg(syscmd, "mkdir -p \"%s\"", newPath);
   int rc = system(syscmd);
   if (rc < 0) {
      printf("ERROR : Could not create path %s. Error: %d\n",
            newPath, rc);
      exit(-1);
   }
   free_and_null_pool_memory(syscmd);
#else
   POOLMEM *fullPath = get_pool_memory(PM_MESSAGE);
   Mmsg(fullPath, "%s", newPath);
   int rc = _mkdir(fullPath);
   if (rc < 0) {
      printf("ERROR : Could not create path %s. Error: %s\n",
            newPath, strerror(errno));
      exit(-1);
   }
   free_and_null_pool_memory(fullPath);
#endif
}

void fsu_mkfile(const char *fpath, const char *fcontents)
{
   FILE *fp = fopen(fpath, "wb");

   if (!fp) {
      printf("ERROR : Could not open File: %s\n", fpath);
      exit(-1);
   }

   if (fprintf(fp, "%s", fcontents) < 0) {
      printf("ERROR : Could not write File: %s\n", fpath);
      exit(-1);
   }

   fclose(fp);
}

void fsu_mkfile(const char *fpath)
{
   fsu_mkfile(fpath, fpath);
}

void unix_to_win_path(char *path)
{
   for (int i = 0; path[i] != '\0'; i++) {
      if (path[i] == '/') {
         path[i] = '\\';
      }
   }
   return;
}
