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
 *   Bacula UA authentication. Provides authentication with
 *     the Director.
 *
 *     Kern Sibbald, June MMI
 *
 *    This routine runs as a thread and must be thread reentrant.
 *
 *  Basic tasks done here:
 *
 */

#include "bacula.h"
#include "console_conf.h"

/*
 * Version at end of Hello Enterprise:
 *   prior to 06Aug13 no version
 *   1 06Aug13 - added comm line compression
 * Community
 *   prior to 06Aug13 no version
 *   100  14Feb17 - added comm line compression
 *
 * Starting from version 9.7 both Hellos become the same:
 *    200  1/10/2020 - added `auth interactive` functionality
 */
#define UA_VERSION   UA_VERSION_PLUGINAUTH   /* Enterprise and Community */

void senditf(const char *fmt, ...);
void sendit(const char *buf);

/* Commands sent to Director */
static char hello[]    = "Hello %s calling %d tlspsk=%d\n";

/* Response from Director */
static char oldOKhello[]   = "1000 OK:";
static char newOKhello[]   = "1000 OK: %d";
static char FDOKhello[]   = "2000 OK Hello %d";

class ConsoleAuthenticate: public AuthenticateBase
{
public:
   ConsoleAuthenticate(BSOCK *dir):
   AuthenticateBase(NULL, dir, dtCli, dcCON, dcDIR),
   avoid_getpass(false)
   {
   }
   virtual ~ConsoleAuthenticate() {};
   virtual void TLSFailure() { sendit(_("TLS negotiation failed\n")); }
   virtual bool CheckTLSRequirement();

   int authenticate_director(DIRRES *director, CONRES *cons);
   bool ClientAuthenticate(CONRES *cons, const char *password);

   bool avoid_getpass;           // this is for regress testing
};

int authenticate_director(BSOCK *dir, DIRRES *director, CONRES *cons, bool avoid_getpass = false)
{
   ConsoleAuthenticate auth(dir);
   auth.avoid_getpass = avoid_getpass;
   return auth.authenticate_director(director, cons);
}

bool ConsoleAuthenticate::CheckTLSRequirement()
{
   /* Verify that the connection is willing to meet our TLS requirements */
   switch (TestTLSRequirement()) {
   case TLS_REQ_ERR_LOCAL:
     sendit(_("Authorization problem:"
             " Remote server requires TLS.\n"));
      return false;

   case TLS_REQ_ERR_REMOTE:
      sendit(_("Authorization problem:"
             " Remote server did not advertise required TLS support.\n"));
      return false;
   case TLS_REQ_OK:
      break;
   }
   return true;
}

bool ConsoleAuthenticate::ClientAuthenticate(CONRES *cons, const char *password)
{
   BSOCK *dir = bsock;
   bool legacy = true;        // by default we should execute legacy authentication
   POOL_MEM msg(PM_MESSAGE);

   // at first handle optional startTLS
   if (!ClientEarlyTLS()) {
      return false;
   }

   // auth interactive is handled in named console only
   if (cons){
      // if starttls does not left our auth interactive command get it from director
      if (!check_early_tls){
         if (dir->recv() <= 0) {
            bmicrosleep(5, 0); // original cram_md5_respond() wait for 5s here
            return false;
         }
      }

      // temporary buffer
      pm_strcpy(msg, dir->msg);

      // the check_early_tls=false means legacy authentication should do standard recv() else it can get command already in the bsock->msg
      check_early_tls = true;

      if (strncmp(msg.c_str(), UA_AUTH_INTERACTIVE, strlen(UA_AUTH_INTERACTIVE)) == 0){
         // do challenge/response chatting
         check_early_tls = false;   // "tell" cram_md5_respond to do a recv()
         legacy = false;            // we do the authentication

         // first display a welcome string if available
         char *welcome = msg.c_str() + strlen(UA_AUTH_INTERACTIVE) + 1;
         if (strlen(welcome) > 2){
            sendit("> ");
            sendit(welcome);
         }

         for (;;){
            // loop over operations
            if (dir->recv() <= 0) {
               bmicrosleep(5, 0); // original cram_md5_respond() wait for 5s here
               return false;
            }

            // early check if auth interaction finish
            if (dir->msg[0] == UA_AUTH_INTERACTIVE_FINISH){
               // break the loop
               break;
            }

            pm_strcpy(msg, NULL);
            pm_strcpy(msg, dir->msg + 1);
            strip_trailing_junk(msg.c_str());

            char *data = msg.c_str();
            POOL_MEM buf(PM_MESSAGE);
            char *input;

            // the command is encoded as a first char of the message
            switch (dir->msg[0]){
               case UA_AUTH_INTERACTIVE_PLAIN:
                  sendit(data);
                  input = fgets(buf.c_str(), buf.size(), stdin);
                  if (!input){
                     Dmsg0(1, "Error reading user input!\n");
                     return false;
                  }

                  // now we should return it to director
                  strip_trailing_junk(buf.c_str());
                  dir->fsend("%c%s", UA_AUTH_INTERACTIVE_RESPONSE, buf.c_str());
                  break;

               case UA_AUTH_INTERACTIVE_HIDDEN:
#if defined(DEVELOPER)
                  if (!avoid_getpass){
                     // normal pass handling
#endif
#if defined(HAVE_WIN32)
                     sendit(data);
                     if (win32_cgets(buf.c_str(), buf.size()) == NULL) {
                        pm_strcpy(buf, NULL);
                     }
#else
                     bstrncpy(buf.c_str(), getpass(data), buf.size());
#endif
#if defined(DEVELOPER)
                  } else {
                     sendit(data);
                     input = fgets(buf.c_str(), buf.size(), stdin);
                     if (!input){
                        Dmsg0(1, "Error reading user input!\n");
                        return false;
                     }
                     strip_trailing_junk(buf.c_str());
                  }
#endif
                  // now we should get a hidden response at `buf` class, return it to director
                  dir->fsend("%c%s", UA_AUTH_INTERACTIVE_RESPONSE, buf.c_str());
                  break;

               case UA_AUTH_INTERACTIVE_MESSAGE:
                  sendit(data);
                  sendit("\n");
                  break;

               case UA_AUTH_INTERACTIVE_FINISH:
                  // well it is not possible that we will reach this code, so report insanity
                  return false;

               default:
                  bmicrosleep(5, 0); // original cram_md5_respond() wait for 5s here
                  return false;
            }
         }

         // now check if authorized
         if (bsock->wait_data(180) <= 0 || bsock->recv() <= 0) {
            Dmsg1(1, "Receive auth confirmation failed. ERR=%s\n", bsock->bstrerror());
            bmicrosleep(5, 0);
            return false;
         }
         if (strcmp(bsock->msg, "1000 OK auth\n") == 0) {
            // authorization ok
            return true;
         }
         Dmsg1(1, "Received bad response: %s\n", bsock->msg);
         bmicrosleep(5, 0);
         return false;
      }
   }

   /* standard md5 handle */
   if (legacy && !ClientCramMD5AuthenticateBase(password)) {
      return false;
   }

   return true;
}

