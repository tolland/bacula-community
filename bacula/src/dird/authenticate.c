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
 *   Bacula Director -- authenticate.c -- handles authorization of
 *     Storage and File daemons.
 *
 *    Written by: Kern Sibbald, May MMI
 *
 *    This routine runs as a thread and must be thread reentrant.
 *
 */

#include "bacula.h"
#include "dird.h"
#include "dir_authplugin.h"

static const int dbglvl = 50;

extern DIRRES *director;

/* Version at end of Hello
 * Note: Enterprise versions now are in 10000 range
 *   prior to 06Aug13 no version
 *       1 06Aug13 - added comm line compression
 *     102 04Jun15 - added jobmedia change
 *     103 14Feb17 - added comm line compression
 *   10002 04Jun15 - added jobmedia batching (from queue in SD)
 */
#define DIR_VERSION 10002


/* Command sent to SD */
static char hello[]    = "Hello %sDirector %s calling %d tlspsk=%d\n";

/* Responses from Storage and File daemons */
static char OKhello[]      = "3000 OK Hello";
static char SDOKnewHello[] = "3000 OK Hello %d";
static char FDOKhello[]    = "2000 OK Hello";
static char FDOKnewHello[] = "2000 OK Hello %d";

/* Sent to User Agent */
static char Dir_sorry[]  = "1999 You are not authorized.\n";

/* Forward referenced functions */

class DIRAuthenticateSD: public AuthenticateBase
{
public:
   DIRAuthenticateSD(JCR *jcr):
   AuthenticateBase(jcr, jcr->store_bsock, dtCli, dcDIR, dcSD)
   {
   }
   virtual ~DIRAuthenticateSD() {};
   bool authenticate_storage_daemon(STORE *store);
};

bool authenticate_storage_daemon(JCR *jcr, STORE *store, int *status, POOLMEM **errmsg)
{
   DIRAuthenticateSD elt(jcr);
   bool ret = elt.authenticate_storage_daemon(store);
   if (!ret) {
      pm_strcpy(errmsg, elt.errmsg);
      *status = elt.status;
   }
   return ret;
}

bool DIRAuthenticateSD::authenticate_storage_daemon(STORE *store)
{
   BSOCK *sd = bsock;
   char dirname[MAX_NAME_LENGTH];

   /* Calculate tls_local_need from the resource */
   CalcLocalTLSNeedFromRes(store->tls_enable, store->tls_require,
         store->tls_authenticate, false, NULL, store->tls_ctx,
         store->tls_psk_enable, store->psk_ctx, store->password);

   /*
    * Send my name to the Storage daemon then do authentication
    */
   bstrncpy(dirname, director->name(), sizeof(dirname));
   bash_spaces(dirname);
   /* Timeout Hello after 1 min */
   StartAuthTimeout();
   /* Sent Hello SD: Bacula Director <dirname> calling <version> */
   if (!sd->fsend(hello, "SD: Bacula ", dirname, DIR_VERSION, tlspsk_local_need)) {
      status = M_FATAL;
      MmsgD3(dbglvl, errmsg, _("[DE0031] Error sending Hello to Storage daemon at \"%s:%d\". ERR=%s\n"),
             sd->host(), sd->port(), sd->bstrerror());
      return false;
   }

   /* Try to authenticate using cram-md5 */
   if (!ClientCramMD5Authenticate(store->password)) {
      return false;
   }

   if (!HandleTLS()) {
      return false;
   }

   Dmsg1(116, ">stored: %s", sd->msg);
   if (sd->recv() <= 0) {
      status = M_FATAL;
      Mmsg(errmsg, _("[DE0031] bdird<stored: \"%s:%s\" bad response to Hello command: ERR=%s\n"),
           sd->who(), sd->host(), sd->bstrerror());
      return 0;
   }
   Dmsg1(110, "<stored: %s", sd->msg);
   jcr->SDVersion = 0;
   if (sscanf(sd->msg, SDOKnewHello, &jcr->SDVersion) != 1 &&
       strncmp(sd->msg, OKhello, sizeof(OKhello)) != 0)
   {
      status = M_FATAL;
      MmsgD2(dbglvl, errmsg, _("[DE0031] Storage daemon at \"%s:%d\" rejected Hello command\n"),
             sd->host(), sd->port());
      return 0;
   }
   /* For newer SD turn on comm line compression */
   if (jcr->SDVersion >= 1 && director->comm_compression) {
      sd->set_compress();
   } else {
      sd->clear_compress();
      Dmsg0(050, "*** No Dir compression to SD\n");
   }
   // workaround with 30008 can be removed in 18.0.0
   if (jcr->SDVersion < SD_VERSION && !(jcr->SDVersion == 30007 && SD_VERSION == 30008)) {
      status = M_FATAL;
      Mmsg(errmsg, _("[DE0031] Older Storage daemon at \"%s:%d\" incompatible with this Director.\n"),
           sd->host(), sd->port());
      return 0;
   }
   return 1;
}

