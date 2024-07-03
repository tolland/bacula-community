/*
   Bacula(R) - The Network Backup Solution

   Copyright (C) 2000-2023 Kern Sibbald

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
 *   bpipe.c bi-directional pipe
 *
 *    Kern Sibbald, November MMII
 *
 */


#include "bacula.h"
#include "jcr.h"

int execvp_errors[] = {
        EACCES,
        ENOEXEC,
        EFAULT,
        EINTR,
        E2BIG,
        ENAMETOOLONG,
        ENOMEM,
#ifndef HAVE_WIN32
        ETXTBSY,
#endif
        ENOENT
};
int num_execvp_errors = (int)(sizeof(execvp_errors)/sizeof(int));


#define MAX_ARGV 100

#define MODE_READ 1
#define MODE_WRITE 2
#define MODE_SHELL 4
#define MODE_STDERR 8
#define MODE_NOSTDERR 16

#if !defined(HAVE_WIN32)
static void build_argc_argv(char *cmd, int *bargc, char *bargv[], int max_arg);

void build_sh_argc_argv(char *cmd, int *bargc, char *bargv[], int max_arg)
{
   bargv[0] = (char *)"/bin/sh";
   bargv[1] = (char *)"-c";
   bargv[2] = cmd;
   bargv[3] = NULL;
   *bargc = 3;
}

int forbidden_chars[] = {
   '$', '!', ';', '\\', '&', '|', '<', '>', '`', '(', ')'
};

static bool check_for_forbidden_chars(const char *str)
{
   bool ret = true;
   int size = sizeof(forbidden_chars) / sizeof(int);
   for (int i=0; i<size; i++) {
      if (strchr(str, forbidden_chars[i]) != NULL) {
         ret = false;
         break;
      }
   }

   return ret;
}

/*
 * Run an external program. Optionally wait a specified number
 *   of seconds. Program killed if wait exceeded. We open
 *   a bi-directional pipe so that the user can read from and
 *   write to the program.
 */
