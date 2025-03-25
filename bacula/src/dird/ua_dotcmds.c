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
 *
 *   Bacula Director -- User Agent Commands
 *     These are "dot" commands, i.e. commands preceded
 *        by a period. These commands are meant to be used
 *        by a program, so there is no prompting, and the
 *        returned results are (supposed to be) predictable.
 *
 *     Kern Sibbald, April MMII
 *
 */

#include "bacula.h"
#include "dird.h"
#include "cats/bvfs.h"
#include "findlib/find.h"

/* Imported variables */
extern struct s_jl joblevels[];
extern struct s_jt jobtypes[];

/* Imported functions */
extern void do_messages(UAContext *ua, const char *cmd);
extern int quit_cmd(UAContext *ua, const char *cmd);
extern bool dot_status_cmd(UAContext *ua, const char *cmd);
extern void bvfs_set_acl(UAContext *ua, Bvfs *bvfs);
extern bool jlist_cmd(UAContext *ua, const char *cmd);
extern int qhelp_cmd(UAContext *ua, const char *cmd);

/* Forward referenced functions */
static bool admin_cmds(UAContext *ua, const char *cmd);
static bool jobscmd(UAContext *ua, const char *cmd);
static bool dotestimatecmd(UAContext *ua, const char *cmd);
static bool filesetscmd(UAContext *ua, const char *cmd);
static bool clientscmd(UAContext *ua, const char *cmd);
static bool msgscmd(UAContext *ua, const char *cmd);
static bool poolscmd(UAContext *ua, const char *cmd);
static bool schedulescmd(UAContext *ua, const char *cmd);
static bool storagecmd(UAContext *ua, const char *cmd);
static bool defaultscmd(UAContext *ua, const char *cmd);
static bool typescmd(UAContext *ua, const char *cmd);
static bool tagscmd(UAContext *ua, const char *cmd);
static bool backupscmd(UAContext *ua, const char *cmd);
static bool levelscmd(UAContext *ua, const char *cmd);
static bool getmsgscmd(UAContext *ua, const char *cmd);
static bool volstatuscmd(UAContext *ua, const char *cmd);
static bool mediatypescmd(UAContext *ua, const char *cmd);
static bool locationscmd(UAContext *ua, const char *cmd);
static bool mediacmd(UAContext *ua, const char *cmd);
static bool aopcmd(UAContext *ua, const char *cmd);
static bool catalogscmd(UAContext *ua, const char *cmd);

static bool dot_ls_cmd(UAContext *ua, const char *cmd);
static bool dot_bvfs_lsdirs(UAContext *ua, const char *cmd);
static bool dot_bvfs_lsfiles(UAContext *ua, const char *cmd);
static bool dot_bvfs_update(UAContext *ua, const char *cmd);
static bool dot_bvfs_get_jobids(UAContext *ua, const char *cmd);
static bool dot_bvfs_versions(UAContext *ua, const char *cmd);
static bool dot_bvfs_restore(UAContext *ua, const char *cmd);
static bool dot_bvfs_cleanup(UAContext *ua, const char *cmd);
static bool dot_bvfs_clear_cache(UAContext *ua, const char *cmd);
static bool dot_bvfs_decode_lstat(UAContext *ua, const char *cmd);
static bool dot_bvfs_update_fv(UAContext *ua, const char *cmd);
static bool dot_bvfs_get_volumes(UAContext *ua, const char *cmd);
static bool dot_bvfs_get_jobs(UAContext *ua, const char *cmd);
static bool dot_bvfs_get_bootstrap(UAContext *ua, const char *cmd);
static bool dot_bvfs_get_fileindex(UAContext *ua, const char *cmd);
static bool dot_bvfs_get_delta(UAContext *ua, const char *cmd);
static bool dot_bvfs_delete_fileid(UAContext *ua, const char *cmd);
static bool bvfs_get_filter(UAContext *ua, POOL_MEM &where, char *limit, int len);
static bool dot_setuid(UAContext *ua, const char *cmd);
static bool dot_querycmd(UAContext *ua, const char *cmd);
static bool putfile_cmd(UAContext *ua, const char *cmd);
static bool api_cmd(UAContext *ua, const char *cmd);
static bool sql_cmd(UAContext *ua, const char *cmd);
static bool dot_quit_cmd(UAContext *ua, const char *cmd);
static bool dot_help_cmd(UAContext *ua, const char *cmd);
static bool dot_add_events(UAContext *ua, const char *cmd);
static int one_handler(void *ctx, int num_field, char **row);
static bool dot_search(UAContext *ua, const char *cmd);

struct dcmd_struct { const char *key; bool (*func)(UAContext *ua, const char *cmd); const char *help; const char *usage;const bool use_in_rs;};
static struct dcmd_struct commands[] = { /* help */  /* can be used in runscript */
   { NT_(".api"),        api_cmd,        _("Set the API options"),  ".api 2\n.api 2 api_opts=j",             false},
   { NT_(".backups"),    backupscmd,     _("Display the list of the authorized Backup Job resources"), NULL, false},
   { NT_(".clients"),    clientscmd,     _("Display the list of the authorized Client resources"),     NULL, true},
   { NT_(".catalogs"),   catalogscmd,    _("Display the list of the authorized Catalog resources"),    NULL, false},
   { NT_(".defaults"),   defaultscmd,    _("Display default values for a Job/Pool/Storage/Client"),
     ".defaults job=<job>\n"
     "\t.defaults pool=<pool>\n"
     "\t.defaults storage=<sto>\n"
     "\t.defaults client=<cli>",                                    false},
 { NT_(".die"),        admin_cmds,               NULL,                                                 NULL, false},
 { NT_(".dump"),       admin_cmds,               NULL,                                                 NULL, false},
 { NT_(".exit"),       admin_cmds,               _("Exit bconsole"),                                   NULL, false},
 { NT_(".events"),     dot_add_events,           _("Send event from the console"),
   ".events source=<txt> type=<type> text=<txt>",               false},
 { NT_(".filesets"),   filesetscmd,              _("Display the list of the authorized FileSet resources"),      NULL, false},
 { NT_(".help"),       dot_help_cmd,             _("Display help message on commands"),   ".help .status", false},
 { NT_(".jobs"),       jobscmd,                  _("Display the list of the authorized Job resources"),          NULL, true},
 { NT_(".estimate"),   dotestimatecmd,           NULL,                                                           NULL, false},
 { NT_(".jlist"),      jlist_cmd,                _("JSON friendly version of the list command"),                 NULL, false},
 { NT_(".levels"),     levelscmd,                _("Display the list of the Job Levels"),                        NULL, false},
 { NT_(".messages"),   getmsgscmd,               NULL,       NULL, false},
 { NT_(".msgs"),       msgscmd,                  NULL,       NULL, false},
 { NT_(".pools"),      poolscmd,                 _("Display the list of the authorized Pool resources"),         NULL, true},
 { NT_(".quit"),       dot_quit_cmd,             NULL,       NULL, false},
 { NT_(".putfile"),    putfile_cmd,              NULL,       NULL, false}, /* use @putfile */
 { NT_(".search"),     dot_search,               _("Search in the catalog"),
   ".search text=xxx [category=(client|job|volume)]",           false},
 { NT_(".schedule"),   schedulescmd,             _("Display the list of the authorized Schedule resources"),     NULL, false},
 { NT_(".sql"),        sql_cmd,                  NULL,       NULL, false},
 { NT_(".status"),     dot_status_cmd,           _("Query Bacula daemons"),
   ".status dir [header|running [client=<cli>]|scheduled|terminated|statistics]\n"
   "\t.status client=<cli> [header|running|terminated]\n"
   "\t.status storage=<sto> [device=<dev>] [header|running|terminated|collector|devices]", false},
 { NT_(".storage"),    storagecmd,               _("Display the list of Storage resource defined"),       NULL, true},
 { NT_(".volstatus"),  volstatuscmd,             _("Display the list of the Volume statuses"),            NULL, true},
 { NT_(".media"),      mediacmd,                 _("Display the list of the authorized Volumes"),         NULL, true},
 { NT_(".mediatypes"), mediatypescmd,            _("Display the list of the defined MediaType"),          NULL, true},
 { NT_(".locations"),  locationscmd,             _("Display the list of the defined Location"),           NULL, true},
 { NT_(".actiononpurge"),aopcmd,                 _("Display the list of the possible values for ActionOnPurge"),       NULL, true},
 { NT_(".bvfs_lsdirs"), dot_bvfs_lsdirs,         NULL,       NULL, true},
 { NT_(".bvfs_lsfiles"),dot_bvfs_lsfiles,        NULL,       NULL, true},
 { NT_(".bvfs_get_volumes"),dot_bvfs_get_volumes,NULL,       NULL, true},
 { NT_(".bvfs_update"), dot_bvfs_update,         NULL,       NULL, true},
 { NT_(".bvfs_get_jobids"), dot_bvfs_get_jobids, NULL,       NULL, true},
 { NT_(".bvfs_get_jobs"), dot_bvfs_get_jobs,     NULL,       NULL, true},
 { NT_(".bvfs_get_bootstrap"), dot_bvfs_get_bootstrap,NULL,  NULL, true},
 { NT_(".bvfs_get_fileindex"), dot_bvfs_get_fileindex,NULL,  NULL, true},
 { NT_(".bvfs_versions"), dot_bvfs_versions,     NULL,       NULL, true},
 { NT_(".bvfs_get_delta"), dot_bvfs_get_delta,   NULL,       NULL, true},
 { NT_(".bvfs_restore"),  dot_bvfs_restore,      NULL,       NULL, true},
 { NT_(".bvfs_cleanup"),  dot_bvfs_cleanup,      NULL,       NULL, true},
 { NT_(".bvfs_decode_lstat"),dot_bvfs_decode_lstat,NULL,     NULL, true},
 { NT_(".bvfs_clear_cache"),dot_bvfs_clear_cache,NULL,       NULL, false},
 { NT_(".bvfs_update_fv"),dot_bvfs_update_fv,    NULL,       NULL, true},
 { NT_(".bvfs_delete_fileid"),dot_bvfs_delete_fileid,  NULL, NULL, false},
 { NT_(".setuid"), dot_setuid,                   _("Set UID/GID for the current session"), ".setuid uid=<uid> gid=<gid>", false},
 { NT_(".ls"), dot_ls_cmd,                       _("List files/directories on Client"),
                                                   ".ls client=<cli> path=<str> [dironly] [plugin=<str>]",       false},
 { NT_(".types"),      typescmd,                 _("List Job Types defined"),        NULL, false},
 { NT_(".query"),     dot_querycmd,              _("Query Plugin's Client"),
   ".query client=<cli> plugin=<str> parameter=<str>", false},
 { NT_(".tags"),      tagscmd,                   _("List debug tags defined"),       NULL, false}
};
#define comsize ((int)(sizeof(commands)/sizeof(struct dcmd_struct)))

/*
 * Execute a command from the UA
 */
bool do_a_dot_command(UAContext *ua)
{
   int i;
   int len;
   bool ok = false;
   bool found = false;

   Dmsg1(1400, "Dot command: %s\n", ua->UA_sock?ua->UA_sock->msg:"");
   if (ua->argc == 0) {
      return false;
   }

   len = strlen(ua->argk[0]);
   if (len == 1) {
      if (ua->api) ua->signal(BNET_CMD_BEGIN);
      if (ua->api) ua->signal(BNET_CMD_OK);
      return true;                    /* no op */
   }
   for (i=0; i<comsize; i++) {     /* search for command */
      if (strncasecmp(ua->argk[0],  _(commands[i].key), len) == 0) {
         /* Check if this command is authorized in RunScript */
         if (ua->runscript && !commands[i].use_in_rs) {
            ua->error_msg(_("Can't use %s command in a runscript"), ua->argk[0]);
            break;
         }
         bool gui = ua->gui;
         /* Check if command permitted, but "quit" is always OK */
         if (strcmp(ua->argk[0], NT_(".quit")) != 0 &&
             strcmp(ua->argk[0], NT_(".api"))  != 0 &&
             !acl_access_ok(ua, Command_ACL, ua->argk[0], len)) {
            Dmsg1(100, "not allowed %s\n", ua->cmd);
            break;
         }
         Dmsg1(100, "Cmd: %s\n", ua->cmd);
         ua->gui = true;
         if (ua->api) ua->signal(BNET_CMD_BEGIN);
         ok = (*commands[i].func)(ua, ua->cmd);   /* go execute command */
         if (ua->api) ua->signal(ok?BNET_CMD_OK:BNET_CMD_FAILED);
         ua->gui = gui;
         if (ua->UA_sock && ua->UA_sock->is_stop()) {
            found = false;
         } else {
            found = true;
         }
         break;
      }
   }
   if (!found) {
      ua->error_msg("%s%s", ua->argk[0], _(": is an invalid command.\n"));
      ok = false;
   }
   return ok;
}

/*
 * .setuid uid=<uid> gid=<gid>
 * Store the current uid/gid restriction for the current bconsole session
 */
