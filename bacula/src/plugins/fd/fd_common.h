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

/* You can include this file to your plugin to have
 * access to some common tools and utilities provided by Bacula
 */

#ifndef PCOMMON_H
#define PCOMMON_H

#define JT_BACKUP                'B'  /* Backup Job */
#define JT_RESTORE               'R'  /* Restore Job */

#define L_FULL                   'F'  /* Full backup */
#define L_INCREMENTAL            'I'  /* since last backup */
#define L_DIFFERENTIAL           'D'  /* since last full backup */

#ifndef DLL_IMP_EXP
# if defined(BUILDING_DLL)
#   define DLL_IMP_EXP   __declspec(dllexport)
# elif defined(USING_DLL)
#   define DLL_IMP_EXP   __declspec(dllimport)
# else
#   define DLL_IMP_EXP
# endif
#endif

#ifdef SMARTALLOC
DLL_IMP_EXP void *sm_malloc(const char *fname, int lineno, size_t nbytes);
DLL_IMP_EXP void sm_free(const char *file, int line, void *fp);
DLL_IMP_EXP void *reallymalloc(const char *fname, int lineno, size_t nbytes);
DLL_IMP_EXP void reallyfree(const char *file, int line, void *fp);

#ifndef bmalloc
# define bmalloc(s)      sm_malloc(__FILE__, __LINE__, (s))
# define bfree(o)        sm_free(__FILE__, __LINE__, (o))
#endif

#define SM_CHECK sm_check(__FILE__, __LINE__, false)

#ifdef malloc
#undef malloc
#undef free
#endif

#define malloc(s)    sm_malloc(__FILE__, __LINE__, (s))
#define free(o)      sm_free(__FILE__, __LINE__, (o))

/* Looks to be broken on scientific linux */
#ifdef xxxx
inline void *operator new(size_t size, char const * file, int line)
{
   void *pnew = sm_malloc(file,line, size);
   memset((char *)pnew, 0, size);
   return pnew;
}

inline void *operator new[](size_t size, char const * file, int line)
{
   void *pnew = sm_malloc(file, line, size);
   memset((char *)pnew, 0, size);
   return pnew;
}

inline void *operator new(size_t size)
{
   void *pnew = sm_malloc(__FILE__, __LINE__, size);
   memset((char *)pnew, 0, size);
   return pnew;
}

inline void *operator new[](size_t size)
{
   void *pnew = sm_malloc(__FILE__, __LINE__, size);
   memset((char *)pnew, 0, size);
   return pnew;
}

#define new   new(__FILE__, __LINE__)

inline void operator delete(void *buf)
{
   sm_free( __FILE__, __LINE__, buf);
}

inline void operator delete[] (void *buf)
{
  sm_free(__FILE__, __LINE__, buf);
}

inline void operator delete[] (void *buf, char const * file, int line)
{
  sm_free(file, line, buf);
}

inline void operator delete(void *buf, char const * file, int line)
{
   sm_free(file, line, buf);
}


#endif
#endif  /* !SMARTALLOC */

#ifdef UNITTESTS

#define Dmsg(context, level,  ...) printf( __VA_ARGS__ )
#define Jmsg(context, level,  ...) printf( __VA_ARGS__ )

#else

#ifdef COMMAND_LINE_TOOL

#define dbglvl 10
#define Dmsg(context, level,  ...) if (level <= debug_level) printf( __VA_ARGS__ )
#define Jmsg(context, level,  ...) if (level <= debug_level) printf( __VA_ARGS__ )

#else

#define Dmsg(context, level,  ...) bfuncs->DebugMessage(context, __FILE__, __LINE__, level, __VA_ARGS__ )
#define Jmsg(context, type,  ...) bfuncs->JobMessage(context, __FILE__, __LINE__, type, 0, __VA_ARGS__ )

#endif
#endif

#ifdef USE_CMD_PARSER
#include "lib/cmd_parser.h"
#endif /* USE_CMD_PARSER */