BPIPE *open_bpipe(char *prog, int wait, const char *mode, char *envp[])
{
   char *bargv[MAX_ARGV];
   int bargc, i;
   int readp[2], writep[2], errp[2];
   POOLMEM *tprog;
   int mode_map = 0;
   BPIPE *bpipe;
   JCR *jcr;
   int save_errno;
   bool allowed = false;

   if (!prog || !*prog) {
      /* execve(3) A component of the file does not name an existing file or file is an empty string. */
      errno = ENOENT; 
      return NULL;
   }

   /* Get JCR from the thread context.
    * It's needed for per-daemon specific data, e.g. list of allowed scripts/programs directories. */
   jcr = get_jcr_from_tsd();
   if (!jcr) {
      allowed = true; /* Allow everything */
   } else if (jcr->allowed_script_dirs) {
      /* JCR has some allowed script directories set, so we need to check if command matches it */
      char *dir, *p;
      foreach_alist(dir, jcr->allowed_script_dirs) {
         if ((p = b_path_match(prog, dir)) == prog) {
            /* Path is ok, now check if program contains forbidden characters */
            //TODO too many ifs, refactor that
            if (!check_for_forbidden_chars(prog)) {
               errno = berr_not_allowed_char;
               allowed = false;
               break;
            } else {
               allowed = true;
            }
            break;
         }
      }

      if (!allowed && errno != berr_not_allowed_char) {
         errno = berr_not_allowed_path;
      }

   } else {
      /* Nothing specified, so we should allow to run scripts from every dir provided */
      allowed = true;
   }

   /* Check if script/program can be executed */
   if (!allowed) {
      return NULL;
   }

   bpipe = (BPIPE *)malloc(sizeof(BPIPE));
   memset(bpipe, 0, sizeof(BPIPE));
   if (strchr(mode,'r')) mode_map|=MODE_READ;
   if (strchr(mode,'w')) mode_map|=MODE_WRITE;
   if (strchr(mode,'s')) mode_map|=MODE_SHELL;
   if (strchr(mode,'e')) mode_map|=MODE_STDERR;
   if (strchr(mode,'E')) mode_map|=MODE_NOSTDERR;

   /* Build arguments for running program. */
   tprog = get_pool_memory(PM_FNAME);
   pm_strcpy(tprog, prog);
   if (mode_map & MODE_SHELL) {
      build_sh_argc_argv(tprog, &bargc, bargv, MAX_ARGV);
   } else {
      build_argc_argv(tprog, &bargc, bargv, MAX_ARGV);
   }

   /* Unable to parse the command, avoid segfault after the fork() */
   if (bargc == 0 || bargv[0] == NULL) {
      free_pool_memory(tprog);
      free(bpipe);
      /* execve(3) A component of the file does not name an existing file or file is an empty string. */
      errno = ENOENT;
      return NULL;
   }

#ifdef  xxxxxx
   printf("argc=%d\n", bargc);
   for (i=0; i<bargc; i++) {
      printf("argc=%d argv=%s:\n", i, bargv[i]);
   }
#endif

   /* Each pipe is one way, write one end, read the other, so we need two */
   if ((mode_map & MODE_WRITE) && pipe(writep) == -1) {
      save_errno = errno;
      free(bpipe);
      free_pool_memory(tprog);
      errno = save_errno;
      return NULL;
   }
   if ((mode_map & MODE_READ) && pipe(readp) == -1) {
      save_errno = errno;
      if (mode_map & MODE_WRITE) {
         close(writep[0]);
         close(writep[1]);
      }
      free(bpipe);
      free_pool_memory(tprog);
      errno = save_errno;
      return NULL;
   }
   if ((mode_map & MODE_STDERR) && pipe(errp) == -1) {
      save_errno = errno;
      if (mode_map & MODE_WRITE) {
         close(writep[0]);
         close(writep[1]);
      }
      if (mode_map & MODE_READ) {
         close(readp[0]);
         close(readp[1]);
      }
      free(bpipe);
      free_pool_memory(tprog);
      errno = save_errno;
      return NULL;
   }

   /* Start worker process */
   switch (bpipe->worker_pid = fork()) {
   case -1:                           /* error */
      save_errno = errno;
      if (mode_map & MODE_WRITE) {
         close(writep[0]);
         close(writep[1]);
      }
      if (mode_map & MODE_READ) {
         close(readp[0]);
         close(readp[1]);
      }
      if (mode_map & MODE_STDERR) {
         close(errp[0]);
         close(errp[1]);
      }
      free(bpipe);
      free_pool_memory(tprog);
      errno = save_errno;
      return NULL;

   case 0:                            /* child */
      if (mode_map & MODE_WRITE) {
         close(writep[1]);
         dup2(writep[0], 0);          /* Dup our write to his stdin */
	 close(writep[0]);
      }
      if (mode_map & MODE_READ) {
         close(readp[0]);             /* Close unused child fds */
         dup2(readp[1], 1);           /* dup our read to his stdout */
         if (mode_map & MODE_STDERR) {  /*   and handle stderr */
            close(errp[0]); 
            dup2(errp[1], 2);

         } else if (mode_map & MODE_NOSTDERR) {
            /* We open STDERR and map it to /dev/null directly */
            int fd = open("/dev/null", O_WRONLY);
            if (fd != 2) {
               dup2(fd, 2);
               close(fd);
            }
         } else {
            /* by default STDERR and STDOUT are merged */
            dup2(readp[1], 2);
         }
	 if (mode_map & MODE_READ) {
	    close(readp[1]);
	 }
	 if (mode_map & MODE_STDERR) {
	    close(errp[1]);
	 }
      }

      /* Close everything after STDERR. Should be pretty fast when the OS implements CLOEXEC */
      bclose_from(3);

      /* Setup the environment if requested, we do not use execvpe()
       * because it's not wildly available
       * TODO: Implement envp to windows version of bpipe
       */
      setup_env(envp);

      execvp(bargv[0], bargv);        /* call the program */
      /* Convert errno into an exit code for later analysis */
      for (i=0; i< num_execvp_errors; i++) {
         if (execvp_errors[i] == errno) {
            _exit(200 + i);            /* exit code => errno */
         }
      }
      /* Do not flush stdio */
      _exit(255);                      /* unknown errno */

   default:                           /* parent */
      break;
   }
   free_pool_memory(tprog);
   if (mode_map & MODE_READ) {
      close(readp[1]);                /* close unused parent fds */
      bpipe->rfd = fdopen(readp[0], "r"); /* open file descriptor */
   }
   if (mode_map & MODE_STDERR) {
      close(errp[1]);                /* close unused parent fds */
      bpipe->efd = fdopen(errp[0], "r"); /* open file descriptor */
   }
   if (mode_map & MODE_WRITE) {
      close(writep[0]);
      bpipe->wfd = fdopen(writep[1], "w");
   }
   bpipe->worker_stime = time(NULL);
   bpipe->wait = wait;
   if (wait > 0) {
      bpipe->timer_id = start_child_timer(NULL, bpipe->worker_pid, wait);
   }
   return bpipe;
}