class DIRAuthenticateFD: public AuthenticateBase
{
public:
   DIRAuthenticateFD(JCR *jcr):
   AuthenticateBase(jcr, jcr->file_bsock, dtCli, dcDIR, dcFD)
   {
   }
   virtual ~DIRAuthenticateFD() {};
   int authenticate_file_daemon();
};

int authenticate_file_daemon(JCR *jcr, int *status, POOLMEM **errmsg)
{
   DIRAuthenticateFD auth(jcr);
   int ret = auth.authenticate_file_daemon();
   if (!ret) {
      *status = auth.status;
      pm_strcpy(errmsg, auth.errmsg);
   }
   return ret;
}

int DIRAuthenticateFD::authenticate_file_daemon()
{
   BSOCK *fd = bsock;
   CLIENT *client = jcr->client;
   char dirname[MAX_NAME_LENGTH];

   /* Calculate tls_local_need from the resource */
   CalcLocalTLSNeedFromRes(client->tls_enable, client->tls_require, client->tls_authenticate,
   client->tls_allowed_cns, client->tls_allowed_cns, client->tls_ctx,
   client->tls_psk_enable, client->psk_ctx, client->password);

   /*
    * Send my name to the File daemon then do authentication
    */
   bstrncpy(dirname, director->name(), sizeof(dirname));
   bash_spaces(dirname);
   /* Timeout Hello after 1 min */
   StartAuthTimeout();
   if (!fd->fsend(hello, "", dirname, DIR_VERSION, tlspsk_local_need)) {
      status = M_FATAL;
      MmsgD3(dbglvl, errmsg, _("[DE0031] Error sending Hello to File daemon at \"%s:%d\". ERR=%s\n"),
           fd->host(), fd->port(), fd->bstrerror());
      return false;
   }

   /* Try to authenticate using cram-md5 */
   if (!ClientCramMD5Authenticate(client->password)) {
      return false;
   }

   if (!HandleTLS()) {
      return false;
   }

   Dmsg1(116, ">filed: %s", fd->msg);
   if (fd->recv() <= 0) {
      status = M_FATAL;
      MmsgD3(dbglvl, errmsg, _("[DE0031] Bad response from File daemon at \"%s:%d\" to Hello command: ERR=%s\n"),
             fd->host(), fd->port(), fd->bstrerror());
      return 0;
   }
   Dmsg1(110, "<filed: %s", fd->msg);
   StopAuthTimeout();
   jcr->FDVersion = 0;
   if (strncmp(fd->msg, FDOKhello, sizeof(FDOKhello)) != 0 &&
       sscanf(fd->msg, FDOKnewHello, &jcr->FDVersion) != 1)
   {
      status = M_FATAL;
      MmsgD2(dbglvl, errmsg, _("[DE0031] File daemon at \"%s:%d\" rejected Hello command\n"),
             fd->host(), fd->port());
      return 0;
   }
   /* For newer FD turn on comm line compression */
   if (jcr->FDVersion >= 9 && jcr->FDVersion != 213 && director->comm_compression) {
      fd->set_compress();
   } else {
      fd->clear_compress();
      Dmsg0(050, "*** No Dir compression to FD\n");
   }
   return 1;
}

class UAAuthenticate: public AuthenticateBase
{
   UAContext *uac;
public:
   UAAuthenticate(UAContext *uac):
   AuthenticateBase(NULL, uac->UA_sock, dtSrv, dcDIR, dcCON),
   uac(uac)
   {
   }
   virtual ~UAAuthenticate() {};
   void TLSFailure() {
      status = M_SECURITY;
      MmsgD3(dbglvl, errmsg, _("TLS negotiation failed with %s at \"%s:%d\"\n"),
             GetRemoteClassShortName(), bsock->host(), bsock->port());
   }

