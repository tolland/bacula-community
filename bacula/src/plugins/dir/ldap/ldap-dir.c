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
 * Bacula Pluggable Authentication Modules - LDAP Plugin
 *
 * Author: Radosław Korzeniewski, radoslaw@korzeniewski.net, Inteos Sp. z o.o.
 */

#include "ldap-dir.h"

/*
 * This is a BPAM (Bacula Pluggable Authentication Modules) which authenticate users
 * found in any LDAP Directory Server. It support OpenLDAP and Microsoft Active Directory
 * among others standard LDAPs.
 */


// a list of authentication operations we expect and handle before final authenticate
static bDirAuthenticationData bpamldapquestions[] =
{
   // operation; question; seqdata;
   {bDirAuthenticationOperationLogin, "Username:", 0},
   {bDirAuthenticationOperationPassword, "Password:", 1},
};

#define LDAP_WELCOME       "LDAP Authentication"

// plugin authentication registration struct
static bDirAuthenticationRegister bpamldapregister =
{
#if __cplusplus > 201103L
   .name = PLUGIN_NAME,
   .welcome = LDAP_WELCOME,            // our plugin name which should correspond to plugin filename
   .num = 2,                           // it shows the number of defined auth operations
   .data = bpamldapquestions,          // our auth operations list
   .nsTTL = 0,                         // future usage
#else
   PLUGIN_NAME, LDAP_WELCOME, 2, bpamldapquestions, 0
#endif
};

#ifdef __cplusplus
extern "C" {
#endif

/* Forward referenced functions */
static bRC newPlugin(bpContext *ctx);
static bRC freePlugin(bpContext *ctx);
static bRC handlePluginEvent(bpContext *ctx, bDirEvent *event, void *value);
static bRC getAuthenticationData(bpContext *ctx, const char *console, const char *param, void **data);
static bRC getAuthorizationData(bpContext *ctx, const char *console, const char *param, void **data);

/* Pointers to Bacula functions */
bDirFuncs *bfuncs = NULL;
bDirInfo  *binfo = NULL;

static pDirInfo pluginInfo = {
   sizeof(pluginInfo),
   DIR_PLUGIN_INTERFACE_VERSION,
   DIR_PLUGIN_MAGIC,
   PLUGIN_LICENSE,
   PLUGIN_AUTHOR,
   PLUGIN_DATE,
   PLUGIN_VERSION,
   PLUGIN_DESCRIPTION
};

static pDirFuncs pluginFuncs = {
   sizeof(pluginFuncs),
   DIR_PLUGIN_INTERFACE_VERSION,

   /* Entry points into plugin */
   newPlugin,                          /* new plugin instance */
   freePlugin,                         /* free plugin instance */
   NULL,                               // we do not provide this callback
   NULL,                               // we do not provide this callback
   handlePluginEvent,
   getAuthenticationData,
   getAuthorizationData,
};

bRC loadPlugin(bDirInfo *lbinfo, bDirFuncs *lbfuncs, pDirInfo **pinfo, pDirFuncs **pfuncs)
{
   bfuncs = lbfuncs;                  /* set Bacula funct pointers */
   binfo  = lbinfo;
   Dmsg2(DINFO, "Loaded: size=%d version=%d\n", bfuncs->size, bfuncs->version);

   *pinfo  = &pluginInfo;             /* return pointer to our info */
   *pfuncs = &pluginFuncs;            /* return pointer to our functions */

   return bRC_OK;
}

bRC unloadPlugin()
{
   Dmsg0(DINFO, "plugin: Unloaded\n");
   return bRC_OK;
}

static bRC newPlugin(bpContext *ctx)
{
   BPAMLDAP *self = New (BPAMLDAP(ctx));
   DMSG0(ctx, DINFO, "newPlugin\n");
   ctx->pContext = (void*)self;

   return bRC_OK;
}

static bRC freePlugin(bpContext *ctx)
{
   DMSG0(ctx, DINFO, "freePlugin\n");
   if (!ctx){
      return bRC_Error;
   }

   BPAMLDAP *self = (BPAMLDAP*)ctx->pContext;

   if (!self){
      return bRC_Error;
   }
   delete self;

   return bRC_OK;
}

static bRC handlePluginEvent(bpContext *ctx, bDirEvent *event, void *value)
{
   DMSG(ctx, D1, "handlePluginEvent (%i)\n", event->eventType);
   BPAMLDAP *self = (BPAMLDAP*)ctx->pContext;
   return self->handlePluginEvent(event, value);
}

static bRC getAuthenticationData(bpContext *ctx, const char *console, const char *param, void **data)
{
   DMSG(ctx, D1, "getAuthenticationData (%s)\n", param);
   BPAMLDAP *self = (BPAMLDAP*)ctx->pContext;
   return self->getAuthenticationData(param, data);
}

static bRC getAuthorizationData(bpContext *ctx, const char *console, const char *param, void **data)
{
   DMSG(ctx, D1, "getAuthorizationData (%s)\n", param);
   BPAMLDAP *self = (BPAMLDAP*)ctx->pContext;
   return self->getAuthorizationData(param, data);
}

#ifdef __cplusplus
}
#endif