#ifdef USE_ADD_DRIVE
/* Keep drive letters for windows vss snapshot */
static void add_drive(char *drives, int *nCount, char *fname) {
   if (strlen(fname) >= 2 && B_ISALPHA(fname[0]) && fname[1] == ':') {
      /* always add in uppercase */
      char ch = toupper(fname[0]);
      /* if not found in string, add drive letter */
      if (!strchr(drives,ch)) {
         drives[*nCount] = ch;
         drives[*nCount+1] = 0;
         (*nCount)++;
      }                                
   }
}

/* Copy our drive list to Bacula core list */
static void copy_drives(char *drives, char *dest) {
   int last = strlen(dest);     /* dest is 27 bytes long */
   for (char *p = drives; *p && last < 26; p++) {
      if (!strchr(dest, *p)) {
         dest[last++] = *p;
         dest[last] = 0;
      }
   }
}
#endif  /* USE_ADD_DRIVE */

#endif  /* ! PCOMMON_H */

#ifdef USE_JOB_LIST

/* This class is used to store locally the job history, you can attach data
 * to it such as snapshot names
 * !!! Don't forget that this file may be deleted by the user. !!!
 */

class joblist: public SMARTALLOC
{
private:
   bpContext *ctx;
   bool return_jobname_on_prune;

public:
   char level;                  /* level of the job */

   POOLMEM *base;               /* base name */
   POOLMEM *key;                /* group of backup */
   POOLMEM *name;               /* job name */
   POOLMEM *prev;               /* based on jobname */
   POOLMEM *root;               /* root of this branch */
   POOLMEM *rootdiff;           /* root of diff if any */

   btime_t job_time;           /* job time */

   void init() {
      level = 0;
      job_time = 0;
      if (!base) {
         base = get_pool_memory(PM_FNAME);
         key = get_pool_memory(PM_FNAME);
         name = get_pool_memory(PM_FNAME);
         prev = get_pool_memory(PM_FNAME);
         root = get_pool_memory(PM_FNAME);
         rootdiff = get_pool_memory(PM_FNAME);
      }
      *key = *name = *prev = *root = *rootdiff = 0;
      set_base("jobs.dat");
      ctx = NULL;
      return_jobname_on_prune = false;
   }

   void set_base(const char *b) {
      pm_strcpy(base, b);
   }

   /* If true when prune_job is called return jobnanme instead of job data in alist */
   void set_return_jobname_on_prune(bool value) {
      return_jobname_on_prune = value;
   }

   joblist(bpContext *actx): ctx(NULL), level(0), base(NULL), key(NULL), name(NULL), prev(NULL), root(NULL), rootdiff(NULL), job_time(0)
   {
      init();
      ctx = actx;
   }

   joblist(bpContext *actx, 
        const char *akey, 
        const char *jobname, 
        const char *prevjobname, 
       char joblevel):
   ctx(NULL), level(0), base(NULL), key(NULL), name(NULL), prev(NULL), root(NULL), rootdiff(NULL), job_time(0)
   {
      init();
      ctx = actx;
      if (jobname) {
         pm_strcpy(name, jobname);
      }

      if (prevjobname) {
         pm_strcpy(prev, prevjobname);
      }

      level = joblevel;

      if (akey) {
         pm_strcpy(key, akey);

      } else {
         get_key_from_name();
      }
   }

   ~joblist() {
      free_pool_memory(base);
      free_pool_memory(key);
      free_pool_memory(name);
      free_pool_memory(prev);
      free_pool_memory(root);
      free_pool_memory(rootdiff);
   }

   /* Will extract the name from the full job name */
   bool get_key_from_name() {
      key = check_pool_memory_size(key, sizeof_pool_memory(name));
      return get_key_from_name(ctx, name, key, sizeof_pool_memory(key));
   };