/*
 * Authenticate Director
 */
int ConsoleAuthenticate::authenticate_director(DIRRES *director, CONRES *cons)
{
   BSOCK *dir = bsock;
   int dir_version = 0;
   char bashed_name[MAX_NAME_LENGTH];
   bool skip_msg = false;

   /*
    * Send my name to the Director then do authentication
    */
   if (cons) {
      bstrncpy(bashed_name, cons->hdr.name, sizeof(bashed_name));
      bash_spaces(bashed_name);
      CalcLocalTLSNeedFromRes(cons->tls_enable, cons->tls_require,
            cons->tls_authenticate, false, NULL, cons->tls_ctx,
            cons->tls_psk_enable, cons->psk_ctx, cons->password);

   } else {
      bstrncpy(bashed_name, "*UserAgent*", sizeof(bashed_name));
      CalcLocalTLSNeedFromRes(director->tls_enable, director->tls_require,
            director->tls_authenticate, false, NULL, director->tls_ctx,
            director->tls_psk_enable, director->psk_ctx, director->password);
   }

   /* Timeout Hello after 15 secs */
   StartAuthTimeout(1500);

   dir->fsend(hello, bashed_name, UA_VERSION, tlspsk_local_need);

   if (!ClientAuthenticate(cons, password)){
      if (dir->is_timed_out()) {
         sendit(_("The Director is busy or the MaximumConsoleConnections limit is reached.\n"));
         skip_msg = true;
      }
      goto bail_out;
   }

   if (!HandleTLS()) {
      goto bail_out;
   }

   /*
    * It's possible that the TLS connection will
    * be dropped here if an invalid client certificate was presented
    */
   Dmsg1(6, ">dird: %s", dir->msg);
   if (dir->recv() <= 0) {
      senditf(_("Bad response to Hello command: ERR=%s\n"),
         dir->bstrerror());
      goto bail_out;
   }

   Dmsg1(10, "<dird: %s", dir->msg);
   if (strncmp(dir->msg, oldOKhello, sizeof(oldOKhello)-1) == 0) {
      /* If Dir version exists, get it */
      sscanf(dir->msg, newOKhello, &dir_version);
      sendit(dir->msg);

      /* We do not check the last %d */
   } else if (strncmp(dir->msg, FDOKhello, sizeof(FDOKhello)-3) == 0) {
      sscanf(dir->msg, FDOKhello, &dir_version);
      sendit(dir->msg);

   } else {
      sendit(_("Director rejected Hello command\n"));
      goto bail_out;
   }
   /* Turn on compression for newer Directors */
   if (dir_version >= 1 && (!cons || cons->comm_compression)) {
      dir->set_compress();
   } else {
      dir->clear_compress();
   }
   return 1;

bail_out:
   if (!skip_msg) {
      sendit( _("Director authorization problem.\n"
             "Most likely the passwords do not agree.\n"
             "If you are using TLS, there may have been a certificate validation error during the TLS handshake.\n"
             "For help, please see " MANUAL_AUTH_URL "\n"));
   }
   return 0;
}