/* Close the write pipe only
 * BE careful ! return 1 if ok */
int close_wpipe(BPIPE *bpipe)
{
   int stat = 1;

   if (bpipe->wfd) {
      fflush(bpipe->wfd);
      if (fclose(bpipe->wfd) != 0) {
         stat = 0;
      }
      bpipe->wfd = NULL;
   }
   return stat;
}

/* Close the stderror pipe only */
int close_epipe(BPIPE *bpipe)
{
   int stat = 1;

   if (bpipe->efd) {
      if (fclose(bpipe->efd) != 0) {
         stat = 0;
      }
      bpipe->efd = NULL;
   }
   return stat;
}

/*
 * Close both pipes and free resources
 *
 *  Returns: 0 on success
 *           berrno on failure
 */
int close_bpipe(BPIPE *bpipe)
{
   int chldstatus = 0;
   int stat = 0;
   int wait_option;
   int remaining_wait;
   pid_t wpid = 0;


   /* Close pipes */
   if (bpipe->rfd) {
      fclose(bpipe->rfd);
      bpipe->rfd = NULL;
   }
   if (bpipe->wfd) {
      fclose(bpipe->wfd);
      bpipe->wfd = NULL;
   }
   if (bpipe->efd) {
      fclose(bpipe->efd);
      bpipe->efd = NULL;
   }

   if (bpipe->wait == 0) {
      wait_option = 0;                /* wait indefinitely */
   } else {
      wait_option = WNOHANG;          /* don't hang */
   }
   remaining_wait = bpipe->wait;

   /* wait for worker child to exit */
   for ( ;; ) {
      Dmsg2(100, "Wait for %d opt=%d\n", bpipe->worker_pid, wait_option);
      do {
         wpid = waitpid(bpipe->worker_pid, &chldstatus, wait_option);
      } while (wpid == -1 && (errno == EINTR || errno == EAGAIN));
      if (wpid == bpipe->worker_pid || wpid == -1) {
         berrno be;
         stat = errno;
         Dmsg3(100, "Got break wpid=%d status=%d ERR=%s\n", wpid, chldstatus,
            wpid==-1?be.bstrerror():"none");
         break;
      }
      Dmsg3(100, "Got wpid=%d status=%d ERR=%s\n", wpid, chldstatus,
            wpid==-1?strerror(errno):"none");
      if (remaining_wait > 0) {
         bmicrosleep(1, 0);           /* wait one second */
         remaining_wait--;
      } else {
         stat = ETIME;                /* set error status */
         wpid = -1;
         break;                       /* don't wait any longer */
      }
   }
   if (wpid > 0) {
      if (WIFEXITED(chldstatus)) {    /* process exit()ed */
         stat = WEXITSTATUS(chldstatus);
         if (stat != 0) {
            Dmsg1(100, "Non-zero status %d returned from child.\n", stat);
            stat |= b_errno_exit;        /* exit status returned */
         }
         Dmsg1(100, "child status=%d\n", stat & ~b_errno_exit);
      } else if (WIFSIGNALED(chldstatus)) {  /* process died */
#ifndef HAVE_WIN32
         stat = WTERMSIG(chldstatus);
#else
         stat = 1;                    /* fake child status */
#endif
         Dmsg1(100, "Child died from signal %d\n", stat);
         stat |= b_errno_signal;      /* exit signal returned */
      }
   }
   if (bpipe->timer_id) {
      stop_child_timer(bpipe->timer_id);
   }
   free(bpipe);
   Dmsg2(100, "returning stat=%d,%d\n", stat & ~(b_errno_exit|b_errno_signal), stat);
   return stat;
}