static bool dot_setuid(UAContext *ua, const char *cmd)
{
   ua->uid = 0;
   ua->gid = 0;

   /* TODO: See if we check the UserIdACL to specify a uid/gid */
   for(int i=0; i < ua->argc ; i++) {
      if (strcasecmp(ua->argk[i], NT_("uid")) == 0) {
         if (is_a_number(ua->argv[i])) {
            ua->uid = str_to_int64(ua->argv[i]);
         }
      }

      if (strcasecmp(ua->argk[i], NT_("gid")) == 0) {
         if (is_a_number(ua->argv[i])) {
            ua->gid = str_to_int64(ua->argv[i]);
         }
      }
   }
   return true;
}

/*
 * .ls client=<cli> [plugin=<plugin>|path=<path>]
 * Send ls to Client to list files and directories
 */
static bool dot_ls_cmd(UAContext *ua, const char *cmd)
{
   POOL_MEM buf;
   CLIENT *client = NULL;
   char *path = NULL;
   char *plugin = NULL;
   JCR *jcr = ua->jcr;
   int i;
   bool ret = false;
   bool dironly = false;

   jcr->setJobLevel(L_FULL);
   i = find_arg_with_value(ua, NT_("client"));
   if (i > 0) {
      client = GetClientResWithName(ua->argv[i]);
      if (!client) {
         ua->error_msg(_("Client \"%s\" not found.\n"), ua->argv[i]);
         return false;
      }
      if (!acl_access_client_ok(ua, client->name(), JT_BACKUP)) {
         ua->error_msg(_("No authorization for Client \"%s\"\n"), client->name());
         return false;
      }

   } else {
      ua->error_msg(_("Client name missing.\n"));
      return false;
   }

   i = find_arg_with_value(ua, NT_("path"));
   if (i > 0) {
      path = ua->argv[i];

   } else {
      ua->error_msg(_("path name missing.\n"));
      return false;
   }

   i = find_arg(ua, NT_("dironly"));
   if (i > 0) {
      dironly = true;
   }

   /* optional plugin=... parameter */
   i = find_arg_with_value(ua, NT_("plugin"));
   if (i > 0) {
      plugin = ua->argv[i];
   }

   jcr->client = client;

   jcr->setJobType(JT_BACKUP);
   jcr->start_time = time(NULL);
   init_jcr_job_record(jcr);           // need job

   ua->send_msg(_("Connecting to Client %s at %s:%d\n"),
                jcr->client->name(), get_client_address(jcr, jcr->client, buf.addr()),
                jcr->client->FDport);

   if (!connect_to_file_daemon(jcr, 1, 15, 0)) {
      ua->error_msg("%s", jcr->errmsg);
      goto bail_out;
   }

   /* when .ls plugin prepare a special ls_plugin_fileset */
   if (plugin){
      if (!send_ls_plugin_fileset(jcr, plugin, path)) {
         ua->error_msg(_("Failed to send plugin command to Client.\n"));
         goto bail_out;
      }
   } else {
      if (!send_ls_fileset(jcr, path, dironly)) {
         ua->error_msg(_("Failed to send command to Client.\n"));
         goto bail_out;
      }
   }

   jcr->file_bsock->fsend("estimate listing=%d\n", 1);
   while (jcr->file_bsock->recv() >= 0) {
      ua->send_msg("%s", jcr->file_bsock->msg);
   }

   ret = true;

bail_out:
   if (jcr->file_bsock) {
      jcr->file_bsock->signal(BNET_TERMINATE);
      free_bsock(ua->jcr->file_bsock);
   }
   jcr->client = NULL;
   return ret;
}

#ifdef COMMUNITY
void bvfs_set_acl(UAContext *ua, Bvfs *bvfs)
{
}
#endif

/*
 * .bvfs_decode_lstat lstat="AAAA AAAA AAAA AAAA"
 * Decode the Lstat database field
 */
static bool dot_bvfs_decode_lstat(UAContext *ua, const char *cmd)
{
   int32_t LinkFI;
   struct stat sp;
   POOL_MEM q;
   char buf[32];
   int pos = find_arg_with_value(ua, "lstat");

   if (pos > 0) {
      for (char *p = ua->argv[pos] ; *p ; p++) {
         if (! (B_ISALPHA(*p) || B_ISDIGIT(*p) || B_ISSPACE(*p) || *p == '/' || *p == '+' || *p == '-')) {
            ua->error_msg("Can't accept %c in lstat\n", *p);
            return true;
         }
      }

      decode_stat(ua->argv[pos], &sp, sizeof(sp), &LinkFI);
      encode_mode(sp.st_mode, buf);
      Mmsg(q, "st_nlink=%lld\nst_mode=%lld\nperm=%s\nst_uid=%lld\nst_gid=%lld\n"
              "st_size=%lld\nst_blocks=%lld\nst_ino=%lld\nst_ctime=%lld\n"
              "st_mtime=%lld\nst_atime=%lld\nst_dev=%lld\nLinkFI=%lld\n",
           (int64_t) sp.st_nlink,
           (int64_t) sp.st_mode,
           buf,
           (int64_t) sp.st_uid,
           (int64_t) sp.st_gid,
           (int64_t) sp.st_size,
           (int64_t) sp.st_blocks,
           (int64_t) sp.st_ino,
           (int64_t) sp.st_ctime,
           (int64_t) sp.st_mtime,
           (int64_t) sp.st_atime,
           (int64_t) sp.st_dev,
           (int64_t) LinkFI
         );

      ua->send_msg("%s", q.c_str());
   }
   return true;
}

static bool dot_bvfs_update(UAContext *ua, const char *cmd)
{
   if (!open_new_client_db(ua)) {
      return 1;
   }

   int pos = find_arg_with_value(ua, "jobid");
   if (pos != -1 && is_a_number_list(ua->argv[pos])) {
      if (!bvfs_update_path_hierarchy_cache(ua->jcr, ua->db, ua->argv[pos])) {
         ua->error_msg("ERROR: BVFS reported a problem for %s\n",
                       ua->argv[pos]);
      }
   } else {
      /* update cache for all jobids */
      bvfs_update_cache(ua->jcr, ua->db);
   }

   return true;
}

static bool dot_bvfs_update_fv(UAContext *ua, const char *cmd)
{
   int pos = find_arg_with_value(ua, "jobid");

   if (pos == -1 || !is_a_number_list(ua->argv[pos])) {
      ua->error_msg("Expecting to find jobid=1,2,3 argument\n");
      return 1;
   }

   if (!open_new_client_db(ua)) {
      return 1;
   }

   bvfs_update_path_hierarchy_cache(ua->jcr, ua->db, ua->argv[pos]);
   bvfs_update_fv_cache(ua->jcr, ua->db, ua->argv[pos]);

   ua->info_msg("OK\n");

   return true;
}

static bool dot_bvfs_delete_fileid(UAContext *ua, const char *cmd)
{
   char *jobid, *fileid;
   int pos = find_arg_with_value(ua, "jobid");

   if (pos == -1 || !is_a_number_list(ua->argv[pos])) {
      ua->error_msg("Expecting to find jobid=1,2,3 argument\n");
      return 1;
   }
   jobid = ua->argv[pos];

   pos = find_arg_with_value(ua, "fileid");
   if (pos == -1 || !is_a_number_list(ua->argv[pos])) {
      ua->error_msg("Expecting to find fileid=1,2,3 argument\n");
      return 1;
   }
   fileid = ua->argv[pos];

   if (!open_new_client_db(ua)) {
      return 1;
   }

   Bvfs fs(ua->jcr, ua->db);
   bvfs_set_acl(ua, &fs);
   fs.set_jobids(jobid);
   ua->bvfs = &fs;

   if (fs.delete_fileid(fileid)) {
      ua->info_msg("OK\n");
   } else {
      ua->error_msg("Unable to delete fileid=%s\n", fileid);
   }
   return true;
}

static bool dot_bvfs_clear_cache(UAContext *ua, const char *cmd)
{
   if (!open_client_db(ua)) {
      return 1;
   }

   int pos = find_arg(ua, "yes");
   if (pos != -1) {
      Bvfs fs(ua->jcr, ua->db);
      fs.clear_cache();
      ua->info_msg("OK\n");
   } else {
      ua->error_msg("Can't find 'yes' argument\n");
   }

   return true;
}

static int bvfs_result_handler(void *ctx, int fields, char **row)
{
   UAContext *ua = (UAContext *)ctx;
   struct stat statp;
   int32_t LinkFI;
   Bvfs *fs = (Bvfs*) ua->bvfs;
   char *fileid=row[BVFS_FileId];
   char *lstat=row[BVFS_LStat];
   char *jobid=row[BVFS_JobId];

   char empty[] = "A A A A A A A A A A A A A A";
   char zero[] = "0";

   /* We need to deal with non existant path */
   if (!fileid || !is_a_number(fileid)) {
      lstat = empty;
      jobid = zero;
      fileid = zero;
   }

   memset(&statp, 0, sizeof(struct stat));
   decode_stat(lstat, &statp, sizeof(statp), &LinkFI);
   if (!fs->can_access(&statp)) {
      Dmsg5(100, "Access denied for %d:%d (%d:%d) st_mode=%d\n",
            (int)ua->uid, (int)ua->gid, (int)statp.st_uid, (int)statp.st_gid, (int)statp.st_mode);
      return 0;
   }
   Dmsg1(100, "type=%s\n", row[0]);
   if (bvfs_is_dir(row)) {
      char *path = bvfs_basename_dir(row[BVFS_Name]);
      ua->send_msg("%s\t0\t%s\t%s\t%s\t%s\n", row[BVFS_PathId], fileid,
                   jobid, lstat, path);

   } else if (bvfs_is_version(row)) {
      ua->send_msg("%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n", row[BVFS_PathId],
                   row[BVFS_FilenameId], fileid, jobid,
                   lstat, row[BVFS_Md5], row[BVFS_VolName],
                   row[BVFS_VolInchanger], row[BVFS_VolType]);

   } else if (bvfs_is_file(row)) {
      ua->send_msg("%s\t%s\t%s\t%s\t%s\t%s\n", row[BVFS_PathId],
                   row[BVFS_FilenameId], fileid, jobid,
                   lstat, row[BVFS_Name]);

   } else if (bvfs_is_volume_list(row)) {
      ua->send_msg("%s\t%s\t%s\n", row[BVFS_VolName],
                   row[BVFS_VolInchanger], row[BVFS_VolType]);

   } else if (bvfs_is_delta_list(row)) {
      ua->send_msg("%s\t%s\t%s\t%s\t%s\t%s\t%s\n", row[BVFS_PathId],
                   row[BVFS_FilenameId], fileid, jobid,
                   lstat, row[BVFS_DeltaSeq], row[BVFS_JobTDate]);
   }

   return 0;
}

static void parse_list(char *items, alist *list)
{
   char *start;
   for(char *p = start = items; *p ; p++) {
      if (*p == ',') {
         *p = 0;
         if (p > start) {
            list->append(bstrdup(start));
         }
         *p = ',';
         start = p + 1;
      }
   }
   if (*start) {
      list->append(bstrdup(start));
   }
}

static bool bvfs_parse_arg_version(UAContext *ua,
                                   char **client,
                                   alist *clients,
                                   FileId_t *fnid,
                                   bool *versions,
                                   bool *copies)
{
   bool fnid_found=false;
   *client=NULL;
   if (copies) {
      *copies=false;
   }
   if (fnid) {
      *fnid=0;
   }
   if (versions) {
      *versions=false;
   }

   for (int i=1; i<ua->argc; i++) {
      if (fnid && strcasecmp(ua->argk[i], NT_("fnid")) == 0) {
         if (is_a_number(ua->argv[i])) {
            fnid_found = true;
            *fnid = str_to_int64(ua->argv[i]);
         }
      }

      if (strcasecmp(ua->argk[i], NT_("client")) == 0) {
         *client = ua->argv[i];
         if (clients) {
            clients->append(bstrdup(*client));
         }
      }

      if (clients != NULL && strcasecmp(ua->argk[i], NT_("clients")) == 0) {
         /* Turn client1,client2,client3 to a alist of clients */
         parse_list(ua->argv[i], clients);
      }

      if (copies && strcasecmp(ua->argk[i], NT_("copies")) == 0) {
         *copies = true;
      }

      if (versions && strcasecmp(ua->argk[i], NT_("versions")) == 0) {
         *versions = true;
      }
   }
   return ((*client || (clients && clients->size() > 0)) && fnid_found);
}