   int authenticate_user_agent();
   bool authenticate_with_plugin(CONRES * cons);
};

int authenticate_user_agent(UAContext *uac)
{
   return UAAuthenticate(uac).authenticate_user_agent();
}

/*********************************************************************
 *
 */
int UAAuthenticate::authenticate_user_agent()
{
   char name[MAX_NAME_LENGTH];
   CONRES *cons = NULL;
   BSOCK *ua = uac->UA_sock;
   int ua_version = 0;
   int tlspsk_remote = 0;
   bool fdcallsdir=false;
   CLIENT *cli=NULL;
   bool legacy_auth = true;

   if (ua->msglen < 16 || ua->msglen >= MAX_NAME_LENGTH + 15) {
      status = M_SECURITY;
      Mmsg(errmsg, _("[DE0031] UA Hello from %s:%s is invalid. Len=%d\n"), ua->who(),
           ua->host(), ua->msglen);
      sleep(5);
      return 0;
   }
   Dmsg1(dbglvl, "authenticate user agent: %s", ua->msg);
   if (scan_string(ua->msg, "Hello %127s fdcallsdir %d tlspsk=%d", name, &ua_version, &tlspsk_remote) == 3 ||
       scan_string(ua->msg, "Hello %127s fdcallsdir %d", name, &ua_version) == 2) {
      fdcallsdir = true;
   } else if (scan_string(ua->msg, "Hello %127s calling %d tlspsk=%d", name, &ua_version, &tlspsk_remote) != 3 &&
       scan_string(ua->msg, "Hello %127s calling %d", name, &ua_version) != 2 &&
       scan_string(ua->msg, "Hello %127s calling", name) != 1)
   {
      ua->msg[100] = 0;               /* terminate string */
      status = M_SECURITY;
      Mmsg(errmsg, _("[DE0031] UA Hello from %s:%s is invalid. Got: %s\n"), ua->who(),
           ua->host(), ua->msg);
      sleep(5);
      return 0;
   }

   /* Turn on compression for newer consoles */
   if (ua_version >= 1  && director->comm_compression) {
      ua->set_compress();
   } else {
      Dmsg0(050, "*** No Dir compression to UA\n");
   }

   name[sizeof(name)-1] = 0;             /* terminate name */
   if (strcmp(name, "*UserAgent*") == 0) {  /* default console */
      /* TLS Requirement */
      CalcLocalTLSNeedFromRes(director->tls_enable, director->tls_require,
            director->tls_authenticate, director->tls_verify_peer,
            director->tls_allowed_cns, director->tls_ctx,
            director->tls_psk_enable, director->psk_ctx, director->password);

   } else if (fdcallsdir) {
      unbash_spaces(name);
      cli = (CLIENT *)GetResWithName(R_CLIENT, name);
      if (cli && cli->allow_fd_connections) {
         /* TLS Requirement */
         CalcLocalTLSNeedFromRes(cli->tls_enable, cli->tls_require,
                                 cli->tls_authenticate, cli->tls_verify_peer,
                                 cli->tls_allowed_cns, cli->tls_ctx,
                                 cli->tls_psk_enable, cli->psk_ctx, cli->password);
      } else {
         if (cli) {
            Dmsg1(10, "AllowFDConnections not set for %s\n", name);
         }
         auth_success = false;
         goto auth_done;
      }

   } else {
      unbash_spaces(name);
      cons = (CONRES *)GetResWithName(R_CONSOLE, name);
      if (cons) {
         /* TLS Requirement */
         CalcLocalTLSNeedFromRes(cons->tls_enable, cons->tls_require,
               cons->tls_authenticate, cons->tls_verify_peer, cons->tls_allowed_cns,
               cons->tls_ctx, cons->tls_psk_enable, cons->psk_ctx, cons->password);
      } else {
         auth_success = false;
         goto auth_done;
      }
   }
   DecodeRemoteTLSPSKNeed(tlspsk_remote);

   // if user console
   if (cons){
      uac->cons = cons;         /* save console resource pointer */
      bstrncpy(uac->name, uac->cons->hdr.name, sizeof(uac->name));

      // require authentication and user agent is on proper version
      if (cons->authenticationplugin){
         legacy_auth = false;
         Dmsg1(dbglvl, "authenticate with Plugin=%s\n", cons->authenticationplugin);
         // TLS will be started during the plugin authentication process
         if (ua_version < UA_VERSION_PLUGINAUTH || !authenticate_with_plugin(cons)){
            auth_success = false;
            goto auth_done;
         }
      }
   }

   if (legacy_auth && !ServerCramMD5Authenticate(password)) {
      goto auth_done;
   }

   this->auth_success = HandleTLS();

   if (!auth_success) {
      goto auth_done;
   }

/* Authorization Completed */
auth_done:
   if (!auth_success) {
      ua->fsend("%s", _(Dir_sorry));
      status = M_SECURITY;
      Mmsg(errmsg, _("[DE0020] Unable to authenticate console \"%s\" at %s:%s:%d.\n"),
           name, ua->who(), ua->host(), ua->port());
      sleep(5);
      return 0;
   }
   ua->fsend(_("1000 OK: %d %s %sVersion: %s (%s)\n"),
      DIR_VERSION, my_name, BDEMO, VERSION, BDATE);

   if (fdcallsdir) {
      Dmsg1(10, "FDCallsDir OK for %s\n", name);
      ua->fsend(_("OK\n"));
      cli->setBSOCK(ua);
      uac->UA_sock = NULL;
      uac->quit = true;
   }
   return 1;
}