   /* Will extract the name from the full job name */
   static bool get_key_from_name(bpContext *ctx, const char *n, char *d, int len) {
      // pluginTest.2012-07-19_16.59.21_11
      int l = strlen(n);
      int dlen = 23; // strlen(".2012-07-19_16.59.21_11");

      if (l > dlen) {           /* we probably have a key */
         int start = l - dlen;
         if (start + 1 > len) {
            Dmsg(ctx, dbglvl+100, "Key is too long\n");
            return false;
         }
         if (n[start] == '.' &&
             B_ISDIGIT(n[start + 1]) &&   // 2
             B_ISDIGIT(n[start + 2]) &&   // 0
             B_ISDIGIT(n[start + 3]) &&   // 1
             B_ISDIGIT(n[start + 4]) &&   // 2
             n[start + 5] == '-' &&       // -
             B_ISDIGIT(n[start + 6]) &&   // 0
             B_ISDIGIT(n[start + 7]))     // 7
         {
            bstrncpy(d, n, start + 1);
            Dmsg(ctx, dbglvl+100, "key is %s from jobname %s\n", d, n);
            return true;
         }
      }
      Dmsg(ctx, dbglvl+100, "Unable to get key from jobname %s\n", n);
      return false;
   };

   bool find_job(const char *name, POOLMEM **data=NULL);   /* set root, job_time */
   bool find_root_job();
   bool store_job(char *data);
   void prune_jobs(char *build_cmd(void *arg, const char *data, const char *job), 
                   void *arg, alist *jobs);
};

static pthread_mutex_t joblist_mutex = PTHREAD_MUTEX_INITIALIZER;

bool joblist::find_job(const char *name, POOLMEM **data)
{
   FILE *f;
   POOLMEM *tmp;
   POOLMEM *buf;
   POOLMEM *curkey;             /* key */
   POOLMEM *curjobname;         /* jobname */
   POOLMEM *prevjob;            /* last jobname */
   POOLMEM *rootjob;            /* root jobname */
   char t[MAX_NAME_LENGTH];     /* store time */
   char curlevel;
   bool ok=false;

   *root = 0;
   job_time = 0;
   *rootdiff = 0;

   tmp = get_pool_memory(PM_FNAME);
   buf = get_pool_memory(PM_FNAME);
   curkey = get_pool_memory(PM_FNAME);
   curjobname = get_pool_memory(PM_FNAME);
   prevjob = get_pool_memory(PM_FNAME);
   rootjob = get_pool_memory(PM_FNAME);

   Mmsg(tmp, "%s/%s", working, base);

   P(joblist_mutex);
   f = bfopen(tmp, "r");
   if (!f) {
      berrno be;
      Jmsg(ctx, M_ERROR, "Unable to open job database. ERR=%s\n",
           be.bstrerror(errno));
      goto bail_out;
   }

   while (!ok && bfgets(buf, f) != NULL) {
      curkey = check_pool_memory_size(curkey, sizeof_pool_memory(buf));
      curjobname = check_pool_memory_size(curjobname, sizeof_pool_memory(buf));
      rootjob = check_pool_memory_size(rootjob, sizeof_pool_memory(buf));
      prevjob = check_pool_memory_size(prevjob, sizeof_pool_memory(buf));
      *curkey = *curjobname = *rootjob = *prevjob = 0;
      
      Dmsg(ctx, dbglvl+100, "line = [%s]\n", buf);

      if (scan_string(buf, "time=%60s level=%c key=%s name=%s root=%s prev=%s", 
                      t, &curlevel, curkey, curjobname, rootjob, prevjob) != 6) {

         if (scan_string(buf, "time=%60s level=F key=%s name=%s", 
                         t, curkey, curjobname) != 3) {
            Dmsg(ctx, dbglvl+100, "Bad line l=[%s]\n", buf);
            continue;
         }
      }
      
      if (strcmp(name, curjobname) == 0 &&
          strcmp(key, curkey) == 0)
      {
         job_time = str_to_uint64(t);
         pm_strcpy(root, rootjob);
         if (curlevel == 'D') {
            pm_strcpy(rootdiff, curjobname);
         }

         if (data) {
            pm_strcpy(data, strstr(buf, " vol=") +  5);
            strip_trailing_newline(*data);
            unbash_spaces(*data);
         }

         ok = true;
         Dmsg(ctx, dbglvl+100, "Found job root %s -> %s -> %s\n",
              rootdiff, root, curjobname);
      } 
   }

   fclose(f);

bail_out:
   V(joblist_mutex);
   free_pool_memory(buf);
   free_pool_memory(curkey);
   free_pool_memory(curjobname);
   free_pool_memory(prevjob);
   free_pool_memory(rootjob);
   free_pool_memory(tmp);
   return ok;

}