static bool bvfs_parse_arg(UAContext *ua,
                           DBId_t *pathid, char **path, char **jobid,
                           char **username,
                           int *limit, int *offset)
{
   if (pathid) {
      *pathid=0;
   }
   if (path) {
      *path=NULL;
   }
   *limit=2000;
   *offset=0;
   *username=NULL;
   if (jobid) {
      *jobid=NULL;
   }

   for (int i=1; i<ua->argc; i++) {
      if (!ua->argv[i]) {
         continue;
      }
      if (pathid && strcasecmp(ua->argk[i], NT_("pathid")) == 0) {
         if (is_a_number(ua->argv[i])) {
            *pathid = str_to_int64(ua->argv[i]);
         }
      }

      if (path && strcasecmp(ua->argk[i], NT_("path")) == 0) {
         *path = ua->argv[i];
      }

      if (strcasecmp(ua->argk[i], NT_("username")) == 0) {
         *username = ua->argv[i];
      }

      if (jobid && strcasecmp(ua->argk[i], NT_("jobid")) == 0) {
         if (is_a_number_list(ua->argv[i])) {
            *jobid = ua->argv[i];
         }
      }

      if (strcasecmp(ua->argk[i], NT_("ujobid")) == 0) {
         JOB_DBR jr;
         memset(&jr, 0, sizeof(jr));
         bstrncpy(jr.Job, ua->argv[i], sizeof(jr.Job));
         if (!open_new_client_db(ua)) {
            return false;
         }
         if (!db_get_job_record(ua->jcr, ua->db, &jr)) {
            return false;
         }
         if (!acl_access_ok(ua, Job_ACL, jr.Name)) {
            return false;
         }
         /* Store the jobid after the ua->cmd, a bit kluggy */
         int len = strlen(ua->cmd);
         ua->cmd = check_pool_memory_size(ua->cmd, len + 1 + 50);
         if (jobid) {
            *jobid = edit_uint64(jr.JobId, ua->cmd + len + 1);
         }
      }

      if (strcasecmp(ua->argk[i], NT_("limit")) == 0) {
         if (is_a_number(ua->argv[i])) {
            *limit = str_to_int64(ua->argv[i]);
         }
      }

      if (strcasecmp(ua->argk[i], NT_("offset")) == 0) {
         if (is_a_number(ua->argv[i])) {
            *offset = str_to_int64(ua->argv[i]);
         }
      }
   }

   if (jobid && *jobid == NULL) {
      return false;
   }

   if (path && pathid) {
      if (!(*pathid || *path)) {
         return false;
      }
   }

   return true;
}

static int get_client_jobids(UAContext *ua, Bvfs *fs)
{
   POOL_MEM where;
   char limit[50];
   alist clients(10, owned_by_alist);
   db_list_ctx jobids;
   bool ret, use_all_jobtype=false;

   int pos = find_arg_with_value(ua, "client");
   if (pos < 0) {
      pos = find_arg_with_value(ua, "clients");
   }
   if (pos < 0) {
      Dmsg0(50, "client not found on command line\n");
      return -1;
   }
   /* Turn client1,client2,client3 to a alist of clients */
   parse_list(ua->argv[pos], &clients);

   db_lock(ua->db);
   use_all_jobtype = bvfs_get_filter(ua, where, limit, sizeof(limit));
   Mmsg(ua->db->cmd,
      "SELECT JobId "
        "FROM Job JOIN Client USING (ClientId) "
         "WHERE Client.Name IN (%s) "
           "AND %s AND Job.JobStatus IN ('T', 'W') %s "
         "ORDER By JobTDate ASC %s",
           fs->escape_list(&clients),
           use_all_jobtype ? " Job.Type IN ('B', 'C', 'M') " : " Job.Type = 'B' ",
           where.c_str(), limit);
   ret = db_sql_query(ua->db, ua->db->cmd, db_list_handler, &jobids);
   db_unlock(ua->db);

   if (!ret) {
      ua->error_msg(_("Unable to get last Job record for Client=%s\n"),
                    ua->argv[pos]);
      return -1;
   }
   Dmsg2(50, "jobid for %s is %s\n", ua->argv[pos], jobids.list);
   return fs->set_jobids(jobids.list);
}

/* .bvfs_cleanup path=b2XXXXX
 */
static bool dot_bvfs_cleanup(UAContext *ua, const char *cmd)
{
   int i;
   if ((i = find_arg_with_value(ua, "path")) >= 0) {
      if (!open_client_db(ua)) {
         return 1;
      }
      Bvfs fs(ua->jcr, ua->db);
      fs.drop_restore_list(ua->argv[i]);
   }
   return true;
}

static bool check_plugin_object_acl(UAContext *ua, OBJECT_DBR *obj_r, JOB_DBR *jr)
{
   bool ret = false;
   memset(jr, 0, sizeof(JOB_DBR));

   jr->JobId = obj_r->JobId;
   if (!db_get_job_record(ua->jcr, ua->db, jr)) {
      ua->error_msg(_("Unable to get Job record for JobId=%s: ERR=%s\n"),
                    ua->cmd, db_strerror(ua->db));
      goto bail_out;
   }

   if (!acl_access_ok(ua, Job_ACL, jr->Name)) {
      ua->error_msg(_("Access to JobId=%d (Job \"%s\") not authorized.\n"),
            jr->JobId, jr->Name);
      goto bail_out;
   }

   CLIENT_DBR cr;
   cr.ClientId = jr->ClientId;
   if (!db_get_client_record(ua->jcr, ua->db, &cr)) {
      ua->error_msg(_("Unable to get Job record for JobId=%s: ERR=%s\n"),
                    ua->cmd, db_strerror(ua->db));
      goto bail_out;
   }

   if (!acl_access_ok(ua, Client_ACL, cr.Name)) {
      ua->error_msg(_("Access to ClientId=%d not authorized.\n"),
            jr->ClientId);
      goto bail_out;
   }

   ret = true;

bail_out:
   return ret;
}

static bool compute_ids_from_object(UAContext *ua, DBId_t objectid,
                                    db_list_ctx *jobid, db_list_ctx *fileid, db_list_ctx *dirid)
{
   bool ret = false;
   POOL_MEM query, filename, obj_path;
   OBJECT_DBR obj_r;
   JOB_DBR jr;
   db_list_ctx tempids;
   obj_r.ObjectId = objectid;

   if (!db_get_plugin_object_record(ua->jcr, ua->db, &obj_r)) {
      goto bail_out;
   }

   if (!check_plugin_object_acl(ua, &obj_r, &jr)) {
      goto bail_out;
   }

   jr.JobLevel = L_INCREMENTAL; /* Take Full+Diff+Incr */
   /* Compute the list of the jobs that are involved with this object */
   if (!db_get_accurate_jobids(ua->jcr, ua->db, &jr, &tempids)) {
      goto bail_out;
   }
   jobid->add(tempids);

   if (obj_r.Filename[0] != 0) {
      filename.check_size(strlen(obj_r.Filename)*2+1);
      db_escape_string(ua->jcr, ua->db, filename.c_str(), obj_r.Filename, strlen(obj_r.Filename));
   }

   if (obj_r.Path[0] != 0) {
      obj_path.check_size(strlen(obj_r.Path)*2+1);
      db_escape_string(ua->jcr, ua->db, obj_path.c_str(), obj_r.Path, strlen(obj_r.Path));
   }

   if (obj_r.Filename[0] != 0) {
      Mmsg(query, "SELECT FileId FROM File WHERE PathId=(SELECT PathId FROM Path WHERE Path='%s') "
               "AND Filename='%s' AND JobId=%ld", obj_path.c_str(), filename.c_str(), obj_r.JobId);
      if (!db_sql_query(ua->db, query.c_str(), db_list_handler, fileid)) {
         ua->error_msg(_("SQL query: %s failed with error: %s\n"),
                       query.c_str(), db_strerror(ua->db));
         goto bail_out;
      }

   } else {
      Mmsg(query, "SELECT PathId FROM Path WHERE Path='%s'", obj_path.c_str());
      if (!db_sql_query(ua->db, query.c_str(), db_list_handler, dirid)) {
         ua->error_msg(_("SQL query: %s failed with error: %s\n"),
                       query.c_str(), db_strerror(ua->db));
         goto bail_out;
      }

   }

   ret = true;

bail_out:
   return ret;
}

/* .bvfs_restore path=b2XXXXX jobid=1,2 fileid=1,2 dirid=1,2 fileindex=jobid,fidx,jobid,fidx objectid=1,2
 */
static bool dot_bvfs_restore(UAContext *ua, const char *cmd)
{
   DBId_t pathid=0;
   int limit=2000, offset=0, i;
   char *path=NULL, *jobid = NULL, *username=NULL, *fileindex=NULL;
   db_list_ctx jobids, fileid, dirid;
   bool object = false, ret = false;

   ret = bvfs_parse_arg(ua, &pathid, &path, &jobid, &username,
                       &limit, &offset);

   if (path && find_arg(ua, "objectid") > 0) {
      object = true;            /* restoring by objectid */
   } else if (!ret || !path) {
      ua->error_msg("Can't find jobid, pathid or path argument\n");
      return true;              /* not enough param */
   }

   if (!open_new_client_db(ua)) {
      return true;
   }

   /* If user specified jobids list as well as objectid we need to copy it into poolmem to
    * append the jobid related to the plugin object. */
   if (jobid) {
      jobids.add(jobid);
   }

   Bvfs fs(ua->jcr, ua->db);
   bvfs_set_acl(ua, &fs);
   fs.set_username(username);

   if ((i = find_arg_with_value(ua, "fileid")) >= 0) {
      if (ua->argv[i][0] != '\0' && !is_a_number_list(ua->argv[i])) {
         ua->error_msg("Please provide fileid as a list of integers!\n");
         return true;
      }
      fileid.add(ua->argv[i]);
   }

   if ((i = find_arg_with_value(ua, "dirid")) >= 0) {
      if (ua->argv[i][0] != '\0' && !is_a_number_list(ua->argv[i])) {
         ua->error_msg("Please provide dirid as a list of integers!\n");
         return true;
      }
      dirid.add(ua->argv[i]);
   }

   if ((i = find_arg_with_value(ua, "fileindex")) >= 0) {
      if (ua->argv[i][0] != '\0' && !is_a_number_list(ua->argv[i])) {
         ua->error_msg("Please provide fileid as a list of integers!\n");
         return true;
      }

      int64_t jobid, fidx;
      sellist sel;
      sel.set_string(ua->argv[i], true);

      /* check the format */
      foreach_sellist(jobid, &sel) {
         fidx = sel.next();
         if (fidx <= 0) {
            ua->error_msg("Please provide jobid,fileindex as a list of integers!\n");
            return true;
         }
      }
      fileindex = ua->argv[i];
   }

   if (object) {
      i = find_arg_with_value(ua, "objectid");
      if (ua->argv[i][0] != '\0' && !is_a_number_list(ua->argv[i])) {
         ua->error_msg("Please provide objectid as a list of integers!\n");
         return true;
      }
      int objectid;
      sellist sel;
      sel.set_string(ua->argv[i], true);

      foreach_sellist(objectid, &sel) {
         if (!compute_ids_from_object(ua, objectid, &jobids, &fileid, &dirid)) {
            return true;

         }
      }
   }

   /* Now jobid list is complete and can be used */
   fs.set_jobids(jobids.list);

   if ((i = find_arg(ua, "nodelta")) >= 0) {
      fs.set_compute_delta(false);
   }
   if (fs.compute_restore_list(fileid.list, dirid.list, fileindex, path)) {
      ua->send_msg("OK\n");
   } else {
      ua->error_msg("Can't create restore list\n");
   }

   return true;
}

/* Get a bootstrap for a given bvfs restore session
 * .bvfs_get_bootstrap path=b21xxxxxx
 * Volume=Vol1
 * Storage=Store1
 * VolAddress=10
 * VolSessionTime=xxx
 * VolSessionId=yyyy
 */
static bool dot_bvfs_get_bootstrap(UAContext *ua, const char *cmd)
{
   RESTORE_CTX rx;                    /* restore context */
   POOLMEM *buf = get_pool_memory(PM_MESSAGE);
   int pos;

   new_rx(&rx);
   if (!open_new_client_db(ua)) {
      ua->error_msg("ERROR: Unable to open database\n");
      goto bail_out;
   }
   pos = find_arg_with_value(ua, "path");
   if (pos < 0) {
      ua->error_msg("ERROR: Unable to get path argument\n");
      goto bail_out;
   }

   insert_table_into_findex_list(ua, &rx, ua->argv[pos]);

   if (rx.bsr_list->size() > 0) {
      if (!complete_bsr(ua, rx.bsr_list)) {   /* find Vol, SessId, SessTime from JobIds */
         ua->error_msg("ERROR: Unable to construct a valid BSR. Cannot continue.\n");
         goto bail_out;
      }
      if (!(rx.selected_files = write_bsr_file(ua, rx))) {
         ua->error_msg("ERROR: No files selected to be restored.\n");
         goto bail_out;
      }
      FILE *fp = bfopen(ua->jcr->RestoreBootstrap, "r");
      if (!fp) {
         ua->error_msg("ERROR: Unable to open bootstrap file\n");
         goto bail_out;
      }
      while (bfgets(buf, fp)) {
         ua->send_msg("%s", buf);
      }
      fclose(fp);
   } else {
      ua->error_msg("ERROR: Unable to find files to restore\n");
      goto bail_out;
   }

bail_out:
   if (ua->jcr->unlink_bsr) {
      unlink(ua->jcr->RestoreBootstrap);
      ua->jcr->unlink_bsr = false;
   }
   free_pool_memory(buf);
   free_rx(&rx);
   return true;
}

/* List FileMedia index for a given File
 * .bvfs_get_fileindex fileid=10 jobid=5
 */