/**
 * @brief display common connection error message and diagnose
 *
 * @param ret the return code from ldap_* functions to analize and display
 */
void BPAMLDAP::ldapserverconnectionerror(int ret)
{
   char *errmsg = NULL;

   ldap_get_option(ld, LDAP_OPT_DIAGNOSTIC_MESSAGE, (void*)&errmsg);
   DMSG1(ctx, D1, "LDAP Server connection error: %s\n", ldap_err2string(ret));
   if (errmsg){
      DMSG1(ctx, D1, "diagnose error: %s\n", errmsg);
   }
   ldap_memfree(errmsg);
}

/**
 * @brief connect to LDAP service
 *
 * @return bRC on success bRC_OK, on error bRC_Error
 */
bRC BPAMLDAP::ldapconnect()
{
   const int desired_version = LDAP_VERSION3;
   POOL_MEM tmp(PM_FNAME);
   int ret;
   struct berval cred;
   struct berval *msgidp = NULL;
   bool starttls = false;
   bool starttlsforce = false;

   // first initialize ldap connection context `LDAP`
#ifdef __sun__
   // In Solaris our `url` parameter is a host name only and not a real URL.
   // It should be notice in documentation
   if ((ld = ldap_init(url.c_str(), LDAP_PORT)) == NULL) {
      return bRC_Error;
   }
#else
   if (ldap_initialize(&ld, url.c_str()) != LDAP_SUCCESS) {
      return bRC_Error;
   }
#endif

   // required a desired LDAP protocol version
#ifdef __sun__
   if ((ret = ldap_set_option(ld, LDAP_OPT_PROTOCOL_VERSION, &desired_version)) != LDAP_SUCCESS)
#else
   if ((ret = ldap_set_option(ld, LDAP_OPT_PROTOCOL_VERSION, &desired_version)) != LDAP_OPT_SUCCESS)
#endif
   {
      ldapserverconnectionerror(ret);
      return bRC_Error;
   }

   // disable referals return in queries - we do not support it
#ifdef __sun__
   if ((ret = ldap_set_option(ld, LDAP_OPT_REFERRALS, LDAP_OPT_OFF)) != LDAP_SUCCESS)
#else
   if ((ret = ldap_set_option(ld, LDAP_OPT_REFERRALS, LDAP_OPT_OFF)) != LDAP_OPT_SUCCESS)
#endif
   {
      ldapserverconnectionerror(ret);
      return bRC_Error;
   }

   // handle starttls if requested in configuration
#ifdef HAVE_LDAP_START_TLS
   //
   // Start TLS support
   //
   // To enable StartTLS for LDAP connection you have to setup 'starttls' or 'starttlsforce' plugin options
   // where 'starttlsforce' will fail connection if server do not support it.
   //
   if (starttls || starttlsforce){
      DMSG0(ctx, D2, "executing ldap_start_tls_s\n");
      if ((ret = ldap_start_tls_s(ld, NULL, NULL)) != LDAP_SUCCESS)
      {
         ldapserverconnectionerror(ret);
         if (starttlsforce)
         {
            DMSG0(ctx, D1, "STARTTLSFORCE set, cannot continue!\n");
            return bRC_Error;
         }
      }
   }
#endif   /* HAVE_LDAP_START_TLS */

   DMSG2(ctx, D1, "credentials to connect: binddn='%s' pass='%s'\n", binddn.c_str(), bindpass.c_str());

   cred.bv_val = bindpass.c_str();
   cred.bv_len = strlen(cred.bv_val);

   if ((ret = ldap_sasl_bind_s(ld, binddn.c_str(), LDAP_SASL_SIMPLE, &cred, NULL, NULL, &msgidp)) != LDAP_SUCCESS)
   {
      ldapserverconnectionerror(ret);
      if (strcasestr(url.c_str(), "ldaps://") != NULL || starttls || starttlsforce){
         DMSG0(ctx, M_INFO, "Using TLS/SSL for LDAP service require CA certificate configuration on the backup server.\n");
         DMSG0(ctx, M_INFO, "If it is not configured properly a connection over TLS/SSL will fail.\n");
      }
      return bRC_Error;
   }

   DMSG0(ctx, DDEBUG, "LDAP connection successful\n");
   return bRC_OK;
}