/* Find the root job for the current job */
bool joblist::find_root_job()
{
   FILE *f;
   POOLMEM *tmp;
   POOLMEM *buf;
   POOLMEM *curkey;             /* key */
   POOLMEM *curjobname;         /* jobname */
   POOLMEM *prevjob;            /* last jobname */
   POOLMEM *rootjob;            /* root jobname */
   char t[MAX_NAME_LENGTH];
   char curlevel;
   bool ok=false;

   *root = 0;
   job_time = 0;

   if (level == 'F') {
      pm_strcpy(root, name);
      return true;
   }

   buf = get_pool_memory(PM_FNAME);
   curkey = get_pool_memory(PM_FNAME);
   curjobname = get_pool_memory(PM_FNAME);
   prevjob = get_pool_memory(PM_FNAME);
   rootjob = get_pool_memory(PM_FNAME);


   tmp = get_pool_memory(PM_FNAME);
   Mmsg(tmp, "%s/%s", working, base);

   P(joblist_mutex);
   f = bfopen(tmp, "r");
   if (!f) {
      berrno be;
      Jmsg(ctx, M_ERROR, "Unable to prune previous jobs. ERR=%s\n",
           be.bstrerror(errno));
      goto bail_out;
   }

   while (!ok && bfgets(buf, f) != NULL) {
      curkey = check_pool_memory_size(curkey, sizeof_pool_memory(buf));
      curjobname = check_pool_memory_size(curjobname, sizeof_pool_memory(buf));
      rootjob = check_pool_memory_size(rootjob, sizeof_pool_memory(buf));
      prevjob = check_pool_memory_size(prevjob, sizeof_pool_memory(buf));
      *curkey = *curjobname = *rootjob = *prevjob = 0;

      Dmsg(ctx, dbglvl+100, "line = [%s]\n", buf);

      if (scan_string(buf, "time=%60s level=%c key=%s name=%s root=%s prev=%s", 
                      t, &curlevel, curkey, curjobname, rootjob, prevjob) != 6) {

         if (scan_string(buf, "time=%60s level=F key=%s name=%s", 
                         t, curkey, curjobname) == 3) {
            pm_strcpy(rootjob, curjobname);
            *prevjob = 0;
            curlevel = 'F';

         } else {
            Dmsg(ctx, dbglvl+100, "Bad line l=[%s]\n", buf);
            continue;
         }
      }
      
      if (strcmp(key,  curkey)  == 0  &&
          strcmp(prev, curjobname) == 0) 
      {
         pm_strcpy(root, rootjob);

         if (curlevel == 'D') {
            pm_strcpy(rootdiff, curjobname);
         }
         ok = true;
         Dmsg(ctx, dbglvl+100, "Found job root %s -> %s -> %s\n",
              rootdiff, root, curjobname);
      } 
   }

   fclose(f);

bail_out:
   V(joblist_mutex);
   free_pool_memory(tmp);
   free_pool_memory(buf);
   free_pool_memory(curkey);
   free_pool_memory(curjobname);
   free_pool_memory(prevjob);
   free_pool_memory(rootjob);

   return true;
}