static bool dot_bvfs_get_fileindex(UAContext *ua, const char *cmd)
{
   POOLMEM *buf = get_pool_memory(PM_MESSAGE);
   int pos;
   int64_t fileid=0;
   DBId_t fileindex=0;
   DBId_t jobid=0;
   JOB_DBR jr;
   memset(&jr, 0, sizeof(jr));

   if (!open_new_client_db(ua)) {
      ua->error_msg("ERROR: Unable to open database\n");
      goto bail_out;
   }
   if ((pos = find_arg_with_value(ua, "fileindex")) > 0) {
      fileindex = str_to_int64(ua->argv[pos]);
   }

   if ((pos = find_arg_with_value(ua, "fileid")) > 0) {
      fileid = str_to_int64(ua->argv[pos]);
   }

   if ((pos = find_arg_with_value(ua, "jobid")) > 0) {
      jobid = str_to_int64(ua->argv[pos]);
   }

   if (!jobid || (!fileid && !fileindex)) {
      ua->error_msg("ERROR: Unable to get jobid, fileid or fileindex arguments\n");
      goto bail_out;
   }

   jr.JobId = jobid;
   if (!db_get_job_record(ua->jcr, ua->db, &jr)) {
      ua->error_msg("ERROR: Unable to get Job record\n");
      goto bail_out;
   }
   if (!acl_access_ok(ua, Job_ACL, jr.Name)) {
      ua->error_msg("ERROR: Job unauthorized\n");
      goto bail_out;
   }

   if (fileid && !fileindex) {
      FILE_DBR fr;
      memset(&fr, 0, sizeof(fr));
      fr.FileId = fileid;
      fr.JobId = jobid;
      if (!db_get_file_record(ua->jcr, ua->db, &jr, &fr)) {
         ua->error_msg("ERROR: Unable to get File record\n");
         goto bail_out;
      }
      if (!fr.FileIndex) {
         ua->error_msg("ERROR: Unable to get File record\n");
         goto bail_out;
      }
      fileindex = fr.FileIndex;
   }

   db_list_filemedia_records(ua->jcr, ua->db, jobid, fileindex, prtit, ua, ARG_LIST);

bail_out:
   free_pool_memory(buf);
   return true;
}

/*
 * .bvfs_get_volumes [path=/ filename=test jobid=1 | fileid=1]
 * Vol001
 * Vol002
 * Vol003
 */
static bool dot_bvfs_get_volumes(UAContext *ua, const char *cmd)
{
   DBId_t pathid=0;
   FileId_t fileid=0;
   char  *path=NULL, *jobid=NULL, *username=NULL;
   char  *filename=NULL;
   int    limit=2000, offset=0;
   int    i;

   bvfs_parse_arg(ua, &pathid, &path, &jobid, &username, &limit, &offset);

   if ((i = find_arg_with_value(ua, "filename")) >= 0) {
      if (!(jobid && (path || pathid))) { /* Need JobId and Path/PathId */
         ua->error_msg("Can't find jobid, pathid or path argument\n");
         return true;
      }

      filename = ua->argv[i];

   } else if ((i = find_arg_with_value(ua, "fileid")) >= 0) {
      if (!is_a_number(ua->argv[i])) {
         ua->error_msg("Expecting integer for FileId, got %s\n", ua->argv[i]);
         return true;
      }
      fileid = str_to_int64(ua->argv[i]);
   }

   if (!open_new_client_db(ua)) {
      return 1;
   }

   Bvfs fs(ua->jcr, ua->db);
   bvfs_set_acl(ua, &fs);
   fs.set_username(username);
   fs.set_handler(bvfs_result_handler, ua);
   fs.set_limit(limit);
   ua->bvfs = &fs;

   if (filename) {
      /* TODO */

   } else {
      fs.get_volumes(fileid);
   }
   ua->bvfs = NULL;
   return true;
}

/*
 * .bvfs_lsfiles allfiles jobid=1,2,3,4
 */
static bool dot_bvfs_ls_all_files(UAContext *ua, const char *cmd)
{
   int limit=2000, offset=0;
   char *jobid=NULL, *username=NULL;
   char *pattern=NULL, *filename=NULL;
   int i;

   if (!bvfs_parse_arg(ua, NULL, NULL, &jobid, &username,
                       &limit, &offset))
   {
      ua->error_msg("Can't find jobid argument\n");
      return true;              /* not enough param */
   }
   if ((i = find_arg_with_value(ua, "pattern")) >= 0) {
      pattern = ua->argv[i];
   }
   if ((i = find_arg_with_value(ua, "filename")) >= 0) {
      filename = ua->argv[i];
   }

   if (!open_new_client_db(ua)) {
      return 1;
   }

   Bvfs fs(ua->jcr, ua->db);
   bvfs_set_acl(ua, &fs);
   fs.set_username(username);
   if (str_to_int64(jobid) == 0) {
      if (get_client_jobids(ua, &fs) <= 0) {
         goto bail_out;
      }
   } else {
      fs.set_jobids(jobid);
   }
   fs.set_handler(bvfs_result_handler, ua);
   fs.set_limit(limit);
   ua->bvfs = &fs;
   if (pattern) {
      fs.set_pattern(pattern);
   }
   if (filename) {
      fs.set_filename(filename);
   }

   fs.set_offset(offset);
   fs.ls_all_files();

bail_out:
   ua->bvfs = NULL;
   return true;
}

/*
 * .bvfs_lsfiles jobid=1,2,3,4 pathid=10
 * .bvfs_lsfiles jobid=1,2,3,4 path=/
 */
static bool dot_bvfs_lsfiles(UAContext *ua, const char *cmd)
{
   DBId_t pathid=0;
   int limit=2000, offset=0;
   char *path=NULL, *jobid=NULL, *username=NULL;
   char *pattern=NULL, *filename=NULL;
   bool ok;
   int i;

   if (find_arg(ua, "allfiles") > 0) {
      return dot_bvfs_ls_all_files(ua, cmd);
   }

   if (!bvfs_parse_arg(ua, &pathid, &path, &jobid, &username,
                       &limit, &offset))
   {
      ua->error_msg("Can't find jobid, pathid or path argument\n");
      return true;              /* not enough param */
   }
   if ((i = find_arg_with_value(ua, "pattern")) >= 0) {
      pattern = ua->argv[i];
   }
   if ((i = find_arg_with_value(ua, "filename")) >= 0) {
      filename = ua->argv[i];
   }

   if (!open_new_client_db(ua)) {
      return 1;
   }

   Bvfs fs(ua->jcr, ua->db);
   bvfs_set_acl(ua, &fs);
   fs.set_username(username);
   if (str_to_int64(jobid) == 0) {
      if (get_client_jobids(ua, &fs) <= 0) {
         goto bail_out;
      }
   } else {
      fs.set_jobids(jobid);
   }
   fs.set_handler(bvfs_result_handler, ua);
   fs.set_limit(limit);
   ua->bvfs = &fs;
   if (pattern) {
      fs.set_pattern(pattern);
   }
   if (filename) {
      fs.set_filename(filename);
   }
   if (pathid) {
      ok = fs.ch_dir(pathid);
   } else {
      ok = fs.ch_dir(path);
   }
   if (!ok) {
      goto bail_out;
   }

   fs.set_offset(offset);
   fs.ls_files();

bail_out:
   ua->bvfs = NULL;
   return true;
}

/*
 * .bvfs_lsdirs jobid=1,2,3,4 pathid=10
 * .bvfs_lsdirs jobid=1,2,3,4 path=/
 * .bvfs_lsdirs jobid=1,2,3,4 path=
 */
static bool dot_bvfs_lsdirs(UAContext *ua, const char *cmd)
{
   DBId_t pathid=0;
   int   limit=2000, offset=0;
   char *path=NULL, *jobid=NULL, *username=NULL;
   char *pattern=NULL;
   int   dironly;
   bool  ok;
   int i;

   if (!bvfs_parse_arg(ua, &pathid, &path, &jobid, &username,
                       &limit, &offset))
   {
      ua->error_msg("Can't find jobid, pathid or path argument\n");
      return true;              /* not enough param */
   }

   if ((i = find_arg_with_value(ua, "pattern")) >= 0) {
      pattern = ua->argv[i];
   }

   dironly = find_arg(ua, "dironly");

   if (!open_new_client_db(ua)) {
      return 1;
   }

   Bvfs fs(ua->jcr, ua->db);
   bvfs_set_acl(ua, &fs);
   fs.set_username(username);
   if (str_to_int64(jobid) == 0) {
      if (get_client_jobids(ua, &fs) <= 0) {
         goto bail_out;
      }
   } else {
      fs.set_jobids(jobid);
   }
   fs.set_limit(limit);
   fs.set_handler(bvfs_result_handler, ua);
   ua->bvfs = &fs;

   if (pattern) {
      fs.set_pattern(pattern);
   }

   if (pathid) {
      ok = fs.ch_dir(pathid);
   } else {
      ok = fs.ch_dir(path);
   }

   if (!ok) {
      goto bail_out;
   }

   fs.set_offset(offset);
   fs.ls_special_dirs();

   if (dironly < 0) {
      fs.ls_dirs();
   }
bail_out:
   ua->bvfs = NULL;
   return true;
}

/*
 * .bvfs_get_delta fileid=10 
 *
 */
static bool dot_bvfs_get_delta(UAContext *ua, const char *cmd)
{
   bool ret;
   FileId_t fileid=0;
   int i;

   if ((i = find_arg_with_value(ua, "fileid")) >= 0) {
      if (!is_a_number(ua->argv[i])) {
         ua->error_msg("Expecting integer for FileId, got %s\n", ua->argv[i]);
         return true;
      }
      fileid = str_to_int64(ua->argv[i]);

   } else {
      ua->error_msg("Expecting FileId\n");
      return true;
   }

   if (!open_new_client_db(ua)) {
      return 1;
   }
   Bvfs fs(ua->jcr, ua->db);
   bvfs_set_acl(ua, &fs);
   fs.set_handler(bvfs_result_handler, ua);
   ua->bvfs = &fs;
   ret = fs.get_delta(fileid);
   ua->bvfs = NULL;
   return ret;
}

/*
 * .bvfs_versions fnid=10 pathid=10 [client=xxx | clients=xxx,yyy] copies versions
 *
 */
static bool dot_bvfs_versions(UAContext *ua, const char *cmd)
{
   int i;
   DBId_t pathid=0;
   FileId_t fnid=0;
   int limit=2000, offset=0;
   char *path=NULL, *client=NULL, *username=NULL, *filename=NULL;
   bool copies=false, versions=false;
   alist clients(10, owned_by_alist);

   if (!bvfs_parse_arg(ua, &pathid, &path, NULL, &username,
                       &limit, &offset))
   {
      ua->error_msg("Can't find pathid or path argument\n");
      return true;              /* not enough param */
   }

   if (!bvfs_parse_arg_version(ua, &client, &clients, &fnid, &versions, &copies))
   {
      ua->error_msg("Can't find client or fnid argument\n");
      return true;              /* not enough param */
   }

   if ((i = find_arg_with_value(ua, "filename")) >= 0) {
      filename = ua->argv[i];
   }

   if (!open_new_client_db(ua)) {
      return 1;
   }

   Bvfs fs(ua->jcr, ua->db);
   bvfs_set_acl(ua, &fs);
   fs.set_limit(limit);
   fs.set_see_all_versions(versions);
   fs.set_see_copies(copies);
   fs.set_handler(bvfs_result_handler, ua);

   if (filename) {
      fs.set_filename(filename);
      if (path) {
         if (!fs.ch_dir(path)) {
            goto bail_out;
         }
         pathid = fs.get_pwd();
      }
   }
   ua->bvfs = &fs;

   fs.set_offset(offset);
   fs.get_all_file_versions(pathid, fnid, &clients);

bail_out:
   ua->bvfs = NULL;
   return true;
}

/* .bvfs_get_jobids jobid=1
 *  -> returns needed jobids to restore
 * .bvfs_get_jobids ujobid=xxx only
 *  -> returns the jobid of the job
 * .bvfs_get_jobids jobid=1 jobname
 *  -> returns the jobname
 * .bvfs_get_jobids client=xxx [ujobid=yyyy] [jobname=<glob>] [fileset=<glob>] [start=<ts>] [end=<ts>]
 *  -> returns all jobid for the client
 * .bvfs_get_jobids client=xxx count
 *  -> returns the number of jobids for the client
 * .bvfs_get_jobids jobid=1 all
 *  -> returns needed jobids to restore with all filesets a JobId=1 time
 * .bvfs_get_jobids job=XXXXX
 *  -> returns needed jobids to restore with the jobname
 * .bvfs_get_jobids ujobid=JobName
 *  -> returns needed jobids to restore
 */