/**
 * @brief disconnect from LDAP service
 *
 * @return bRC on success bRC_OK, on error bRC_Error
 */
bRC BPAMLDAP::ldapdisconnect()
{
   int rc;

   rc = ldap_unbind_ext(ld, NULL, NULL);
   if (rc != LDAP_SUCCESS) {
      return bRC_Error;
   }

   return bRC_OK;
}

/**
 * @brief check if supplied dn exist in directory
 *
 * @param dn checked dn
 * @return true dn exist in directory
 * @return false dn does not exist
 */
bool BPAMLDAP::ldapsearchonedn()
{
   int rc;
   bool ret = false;
   int type;
   const char *attrs[] = { "cn", NULL };
   LDAPMessage *msg = NULL;
   const char *dn;

   DMSG2(ctx, D3, "ldapsearchonedn for: %s and filter: %s\n", basedn.c_str(), filter.c_str());

   rc = ldap_search_ext_s(ld, basedn.c_str(), LDAP_SCOPE_SUB, filter.c_str(), (char **)attrs,
           0, NULL, NULL, NULL, LDAP_NO_LIMIT, &msg);

   switch (rc){
      case LDAP_NO_SUCH_OBJECT:     /* object not found */
      case LDAP_REFERRAL:           /* for LDAP we reached an out of scope query */
         DMSG0(ctx, DDEBUG, "no such object or referral found\n");
         break;
      case LDAP_SUCCESS:
         type = ldap_msgtype(msg);
         DMSG(ctx, D3, "ldapsearchonedn resulting msgtype: %i\n", type);
         if (type == LDAP_RES_SEARCH_ENTRY){
            ret = true;
            dn = ldap_get_dn(ld, msg);
            if (!dn){
               DMSG0(ctx, DERROR, "ldapsearchonedn cannot get entry DN!\n");
               ret = false;
            } else {
               DMSG1(ctx, DDEBUG, "ldapsearchonedn get DN: %s\n", dn);
               pm_strcpy(userdn, dn);
            }
         }
         break;
      default:
         /* we reach some error */
         DMSG2(ctx, D1, "ldapsearchonedn search error: %s for: %s\n", ldap_err2string(rc), basedn.c_str());
         break;
   }

   ldap_msgfree(msg);

   return ret;
}

