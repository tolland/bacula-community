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
 * This is a Bacula plugin for backup/restore Docker using native tools.
 */
/**
 * @file dkid_test.c
 * @author Radoslaw Korzeniewski (radoslaw@korzeniewski.net)
 * @brief This is a Bacula plugin for backup/restore Docker using native tools - unittest.
 * @version 1.2.1
 * @date 2020-01-05
 * This is a Bacula plugin for backup/restore Docker using native tools.
 *
 * @copyright Copyright (c) 2021 All rights reserved. IP transferred to Bacula Systems according to agreement.
 */

#include "bacula.h"
#include "unittests.h"
#include "dkid.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

const char *dig1     = "66f45d8601bae26a6b2ffeb46922318534d3b3905377b3a224693bd78601cb3b";
const char *sdig1    = "66f45d8601ba";
const int64_t vdig1  = 0x66f45d8601ba;
const char *dig2     = "B546087C43F75A2C1484B4AEE0737499AA69A09067B04237907FCCD4BDE938C7";
const char *sdig2    = "B546087C43F7";
const int64_t vdig2  = 0xb546087c43f7;
const char *sdig3    = "0f601bcb1ef5";
const int64_t vdig3  = 0x0f601bcb1ef5;
const char *sdig4    = "00571da76d";
const int64_t vdig4  = 0x00571da76d;
const char *dig5     = "sha256:daabf4372f900cb1ad0db17d26abf3acce55224275d1850f02459180e4dacf1d";
const char *tdig5    = "daabf4372f900cb1ad0db17d26abf3acce55224275d1850f02459180e4dacf1d";
const char *sdig5    = "daabf4372f90";
const int64_t vdig5  = 0xdaabf4372f90;
const char *sinv1    = "Invalid initialization string";
const char *sinv2    = "brave_edison";
const char *sinv3    = "0xDEADBEEF";
const char *sinv4    = "c0a478d317195b…";
const char *sinv5    = "a478d317195b…";
const char *sinv6    = "78d317195b…";

int main()
{
   Unittests dkid_test("dkid_test");
   char *p;
   int64_t v;
   POOL_MEM m(PM_FNAME);

   Pmsg0(0, "Initialize tests ...\n");

   DKID id1;
   ok(id1.id() == DKIDInvalid, "Check default initialization short");
   ok(strlen(id1.digest()) == 0, "Check default initialization full");
   ok(strlen(id1.digest_short()) == 0, "Check short default initialization full");

   DKID id2(dig1);
   ok(id2.id() == vdig1, "Check param initialization short");
   ok(bstrcmp(id2.digest(), dig1), "Check param initialization full");
   ok(bstrcmp(id2.digest_short(), sdig1), "Check short param initialization");

   DKID id3(dig2);
   ok(id3.id() == vdig2, "Check param initialization short upper");
   ok(bstrcmp(id3.digest(), dig2), "Check param initialization full upper");
   ok(bstrcmp(id3.digest_short(), sdig2), "Check short param initialization full upper");

   pm_strcpy(m, dig1);
   DKID id4(m);
   ok(id4.id() == vdig1, "Check pool_mem initialization short");
   ok(bstrcmp(id4.digest(), dig1), "Check pool_mem initialization full");
   ok(bstrcmp(id4.digest_short(), sdig1), "Check short pool_mem initialization full");

   DKID id5(sdig3);
   ok(id5.id() == vdig3, "Check short digest initialization");
   Mmsg(m, "%s(...)", sdig3);
   ok(bstrcmp(id5.digest(), m.c_str()), "Check short digest initialization full str");
   ok(bstrcmp(id5.digest_short(), sdig3), "Check short for short digest initialization");

   DKID id6(sdig4);
   ok(id6.id() == vdig4, "Check shorter digest initialization");
   Mmsg(m, "%s(...)", sdig4);
   ok(bstrcmp(id6.digest(), m.c_str()), "Check shorter digest initialization full str");
   ok(bstrcmp(id6.digest_short(), sdig4), "Check short for shorter digest initialization");

   DKID id7(dig5);
   ok(id7.id() == vdig5, "Check param initialization with sha256: prefix");
   ok(bstrcmp(id7.digest(), tdig5), "Check param initialization full with sha256: prefix");
   ok(bstrcmp(id7.digest_short(), sdig5), "Check short param initialization with sha256: prefix");

   Pmsg0(0, "Invalid initialization tests ...\n");

   DKID id8(sinv1);
   ok(id8.id() < 0, "Checking invalid digest string long");

   DKID id9(sinv2);
   ok(id9.id() < 0, "Checking invalid digest string short");

   DKID id10 (sinv3);
   ok(id10.id() < 0, "Checking invalid digest string hex");

   DKID id11(sinv4);
   ok(id11.id() >= 0, "Checking digest string with ellipsis");

   DKID id12(sinv5);
   ok(id12.id() >= 0, "Checking digest string with ellipsis short");

   DKID id13(sinv6);
   ok(id13.id() < 0, "Checking invalid digest string with ellipsis short");

   Pmsg0(0, "Operators tests ...\n");

   DKID id14(dig1);
   p = (char*)id14;
   ok(bstrcmp(p, dig1), "Checking operator char* ()");
   v = id14;
   ok(v == vdig1, "Checking operator int64_t ()");

   DKID *id14P = New(DKID(dig1));
   p = (char*)*id14P;
   ok(bstrcmp(p, dig1), "Checking operator char* () on ptr");
   v = *id14P;
   ok(v == vdig1, "Checking operator int64_t () on ptr");
   delete(id14P);

   DKID id15(dig2);
   DKID id15A(dig1);
   id15 = id15A;
   ok(id15A.id() == vdig1, "Checking operator= (DKID&)");
   ok(id15A == id15, "Checking operator== on the same");
   nok(id15A != id15, "Checking operator!= on the same");

   DKID id15B(dig1);
   id15A = (char*)dig2;
   ok(id15A.id() == vdig2, "Checking operator= (char*)");
   nok(id15A == id15B, "Checking operator== on different");
   ok(id15A != id15B, "Checking operator!= on different");

   Mmsg(m, "%s", sdig1);
   id15A = m;
   ok(id15A.id() == vdig1, "Checking operator= (POOL_MEM&)");

   id15A = (char*)dig2;
   ok(id15A.id() == vdig2, "Checking operator= (char*)");

   DKID id16(sinv1);
   id15A = id16;
   nok (id15A == id16, "Checking operator== on invalid digest");
   nok (id15A != id16, "Checking operator!= on invalid digest");

   DKID id17(sdig1);
   id15A = (char*)dig1;
   ok (id15A == id17, "Checking operator== on full and short digest");
   nok (id15A != id17, "Checking operator!= on full and short digest");

   return report();
}