static bool dot_bvfs_get_jobids(UAContext *ua, const char *cmd)
{
   JOB_DBR jr;
   uint32_t jobid_hint = 0; /* if specified with a jobid=XXX */
   memset(&jr, 0, sizeof(JOB_DBR));

   db_list_ctx jobids, tempids;
   int pos;
   char ed1[50];
   POOL_MEM query;
   dbid_list ids;               /* Store all FileSetIds for this client */

   if (!open_new_client_db(ua)) {
      return true;
   }

   Bvfs fs(ua->jcr, ua->db);
   bvfs_set_acl(ua, &fs);

   if ((pos = find_arg_with_value(ua, "username")) >= 0) {
      fs.set_username(ua->argv[pos]);
   }

   if ((pos = find_arg_with_value(ua, "ujobid")) >= 0) {
      bstrncpy(jr.Job, ua->argv[pos], sizeof(jr.Job));
   }

   if ((pos = find_arg_with_value(ua, "jobid")) >= 0) {
      jr.JobId = str_to_int64(ua->argv[pos]);
      jobid_hint = jr.JobId;
   /* Guess JobId from Job name, take the last successful jobid */
   } else if ((pos = find_arg_with_value(ua, "job")) >= 0) {
      JOB *job;
      bool ret;
      int32_t JobId=0;

      bstrncpy(jr.Name, ua->argv[pos], MAX_NAME_LENGTH);
      /* TODO: enhance this function to take client and/or fileset as argument*/

      job = GetJobResWithName(jr.Name);
      if (!job) {
         ua->error_msg(_("Unable to get Job record for Job=%s\n"), jr.Name);
         return true;
      }
      db_lock(ua->db);
      Mmsg(ua->db->cmd,
      "SELECT JobId "
        "FROM Job JOIN FileSet USING (FileSetId) JOIN Client USING (ClientId) "
         "WHERE Client.Name = '%s' AND FileSet.FileSet = '%s' "
           "AND Job.Type = 'B' AND Job.JobStatus IN ('T', 'W') "
         "ORDER By JobTDate DESC LIMIT 1",
           job->client->name(), job->fileset->name());
      ret = db_sql_query(ua->db, ua->db->cmd, db_int_handler, &JobId);
      db_unlock(ua->db);

      if (!ret) {
         ua->error_msg(_("Unable to get last Job record for Job=%s\n"),jr.Name);
      }

      jr.JobId = JobId;

   /* Get JobId from ujobid */
   } else if ((pos = find_arg_with_value(ua, "ujobid")) >= 0) {
      bstrncpy(jr.Job, ua->argv[pos], sizeof(jr.Job));
      bstrncpy(jr.PriorJob, ua->argv[pos], sizeof(jr.PriorJob));

   /* Return all backup jobid for a client list */
   } else if ((pos = find_arg_with_value(ua, "client")) >= 0 ||
              (pos = find_arg_with_value(ua, "clients")) >= 0) {

      int nbjobs = get_client_jobids(ua, &fs);
      if (nbjobs >= 0) {
         /* Apply the ACL filter on JobIds */
         if (find_arg(ua, "count") >= 0) {
            ua->send_msg("%d\n", nbjobs);

         } else {
            ua->send_msg("%s\n", fs.get_jobids());
         }
      }
      return true;
   }

   if (!db_get_job_record(ua->jcr, ua->db, &jr)) {
      ua->error_msg(_("Unable to get Job record for JobId=%s: ERR=%s\n"),
                    ua->cmd, db_strerror(ua->db));
      return true;
   }

   if (!acl_access_ok(ua, Job_ACL, jr.Name)) {
      return true;
   }

   if (have_restricted_acl(ua, Client_ACL)) {
      CLIENT_DBR cr;
      cr.ClientId = jr.ClientId;
      if (!db_get_client_record(ua->jcr, ua->db, &cr)) {
         Dmsg1(10,_("Unable to get client record for job %d\n"), jr.JobId);
         return true;
      }
      if (!acl_access_ok(ua, Client_ACL, cr.Name)) {
         Dmsg1(10,_("Access to ClientId=%d not authorized.\n"), jr.ClientId);
         return true;
      }
   }
   if (have_restricted_acl(ua, Pool_ACL)) {
      POOL_DBR pr;
      pr.PoolId = jr.PoolId;
      if (!db_get_pool_record(ua->jcr, ua->db, &pr)) {
         Dmsg1(10,_("Unable to get pool record for job %d\n"), jr.JobId);
         return true;
      }
      if (!acl_access_ok(ua, Pool_ACL, pr.Name)) {
         Dmsg1(10,_("Access to PoolId=%d not authorized.\n"), jr.PoolId);
         return true;
      }
   }
   /* Display only the requested jobid or
    * When in level base, we don't rely on any Full/Incr/Diff
    */
   if (find_arg(ua, "only") > 0 || jr.JobLevel == L_BASE) {
      /* Apply the ACL filter on JobIds */
      fs.set_jobid(jr.JobId);
      ua->send_msg("%s\n", fs.get_jobids());
      return true;
   }

   /* Display only the requested job name
    */
   if (find_arg(ua, "jobname") > 0) {
      /* Apply the ACL filter on JobIds */
      fs.set_jobid(jr.JobId);
      if (str_to_int64(fs.get_jobids()) == (int64_t)jr.JobId) {
         ua->send_msg("%s\n", jr.Job);
      }
      return true;
   }

   /* If we have the "all" option, we do a search on all defined fileset
    * for this client
    */
   if (find_arg(ua, "all") > 0) {
      edit_int64(jr.ClientId, ed1);
      Mmsg(query, uar_sel_filesetid, ed1);
      db_get_query_dbids(ua->jcr, ua->db, query, ids);

   } else {
      ids.num_ids = 1;
      ids.DBId[0] = jr.FileSetId;
   }

   jr.JobLevel = L_INCREMENTAL; /* Take Full+Diff+Incr */

   /* Foreach different FileSet, we build a restore jobid list */
   for (int i=0; i < ids.num_ids; i++) {
      jr.FileSetId = ids.DBId[i];
      if (!db_get_accurate_jobids_from_jobid(ua->jcr, ua->db, &jr, jobid_hint, &tempids)) {
         return true;
      }
      jobids.add(tempids);
   }

   fs.set_jobids(jobids.list);
   ua->send_msg("%s\n", fs.get_jobids());
   return true;
}

static int jobs_handler(void *ctx, int num_field, char **row)
{
   UAContext *ua = (UAContext *)ctx;
   ua->send_msg("%s %s %s %s\n", row[0], row[1], row[2], row[3]);
   return 0;
}

static char *get_argument(UAContext *ua, const char *arg, char *esc, bool convert)
{
   int pos;
   if (((pos = find_arg_with_value(ua, arg)) < 0) ||
       (strlen(ua->argv[pos]) > MAX_NAME_LENGTH))
   {
      return NULL;
   }
   db_escape_string(ua->jcr, ua->db, esc,
                    ua->argv[pos], strlen(ua->argv[pos]));
   if (convert) {
      for (int i=0; esc[i] ; i++) {
         if (esc[i] == '*') {
            esc[i] = '%';       /* TODO: Need to escape _ as well */
         }
      }
   }
   return esc;
}

/* The DB should be locked 
 * Return:
 * true: Can select all kind of job type (M, C, B) (we have  a job or a jobid)
 * false: Filter on Backup jobs
 */
static bool bvfs_get_filter(UAContext *ua, POOL_MEM &where, char *limit, int len)
{
   POOL_MEM tmp;
   char esc_name[MAX_ESCAPE_NAME_LENGTH];
   bool ret = false;
   
   if (get_argument(ua, "jobname", esc_name, true) != NULL) {
      Mmsg(where, "AND Job.Job LIKE '%s' ", esc_name);
   }

   if (get_argument(ua, "fileset", esc_name, true) != NULL) {
      Mmsg(tmp, "AND FileSet.FileSet LIKE '%s' ", esc_name);
      pm_strcat(where, tmp.c_str());
   }

   if (get_argument(ua, "jobid", esc_name, false) != NULL) {
      /* 0 is a special value, means everything */
      if (strcmp(esc_name, "0") != 0) {
         Mmsg(tmp, "AND Job.JobId = '%s' ", esc_name);
         pm_strcat(where, tmp.c_str());
         ret = true;
      }
   }

   if (get_argument(ua, "ujobid", esc_name, false) != NULL) {
      Mmsg(tmp, "AND Job.Job = '%s' ", esc_name);
      pm_strcat(where, tmp.c_str());
      ret = true;
   }

   if (get_argument(ua, "start", esc_name, false) != NULL) {
      Mmsg(tmp, "AND Job.StartTime >= '%s' ", esc_name);
      pm_strcat(where, tmp.c_str());
   }

   if (get_argument(ua, "end", esc_name, false) != NULL) {
      Mmsg(tmp, "AND Job.EndTime <= '%s' ", esc_name);
      pm_strcat(where, tmp.c_str());
   }

   *limit = 0;
   if (get_argument(ua, "limit", esc_name, false) != NULL) {
      if (is_a_number(esc_name)) {
         bsnprintf(limit, len, "LIMIT %s ", esc_name);
      }
   }
   return ret;
}

/* .bvfs_get_jobs client=xxx [ujobid=yyyy] [jobname=<glob>] [fileset=<glob>] [start=<ts>] [end=<ts>]
 * 1 yyyyy 1 Backup1_xxx_xxx_xxxx_xxx
 * 2 yyyyy 0 Backup1_xxx_xxx_xxxx_xxx
 */
static bool dot_bvfs_get_jobs(UAContext *ua, const char *cmd)
{
   int pos;
   POOL_MEM where;
   char esc_cli[MAX_ESCAPE_NAME_LENGTH];
   char limit[MAX_ESCAPE_NAME_LENGTH];
   bool use_all_jobtype=false;

   if (!open_new_client_db(ua)) {
      return true;
   }

   if (((pos = find_arg_with_value(ua, "client")) < 0) ||
       (strlen(ua->argv[pos]) > MAX_NAME_LENGTH))
   {
      return true;
   }

   /* TODO: Do checks on Jobs, FileSet, etc... */
   if (!acl_access_client_ok(ua, ua->argv[pos], JT_BACKUP_RESTORE)) {
      return true;
   }

   db_lock(ua->db);
   db_escape_string(ua->jcr, ua->db, esc_cli,
                    ua->argv[pos], strlen(ua->argv[pos]));

   use_all_jobtype = bvfs_get_filter(ua, where, limit, sizeof(limit));

   Mmsg(ua->db->cmd,
        "SELECT JobId, JobTDate, HasCache, Job "
          "FROM Job JOIN Client USING (ClientId) JOIN FileSet USING (FileSetId) "
         "WHERE Client.Name = '%s' AND %s AND Job.JobStatus IN ('T', 'W') "
            "%s "
         "ORDER By JobTDate DESC %s",
        esc_cli,
        use_all_jobtype ? " Job.Type IN ('B', 'C', 'M') " : " Job.Type = 'B' ",
        where.c_str(), limit);

   db_sql_query(ua->db, ua->db->cmd, jobs_handler, ua);
   db_unlock(ua->db);
   return true;
}

static bool dot_quit_cmd(UAContext *ua, const char *cmd)
{
   quit_cmd(ua, cmd);
   return true;
}

static bool dot_help_cmd(UAContext *ua, const char *cmd)
{
   int i;
   const char *msg="";
   const char *kw = ua->argk[1];

   if (ua->argc == 2) {
      /* Specific case for Tab completion */
      if (strcasecmp(ua->argk[1], NT_("item")) == 0) {
         return qhelp_cmd(ua, cmd);
      }
      /* Specific case for Tab completion */
      if (strcasecmp(ua->argk[1], NT_("all")) == 0) {
         return qhelp_cmd(ua, cmd);
      }
      if (strcasecmp(ua->argk[0], NT_(".help")) == 0) {
         kw = ua->argk[1];

      } else {
         kw = ua->argk[0];
      }
   }
   ua->send_msg(_("  Command       Description\n  =======       ===========\n"));
   for (i=0; i<comsize; i++) {
      if (ua->argc == 2) {
         if (!strcasecmp(kw, commands[i].key)) {
            ua->send_msg(_("  %-13s %s\n\nArguments:\n\t%s\n"), commands[i].key,
                         NPRTB(commands[i].help), NPRTB(commands[i].usage));
            break;
         }
      } else {
         if (acl_access_ok(ua, Command_ACL, commands[i].key, strlen(commands[i].key))) {
            ua->send_msg(_(" %-13s %s\n"),
                         NPRTB(commands[i].key), NPRTB(commands[i].help));
         } else {
            msg = _("Some commands are not available in this restricted console. ");
         }
      }
   }
   if (i == comsize && ua->argc == 2) {
      ua->send_msg(_("\nCan't find %s command.\n\n"), kw);
   }
   ua->send_msg(_("\n%sWhen at a prompt, entering a period cancels the command.\n\n"), msg);
   return true;
}

static bool getmsgscmd(UAContext *ua, const char *cmd)
{
   if (console_msg_pending) {
      do_messages(ua, cmd);
   }
   return 1;
}

#ifdef DEVELOPER
static void do_storage_cmd(UAContext *ua, STORE *store, const char *cmd)
{
   BSOCK *sd;
   JCR *jcr = ua->jcr;

   jcr->store_mngr->set_wstorage(store, _("unknown source"));
   /* Try connecting for up to 15 seconds */
   ua->send_msg(_("Connecting to Storage daemon %s at %s:%d\n"),
      store->name(), store->address, store->SDport);
   if (!connect_to_storage_daemon(jcr, 1, 15, 0)) {
      ua->error_msg("%s", jcr->errmsg);
      goto bail_out;
   }
   Dmsg0(120, _("Connected to storage daemon\n"));
   sd = jcr->store_bsock;
   sd->fsend("%s", cmd);
   if (sd->recv() >= 0) {
      ua->send_msg("%s", sd->msg);
   }
   sd->signal(BNET_TERMINATE);

bail_out:
   free_bsock(ua->jcr->store_bsock);
   return;
}

