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
/**
 * @file metaplugin_tests.cpp
 * @author Radosław Korzeniewski (radoslaw@korzeniewski.net)
 * @brief This is a Bacula metaplugin unittests
 * @version 1.0.0
 * @date 2021-04-07
 *
 * @copyright Copyright (c) 2021 All rights reserved.
 *            IP transferred to Bacula Systems according to agreement.
 */

#include "metaplugin.h"
#include "bacula.h"
#include "unittests.h"

/* Plugin Info definitions */
const char *PLUGIN_LICENSE       = "Bacula AGPLv3";
const char *PLUGIN_AUTHOR        = "Radoslaw Korzeniewski";
const char *PLUGIN_DATE          = "April 2021";
const char *PLUGIN_VERSION       = "1.0.0";
const char *PLUGIN_DESCRIPTION   = "Bacula Enterprise MetaPlugin";

/* Plugin compile time variables */
const char *PLUGINPREFIX         = "metaplugin:";
const char *PLUGINNAME           = "metaplugin";
const char *PLUGINNAMESPACE      = "@metaplugin";
const bool CUSTOMNAMESPACE       = true;
const char *PLUGINAPI            = "4";
const char *BACKEND_CMD          = "/bin/true";

static bRC mycheckFile(bpContext *ctx, char *fname);
checkFile_t checkFile = NULL;
// checkFile_t checkFile = mycheckFile;

struct ini_items plugin_items_dump[] = { {NULL, NULL, NULL, 0, NULL} };
const char * valid_params[] = { NULL };

#ifdef DEVELOPER
const metadataTypeMap plugin_metadata_map[] = {{"METADATA_STREAM", plugin_meta_blob}};
#else
const metadataTypeMap plugin_metadata_map[] = {{NULL, plugin_meta_invalid}};
#endif

/* In order to link with libbaccfg */
int32_t r_last;
int32_t r_first;
RES_HEAD **res_head;
bool save_resource(RES_HEAD **rhead, int type, RES_ITEM *items, int pass){return false;}
bool save_resource(CONFIG*, int, RES_ITEM*, int) {return false;}
void dump_resource(int type, RES *ares, void sendit(void *sock, const char *fmt, ...), void *sock){}
void free_resource(RES *rres, int type){}
union URES {};
RES_TABLE resources[] = {};
URES res_all;

static bRC mycheckFile(bpContext *ctx, char *fname)
{
   return bRC_Error;
}

struct vectstruct
{
   const char *fname;
   bRC status;
};

static const vectstruct testvect1[] = {
   {"/etc/passwd", bRC_OK},
   {"/@MYSQL/", bRC_OK},
   {"/@metaplugin/test/file", bRC_Seen},
   {"/@kubernetes/namespace/test", bRC_OK},
   {"@metaplugin/other", bRC_Seen},
   {"test/file", bRC_OK},
   {NULL, bRC_Error},
};

int main()
{
   Unittests metaplugin_test("metaplugin_test");

   struct bpContext bpctx = {};
   bpContext *bpctxp = &bpctx;
   POOL_MEM testfname(PM_FNAME);
   METAPLUGIN mp(bpctxp);

   for (int i = 0; testvect1[i].fname != NULL; i++){
      pm_strcpy(testfname, testvect1[i].fname);
      bRC status = mp.checkFile(bpctxp, testfname.c_str());
      ok(status == testvect1[i].status, testfname.c_str());
   }

   checkFile = mycheckFile;
   pm_strcpy(testfname, "@metaplugin/");
   ok(mp.checkFile(bpctxp, testfname.c_str()) == bRC_Error, "custom mycheckFile()");
}