/* Store the current job in the jobs.dat for a specific data list */
bool joblist::store_job(char *data)
{
   FILE *fp;
   int l;
   POOLMEM *tmp = NULL;
   btime_t now;
   bool ret = true;

   /* Not initialized, no need to store jobs */
   if (*name == 0 || !level) {
      Dmsg(ctx, dbglvl+100, "store_job fail name=%s level=%d\n", name, level);
      return false;
   }

   find_root_job();

   P(joblist_mutex);

   tmp = get_pool_memory(PM_FNAME);
   Mmsg(tmp, "%s/%s", working, base);
   fp = bfopen(tmp, "a+");
   if (!fp) {
      berrno be;
      Jmsg(ctx, M_ERROR, "Unable to update the job history. ERR=%s\n",
           be.bstrerror(errno));
      ret = false;
      goto bail_out;
   }

   now = time(NULL);
   
   bash_spaces(data);

   if (level == 'F') {
      l = fprintf(fp, "time=%lld level=%c key=%s name=%s vollen=%d vol=%s\n", 
               (long long)now, level, key, name, (int)strlen(data), data);

   } else {
      l = fprintf(fp, "time=%lld level=%c key=%s name=%s root=%s prev=%s vollen=%d vol=%s\n",
            (long long)now, level, key, name, root, prev, (int)strlen(data), data);
   }

   unbash_spaces(data);

   if (l < 0) {
      berrno be;
      Jmsg(ctx, M_ERROR, "Unable to update the job history. ERR=%s\n",
           be.bstrerror(errno));
      ret = false;
   }

   if (fclose(fp) != 0) {
      ret = false;
   }

bail_out:
   V(joblist_mutex);
   free_pool_memory(tmp);
   return ret;
}

/* Prune jobs at the end of the job, this function can generate commands
 * in order to cleanup something
 */