static void do_client_cmd(UAContext *ua, CLIENT *client, const char *cmd)
{
   BSOCK *fd;
   POOL_MEM buf;
   /* Connect to File daemon */

   ua->jcr->client = client;
   /* Try to connect for 15 seconds */
   ua->send_msg(_("Connecting to Client %s at %s:%d\n"),
                client->name(), get_client_address(ua->jcr, client, buf.addr()), client->FDport);
   if (!connect_to_file_daemon(ua->jcr, 1, 15, 0)) {
      ua->error_msg("%s", ua->jcr->errmsg);
      goto bail_out;
   }
   Dmsg0(120, "Connected to file daemon\n");
   fd = ua->jcr->file_bsock;
   fd->fsend("%s", cmd);
   if (fd->recv() >= 0) {
      ua->send_msg("%s", fd->msg);
   }
   fd->signal(BNET_TERMINATE);

bail_out:
   free_bsock(ua->jcr->file_bsock);
   ua->jcr->client = NULL;
   return;
}

/*
 *   .die (seg fault)
 *   .dump (sm_dump)
 *   .exit (no arg => .quit)
 */
static bool admin_cmds(UAContext *ua, const char *cmd)
{
   pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
   USTORE ustore;
   CLIENT *client=NULL;
   bool dir=false;
   bool do_deadlock=false;
   const char *remote_cmd;
   int i;
   JCR *jcr = NULL;
   int a;
   if (strncmp(ua->argk[0], ".die", 4) == 0) {
      if (find_arg(ua, "deadlock") > 0) {
         do_deadlock = true;
         remote_cmd = ".die deadlock";
      } else {
         remote_cmd = ".die";
      }
   } else if (strncmp(ua->argk[0], ".dump", 5) == 0) {
      remote_cmd = "sm_dump";
   } else if (strncmp(ua->argk[0], ".exit", 5) == 0) {
      remote_cmd = "exit";
   } else {
      ua->error_msg(_("Unknown command: %s\n"), ua->argk[0]);
      return true;
   }
   /* General debug? */
   for (i=1; i<ua->argc; i++) {
      if (strcasecmp(ua->argk[i], "dir") == 0 ||
          strcasecmp(ua->argk[i], "director") == 0) {
         dir = true;
      }
      if (strcasecmp(ua->argk[i], "client") == 0 ||
          strcasecmp(ua->argk[i], "fd") == 0) {
         client = NULL;
         if (ua->argv[i]) {
            client = (CLIENT *)GetResWithName(R_CLIENT, ua->argv[i]);
         }
         if (!client) {
            client = select_client_resource(ua, JT_SYSTEM);
         }
      }

      if (strcasecmp(ua->argk[i], NT_("store")) == 0 ||
          strcasecmp(ua->argk[i], NT_("storage")) == 0 ||
          strcasecmp(ua->argk[i], NT_("sd")) == 0) {
         if (ua->argv[i]) {
            ustore.store = (STORE *)GetResWithName(R_STORAGE, ua->argv[i]);
         }
         if (!ustore.store) {
            get_storage_resource(ua, &ustore, false/*no default*/);
         }
      }
   }

   if (!dir && !ustore.store && !client) {
      /*
       * We didn't find an appropriate keyword above, so
       * prompt the user.
       */
      start_prompt(ua, _("Available daemons are: \n"));
      add_prompt(ua, _("Director"));
      add_prompt(ua, _("Storage"));
      add_prompt(ua, _("Client"));
      switch(do_prompt(ua, "", _("Select daemon type to make die"), NULL, 0)) {
      case 0:                         /* Director */
         dir=true;
         break;
      case 1:
         get_storage_resource(ua, &ustore, false/*no default*/);
         break;
      case 2:
         client = select_client_resource(ua, JT_BACKUP_RESTORE);
         break;
      default:
         break;
      }
   }

   if (ustore.store) {
      do_storage_cmd(ua, ustore.store, remote_cmd);
   }

   if (client) {
      do_client_cmd(ua, client, remote_cmd);
   }

   if (dir) {
      if (strncmp(remote_cmd, ".die", 4) == 0) {
         if (do_deadlock) {
            ua->send_msg(_("The Director will generate a deadlock.\n"));
            P(mutex);
            P(mutex);
         }
         ua->send_msg(_("The Director will segment fault.\n"));
         a = jcr->JobId; /* ref NULL pointer */
         jcr->JobId = 1000; /* another ref NULL pointer */
         jcr->JobId = a;

      } else if (strncmp(remote_cmd, ".dump", 5) == 0) {
         sm_dump(false, true);
      } else if (strncmp(remote_cmd, ".exit", 5) == 0) {
         dot_quit_cmd(ua, cmd);
      }
   }

   return true;
}

#else

/*
 * Dummy routine for non-development version
 */
static bool admin_cmds(UAContext *ua, const char *cmd)
{
   ua->error_msg(_("Unknown command: %s\n"), ua->argk[0]);
   return true;
}

#endif

/*
 * Send a file to the director from bconsole @putfile command
 * The .putfile can not be used directly.
 */
static bool putfile_cmd(UAContext *ua, const char *cmd)
{
   int         pos, i, pnl, fnl;
   bool        ok = true;
   POOLMEM    *name = get_pool_memory(PM_FNAME);
   POOLMEM    *path = get_pool_memory(PM_FNAME);
   POOLMEM    *fname= get_pool_memory(PM_FNAME);
   const char *key = "putfile";
   FILE       *fp = NULL;

   if ((pos = find_arg_with_value(ua, "key")) > 0) {
      /* Check the string if the string is valid */
      for (i=0; ua->argv[pos][i] && isalnum(ua->argv[pos][i]) && i < 16; i++);

      if (ua->argv[pos][i] == 0) {
         key = ua->argv[pos];

      } else {
         ua->error_msg("Invalid key name for putfile command");
         ok = false;
         goto bail_out;
      }
   }

   /* the (intptr_t)ua will allow one file per console session */
   make_unique_filename(&name, (intptr_t)ua, key);

   fp = bfopen(name, "w");
   if (!fp) {
      berrno be;
      ua->error_msg("Unable to open destination file. ERR=%s\n",
                    be.bstrerror(errno));
      ok = false;
      goto bail_out;
   }

   while (ua->UA_sock->recv() > 0) {
      if (fwrite(ua->UA_sock->msg, ua->UA_sock->msglen, 1, fp) != 1) {
         berrno be;
         ua->error_msg("Unable to write to the destination file. ERR=%s\n",
                       be.bstrerror(errno));
         ok = false;
         /* TODO: Check if we need to quit here (data will still be in the
          * buffer...) */
      }
   }

   split_path_and_filename(name, &path, &pnl, &fname, &fnl);

bail_out:
   if (ok) {
      ua->send_msg("OK\n");

   } else {
      ua->send_msg("ERROR\n");
   }

   free_pool_memory(name);
   free_pool_memory(path);
   free_pool_memory(fname);
   if (fp) {
      fclose(fp);
   }
   return true;
}

/*
 * .estimate command 
 */
static bool dotestimatecmd(UAContext *ua, const char *cmd)
{
   JOB *jres;
   JOB_DBR jr;
   //FILESET_DBR fr;
   //CLIENT_DBR cr;
   char *job = NULL, level = 0, *fileset = NULL, *client = NULL;
   memset(&jr, 0, sizeof(jr));

   for (int i = 1 ; i < ua->argc ; i++) {
      if (!ua->argv[i]) {
         ua->error_msg(_("Invalid argument for %s\n"), ua->argk[i]);
         return true;

      } else if (strcasecmp(ua->argk[i], "job") == 0) {
         job = ua->argv[i];

      } else if (strcasecmp(ua->argk[i], "level") == 0) {
         level = toupper(ua->argv[i][0]);

      } else if (strcasecmp(ua->argk[i], "fileset") == 0) {
         fileset = ua->argv[i];

      } else if (strcasecmp(ua->argk[i], "client") == 0) {
         client = ua->argv[i];
      }
   }
   if (!job) {
      ua->error_msg(_("Invalid argument for job\n"));
      return true;
   }
   if (!acl_access_ok(ua, Job_ACL, job) ||
       (fileset && !acl_access_ok(ua, FileSet_ACL, fileset)) ||
       (client && !acl_access_client_ok(ua, client, JT_BACKUP)))
   {
      ua->error_msg(_("Access to specified Job, FileSet or Client not allowed.\n"));
      return true;
   }
   jres = (JOB *) GetResWithName(R_JOB, job);
   if (!jres) {
      ua->error_msg(_("Invalid argument for job\n"));
      return true;
   }
   if (!open_client_db(ua)) {
      ua->error_msg(_("Unable to open the catalog.\n"));
      return true;
   }
   
   bstrncpy(jr.Name, jres->hdr.name, sizeof(jr.Name));
   jr.JobLevel = level ? level : jres->JobLevel;
   if (fileset) {
      /* Get FileSetId */
   }
   if (client) {
      /* Get ClientId */
   }
   db_lock(ua->db);
   if (db_get_job_statistics(ua->jcr, ua->db, &jr)) {
      db_unlock(ua->db);
      OutputWriter o(ua->api_opts);
      char *p = o.get_output(OT_START_OBJ,
                   OT_JOBLEVEL, "level",     jr.JobLevel,
                   OT_INT,      "nbjob",     jr.CorrNbJob,
                   OT_INT,      "corrbytes", jr.CorrJobBytes,
                   OT_SIZE,     "jobbytes",  jr.JobBytes,
                   OT_INT,      "corrfiles", jr.CorrJobFiles,
                   OT_INT32,    "jobfiles",  jr.JobFiles,
                   OT_INT,      "duration",  (int)0,
                   OT_STRING,   "job",       jres->hdr.name,
                   OT_END_OBJ,
                   OT_END);
      ua->send_msg("%s", p);
   } else {
      /* We unlock the DB after the errmsg copy */
      pm_strcpy(ua->jcr->errmsg, ua->db->errmsg);
      db_unlock(ua->db);
      ua->error_msg("Error with .estimate %s\n", ua->jcr->errmsg);
   }
   return true;
}


/*
 * Can use an argument to filter on JobType
 * .jobs [type=B] or [type=!B]
 */
static bool jobscmd(UAContext *ua, const char *cmd)
{
   JOB *job;
   uint32_t type = 0;
   bool exclude=false;
   int pos;
   if ((pos = find_arg_with_value(ua, "type")) >= 0) {
      if (ua->argv[pos][0] == '!') {
         exclude = true;
         type = ua->argv[pos][1];
      } else {
         type = ua->argv[pos][0];
      }
   }
   LockRes();
   foreach_res(job, R_JOB) {
      if (type) {
         if ((exclude && type == job->JobType) || (!exclude && type != job->JobType)) {
            continue;
         }
      }
      if (acl_access_ok(ua, Job_ACL, job->name())) {
         ua->send_msg("%s\n", job->name());
      }
   }
   UnlockRes();
   return true;
}

static bool filesetscmd(UAContext *ua, const char *cmd)
{
   FILESET *fs;
   LockRes();
   foreach_res(fs, R_FILESET) {
      if (acl_access_ok(ua, FileSet_ACL, fs->name())) {
         ua->send_msg("%s\n", fs->name());
      }
   }
   UnlockRes();
   return true;
}

static bool catalogscmd(UAContext *ua, const char *cmd)
{
   CAT *cat;
   LockRes();
   foreach_res(cat, R_CATALOG) {
      if (acl_access_ok(ua, Catalog_ACL, cat->name())) {
         ua->send_msg("%s\n", cat->name());
      }
   }
   UnlockRes();
   return true;
}

/* This is not a good idea to lock the entire resource list to send information
 * on the network or query the DNS. So, we don't use the foreach_res() command
 * with a global lock and we do a copy of the client list in a specific list to
 * avoid any problem, I'm pretty sure we can use the res_head directly without
 * a global lock, but it needs testing to avoid race conditions.
 */
class TmpClient
{
public:
   char *name;
   char *address;

   TmpClient(char *n, char *a):
     name(bstrdup(n)), address(bstrdup(a))
   {
   };
   ~TmpClient() {
      free(name);
      free(address);
   };
};

