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

#include <sys/types.h>
#include <dirent.h>
#include "../bacula.h"
#include "../lib/unittests.h"

int breaddir(DIR *dirp, POOLMEM *&d_name);

void *th1(void *)
{
   int nb=0;
   intptr_t ret;
   POOL_MEM p;
   while (nb++ < 100) {
      DIR *dir = opendir(".");
      if (dir) {
         while ((ret = breaddir(dir, p.addr())) == 0) {
            /* nop */
         }
         if (ret > 0) {
            berrno be;
            Dmsg1(0, "errno=%s\n", be.bstrerror());
         }
         closedir(dir);
      }
   }
   return (void *)ret;
}


int main()
{
   void *ret;
   pthread_t t1[100];

   Unittests t("breaddir_test", true, true);
   t.configure(TEST_QUIET);

   for (int i=0; i < 100; i++) {
      pthread_create(&(t1[i]), NULL, th1, NULL);
   }
   for (int i=0; i < 100; i++) {
      pthread_join(t1[i], &ret);
      ok((intptr_t)ret == -1, "Check the readdir() value");
   }
   return report();
}