void joblist::prune_jobs(char *build_cmd(void *arg, const char *data, const char *job), 
                         void *arg, alist *jobs)
{
   FILE *fout;
   FILE *f=NULL;
   POOLMEM *tmp;
   POOLMEM *tmpout;
   POOLMEM *data;
   POOLMEM *buf;
   POOLMEM *curkey;             /* key */
   POOLMEM *curjobname;         /* jobname */
   POOLMEM *prevjob;            /* last jobname */
   POOLMEM *rootjob;            /* root jobname */
   char t[MAX_NAME_LENGTH];
   uint32_t datalen;
   char curlevel;
   bool keep;
   bool ok=false;
   int count=0;

   /* In Incremental, it means that the previous Full/Diff is well terminated */
   if (level != 'I') {
      return;
   }

   find_root_job();

   tmp = get_pool_memory(PM_FNAME);
   Mmsg(tmp, "%s/%s", working, base);

   tmpout = get_pool_memory(PM_FNAME);
   Mmsg(tmpout, "%s/%s.swap", working, base);

   buf = get_pool_memory(PM_FNAME);
   data = get_pool_memory(PM_FNAME);
   curkey = get_pool_memory(PM_FNAME);
   curjobname = get_pool_memory(PM_FNAME);
   prevjob = get_pool_memory(PM_FNAME);
   rootjob = get_pool_memory(PM_FNAME);

   *curkey = *curjobname = *prevjob = *rootjob = *buf = *data = 0;

   P(joblist_mutex);
   fout = bfopen(tmpout, "w");
   if (!fout) {
      berrno be;
      Jmsg(ctx, M_ERROR, "Unable to prune previous jobs. "
           "Can't open %s for writing ERR=%s\n",
           tmpout, be.bstrerror(errno));
      goto bail_out;
   }

   f = bfopen(tmp, "r");
   if (!f) {
      berrno be;
      Jmsg(ctx, M_ERROR, "Unable to prune previous jobs. ERR=%s\n",
           be.bstrerror(errno));
      goto bail_out;
   }

   while (bfgets(buf, f) != NULL) {
      curkey = check_pool_memory_size(curkey, sizeof_pool_memory(buf));
      curjobname = check_pool_memory_size(curjobname, sizeof_pool_memory(buf));
      rootjob = check_pool_memory_size(rootjob, sizeof_pool_memory(buf));
      prevjob = check_pool_memory_size(prevjob, sizeof_pool_memory(buf));
      data = check_pool_memory_size(data, sizeof_pool_memory(buf));
      *data = *curkey = *curjobname = *rootjob = *prevjob = 0;
      keep = false;
      datalen = 0;

      /* We don't capture the vol list, because our sscanf is limited to 1000 bytes  */
      if (scan_string(buf, "time=%60s level=%c key=%s name=%s root=%s prev=%s vollen=%d vol=", 
                      t, &curlevel, curkey, curjobname, rootjob, prevjob, &datalen) != 7) {

         if (scan_string(buf, "time=%60s level=F key=%s name=%s vollen=%d vol=", 
                         t, curkey, curjobname, &datalen) == 4) {
            *rootdiff = *rootjob = *prevjob = 0;
            curlevel = 'F';

         } else {
            Dmsg(ctx, dbglvl+100, "Bad line l=[%s]\n", buf);
            keep = true;
         }
      }
      
      if (!keep) {
         pm_strcpy(data, strstr(buf, " vol=") +  5);
         strip_trailing_newline(data);
         unbash_spaces(data);

         if (datalen != strlen(data)) {
            Dmsg(ctx, dbglvl+100, "Bad data line datalen != strlen(data) %d != %d\n", datalen, (int)strlen(data)); 
            Dmsg(ctx, dbglvl+100, "v=[%s]\n", data);
         }
      }


      if (!keep &&
          (strcmp(key,  curkey)  != 0 ||
           strcmp(name, curjobname) == 0 ||
           strcmp(prev, curjobname) == 0 ||
           strcmp(root, curjobname) == 0 ||
           strcmp(rootdiff, curjobname) == 0))
      {
         keep = true;
      } 
      
      if (keep) {
         if (fprintf(fout, "%s", buf) < 0) {
            berrno be;
            Jmsg(ctx, M_ERROR, "Unable to update the job history. ERR=%s\n",
                 be.bstrerror(errno));
            goto bail_out;
         }

      } else if (build_cmd) {
         count++;
         Dmsg(ctx, dbglvl+100, "Can prune jobname %s\n", curjobname);

         char *p2 = data;
         for(char *p = data; *p; p++) {
            if (*p == ',') {
               *p = 0;
               jobs->append(bstrdup(build_cmd(arg, p2, curjobname)));
               p2 = p + 1 ;
            }
         }
         jobs->append(bstrdup(build_cmd(arg, p2, curjobname)));

      } else if (jobs) {
         if (return_jobname_on_prune) {
	    jobs->append(bstrdup(curjobname));	
	 } else {
	    jobs->append(bstrdup(data));
	 }
      }
   }

   ok = true;

bail_out:
   if (f) {
      fclose(f);
   }
   if (fout) {
      fclose(fout);
   }

   /* We can switch the file */
   if (ok) {
      unlink(tmp);
      
      if (rename(tmpout, tmp) < 0) {
         berrno be;
         Jmsg(ctx, M_ERROR, "Unable to update the job history. ERR=%s\n",
              be.bstrerror(errno));
      }
   }

   V(joblist_mutex);
   free_pool_memory(tmp);
   free_pool_memory(tmpout);
   free_pool_memory(data);
   free_pool_memory(buf);
   free_pool_memory(curkey);
   free_pool_memory(curjobname);
   free_pool_memory(prevjob);
   free_pool_memory(rootjob);

   Dmsg(ctx, dbglvl+100, "Pruning %d jobs\n", count);
}


#endif  /* ! USE_JOB_LIST */

#ifdef USE_SP_DECODE
/* ***BEEF*** */
static int sp_decode(char *str, POOLMEM **ret)
{
   int  len;                    /* len of the original string */
   int  salt;                   /* salt key used to hide the len */
   char pepper;                 /* pepper key used to hide the string */
   int  retval  = 0;
   POOLMEM *buf = get_pool_memory(PM_FNAME);
   **ret = 0;

   /* The original string should be smaller than
    * the base64 form
    */
   buf = check_pool_memory_size(buf, strlen(str));
   base64_to_bin(buf, sizeof_pool_memory(buf),
                 str, strlen(str));

   /* The two intergers are on 3 digits, the password
    * is starting after 8 bytes
    */
   if (sscanf(buf, "%d:%d:", &salt, &len) != 2) {
      Dmsg0(0, "Unable to decode given malformed string\n");
      goto cleanup;
   }

   pepper = salt % 256;

   len = len ^ salt;            /* get original len */
   if (len > MAX_NAME_LENGTH || (len+8) >= sizeof_pool_memory(buf)) {
      Dmsg0(0, "Unable to decode given string, len is too long\n");
      goto cleanup;
   }

   /* skip %03d:%03d:pass, and force the end of string */
   buf[8+len] = 0;

   for(char *p = buf + 8 ; len > 0 ; len--) {
      *p = *p ^ pepper;
      p++;
   }
   pm_strcpy(ret, buf + 8);
   retval = 1;

cleanup:
   free_pool_memory(buf);
   return retval;
}

