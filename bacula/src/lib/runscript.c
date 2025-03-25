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
 * Manipulation routines for RunScript list
 *
 *  Eric Bollengier, May 2006
 *
 */


#include "bacula.h"
#include "jcr.h"
#include "runscript.h"

/*
 * This function pointer is set only by the Director (dird.c),
 * and is not set in the File daemon, because the File
 * daemon cannot run console commands.
 */
bool (*console_command)(JCR *jcr, const char *cmd) = NULL;


RUNSCRIPT *new_runscript()
{
   Dmsg0(500, "runscript: creating new RUNSCRIPT object\n");
   RUNSCRIPT *cmd = (RUNSCRIPT *)malloc(sizeof(RUNSCRIPT));
   memset(cmd, 0, sizeof(RUNSCRIPT));
   cmd->reset_default();

   return cmd;
}

void RUNSCRIPT::reset_default(bool free_strings)
{
   if (free_strings && command) {
     free_pool_memory(command);
   }
   if (free_strings && target) {
     free_pool_memory(target);
   }

   target = NULL;
   command = NULL;
   on_success = true;
   on_failure = false;
   fail_on_error = true;
   when = SCRIPT_Never;
   wait_time = 0;            /* Infinite by default */
   old_proto = false;        /* TODO: drop this with bacula 1.42 */
   job_code_callback = NULL;
}

RUNSCRIPT *copy_runscript(RUNSCRIPT *src)
{
   Dmsg0(500, "runscript: creating new RUNSCRIPT object from other\n");

   RUNSCRIPT *dst = (RUNSCRIPT *)malloc(sizeof(RUNSCRIPT));
   memcpy(dst, src, sizeof(RUNSCRIPT));

   dst->command = NULL;
   dst->target = NULL;

   dst->set_command(src->command, src->cmd_type);
   dst->set_target(src->target);

   return dst;
}

void free_runscript(RUNSCRIPT *script)
{
   Dmsg0(500, "runscript: freeing RUNSCRIPT object\n");

   if (script->command) {
      free_pool_memory(script->command);
   }
   if (script->target) {
      free_pool_memory(script->target);
   }
   free(script);
}