static bool clientscmd(UAContext *ua, const char *cmd)
{
   int i;
   CLIENT *client;
   const char *ip=NULL;
   bool    found=false;
   alist  *clientlist = NULL;
   TmpClient *elt;
   POOL_MEM buf;

   if ((i = find_arg_with_value(ua, "address")) >= 0) {
      ip = ua->argv[i];
      clientlist = New(alist(50, not_owned_by_alist));
   }

   /* This is not a good idea to lock the entire resource list
    * to send information on the network or query the DNS. So,
    * we don't use the foreach_res() command with a global lock here.
    */
   LockRes();
   foreach_res(client, R_CLIENT) {
      if (acl_access_client_ok(ua, client->name(), JT_BACKUP_RESTORE)) {
         if (ip) {
            elt = new TmpClient(client->name(), get_client_address(ua->jcr, client, buf.addr()));
            clientlist->append(elt);

         } else {
            /* do not check for a specific ip, display everything */
            ua->send_msg("%s\n", client->name());
         }
      }
   }
   UnlockRes();

   if (!ip) {
      return true;
   }

   foreach_alist(elt, clientlist) {
      /* We look for a client that matches the specific ip address */
      dlist  *addr_list=NULL;
      IPADDR *ipaddr;
      char    buf[128];
      const char *errstr;

      if (strcmp(elt->address, ip) == 0) {
         found = true;

      } else if ((addr_list = bnet_host2ipaddrs(elt->address, 0, &errstr)) == NULL) {
         Dmsg2(10, "bnet_host2ipaddrs() for host %s failed: ERR=%s\n",
               elt->address, errstr);

      } else {
         /* Try to find the ip address from the list, we might have
          * other ways to compare ip addresses
          */
         foreach_dlist(ipaddr, addr_list) {
            if (strcmp(ip, ipaddr->get_address(buf, sizeof(buf))) == 0) {
               found = true;
               break;
            }
         }
         free_addresses(addr_list);
      }

      if (found) {
         ua->send_msg("%s\n", elt->name);
         break;
      }
   }
   /* Cleanup the temp list */
   foreach_alist(elt, clientlist) {
      delete elt;
   }
   delete clientlist;
   return true;
}

static bool msgscmd(UAContext *ua, const char *cmd)
{
   MSGS *msgs = NULL;
   LockRes();
   foreach_res(msgs, R_MSGS) {
      ua->send_msg("%s\n", msgs->name());
   }
   UnlockRes();
   return true;
}

static bool poolscmd(UAContext *ua, const char *cmd)
{
   POOL *pool;
   LockRes();
   foreach_res(pool, R_POOL) {
      if (acl_access_ok(ua, Pool_ACL, pool->name())) {
         ua->send_msg("%s\n", pool->name());
      }
   }
   UnlockRes();
   return true;
}

static bool schedulescmd(UAContext *ua, const char *cmd)
{
   SCHED *sched;
   LockRes();
   foreach_res(sched, R_SCHEDULE) {
      if (acl_access_ok(ua, Schedule_ACL, sched->name())) {
         ua->send_msg("%s\n", sched->name());
      }
   }
   UnlockRes();
   return true;
}

static bool storagecmd(UAContext *ua, const char *cmd)
{
   STORE *store;
   POOL_MEM tmp;
   bool unique=false;
   alist *already_in = NULL;

   /* .storage unique */
   if (find_arg(ua, "unique") > 0) {
      unique=true;
      already_in = New(alist(10, owned_by_alist));
   }

   LockRes();
   foreach_res(store, R_STORAGE) {
      if (acl_access_ok(ua, Storage_ACL, store->name())) {
         char *elt;
         bool  display=true;

         if (unique) {
            Mmsg(tmp, "%s:%d", store->address, store->SDport);
            foreach_alist(elt, already_in) { /* TODO: See if we need a hash or an ordered list here */
               if (strcmp(tmp.c_str(), elt) == 0) {
                  display = false;
                  break;
               }
            }
            if (display) {
               already_in->append(bstrdup(tmp.c_str()));
            }
         }
         if (display) {
            ua->send_msg("%s\n", store->name());
         }
      }
   }
   UnlockRes();
   if (already_in) {
      delete already_in;
   }
   return true;
}

static bool aopcmd(UAContext *ua, const char *cmd)
{
   ua->send_msg("None\n");
   ua->send_msg("Truncate\n");
   return true;
}

static bool typescmd(UAContext *ua, const char *cmd)
{
   ua->send_msg("Backup\n");
   ua->send_msg("Restore\n");
   ua->send_msg("Admin\n");
   ua->send_msg("Verify\n");
   ua->send_msg("Migrate\n");
   ua->send_msg("Copy\n");
   return true;
}

static bool dot_querycmd(UAContext *ua, const char *cmd)
{
   POOL_MEM buf;
   CLIENT *client = NULL;
   char *param = NULL;
   char *plugin = NULL;
   char *value = NULL;
   POOL_MEM esc1, esc2, esc3;
   JCR *jcr = ua->jcr;
   int i;
   bool ret = false;
   CLIENT *old_client = jcr->client;

   Dmsg0(200, "dot_querycmd()\n");
   
   jcr->setJobLevel(L_FULL);
   i = find_arg_with_value(ua, NT_("client"));
   if (i > 0) {
      client = GetClientResWithName(ua->argv[i]);
      if (!client) {
         ua->error_msg(_("error=Client \"%s\" not found.\n"), ua->argv[i]);
         return false;
      }
      if (!acl_access_client_ok(ua, client->name(), JT_BACKUP)) {
         ua->error_msg(_("error=No authorization for Client \"%s\"\n"), client->name());
         return false;
      }

   } else {
      ua->error_msg(_("error=Client name missing.\n"));
      return false;
   }

   i = find_arg_with_value(ua, NT_("parameter"));
   if (i > 0) {
      param = ua->argv[i];

   } else {
      ua->error_msg(_("error=parameter missing.\n"));
      return false;
   }

   /* plugin=... parameter */
   i = find_arg_with_value(ua, NT_("plugin"));
   if (i > 0) {
      plugin = ua->argv[i];
   }

   /* value=... parameter */
   i = find_arg_with_value(ua, NT_("value"));
   if (i > 0) {
      value = ua->argv[i];
   }

   jcr->client = client;

   jcr->setJobType(JT_BACKUP);
   jcr->start_time = time(NULL);
   init_jcr_job_record(jcr);           // need job

   if (!connect_to_file_daemon(jcr, 1, 15, 0)) {
      ua->error_msg("%s", jcr->errmsg);
      goto bail_out;
   }

   if (!jcr->file_bsock->fsend("query parameter=%s plugin=%s value=%s\n",
                               quote_string(esc1.addr(), param),
                               quote_string(esc2.addr(), NPRTB(plugin)),
                               quote_string(esc3.addr(), NPRTB(value)))) {
      ua->error_msg(_("error=Failed to send plugin command to Client.\n"));
      goto bail_out;
   }

   while (jcr->file_bsock->recv() >= 0) {
      ua->send_msg("%s", jcr->file_bsock->msg);
   }
   ret = true;

   ret = true;

bail_out:
   if (jcr->file_bsock) {
      jcr->file_bsock->signal(BNET_TERMINATE);
      free_bsock(ua->jcr->file_bsock);
   }
   jcr->client = old_client;
   return ret;
}

static bool tagscmd(UAContext *ua, const char *cmd)
{
   uint32_t i = 0;
   for (const char *p = debug_get_tag(i++, NULL) ; p ; p = debug_get_tag(i++, NULL)) {
      ua->send_msg("%s\n", p);
   }
   return true;
}

/*
 * If this command is called, it tells the director that we
 *  are a program that wants a sort of API, and hence,
 *  we will probably suppress certain output, include more
 *  error codes, and most of all send back a good number
 *  of new signals that indicate whether or not the command
 *  succeeded.
 */
static bool api_cmd(UAContext *ua, const char *cmd)
{
   int i;
   if (ua->argc >= 2) {
      ua->api = atoi(ua->argk[1]);

      /* Get output configuration options such as time format or separator */
      if ((i = find_arg_with_value(ua, "api_opts")) > 0) {
         bstrncpy(ua->api_opts, ua->argv[i], sizeof(ua->api_opts));

      } else {
         *ua->api_opts = 0;
      }
   } else {
      ua->api = 1;
   }
   return true;
}

static int client_backups_handler(void *ctx, int num_field, char **row)
{
   UAContext *ua = (UAContext *)ctx;
   ua->send_msg("| %s | %s | %s | %s | %s | %s | %s | %s |\n",
      row[0], row[1], row[2], row[3], row[4], row[5], row[6], row[7], row[8]);
   return 0;
}

/*
 * Return the backups for this client
 *
 *  .backups client=xxx fileset=yyy
 *
 */
static bool backupscmd(UAContext *ua, const char *cmd)
{
   if (!open_client_db(ua)) {
      return true;
   }
   if (ua->argc != 3 || strcmp(ua->argk[1], "client") != 0 ||
       strcmp(ua->argk[2], "fileset") != 0) {
      return true;
   }
   if (!acl_access_client_ok(ua, ua->argv[1], JT_BACKUP_RESTORE) ||
       !acl_access_ok(ua, FileSet_ACL, ua->argv[2])) {
      ua->error_msg(_("Access to specified Client or FileSet not allowed.\n"));
      return true;
   }
   Mmsg(ua->cmd, client_backups, ua->argv[1], ua->argv[2]);
   if (!db_sql_query(ua->db, ua->cmd, client_backups_handler, (void *)ua)) {
      ua->error_msg(_("Query failed: %s. ERR=%s\n"), ua->cmd, db_strerror(ua->db));
      return true;
   }
   return true;
}

static int sql_handler(void *ctx, int num_field, char **row)
{
   UAContext *ua = (UAContext *)ctx;
   POOL_MEM rows(PM_MESSAGE);

   /* Check for nonsense */
   if (num_field == 0 || row == NULL || row[0] == NULL) {
      return 0;                       /* nothing returned */
   }
   for (int i=0; num_field--; i++) {
      if (i == 0) {
         pm_strcpy(rows, NPRT(row[0]));
      } else {
         pm_strcat(rows, NPRT(row[i]));
      }
      pm_strcat(rows, "\t");
   }
   if (!rows.c_str() || !*rows.c_str()) {
      ua->send_msg("\t");
   } else {
      ua->send_msg("%s", rows.c_str());
   }
   return 0;
}

static bool sql_cmd(UAContext *ua, const char *cmd)
{
   int index;
   if (!open_new_client_db(ua)) {
      return true;
   }
   index = find_arg_with_value(ua, "query");
   if (index < 0) {
      ua->error_msg(_("query keyword not found.\n"));
      return true;
   }
   if (!db_sql_query(ua->db, ua->argv[index], sql_handler, (void *)ua)) {
      Dmsg1(100, "Query failed: ERR=%s\n", db_strerror(ua->db));
      ua->error_msg(_("Query failed: %s. ERR=%s\n"), ua->cmd, db_strerror(ua->db));
      return true;
   }
   return true;
}

static int one_handler(void *ctx, int num_field, char **row)
{
   UAContext *ua = (UAContext *)ctx;
   ua->send_msg("%s\n", row[0]);
   return 0;
}

static bool mediatypescmd(UAContext *ua, const char *cmd)
{
   if (!open_client_db(ua)) {
      return true;
   }
   if (!db_sql_query(ua->db,
           "SELECT MediaType FROM MediaType ORDER BY MediaType",
           one_handler, (void *)ua))
   {
      ua->error_msg(_("List MediaType failed: ERR=%s\n"), db_strerror(ua->db));
   }
   return true;
}

static bool mediacmd(UAContext *ua, const char *cmd)
{
   POOL_MEM query;
   if (!open_client_db(ua)) {
      return true;
   }

   /* If ACLs are used, we restrict the volume list to the pool we can access */
   const char *where = ua->db->get_acl(DB_ACL_POOL, true);
   const char *join = *where ? ua->db->get_acl_join_filter(DB_ACL_BIT(DB_ACL_POOL)) : "";

   Mmsg(query, "SELECT Media.VolumeName FROM Media %s %s ORDER BY VolumeName", join, where);
   if (!db_sql_query(ua->db,
                     query.c_str(),
                     one_handler, (void *)ua))
   {
      ua->error_msg(_("List Media failed: ERR=%s\n"), db_strerror(ua->db));
   }
   return true;
}

static bool locationscmd(UAContext *ua, const char *cmd)
{
   if (!open_client_db(ua)) {
      return true;
   }
   if (!db_sql_query(ua->db,
           "SELECT Location FROM Location ORDER BY Location",
           one_handler, (void *)ua))
   {
      ua->error_msg(_("List Location failed: ERR=%s\n"), db_strerror(ua->db));
   }
   return true;
}

static bool levelscmd(UAContext *ua, const char *cmd)
{
   int i;
   /* Note some levels are blank, which means none is needed */
   if (ua->argc == 1) {
      for (i=0; joblevels[i].level_name; i++) {
         if (joblevels[i].level_name[0] != ' ') {
            ua->send_msg("%s\n", joblevels[i].level_name);
         }
      }
   } else if (ua->argc == 2) {
      int jobtype = 0;
      /* Assume that first argument is the Job Type */
      for (i=0; jobtypes[i].type_name; i++) {
         if (strcasecmp(ua->argk[1], jobtypes[i].type_name) == 0) {
            jobtype = jobtypes[i].job_type;
            break;
         }
      }
      for (i=0; joblevels[i].level_name; i++) {
         if ((joblevels[i].job_type == jobtype) && (joblevels[i].level_name[0] != ' ')) {
            ua->send_msg("%s\n", joblevels[i].level_name);
         }
      }
   }

   return true;
}