/*
 * Build argc and argv from a string
 */
static void build_argc_argv(char *cmd, int *bargc, char *bargv[], int max_argv)
{
   int i;
   char *p, *q, quote;
   int argc = 0;

   argc = 0;
   for (i=0; i<max_argv; i++)
      bargv[i] = NULL;

   p = cmd;
   quote = 0;
   while  (*p && (*p == ' ' || *p == '\t'))
      p++;
   if (*p == '\"' || *p == '\'') {
      quote = *p;
      p++;
   }
   if (*p) {
      while (*p && argc < MAX_ARGV) {
         q = p;
         if (quote) {
            while (*q && *q != quote)
            q++;
            quote = 0;
         } else {
            while (*q && *q != ' ')
            q++;
         }
         if (*q)
            *(q++) = '\0';
         bargv[argc++] = p;
         p = q;
         while (*p && (*p == ' ' || *p == '\t'))
            p++;
         if (*p == '\"' || *p == '\'') {
            quote = *p;
            p++;
         }
      }
   }
   *bargc = argc;
}
#endif /* HAVE_WIN32 */

/*
 * Run an external program. Optionally wait a specified number
 *   of seconds. Program killed if wait exceeded. Optionally
 *   return the output from the program (normally a single line).
 *
 *   If the watchdog kills the program, fgets returns, and ferror is set
 *   to 1 (=>SUCCESS), so we check if the watchdog killed the program.
 *
 * Contrary to my normal calling conventions, this program
 *
 *  Returns: 0 on success
 *           non-zero on error == berrno status
 */
int run_program(char *prog, int wait, POOLMEM *&results)
{
   BPIPE *bpipe;
   int stat1, stat2;
   char *mode;

   mode = (char *)"r";
   bpipe = open_bpipe(prog, wait, mode);
   if (!bpipe) {
      return ENOENT;
   }
   results[0] = 0;
   int len = sizeof_pool_memory(results) - 1;
   fgets(results, len, bpipe->rfd);
   results[len] = 0;
   if (feof(bpipe->rfd)) {
      stat1 = 0;
   } else {
      stat1 = ferror(bpipe->rfd);
   }
   if (stat1 < 0) {
      berrno be;
      Dmsg2(100, "Run program fgets stat=%d ERR=%s\n", stat1, be.bstrerror(errno));
   } else if (stat1 != 0) {
      Dmsg1(100, "Run program fgets stat=%d\n", stat1);
      if (bpipe->timer_id) {
         Dmsg1(100, "Run program fgets killed=%d\n", bpipe->timer_id->killed);
         /* NB: I'm not sure it is really useful for run_program. Without the
          * following lines run_program would not detect if the program was killed
          * by the watchdog. */
         if (bpipe->timer_id->killed) {
            stat1 = ETIME;
            pm_strcpy(results, _("Program killed by Bacula (timeout)\n"));
         }
      }
   }
   stat2 = close_bpipe(bpipe);
   stat1 = stat2 != 0 ? stat2 : stat1;
   Dmsg1(100, "Run program returning %d\n", stat1);
   return stat1;
}