/**
 * @brief This method perform a variable substitute in filter expression.
 *    Substituted variables are:
 *    * %p - for user password response
 *    * %u - for username response
 *    Any single `%` character without next `p` or `u` will be simple rewritten without change.
 *    If you want to rewrite '%p' or '%u' without substituting it with predefined variables then
 *    you have to write it as: '%%p' or '%%u' respectively.
 */
void BPAMLDAP::substitute_filter_parameters()
{
   if (strlen(filter.c_str()) > 0){
      POOL_MEM tmp(PM_MESSAGE);
      char *p;
      char *s;
      const char *c = NULL;
      char *q = s = filter.c_str();
      tmp.c_str()[0] = '\0';
      while ((p = strchr(q, '%')) != NULL)
      {
         switch (p[1]){
            case 'u':
               *p = '\0';
               c = username.c_str();
               p++;
               break;

            case 'p':
               *p = '\0';
               c = password.c_str();
               p++;
               break;

            case '%':
               p[1] = '\0';
               p++;
               c = "";
               break;
            default:
               c = NULL;
               break;
         }
         q = p + 1;
         if (c){
            pm_strcat(tmp, s);
            pm_strcat(tmp, c);
            s = q;
         }
      }
      if (s){
         pm_strcat(tmp, s);
      }
      pm_strcpy(filter, tmp.c_str());
      DMSG1(ctx, DINFO, "filter after substitute: %s\n", filter.c_str());
   }
}

/**
 * @brief Perform a standard LDAP authentication using authentication parameters and user credentials.
 *        The The LDAP authentication procedure is similar to the one implemented by Apache mod_authnz_ldap
 *        (check: https://httpd.apache.org/docs/2.4/mod/mod_authnz_ldap.html) which is known as "search/bind".
 *        In this case Plugin first connects to LDAP Service to search for authenticated user definition
 *        (using binddn and bindpass) as it requires a proper user DN to do the second part of authentication.
 *        If found then it will bind to LDAP on behalf of the user with user credentials from bconsole
 *        interactive query/response session.
 *
 * @return bRC bRC_OK on successful authentication, bRC_Error when not
 */
bRC BPAMLDAP::do_ldap_authenticate()
{
   // prepare search filter based on response variables
   substitute_filter_parameters();

   // connect using bind credentials
   // TODO: we should handle first search without credentials
   if (ldapconnect() != bRC_OK){
      return bRC_Error;
   }

   // check if user query rerurn required object
   if (!ldapsearchonedn()){
      return bRC_Error;
   }

   // its all with bind credentials
   if (ldapdisconnect() != bRC_OK){
      return bRC_Error;
   }

   // now we should connect to LDAP using user credentials
   pm_strcpy(binddn, userdn.c_str());
   pm_strcpy(bindpass, password.c_str());
   // login as user
   if (ldapconnect() != bRC_OK){
      return bRC_Error;
   }
   // disconnect
   if (ldapdisconnect() != bRC_OK){
      return bRC_Error;
   }

   // now we should report login success!
   DMSG0(ctx, DINFO, "LDAP Authentication Successfull!\n");
   return bRC_OK;
}

/**
 * @brief Hndles plugin events defined in BPAM API.
 *
 * @param event the event to handle
 * @param value a pointer to opional value for selected event
 * @return bRC bRC_OK on success, bRC_Error on any error
 */
bRC BPAMLDAP::handlePluginEvent(bDirEvent *event, void *value)
{
   bDirAuthValue *pvalue;

   switch (event->eventType) {
   case bDirEventAuthenticationQuestion:
      break;

   case bDirEventAuthenticationResponse:
      pvalue = (bDirAuthValue *)value;
      DMSG_EVENT_STR(event, pvalue->response);
      switch (pvalue->seqdata){
         case 0:     // username
            pm_strcpy(username, pvalue->response);
            break;
         case 1:     // password
            pm_strcpy(password, pvalue->response);
            break;
         default:
            return bRC_Error;
         }
      break;

   case bDirEventAuthenticate:
      DMSG_EVENT_PTR(event, value);
      return do_ldap_authenticate();

   default:
      break;
   }

   return bRC_OK;
}