static bool volstatuscmd(UAContext *ua, const char *cmd)
{
   ua->send_msg("Append\n");
   ua->send_msg("Archive\n");
   ua->send_msg("Disabled\n");
   ua->send_msg("Full\n");
   ua->send_msg("Used\n");
   ua->send_msg("Cleaning\n");
   ua->send_msg("Recycle\n");
   ua->send_msg("Read-Only\n");
   ua->send_msg("Error\n");
   return true;
}

/*
 * Return default values for a job
 */
static bool defaultscmd(UAContext *ua, const char *cmd)
{
   char ed1[50];
   if (ua->argc != 2 || !ua->argv[1]) {
      return true;
   }

   /* Send Job defaults */
   if (strcmp(ua->argk[1], "job") == 0) {
      if (!acl_access_ok(ua, Job_ACL, ua->argv[1])) {
         return true;
      }
      JOB *job = (JOB *)GetResWithName(R_JOB, ua->argv[1]);
      if (job) {
         USTORE store;
         char edl[50];
         ua->send_msg("job=%s", job->name());
         ua->send_msg("pool=%s", job->pool->name());
         ua->send_msg("messages=%s", job->messages->name());
         ua->send_msg("client=%s", job->client?job->client->name():_("*None*"));
         get_job_storage(&store, job, NULL);
         ua->send_msg("storage=%s", store.store ? store.store->name() :_("*None*"));
         ua->send_msg("where=%s", job->RestoreWhere?job->RestoreWhere:"");
         ua->send_msg("level=%s", level_to_str(edl, sizeof(edl),job->JobLevel));
         ua->send_msg("type=%s", job_type_to_str(job->JobType));
         ua->send_msg("fileset=%s", job->fileset->name());
         ua->send_msg("enabled=%d", job->is_enabled());
         ua->send_msg("catalog=%s", job->client?job->client->catalog->name():_("*None*"));
         ua->send_msg("priority=%d", job->Priority);
         ua->send_msg("replace=%c", job->replace);
      }
   }
   /* Send Pool defaults */
   else if (strcmp(ua->argk[1], "pool") == 0) {
      if (!acl_access_ok(ua, Pool_ACL, ua->argv[1])) {
         return true;
      }
      POOL *pool = (POOL *)GetResWithName(R_POOL, ua->argv[1]);
      if (pool) {
         ua->send_msg("pool=%s", pool->name());
         ua->send_msg("pool_type=%s", pool->pool_type);
         ua->send_msg("label_format=%s", pool->label_format?pool->label_format:"");
         ua->send_msg("use_volume_once=%d", pool->use_volume_once);
         ua->send_msg("purge_oldest_volume=%d", pool->purge_oldest_volume);
         ua->send_msg("recycle_oldest_volume=%d", pool->recycle_oldest_volume);
         ua->send_msg("recycle_current_volume=%d", pool->recycle_current_volume);
         ua->send_msg("max_volumes=%d", pool->max_volumes);
         ua->send_msg("vol_retention=%s", edit_uint64(pool->VolRetention, ed1));
         ua->send_msg("vol_use_duration=%s", edit_uint64(pool->VolUseDuration, ed1));
         ua->send_msg("max_vol_jobs=%d", pool->MaxVolJobs);
         ua->send_msg("max_vol_files=%d", pool->MaxVolFiles);
         ua->send_msg("max_vol_bytes=%s", edit_uint64(pool->MaxVolBytes, ed1));
         ua->send_msg("auto_prune=%d", pool->AutoPrune);
         ua->send_msg("recycle=%d", pool->Recycle);
         ua->send_msg("file_retention=%s", edit_uint64(pool->FileRetention, ed1));
         ua->send_msg("job_retention=%s", edit_uint64(pool->JobRetention, ed1));
      }
   } 
   /* Send Storage defaults */
   else if (strcmp(ua->argk[1], "storage") == 0) {
      if (!acl_access_ok(ua, Storage_ACL, ua->argv[1])) {
         return true;
      }
      STORE *storage = (STORE *)GetResWithName(R_STORAGE, ua->argv[1]);
      DEVICE *device;
      if (storage) {
         ua->send_msg("storage=%s", storage->name());
         ua->send_msg("address=%s", storage->address);
         ua->send_msg("enabled=%d", storage->is_enabled());
         ua->send_msg("media_type=%s", storage->media_type);
         ua->send_msg("sdport=%d", storage->SDport);
         device = (DEVICE *)storage->device->first();
         ua->send_msg("device=%s", device->name());
         if (storage->device && storage->device->size() > 1) {
            while ((device = (DEVICE *)storage->device->next())) {
               ua->send_msg(",%s", device->name());
            }
         }
      }
   } 
   /* Send Client defaults */
   else if (strcmp(ua->argk[1], "client") == 0) {
      if (!acl_access_client_ok(ua, ua->argv[1], JT_BACKUP_RESTORE)) {
         return true;
      }
      CLIENT *client = (CLIENT *)GetResWithName(R_CLIENT, ua->argv[1]);
      if (client) {
         POOL_MEM buf;
         ua->send_msg("client=%s", client->name());
         ua->send_msg("address=%s", get_client_address(ua->jcr, client, buf.addr()));
         ua->send_msg("fdport=%d", client->FDport);
         ua->send_msg("file_retention=%s", edit_uint64(client->FileRetention, ed1));
         ua->send_msg("job_retention=%s", edit_uint64(client->JobRetention, ed1));
         ua->send_msg("autoprune=%d", client->AutoPrune);
         ua->send_msg("catalog=%s", client->catalog->name());
      }
   }
   return true;
}

#if !BEEF
bool get_uid_gid_from_acl(UAContext *ua, alist **uid, alist **gid, alist **dir) { return true;}
#endif

/* Store the current uid/gid restriction */
static bool dot_add_events(UAContext *ua, const char *cmd)
{
   POOL_MEM source, type, text, tmp;
   bool source_set=false, type_set=false, text_set=false;
   for(int i=0; i < ua->argc ; i++) {
      if (strcasecmp(ua->argk[i], NT_("source")) == 0) {
         if (is_name_valid(ua->argv[i], tmp.handle(), ".:-_@+= ")) {
            Mmsg(source, "**%s**", ua->argv[i]);
            source_set=true;
         } else {
            ua->error_msg("source format is invalid. %s\n", tmp.c_str());
            goto bail_out;
         }
      }
      if (strcasecmp(ua->argk[i], NT_("type")) == 0) {
         if (is_name_valid(ua->argv[i], tmp.handle(), "")) {
            pm_strcpy(type, ua->argv[i]);
            type_set=true;
         } else {
            ua->error_msg("type format is invalid. %s\n", tmp.c_str());
            goto bail_out;
         }
      }
      if (strcasecmp(ua->argk[i], NT_("text")) == 0) {
         text_set=true;
         pm_strcpy(text, ua->argv[i]);
      }
   }
   if (text_set && source_set && type_set) {
      events_send_msg(ua->jcr, "DC0017",
                      type.c_str(),
                      source.c_str(),
                      (intptr_t)ua,
                      "%s", text.c_str());
   } else {
      ua->error_msg("Unable to send events, source, type or text parameter are missing\n");
   }
bail_out:
   return true;
}

enum {
   CAT_VOLUME = (1<<0),
   CAT_JOB    = (1<<1),
   CAT_CLIENT = (1<<2),
};

#define CAT_ALL (CAT_VOLUME | CAT_JOB | CAT_CLIENT)

/* Search over our different tables
 *
 * text=xxx [category=client|job|volume]
 * {
 *   "error": 0,
 *   "errmsg", "",
 *   "type": "search",
 *   "data": {
 *      "volume": ["vol1", "vol2"],
 *      "client": ["volA"],
 *      "job": []
 *   }
 * }
 *
 */
static bool dot_search(UAContext *ua, const char *cmd)
{
   uint32_t cat = 0;
   uint64_t limit=0;
   char *text = NULL;
   const char *errmsg = NULL;
   alist res(10, owned_by_alist);
   OutputWriter out(ua->api_opts);
   alist *r = &res;

   for(int i=1; i < ua->argc ; i++) {
      if (strcasecmp(ua->argk[i], NT_("limit")) == 0 && is_a_number(ua->argv[i])) {
         limit = str_to_uint64(ua->argv[i]);

      } else if (strcasecmp(ua->argk[i], NT_("category")) == 0) {
         if (strcasecmp(NPRTB(ua->argv[i]), "all") == 0) {
            cat = CAT_ALL;

         } else if (strcasecmp(NPRTB(ua->argv[i]), "client") == 0) {
            cat |= CAT_CLIENT;

         } else if (strcasecmp(NPRTB(ua->argv[i]), "job") == 0) {
            cat |= CAT_JOB;

         } else if (strcasecmp(NPRTB(ua->argv[i]), "volume") == 0) {
            cat |= CAT_VOLUME;

         } else {
            errmsg = "Incorrect category";
            goto bail_out;
         }

      } else if (strcasecmp(ua->argk[i], NT_("text")) == 0) {
         text = ua->argv[i];

      } else {
         errmsg = "Incorrect parameter";
         goto bail_out;
      }
   }
   if (!text || strlen(text) <= 3) {
      errmsg = "Missing text to search";
      goto bail_out;
   }

   /* TODO: check for nasty characters in text */
   if (!cat) {
      cat = CAT_ALL;
   }
   if (!limit) {
      limit = 10;
   }
   if (!open_new_client_db(ua)) {
      errmsg = "Unable to open the catalog";
      goto bail_out;
   }

   /* We initialize the output */
   out.get_output(OT_START_OBJ,
                  OT_INT,    "error", 0,
                  OT_STRING, "errmsg", "",
                  OT_STRING, "type", "search",
                  OT_END);
   out.start_object("data");

   if (text[0] == '#') {
      /* search for a tag */
      TAG_DBR tr;
      r->destroy();
      if (cat & CAT_CLIENT) {
         tr.zero();
         tr.all = true;
         bstrncpy(tr.Name, text, sizeof(tr.Name));
         strcpy(tr.Client, "1"); // To enable the Client search
         if (!db_search_tag_records(ua->jcr, ua->db, &tr, db_string_list_handler, &r)) {
            errmsg = "Unable to query client tag";
            goto bail_out;
         }
      }
      out.get_output(OT_ALIST_STR, "client", r, OT_END);

      r->destroy();
      if (cat  & CAT_JOB) {
         tr.zero();
         tr.all = true;
         bstrncpy(tr.Name, text, sizeof(tr.Name));
         strcpy(tr.Job, "1"); // To enable the Job search
         if (!db_search_tag_records(ua->jcr, ua->db, &tr, db_string_list_handler, &r)) {
            errmsg = "Unable to query Job tag";
            goto bail_out;
         }
      }
      out.get_output(OT_ALIST_STR, "job", r, OT_END);

      r->destroy();
      if (cat & CAT_VOLUME) {
         tr.zero();
         tr.all = true;
         bstrncpy(tr.Name, text, sizeof(tr.Name));
         strcpy(tr.Volume, "1"); // To enable the Volume search
         if (!db_search_tag_records(ua->jcr, ua->db, &tr, db_string_list_handler, &r)) {
            errmsg = "Unable to query Volume tag";
            goto bail_out;
         }
      }
      out.get_output(OT_ALIST_STR, "volume", r, OT_END);

   } else {
      r->destroy();
      if (cat & CAT_VOLUME) {
         /* Search in volumes */
         MEDIA_DBR mdbr;
         bstrncpy(mdbr.VolumeName, text, sizeof(mdbr.VolumeName));
         mdbr.limit = limit;
         if (!db_search_media_records(ua->jcr, ua->db, &mdbr, db_string_list_handler, &r)) {
            errmsg = "Unable to query Volume";
            goto bail_out;
         }
      }
      out.get_output(OT_ALIST_STR, "volume", r, OT_END);

      /* Search in clients */
      r->destroy();
      if (cat & CAT_CLIENT) {
         CLIENT_DBR cr;
         memset(&cr, 0, sizeof(cr));
         cr.limit = limit;
         bstrncpy(cr.Name, text, sizeof(cr.Name));
         if (!db_search_client_records(ua->jcr, ua->db, &cr, db_string_list_handler, &r)) {
            errmsg = "Unable to query Client";
            goto bail_out;
         }
      }
      out.get_output(OT_ALIST_STR, "client", &res, OT_END);

      /* Search in Job */
      r->destroy();
      if (cat & CAT_JOB) {
         JOB_DBR jr;
         memset(&jr, 0, sizeof(jr));
         jr.limit = limit;
         bstrncpy(jr.Job, text, sizeof(jr.Job));
         if (!db_search_job_records(ua->jcr, ua->db, &jr, db_string_list_handler, &r)) {
            errmsg = "Unable to query Job";
            goto bail_out;
         }
      }
      out.get_output(OT_ALIST_STR, "job", &res, OT_END);
   }
   out.end_object();
   ua->send_msg(out.get_output(OT_END_OBJ, OT_END));
   return true;

bail_out:
   ua->send_msg(out.get_output(OT_CLEAR,
                               OT_START_OBJ,
                               OT_INT,    "error",  1,
                               OT_STRING, "errmsg", errmsg,
                               OT_END_OBJ, OT_END));
   return true;
}