int run_scripts(JCR *jcr, alist *runscripts, const char *label)
{
   Dmsg2(200, "runscript: running all RUNSCRIPT object (%s) JobStatus=%c\n", label, jcr->JobStatus);

   RUNSCRIPT *script;
   bool runit;
   bool ret = true; /* Changed to false in case any of the scripts fails */
   int when;

   if (strstr(label, NT_("Before"))) {
      when = SCRIPT_Before;
   } else if (bstrcmp(label, NT_("ClientAfterVSS"))) {
      when = SCRIPT_AfterVSS;
   } else if (bstrcmp(label, NT_("AtJobCompletion"))) {
      when = SCRIPT_AtJobCompletion;
   } else if (bstrcmp(label, NT_("Queued"))) {
      when = SCRIPT_Queued;
   } else {
      when = SCRIPT_After;
   }

   if (runscripts == NULL) {
      Dmsg0(100, "runscript: WARNING RUNSCRIPTS list is NULL\n");
      return 0;
   }

   foreach_alist(script, runscripts) {
      Dmsg2(200, "runscript: try to run %s:%s\n", NPRT(script->target), NPRT(script->command));
      runit = false;

      if ((script->when & SCRIPT_Before) && (when & SCRIPT_Before)) {
         if ((script->on_success &&
              (jcr->JobStatus == JS_Running || jcr->JobStatus == JS_Created))
            || (script->on_failure &&
                (job_canceled(jcr) || jcr->JobStatus == JS_Differences))
            )
         {
            Dmsg4(200, "runscript: Run it because SCRIPT_Before (%s,%i,%i,%c)\n",
                  script->command, script->on_success, script->on_failure,
                  jcr->JobStatus );
            /* Set job task code */
            jcr->job_task = JOB_TASK_BEFORE_SCRIPT;
            runit = true;
         }
      }

      if ((script->when & SCRIPT_AfterVSS) && (when & SCRIPT_AfterVSS)) {
         if ((script->on_success && (jcr->JobStatus == JS_Blocked))
            || (script->on_failure && job_canceled(jcr))
            )
         {
            Dmsg4(200, "runscript: Run it because SCRIPT_AfterVSS (%s,%i,%i,%c)\n",
                  script->command, script->on_success, script->on_failure,
                  jcr->JobStatus );
            /* Set job task code */
            jcr->job_task = JOB_TASK_AFTER_SCRIPT;
            runit = true;
         }
      }

      if ((script->when & SCRIPT_AtJobCompletion) && (when & SCRIPT_AtJobCompletion)) {
         Dmsg1(200, "AtJobCompletion jobstatus=%c\n", jcr->JobStatus);
         if ((script->on_success &&
              (jcr->JobStatus == JS_Terminated || jcr->JobStatus == JS_Warnings))
            || (script->on_failure &&
                (job_canceled(jcr) || jcr->JobStatus == JS_Differences))
            )
         {
            Dmsg4(200, "runscript: Run it because SCRIPT_AtJobCompletion (%s,%i,%i,%c)\n",
                  script->command, script->on_success, script->on_failure,
                  jcr->JobStatus );
            /* Set job task code */
            jcr->job_task = JOB_TASK_ENDJOB_SCRIPT;
            runit = true;
         }
      }

      if ((script->when & SCRIPT_After) && (when & SCRIPT_After)) {
         if ((script->on_success &&
              (jcr->JobStatus == JS_Terminated || jcr->JobStatus == JS_Warnings))
            || (script->on_failure &&
                (job_canceled(jcr) || jcr->JobStatus == JS_Differences))
            )
         {
            Dmsg4(200, "runscript: Run it because SCRIPT_After (%s,%i,%i,%c)\n",
                  script->command, script->on_success, script->on_failure,
                  jcr->JobStatus );
            /* Set job task code */
            jcr->job_task = JOB_TASK_AFTER_SCRIPT;
            runit = true;
         }
      }

      if (!script->is_local()) {
         runit = false;
      }

      /* we execute it */
      if (runit) {
         if (!script->run(jcr, label)) {
            ret = false; /* return err in case any of the script commands failed */
         }
      }
   }

   /* Script ended, reset operation code */
   jcr->job_task = JOB_TASK_ZERO;

   return ret;
}


/* Function similar to run_scripts() but we report the exit status of the script
 * -1: no script to run
 *  0: ok
 *  1-255: exit code from the script
 */
int run_scripts_get_code(JCR *jcr, alist *runscripts, const char *label)
{
   Dmsg2(200, "runscript: running all RUNSCRIPT object (%s) JobStatus=%c\n", label, jcr->JobStatus);

   RUNSCRIPT *script;
   bool runit;
   int ret = -1; 
   int when;

   if (strstr(label, NT_("Queued"))) {
      when = SCRIPT_Queued;
   } else {
      when = SCRIPT_Never;
   }

   if (runscripts == NULL) {
      Dmsg0(100, "runscript: WARNING RUNSCRIPTS list is NULL\n");
      return -1;
   }

   foreach_alist(script, runscripts) {
      Dmsg2(200, "runscript: try to run %s:%s\n", NPRT(script->target), NPRT(script->command));
      runit = false;

      if ((script->when & SCRIPT_Queued) && (when & SCRIPT_Queued)) {
         if (jcr->job_started == false) {
            Dmsg4(200, "runscript: Run it because SCRIPT_Queued (%s,%i,%i,%c)\n",
                  script->command, script->on_success, script->on_failure,
                  jcr->JobStatus );
            if (!script->wait_time) {
               script->wait_time = 15; // Set a maximum of 15s to not block everything
            }
            /* Set job task code */
            jcr->job_task = JOB_TASK_QUEUED;
            runit = true;
         }
      }

      if (!script->is_local()) {
         runit = false;
      }

      /* we execute it */
      if (runit) {
         berrno be;
         int aret = script->run_get_code(jcr, label);
         ret = MAX(ret, be.code(aret));  // We return the one with the biggest return code
      }
   }

   /* Script ended, reset operation code */
   jcr->job_task = JOB_TASK_ZERO;
   return ret;
}