/**
 * @brief This function perform user authentication procedure augmented with Auth Plugin API.
 *        All bconsole network chatting and interaction is forwarded to dir_authplugins module
 *        and we just provide a required framework and resources, i.e. jcr or bsock to ua.
 *        TLS is started before the authentication exchange. If TLS is not available, the
 *        authentication exchange is aborted.
 *
 * @param cons a Console resource required for auth plugin registration
 * @return true when authentication process finish with success and we can proceed next operations
 * @return false when some error raised or authentication was unsuccessful
 */
bool UAAuthenticate::authenticate_with_plugin(CONRES * cons)
{
   /*
    * The user authentication procedure with Auth Plugins API follow like this:
    * 1. get bDirAuthenticationRegister data which defines what and how we should proceed with authentication
    * 2. if auth operation is not bDirAuthenticationOperationPluginAll (which points that you have to ask plugin for
    *    every auth operation you execute) then iterate over returned data
    * 3. the iteration goes like this:
    *    a. send question to console or simple message do display if operation is bDirAuthenticationOperationMessage
    *    b. receive response from console
    *    c. forward response to plugin with plugin event
    * 4. when all interactions were handled without problem then do authenticate with plugin
    * 5. if plugin return authentication OK? return true, Not? return false
    */
   bDirAuthenticationRegister *authData;

   authData = (bDirAuthenticationRegister*) dir_authplugin_getauthenticationData(uac->jcr, cons->hdr.name, cons->authenticationplugin);
   if (authData == NULL)
   {
      status = M_FATAL;
      Mmsg(errmsg, "[DE0031] Incorrect authentication plugin initialization\n");
      return false;
   }

   // do tls before real auth
   if (!ServerEarlyTLS())
   {                            // errmsg already edited
      return false;
   }
   // We require to have TLS setup to use authentication plugins
   if (!tls_started) {
      status = M_FATAL;
      MmsgD0(dbglvl, errmsg, _("[DE0031] Unable to use Plugin Authentication because TLS is not available\n"));
      return false;
   }
   // send auth plugin start packet and optional welcome string to console
   if (!bsock->fsend("auth interactive %s\n", NPRTB(authData->welcome))) {
      status = M_FATAL;
      MmsgD1(dbglvl, errmsg, "[DE0031] Send interactive start comm error. ERR=%s\n", bsock->bstrerror());
      return false;
   }

   // now a console should know how to deal with it, so iterate over auth data
   const bDirAuthenticationData *data = authData->data;

   for (uint i = 0; i < authData->num; i++){
      Dmsg1(dbglvl, "bDirAuthenticationData step %d\n", i);
      if (dir_authplugin_do_interaction(uac->jcr, bsock, authData->name, (void *)&data[i]) != bRC_OK){
         status = M_FATAL;
         Mmsg(errmsg, "[DE0031] Invalid authentication protocol\n");
         return false;
      }
   }

   if (dir_authplugin_authenticate(uac->jcr, bsock, authData->name) != bRC_OK){
      status = M_FATAL;
      Mmsg(errmsg, "[DE0020] Authorization failed\n");
      bsock->fsend(_("1999 Authorization failed !!!.\n"));
      bmicrosleep(5, 0);
      return false;
   }

   bsock->fsend("1000 OK auth\n");
   return true;
}
