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
#include "filed/fd_plugins.h"
#include "findlib/bfile.h"

static const char *working = "./";

#define USE_JOB_LIST
#define COMMAND_LINE_TOOL	   // See in src/plugin/fd/fd_common.h

#define MAX_RETURN_PRUNE 10
#define RETRY_LOCK 10
#define ALWAYS_LEVEL 0
#define CRITICAL_LEVEL 100
#define STANDARD_LEVEL 300
#define PROG_NAME "bjoblist"


#include "plugins/fd/fd_common.h"

void *start_heap;

int main(int argc, char *argv[])
{
   int opt;
   int lock_return = 1;		   // Good by default
   int lock_fd;
   char lock_name[20] = "/tmp/bjoblist.lck";
   POOL_MEM pool_mem;
   POOLMEM *lock_poolmem = get_pool_memory(PM_NAME);
   alist tmp_list(MAX_RETURN_PRUNE, owned_by_alist);
   bool store_return = true;
   bool search_return = true;
   char *to_delete_job;

   // Args to be received by the command line
   char *dat_file = NULL;
   char *key = NULL;
   char *prev_job = NULL;
   char *curr_job = NULL;
   char level = 0;
   char *data = NULL;
   bool is_store = false;
   bool is_search = false;
   bool use_lock = false;
   debug_level = 0;


   // Parsing of command line arguments
   while ((opt = getopt(argc, argv, "w:f:k:p:j:l:D:sSiLd:h")) != -1) {
      switch (opt) {
      case 'w':		   // Working dir
	 working = working_directory = optarg;	
	 // working is for fd_common.h , working directory global variable in bacula
	 break;

      case 'f':		   // datfile
	 dat_file = optarg;
	 break;

      case 'k':		   // key (hostname + volume name)
	 key = optarg;
	 break;

      case 'p':		   // previous job
	 prev_job = optarg;
	 break;

      case 'j':		   // current job
	 curr_job = optarg;
	 break;

      case 'l':		   // level 
	 level = optarg[0];
	 break;

      case 'D':		   // Data
	 data = optarg;
	 break;

      case 's':		   // Store
	 is_store = true;
	 break;

      case 'S':		   // Search
	 is_search = true;
	 break;

      case 'L':		   // Use file lock
	 use_lock = true;
	 break;

      case 'd':		   // Debug-level
	 debug_level = atoi(optarg);	// global var
	 break;

      case 'h':		   // help default is help as well
      default:
	 Dmsg(NULL, ALWAYS_LEVEL,
	      "bjoblist: joblist command line tool : "
	      "store/search jobs and find deletable ones\n"
	      "USAGE: bjoblist [OPTIONS]\n"
	      "Options-styles: -option\n\n"
	      "\t-w WORK_DIR\t\tWorking directory\n"
	      "\t-f DATA_FILE\t\tDat file with job hierarchy info\n"
	      "\t-k KEY\t\t\tKey host_name:volume_name\n"
	      "\t-p PREVIOUS_JOB\t\tPrevious job\n"
	      "\t-j CURRENT_JOB\t\tCurrent job\n"
	      "\t-l {F,I,D}\t\tLevel {(F)ull, (D)ifferential, (I)ncremental}\n"
	      "\t-D DATA\t\t\tData\n"
	      "\t-d DEBUG_LEVEL\t\tDebug-level\n"
	      "\t-s\t\t\tStore-mode\n"
	      "\t-S\t\t\tSearch-mode\n"
	      "\t-L\t\t\tUse file lock\n" "\t-h\t\t\tDisplay this text\n\n");

	 exit(EXIT_SUCCESS);
      }
   }

   // Check for correctly initialized variables
   // Prev job can eventually be NULL
   if (working_directory == NULL) {
      Dmsg(NULL, CRITICAL_LEVEL, "Working directory not set EXIT\n");
      exit(EXIT_FAILURE);
   }

   // Check if data_file has been initilaized
   if (dat_file == NULL) {
      Dmsg(NULL, CRITICAL_LEVEL, "Data file not set EXIT\n");
      exit(EXIT_FAILURE);
   }

   // Check that current job is not null
   if (curr_job == NULL) {
      Dmsg(NULL, CRITICAL_LEVEL, "Current job not set EXIT\n");
      exit(EXIT_FAILURE);
   }

   // Check if key = either F/I/D
   if (level != 'F' && level != 'I' && level != 'D') {
      Dmsg(NULL, CRITICAL_LEVEL, "Bad key has to be either F/I/D is %d\n", level);
      exit(EXIT_FAILURE);
   }

   // If data is null set it to current job
   if (data == NULL) {
      data = curr_job;
   }

   // Check if there is a search and or store to do
   if (!is_store && !is_search) {
      Dmsg(NULL, CRITICAL_LEVEL, "Store or search not called. Nothing to do EXIT\n");
      exit(EXIT_SUCCESS);
   }

   // If -L is passed try to acquiere lock
   if (use_lock) {
      lock_return =
	 create_lock_file(lock_name, PROG_NAME, lock_name, &lock_poolmem, &lock_fd);
   }

   // If fail to acquiere lock wait and retry
   if (lock_return != 1) {
      Dmsg(NULL, STANDARD_LEVEL, "Fail to acquire lock retry\n");
      bmicrosleep(RETRY_LOCK, 0);
      lock_return =
	 create_lock_file(lock_name, PROG_NAME, lock_name, &lock_poolmem, &lock_fd);

      // IF lock fail once again just abort
      if (lock_return != 1) {
	 Dmsg(NULL, CRITICAL_LEVEL, "Fail to acquire lock twice EXIT\n");
	 exit(EXIT_FAILURE);
      }

   }

   joblist job_history(NULL, key, curr_job, prev_job, level);	// context can be null
   job_history.set_base(dat_file);
   job_history.set_return_jobname_on_prune(true);

   if (is_store) {
      store_return = job_history.store_job(data);	// return boolean
      job_history.prune_jobs(NULL, NULL, &tmp_list);	// retrun void

      // print all deletable jobs
      foreach_alist(to_delete_job, &tmp_list) {
	 Dmsg(NULL, ALWAYS_LEVEL, "%s\n", to_delete_job);
      }
   }

   if (is_search) {
      search_return = job_history.find_job(curr_job, pool_mem.handle());	// Return bool
      Dmsg(NULL, ALWAYS_LEVEL, "%s\n", pool_mem.c_str());
   }

   // Check that search/store op returned correctly (true if not called)
   if (store_return && search_return && lock_return == 1) {
      exit(EXIT_SUCCESS);
   } else {
      Dmsg(NULL, CRITICAL_LEVEL,
	   "Store return : %d, Search return %d Lock return : %d\n", store_return,
	   search_return, lock_return);

      exit(EXIT_FAILURE);
   }
}