bool RUNSCRIPT::is_local()
{
   if (!target || (strcmp(target, "") == 0)) {
      return true;
   } else {
      return false;
   }
}

/* set this->command to cmd */
void RUNSCRIPT::set_command(const char *cmd, int acmd_type)
{
   Dmsg1(500, "runscript: setting command = %s\n", NPRT(cmd));

   if (!cmd) {
      return;
   }

   if (!command) {
      command = get_pool_memory(PM_FNAME);
   }

   pm_strcpy(command, cmd);
   cmd_type = acmd_type;
}

/* set this->target to client_name */
void RUNSCRIPT::set_target(const char *client_name)
{
   Dmsg1(500, "runscript: setting target = %s\n", NPRT(client_name));

   if (!client_name) {
      return;
   }

   if (!target) {
      target = get_pool_memory(PM_FNAME);
   }

   pm_strcpy(target, client_name);
}

/*
 * -1: execution problem 
 *  0..255 excution done
 */
int RUNSCRIPT::run_get_code(JCR *jcr, const char *name)
{
   Dmsg1(100, "runscript: running a RUNSCRIPT object type=%d\n", cmd_type);
   POOLMEM *ecmd = get_pool_memory(PM_FNAME);
   int status = -1;
   BPIPE *bpipe;
   char line[MAXSTRING];

   ecmd = edit_job_codes(jcr, ecmd, this->command, "", this->job_code_callback);
   Dmsg1(100, "runscript: running '%s'...\n", ecmd);
   Jmsg(jcr, M_INFO, 0, _("%s: run %s \"%s\"\n"),
        cmd_type==SHELL_CMD?"shell command":"console command", name, ecmd);

   switch (cmd_type) {
   case SHELL_CMD:
      bpipe = open_bpipe(ecmd, wait_time, "r");
      if (bpipe) {
         while (fgets(line, sizeof(line), bpipe->rfd)) {
            int len = strlen(line);
            if (len > 0 && line[len-1] == '\n') {
               line[len-1] = 0;
            }
            Jmsg(jcr, M_INFO, 0, _("%s: %s\n"), name, line);
         }
         status = close_bpipe(bpipe);
      }
      break;
   case CONSOLE_CMD:
      if (console_command) {                 /* can we run console command? */
         if (!console_command(jcr, ecmd)) {  /* yes, do so */
            status = 1;
         } else {
            status = 0;
         }
      }
      break;
   }
   Dmsg1(100, "runscript status=%d\n", status);
   free_pool_memory(ecmd);
   return status;
}

bool RUNSCRIPT::run(JCR *jcr, const char *name)
{
   int code = run_get_code(jcr, name);
   if (code != 0) {
      berrno be;
      Jmsg(jcr, M_ERROR, 0, _("Runscript: %s returned non-zero status=%d. ERR=%s\n"), name,
           be.code(code), be.bstrerror(code));

      if (fail_on_error) {
         jcr->setJobStatus(JS_ErrorTerminated);
      }
   }
   return code == 0;
}

void free_runscripts(alist *runscripts)
{
   Dmsg0(500, "runscript: freeing all RUNSCRIPTS object\n");

   if (runscripts){
      RUNSCRIPT *elt;
      foreach_alist(elt, runscripts) {
         free_runscript(elt);
      }
   }
}

void RUNSCRIPT::debug()
{
   Dmsg0(200, "runscript: debug\n");
   Dmsg0(200,  _(" --> RunScript\n"));
   Dmsg1(200,  _("  --> Command=%s\n"), NPRT(command));
   Dmsg1(200,  _("  --> Target=%s\n"),  NPRT(target));
   Dmsg1(200,  _("  --> RunOnSuccess=%u\n"),  on_success);
   Dmsg1(200,  _("  --> RunOnFailure=%u\n"),  on_failure);
   Dmsg1(200,  _("  --> FailJobOnError=%u\n"),  fail_on_error);
   Dmsg1(200,  _("  --> RunWhen=%u\n"),  when);
   Dmsg1(200,  _("  --> Timeout=%u\n"),  wait_time);
}

void RUNSCRIPT::set_job_code_callback(job_code_callback_t arg_job_code_callback)
{
   this->job_code_callback = arg_job_code_callback;
}