/*
 *   Duplicate of run_program_full_output, but handles errors.
 *   Run an external program. Optionally wait a specified number
 *   of seconds. Program killed if wait exceeded (it is done by the
 *   watchdog, as fgets is a blocking function).
 *
 *   If the watchdog kills the program, fgets returns, and ferror is set
 *   to 1 (=>SUCCESS), so we check if the watchdog killed the program.
 *
 *   Return the full output from the program (not only the first line).
 *   Also Return the stderr stream.
 *
 * Contrary to my normal calling conventions, this program
 *
 *  Returns: 0 on success
 *           non-zero on error == berrno status
 *
 */
int run_program_full_output_and_error(char *prog, int wait, POOLMEM *&results, POOLMEM *&errors, char *env[], bool cmd_string_opt)
{
   BPIPE *bpipe;
   int stat1=0, stat2=0, stat3=0;
   char *mode;
   POOLMEM* tmp, *tmp_err;
   char *buf, *err;
   const int bufsize = 32000;


   Dsm_check(200);

   tmp = get_pool_memory(PM_MESSAGE);
   tmp_err = get_pool_memory(PM_MESSAGE);
   buf = (char *)malloc(bufsize+1);
   err = (char *)malloc(bufsize+1);

   results[0] = 0;
   mode = (char *)"r";
   if (errors) {
      errors[0] = 0;
      mode = (char *)"re";
   }

#ifdef HAVE_WIN32
   bpipe = open_bpipe(prog, wait, mode, env, cmd_string_opt);
#else
   (void) cmd_string_opt;
   bpipe = open_bpipe(prog, wait, mode, env);
#endif
   if (!bpipe) {
      stat1 = ENOENT;
      goto bail_out;
   }

   Dsm_check(200);
   tmp[0] = 0;
   while (1) {
      buf[0] = 0;
      fgets(buf, bufsize, bpipe->rfd);
      buf[bufsize] = 0;
      pm_strcat(tmp, buf);
      if (feof(bpipe->rfd)) {
         stat1 = 0;
         Dmsg1(100, "Run program fgets stat=%d\n", stat1);
         break;
      } else {
         stat1 = ferror(bpipe->rfd);
      }
      if (stat1 < 0) {
         berrno be;
         Dmsg2(100, "Run program fgets stat=%d ERR=%s\n", stat1, be.bstrerror());
         break;
      } else if (stat1 != 0) {
         Dmsg1(200, "Run program fgets stat=%d\n", stat1);
         if (bpipe->timer_id && bpipe->timer_id->killed) {
            Dmsg1(100, "Run program saw fgets killed=%d\n", bpipe->timer_id->killed);
            break;
         }
      }
   }

   if (errors) {
      tmp_err[0] = 0;
      while (1) {
         err[0] = 0;
         fgets(err, bufsize, bpipe->efd);
         err[bufsize] = 0;
         pm_strcat(tmp_err, err);
         if (feof(bpipe->efd)) {
            stat2 = 0;
            Dmsg1(100, "Run program fgets err stat=%d\n", stat2);
            break;
         } else {
            stat2 = ferror(bpipe->efd);
         }
         if (stat2 < 0) {
            berrno be;
            Dmsg2(100, "Run program fgets stat=%d ERR=%s\n", stat2, be.bstrerror());
            break;
         } else if (stat2 != 0) {
            Dmsg1(200, "Run program fgets stat=%d\n", stat2);
            if (bpipe->timer_id && bpipe->timer_id->killed) {
               Dmsg1(100, "Run program saw fgets killed=%d\n", bpipe->timer_id->killed);
               break;
            }
         }
      }
   }

   /*
    * We always check whether the timer killed the program. We would see
    * an eof even when it does so we just have to trust the killed flag
    * and set the timer values to avoid edge cases where the program ends
    * just as the timer kills it.
    */
   if (bpipe->timer_id && bpipe->timer_id->killed) {
      Dmsg1(100, "Run program fgets killed=%d\n", bpipe->timer_id->killed);
      pm_strcpy(tmp, _("Program killed by Bacula (timeout)\n"));
      stat1 = ETIME;
   }

   pm_strcpy(results, tmp);
   Dmsg3(200, "resadr=0x%x reslen=%d res=%s\n", results, strlen(results), results);

   if (errors) {
      Dmsg3(200, "erradr=0x%x errlen=%d err=%s\n", errors, strlen(errors), errors);
      pm_strcpy(errors, tmp_err);
   }

   stat3 = close_bpipe(bpipe);
   stat1 = stat3 != 0 ? stat3 : stat2 != 0 ? stat2 : stat1;

   Dmsg1(100, "Run program returning %d\n", stat1);
bail_out:
   free_pool_memory(tmp);
   free_pool_memory(tmp_err);
   free(buf);
   free(err);
   return stat1;
}

