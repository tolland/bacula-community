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
 * Common definitions and utility functions for Inteos plugins.
 * Functions defines a common framework used in our utilities and plugins.
 * LDAP plugin specific functions.
 *
 * Author: Radosław Korzeniewski, radoslaw@korzeniewski.net, Inteos Sp. z o.o.
 */

#include "ldap-util.h"
#include "dirpluglib.h"

/*
 * libbac uses its own sscanf implementation which is not compatible with
 * libc implementation, unfortunately. usage of bsscanf require format string rewriting.
 */
#ifdef sscanf
#undef sscanf
#endif

/* Pointers to Bacula functions used in plugins */
extern bDirFuncs *bfuncs;
extern bDirInfo *binfo;

/*
 * display common connection error message and diagnose
 */
void ldapserverconnectionerror(bpContext * ctx, LDAP *ldapconn, int ret)
{
   char *errmsg = NULL;

   ldap_get_option(ldapconn, LDAP_OPT_DIAGNOSTIC_MESSAGE, (void*)&errmsg);
   DMSG1(ctx, D1, "LDAP Server connection error: %s\n", ldap_err2string(ret));
   if (errmsg){
      DMSG1(ctx, D1, "diagnose error: %s\n", errmsg);
   }
   JMSG1(ctx, M_WARNING, "LDAP Server connection error: %s\n", ldap_err2string(ret));
   if (errmsg){
      JMSG1(ctx, M_WARNING, "diagnose error: %s\n", errmsg);
   }
   ldap_memfree(errmsg);
}

/*
 * connect to LDAP service
 *
 * in:
 *    paramlist - list of parameters from config file
 *       required params: LDAPURI, BINDDN, BINDPASS, BINDMODE
 * out:
 *    on success: LDAP * - working LDAP connection
 *    on error:   NULL
 */
LDAP *ldapconnect(bpContext *ctx, ldap_paramlist *paramlist)
{
   LDAP *ldapconn = NULL;
   const char *ldapuri;
   const int desired_version = LDAP_VERSION3;
   const char *binddn;
   char *bindpass;
   POOLMEM *tmp = NULL;
   int ret;
   struct berval cred;
   struct berval *msgidp = NULL;
   bool starttls = false;
   bool starttlsforce = false;

   ldapuri = paramlist->ldapuri;

#ifdef __sun__
/* TODO: change ldapuri to ldaphost + ldapport */
   if ((ldapconn = ldap_init(ldapuri, LDAP_PORT)) == NULL) {
      return NULL;
   }
#else
   if (ldap_initialize(&ldapconn, ldapuri) != LDAP_SUCCESS) {
      return NULL;
   }
#endif

#ifdef __sun__
   if ((ret = ldap_set_option(ldapconn, LDAP_OPT_PROTOCOL_VERSION, &desired_version)) != LDAP_SUCCESS)
#else
   if ((ret = ldap_set_option(ldapconn, LDAP_OPT_PROTOCOL_VERSION, &desired_version)) != LDAP_OPT_SUCCESS)
#endif
   {
      ldapserverconnectionerror(ctx, ldapconn, ret);
      return NULL;
   }
#ifdef __sun__
   if ((ret = ldap_set_option(ldapconn, LDAP_OPT_REFERRALS, LDAP_OPT_OFF)) != LDAP_SUCCESS)
#else
   if ((ret = ldap_set_option(ldapconn, LDAP_OPT_REFERRALS, LDAP_OPT_OFF)) != LDAP_OPT_SUCCESS)
#endif
   {
      ldapserverconnectionerror(ctx, ldapconn, ret);
      return NULL;
   }

#ifdef HAVE_LDAP_START_TLS
   /*
    * Start TLS support
    *
    * To enable StartTLS for LDAP connection you have to setup 'starttls'
    *    or 'starttlsforce' plugin options
    *    where 'starttlsforce' will fail connection if server do not support it.
    */
   starttls = paramlist->starttls;
   starttlsforce = paramlist->starttlsforce;

   if (starttls || starttlsforce){
      DMSG0(ctx, D2, "executing ldap_start_tls_s\n");
      if ((ret = ldap_start_tls_s(ldapconn, NULL, NULL)) != LDAP_SUCCESS)
      {
         ldapserverconnectionerror(ctx, ldapconn, ret);
         if (starttlsforce)
         {
            DMSG0(ctx, D1, "STARTTLSFORCE set, cannot continue!\n");
            JMSG0(ctx, M_ERROR, "STARTTLSFORCE set, cannot continue!\n");
            return NULL;
         }
      }
   }
#endif   /* HAVE_LDAP_START_TLS */

   binddn = paramlist->binddn;

   /* The password might be obfuscated */
   bindpass = paramlist->bindpass;

   DMSG2(ctx, D1, "credentials to connect: binddn='%s' pass='%s'\n", binddn, bindpass);

   cred.bv_val = bindpass;
   cred.bv_len = strlen(cred.bv_val);

   if ((ret = ldap_sasl_bind_s(ldapconn, binddn, LDAP_SASL_SIMPLE, &cred, NULL, NULL, &msgidp)) != LDAP_SUCCESS)
   {
      free_and_null_pool_memory(tmp);
      ldapserverconnectionerror(ctx, ldapconn, ret);
      if (strcasestr(ldapuri, "ldaps://") != NULL || starttls || starttlsforce){
         JMSG0(ctx, M_INFO, "Using TLS/SSL for LDAP service require CA certificate configuration on the backup server.\n");
         JMSG0(ctx, M_INFO, "If it is not configured properly to do a job over TLS/SSL it will fail.\n");
      }
      return NULL;
   }

   free_and_null_pool_memory(tmp);
   return ldapconn;
}

/*
 * disconnect from LDAP service
 *
 * in:
 *    ld - ldap connection handle to unbind
 * out:
 *    0 - on success
 *    1 - on error
 */
int ldapdisconnect(bpContext * ctx, LDAP * ld)
{
   int rc;

   rc = ldap_unbind_ext(ld, NULL, NULL);
   if (rc != LDAP_SUCCESS) {
      return 1;
   }

   return 0;
}

/*
 * check if supplied dn exist in directory
 *
 * in:
 *    ld - ldap connection
 *    dn - checked dn
 * out:
 *    True - dn exist in directory
 *    False - dn does not exist
 */
bool ldapcheckonedn(bpContext * ctx, LDAP * ld, char *dn)
{
   int rc;
   bool ret = false;
   int type;
   LDAPMessage *msg = NULL;

   DMSG(ctx, D3, "ldapcheckonedn for: %s\n", dn);

   rc = ldap_search_ext_s(ld, dn, LDAP_SCOPE_BASE, NULL, NULL,
           0, NULL, NULL, NULL, LDAP_NO_LIMIT, &msg);

   switch (rc){
      case LDAP_NO_SUCH_OBJECT:     /* object not found */
      case LDAP_REFERRAL:           /* for LDAP we reached an out of scope query */
         break;
      case LDAP_SUCCESS:
         type = ldap_msgtype(msg);
         DMSG(ctx, D3, "ldapcheckonedn: resulting msgtype: %i\n", type);
         ret = type == LDAP_RES_SEARCH_ENTRY;
         break;
      default:
         /* we reach some error */
         JMSG2(ctx, M_ERROR, "ldapcheckonedn: search error: %s for: %s\n", ldap_err2string(rc),dn);
         DMSG2(ctx, D1, "ldapcheckonedn: search error: %s for: %s\n", ldap_err2string(rc),dn);
   }

   ldap_msgfree(msg);

   return ret;
}
