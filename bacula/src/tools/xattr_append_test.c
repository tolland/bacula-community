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
#include "../lib/unittests.h"

#define CIFS_XATTR "system.cifs_acl"
#define CIFS_XATTR_LEN 15


int main(int argc, char **argv)
{
   Unittests u("t");
   POOL_MEM q;
   int l, l2;
   char buf[512];

   Mmsg(q, "test1");
   l = strlen(q.c_str());
   l2 = xattr_list_append(q.addr(), l, CIFS_XATTR, CIFS_XATTR_LEN);
   ok(l2 == (l+CIFS_XATTR_LEN+1), "Check add string");
   Dmsg1(0, "%s\n", asciidump(q.c_str(), l2, buf, sizeof(buf)));

   l = xattr_list_append(q.addr(), l2, CIFS_XATTR, CIFS_XATTR_LEN);
   ok(l == l2, "Check string already in (last pos)");
   Dmsg1(0, "%s\n", asciidump(q.c_str(), l, buf, sizeof(buf)));

   l = 5 + 1 + CIFS_XATTR_LEN + 1;
   memcpy(q.c_str(), "test1\0" CIFS_XATTR, l);
   l2 = xattr_list_append(q.addr(), l, CIFS_XATTR, CIFS_XATTR_LEN);
   ok(l == l2, "Check string already in (last pos)");
   Dmsg1(0, "%s\n", asciidump(q.c_str(), l2, buf, sizeof(buf)));

   l = 5 + 1 + 5 + 1;
   memcpy(q.c_str(), "test1\0test2\0", l);
   l2 = xattr_list_append(q.addr(), l, CIFS_XATTR, CIFS_XATTR_LEN);
   ok((l + CIFS_XATTR_LEN + 1) == l2, "Check string added");
   Dmsg1(0, "%s\n", asciidump(q.c_str(), l2, buf, sizeof(buf)));

   l = 5 + 1 + 5 + 1 + CIFS_XATTR_LEN + 1;
   memcpy(q.c_str(), "test1\0test2\0" CIFS_XATTR, l);
   l2 = xattr_list_append(q.addr(), l, CIFS_XATTR, CIFS_XATTR_LEN);
   ok(l == l2, "Check string already in (3rd)");
   Dmsg1(0, "%s\n", asciidump(q.c_str(), l2, buf, sizeof(buf)));

   l = 5 + 1 + 5 + 1 + CIFS_XATTR_LEN + 1 + 5 + 1;
   memcpy(q.c_str(), "test1\0test2\0" CIFS_XATTR "\0test3\0", l);
   l2 = xattr_list_append(q.addr(), l, CIFS_XATTR, CIFS_XATTR_LEN);
   ok(l == l2, "Check string already in (3rd)");
   Dmsg1(0, "%s\n", asciidump(q.c_str(), l2, buf, sizeof(buf)));

   Mmsg(q, "");
   l = strlen(q.c_str());
   l2 = xattr_list_append(q.addr(), l, CIFS_XATTR, CIFS_XATTR_LEN);
   ok(l2 == (l+CIFS_XATTR_LEN+1), "Check add string (empty)");
   Dmsg1(0, "%s\n", asciidump(q.c_str(), l2, buf, sizeof(buf)));

   return report();
}