/*
 * Identical to run_program_full_output_error without error handling
 */
int run_program_full_output(char *prog, int wait, POOLMEM *&results, char *env[])
{
   char *errors = NULL;
   return run_program_full_output_and_error(prog, wait, results, errors, env, false);
}

/*
 * Helper method to build the commands to run for the plugins,
 * so that command is executed in form of executable file.
 * This way we are much less liable to problems like string escaping.
 * If user is given, then the tmp file ownership is changed onto that user,
 * permissions are open otherwise.
 *
 * in:
 *    cmd - Plugin command to write into the executable file
 *    tmp - directory on disk where to store temporary command file
 *    dest - created filename
 *    JobId - job id, used to create unique filename
 *    user - file is owned by this user [optional]
 *    tmp_list - if not null then file created is added to the list for further cleanup
 *    err - buffer where error messages are stored
 *
 * out:
 *    true - in case of success, file can be executed
 *    false - in casef of error, no file was created
 */
bool make_tmp_cmd(const char *cmd, const char *tmp, POOLMEM **dest, uint32_t JobId, const char* user,
                         alist *tmp_list, POOLMEM **err)
{
   bool ret = false;
   berrno be;

   Mmsg(dest, "%s/cmd.%d.XXXXXX", tmp, JobId);

   int temp_fd = mkstemp(*dest);
   if (temp_fd < 0) {
      Mmsg(err, "Unable to create temporary file %s. ERR=%s\n", *dest, be.bstrerror());
      return false;
   }

   if (tmp_list) {
      tmp_list->append(bstrdup(*dest)); // Delete at the end
   }

   /* Write command that was previously built */
   size_t bytes = write(temp_fd, cmd, strlen(cmd));
   if (bytes != strlen(cmd)) {
      Mmsg(err, "Unable to write to %s, expected: %d written: %d error: %s\n",
            *dest, strlen(cmd), bytes, be.bstrerror(errno));
      goto bail_out;
   }

   /* This function is not designed to run on windows systems. TBI */
#ifndef HAVE_WIN32
   /* Make tmp command file executable for the user in which context we run the commands
    * (if possible) */
   if (user && getuid() == 0) {
      struct passwd *pwd = getpwnam(user);
      if (pwd == NULL) {
         Mmsg(err, "getwnam failed for %s, error: %s\n", user, be.bstrerror(errno));
         goto bail_out;
      }

      if (bchown(temp_fd, *dest, pwd->pw_uid, pwd->pw_gid) < 0) {
         Mmsg(err, "bchown failed for %s, error: %s\n", user, be.bstrerror(errno));
         goto bail_out;
      }

      /* Only the file's owner can run the command file in this case */
      if (bchmod(temp_fd, *dest, S_IXUSR|S_IRUSR) < 0) {
         Mmsg(err, "bchmod failed for %s, error: %s\n", *dest, be.bstrerror(errno));
         goto bail_out;
      }
   } else {
      /* We are either not root or no user to change was specified at all.
       * Permissions need to be quite open in this case, Read access is needed for the bpipe,
       * which is used later on */
      if (bchmod(temp_fd, *dest, S_IXUSR|S_IRUSR|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH) < 0) {
         Mmsg(err, "bchmod failed for %s, error: %s\n", *dest, be.bstrerror(errno));
         goto bail_out;
      }
   }
#endif  // HAVE_WIN32

   ret = true;

bail_out:
   close(temp_fd);
   return ret;
}