#endif

#ifdef USE_FULL_READ
/* Handle signal when reading from a pipe, functions based on
 * fread doesn't work very well.
 */
static int32_t full_read(int in, char *buf, uint32_t nbytes)  {
   size_t  nleft;
   ssize_t nread;
   nleft = nbytes;

   while (nleft > 0) {
      errno = 0;
      nread = read(in, buf, nleft);
      if (nread == -1) {
         if (errno == EINTR) {
            continue;
         } 
      }
      if (nread < 0) {
         return nread;             /* error */
      }

      nleft -= nread;
      buf += nread;

      if (nread == 0) {
         return nbytes - nleft;
      }
   }
   return nbytes - nleft;
};
#endif

#ifdef USE_FULL_WRITE
static int32_t full_write(int fd, const char *ptr, uint32_t nbytes, bool *canceled=NULL)
{
   size_t nleft;
   ssize_t nwritten;
   nleft = nbytes;

   while (nleft > 0 && (canceled == NULL || *canceled == false)) {
      do {
         errno = 0;
         nwritten = write(fd, ptr, nleft);
      } while (nwritten == -1 && errno == EINTR && (canceled == NULL || *canceled == false));

      if (nwritten <= 0) {
         return nwritten;          /* error */
      }
      nleft -= nwritten;
      ptr += nwritten;
   }
   return nbytes - nleft;
}
#endif

#ifdef USE_MAKEDIR
/* Skip leading slash(es) */
static bool makedir(char *path, bool is_dir, mode_t mode)
{
   struct stat statp;
   char *p = path;
   char *last;

   /* Handle specific windows paths */
#if HAVE_WIN32
   if (strncmp(p, "\\\\?\\", 4) == 0) {
      p += 4;                   /* Skip windows special path */
   }
   if (B_ISALPHA(p[0]) && p[1] == ':' && p[2] == '\\') {
      p += 3;                   /* Skip Windows drive */
   }
#endif
   while (IsPathSeparator(*p)) {
      p++;
   }
   last = p;
   while ((p = first_path_separator(p))) {
      char save_p;
      save_p = *p;
      *p = 0;
      if (mkdir(path, mode) != 0) {
         if (stat(path, &statp) != 0) {
            *p = save_p;
            return false;
         } else if (!S_ISDIR(statp.st_mode)) {
            *p = save_p;
            return false;
         }
      }
      *p = save_p;
      while (IsPathSeparator(*p)) {
         p++;
      }
      last = p;
   }
   if (is_dir && last && last[0] != '\0') {
      mkdir(path, mode);        // Create the last directory
   }
   return true;
}
#endif

#ifdef USE_LISTUSERS
/* TODO: Adapt the code for windows */
static bool list_users(alist *list, POOLMEM *&error)
{
   POOL_MEM tmp1;
   BPIPE *pfd;
   *error = 0;

   /* Simple way to list users, used by unix_user plugin parameter */
   pfd = open_bpipe((char *)"cut -d: -f1 /etc/passwd", 0, "r");
   if (!pfd) {
      Mmsg(error,  "Unable to list users");
      return false;
   }
   while (bfgets(tmp1.addr(), pfd->rfd)) {
      strip_trailing_junk(tmp1.c_str());
      if (tmp1.c_str()[0] && tmp1.c_str()[0] != '#') {
         list->append(bstrdup(tmp1.c_str()));
      }
   }
   if (close_bpipe(pfd) != 0) {
      Mmsg(error, "Unable to list users");
   }
   return list->size() > 0;
}
#endif  // USE_LISTUSERS
