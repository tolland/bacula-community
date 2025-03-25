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
 * This is a Bacula plugin for making a backup and restore of individual LDAP objects.
 * Author: Radosław Korzeniewski, radekk@inteos.pl, Inteos Sp. z o.o.
 */

#ifndef _LDAP_DIR_H_
#define _LDAP_DIR_H_

#include "bacula.h"
#include "dir_plugins.h"
#include "dir_authplugin.h"
// #include "lib/ini.h"
#include "lib/cmd_parser.h"

/* it is an LDAP authentication plugin, so we need a libldap library */
#ifdef __WIN32__
#include <winldap.h>
#else
#include <ldap.h>
#endif

#include <sys/stat.h>
#include <sys/types.h>
#include <ctype.h>

#include "dirpluglib.h"
#include "ldap-util.h"

/*
 * libbac uses its own sscanf implementation which is not compatible with
 * libc implementation, unfortunately. usage of bsscanf require format string rewriting.
 */
#ifdef sscanf
#undef sscanf
#endif

/* Plugin compile time variables */
#define PLUGIN_LICENSE              "Bacula AGPLv3"
#define PLUGIN_AUTHOR               "Inteos Sp. z o.o."
#define PLUGIN_DATE                 "September 2020"
#define PLUGIN_VERSION              "0.1.0"
#define PLUGIN_DESCRIPTION          "BPAM LDAP plugin (c) Inteos"
#define PLUGIN_NAME                 "ldap"

class BPAMLDAP : public SMARTALLOC
{
private:
   POOL_MEM url;                // This is our LDAP server url
   POOL_MEM binddn;             // This is a binddn (user) to make queries at LDAP server
   POOL_MEM bindpass;            // This is a binddn (user) password
   POOL_MEM basedn;             // This is a LDAP basedn query starting point
   POOL_MEM filter;              // This is a LDAP query string for selecting required username
   bool starttls;
   bool starttlsforce;

   LDAP *ld;                        // This is out connection to LDAP server
   POOL_MEM userdn;

   POOL_MEM username;            // here we will save user response to our queries
   POOL_MEM password;            // here we will save user response to our queries

   bpContext *ctx;

   bRC ldapconnect();
   bRC ldapdisconnect();
   bool ldapsearchonedn();
   void ldapserverconnectionerror(int ret);

   bRC parse_plugin_params(const char *param);
   bRC do_ldap_authenticate();
   void substitute_filter_parameters();

public:
#if __cplusplus > 201103L
   BPAMLDAP() = delete;
   ~BPAMLDAP() = default;
#else
   BPAMLDAP() {};
   ~BPAMLDAP() {};
#endif
   BPAMLDAP(bpContext *pctx) :
      url(PM_FNAME),
      binddn(PM_FNAME),
      bindpass(PM_NAME),
      basedn(PM_FNAME),
      filter(PM_FNAME),
      starttls(false),
      starttlsforce(false),
      ld(NULL),
      userdn(PM_NAME),
      username(PM_NAME),
      password(PM_NAME),
      ctx(pctx)
   {};


   bRC handlePluginEvent(bDirEvent *event, void *value);
   bRC getAuthenticationData(const char *param, void **data);
   bRC getAuthorizationData(const char *param, void **data);
};

#endif /* _LDAP_FD_H_ */