/**
 * @brief Handles Plugin parameters defined in "Authentication Plugin = ..." Console resource parameter.
 *
 * @param param a string from "Authentication Plugin = ..." Console resource parameter
 * @return bRC bRC_OK when all parameters are valid and parsed, bRC_Error on any error
 */
bRC BPAMLDAP::parse_plugin_params(const char *param)
{
   cmd_parser parser;

   if (!param){
      return bRC_Error;
   }

   // and parse command
   if (parser.parse_cmd(param) != bRC_OK) {
      DMSG0(ctx, DERROR, "Unable to parse Plugin parameters.\n");
      return bRC_Error;
   }

   // the first (zero) parameter is a plugin name
   if (!bstrcmp(parser.argk[0], PLUGIN_NAME)){
      return bRC_Error;
   }

   // iterate over next parameters
   for (int i = 1; i < parser.argc; i++) {
      if (bstrcmp(parser.argk[i], "url")){
         pm_strcpy(url, parser.argv[i]);
         DMSG1(ctx, DDEBUG, "parsed url: %s\n", url.c_str());
         continue;
      } else
      if (bstrcmp(parser.argk[i], "binddn")){
         pm_strcpy(binddn, parser.argv[i]);
         DMSG1(ctx, DDEBUG, "parsed binddn: %s\n", binddn.c_str());
         continue;
      } else
      if (bstrcmp(parser.argk[i], "bindpass")) {
         pm_strcpy(bindpass, parser.argv[i]);
         DMSG1(ctx, DDEBUG, "parsed bindpass: %s\n", bindpass.c_str());
         continue;
      } else
      if (bstrcmp(parser.argk[i], "query")) {
         POOL_MEM tmp(PM_MESSAGE);
         pm_strcpy(tmp, parser.argv[i]);
         char *d = strchr(tmp.c_str(), '/');
         if (!d){
            DMSG1(ctx, DERROR, "Cannot find basedn delimiter in query=%s\n", tmp.c_str());
            return bRC_Error;
         }
         // separate basedn and filter
         *d = '\0';
         pm_strcpy(basedn, tmp.c_str());
         pm_strcpy(filter, d + 1);
         DMSG2(ctx, DDEBUG, "parsed query - basedn:%s filter:%s \n", basedn.c_str(), filter.c_str());
         continue;
      } else
      if (bstrcmp(parser.argk[i], "starttls")) {
         starttls = true;
         DMSG0(ctx, DDEBUG, "parsed starttls\n");
         continue;
      } else
      if (bstrcmp(parser.argk[i], "starttlsforce")) {
         starttlsforce = true;
         DMSG0(ctx, DDEBUG, "parsed starttlsforce\n");
         continue;
      } else {
         DMSG1(ctx, DERROR, "unknown parameter: %s\n", parser.argk[i]);
         return bRC_Error;
      }
   }

   return bRC_OK;
}

/**
 * @brief Register BPAM LDAP Plugin authentication operations.
 *
 * @param param a string from "Authentication Plugin = ..." Console resource parameter
 * @param data a double pointer used for return value
 * @return bRC bRC_OK on success, bRC_Error on any error
 */
bRC BPAMLDAP::getAuthenticationData(const char *param, void **data)
{
   bDirAuthenticationRegister **padata = (bDirAuthenticationRegister **)data;

   DMSG1(ctx, DINFO, "registering with: %s\n", NPRT(param));
   if (parse_plugin_params(param) != bRC_OK){
      return bRC_Error;
   }
   *padata = &bpamldapregister;

   return bRC_OK;
}

/**
 * @brief Unimplemented
 *
 * @param param Unimplemented
 * @param data Unimplemented
 * @return bRC Unimplemented
 */
bRC BPAMLDAP::getAuthorizationData(const char *param, void **data)
{
   return bRC_OK;
}
