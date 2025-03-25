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

#ifndef UNITTESTS_H_
#define UNITTESTS_H_

#include "bacula.h"

/* Test success if value x is not zero */
#define ok(x, label) _ok(__FILE__, __LINE__, #x, (x), label)

/* Test success if value x is zero */
#define nok(x, label) _nok(__FILE__, __LINE__, #x, (x), label)

/* Test success if value x is a valid path */
#define stat_ok(x) _stat_ok(__FILE__, __LINE__, (x))
/* Test success if value x is not a valid path */
#define stat_nok(x) _stat_nok(__FILE__, __LINE__, (x))

/* Test if a variable has some value */
#define is(x, y, label) _is(__FILE__, __LINE__, #x, (x), (y), label)
#define isnt(x, y, label) _isnt(__FILE__, __LINE__, #x, (x), (y), label)

/* Test and return if not correct */
#define rok(x, label) { bool v=(x); if (!v) { _ok(__FILE__, __LINE__, #x, v, label); return 1; } }

/* TODO: log() ported from BEE it should be updated. */
#ifdef RTEST_LOG_THREADID
#define log(format, ...)  do { \
   pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL); \
   printf("%p: " format "\n", (void *)pthread_self(),  ##__VA_ARGS__ ); \
   pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);                 \
 } while (0)
#else
#define log(format, ...)  do { \
   printf("\n------------------------------------------\n" format "\n------------------------------------------\n", ##__VA_ARGS__ ); \
 } while (0)
#endif

enum {
   TEST_VERBOSE = 1,            // Display all messages
   TEST_QUIET   = 2,            // Display only ERROR messages
   TEST_END     = 4,            // Not used
   TEST_PRINT_LOCAL = 8         // Dump all local variables after an error
};

/* Configure the unittest framework with the TEST_ options */
void configure_test(uint64_t options);

bool _ok(const char *file, int l, const char *op, int value, const char *label);
bool _nok(const char *file, int l, const char *op, int value, const char *label);
bool _is(const char *file, int l, const char *op, const char *str, const char *str2, const char *label);
bool _isnt(const char *file, int l, const char *op, const char *str, const char *str2, const char *label);
bool _is(const char *file, int l, const char *op, int64_t nb, int64_t nb2, const char *label);
bool _isnt(const char *file, int l, const char *op, int64_t nb, int64_t nb2, const char *label);

bool _stat_ok(const char *file, int l, const char *path);
bool _stat_nok(const char *file, int l, const char *path);


/* Print report about the current usage */
int report();

/* Dummy function used by Bacula signal handler */
void terminate(int sig);

/* Print header message and configure some Bacula features (lock manager) */
void prolog(const char *name, bool lmgr=false, bool motd=true);

/* Unconfigure Bacula features (lock manager, poolmemory) */
void epilog();

/* Get the number of tests done */
int unittest_get_nb_tests();
int unittest_get_nb_errors();

/* Set the number of expected tests */
void unittest_set_nb_tests(int nb);

/* The class based approach for C++ geeks */
class Unittests
{
public:
   Unittests(const char *name, bool lmgr=false, bool motd=true);
   virtual ~Unittests() { epilog(); };
   void configure(uint64_t v) { configure_test(v); };
   void set_nb_tests(int nb) { unittest_set_nb_tests(nb); };
};

/* POOL_MEM subclass with convenience methods */
class bstring : public POOL_MEM {
public:

   bstring() : POOL_MEM() {};
   bstring(const char *str) : POOL_MEM(str) {};
   bstring(const bstring &fmt, ...): POOL_MEM() {
      va_list   arg_ptr;
      int len, maxlen;

      for (;;) {
         maxlen = this->max_size() - 1;
         va_start(arg_ptr, fmt);
         len = bvsnprintf(this->c_str(), maxlen, fmt, arg_ptr);
         va_end(arg_ptr);
         if (len < 0 || len >= (maxlen - 5)) {
            this->realloc_pm(maxlen + maxlen/2);
            continue;
         }
         break;
      }
   };

   bstring &operator=(const char * str) {
      pm_strcpy(*this, str);
      return *this;
   };

   // Allows cast to "char *"
   operator char *() const {
      return c_str();
   };

   int append(const char *str) {
      return strcat(str);
   }
};

/* Set of utilitary functions to manipulate the filesystem */
void fsu_mkfile(const char *path);
void fsu_mkfile(const char *path, const char *fcontents);
void fsu_mkdir(const char *path);
void fsu_mkpath(const char *path);
void fsu_rmdir(const char *path);
void fsu_rmfile(const char *path);
void fsu_touch(const char *path);
void fsu_mvfile(const char *src, const char *dst);
void fsu_cpfile(const char *src, const char *dst);
void unix_to_win_path(char *path);

#endif /* UNITTESTS_H_ */
