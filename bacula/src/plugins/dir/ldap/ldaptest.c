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

#include <ldap.h>
#include <stdio.h>
#include <stdlib.h>
#include "config.h"

/*
  gcc/g++ -Wall ldaptest.c -o ldaptest -lldap
  ./ldaptest "<uri>" "<binddn>" "<bindpass>"
*/

const char *extensionStartTLS_OID   = "1.3.6.1.4.1.1466.20037";
const char *extensionTLSattrib      = "supportedExtension";
const char *extensionSASLattrib     = "supportedSASLMechanisms";

/*
 * connect to LDAP service
 */
LDAP *ldapconnect(char *ldapuri, char *binddn, char *bindpass, bool starttls)
{
   LDAP *ldapconn = NULL;
   int desired_version = LDAP_VERSION3;
   int ret;
   char *errmsg = NULL;
   int debug = 0xffff;
   struct berval cred;
   struct berval *msgidp = NULL;

   if ((ret = ldap_initialize(&ldapconn, ldapuri)) != LDAP_SUCCESS)
   {
      ldap_get_option(ldapconn, LDAP_OPT_DIAGNOSTIC_MESSAGE, (void*)&errmsg);
      printf("LDAP initialize error %s\n", ldap_err2string(ret));
      printf("diagnose error: %s\n", errmsg);
      ldap_memfree(errmsg);
      return NULL;
   }
   if ((ret = ldap_set_option(ldapconn, LDAP_OPT_PROTOCOL_VERSION, &desired_version)) != LDAP_OPT_SUCCESS)
   {
      ldap_get_option(ldapconn, LDAP_OPT_DIAGNOSTIC_MESSAGE, (void*)&errmsg);
      printf("LDAP set option error %s\n", ldap_err2string(ret));
      printf("diagnose error: %s\n", errmsg);
      ldap_memfree(errmsg);
      return NULL;
   }
   if ((ret = ldap_set_option(ldapconn, LDAP_OPT_REFERRALS, LDAP_OPT_OFF)) != LDAP_OPT_SUCCESS)
   {
      ldap_get_option(ldapconn, LDAP_OPT_DIAGNOSTIC_MESSAGE, (void*)&errmsg);
      printf("LDAP set option error %s\n", ldap_err2string(ret));
      printf("diagnose error: %s\n", errmsg);
      ldap_memfree(errmsg);
      return NULL;
   }

#ifdef HAVE_LDAP_START_TLS
   if (starttls){
      if ((ret = ldap_start_tls_s(ldapconn, NULL, NULL)) != LDAP_SUCCESS)
      {
         ldap_get_option(ldapconn, LDAP_OPT_DIAGNOSTIC_MESSAGE, (void*)&errmsg);
         printf("LDAP start TLS error %s\n", ldap_err2string(ret));
         printf("diagnose error: %s\n", errmsg);
         ldap_memfree(errmsg);
         return NULL;
      }
   }
#endif

   if( ldap_set_option(NULL, LDAP_OPT_DEBUG_LEVEL, &debug) != LDAP_OPT_SUCCESS)
   {
      ldap_get_option(ldapconn, LDAP_OPT_DIAGNOSTIC_MESSAGE, (void*)&errmsg);
      printf("LDAP could not set LDAP_OPT_DEBUG_LEVEL %d\n", debug);
      printf("diagnose error: %s\n", errmsg);
      ldap_memfree(errmsg);
   }

   cred.bv_val = bindpass;
   cred.bv_len = strlen(cred.bv_val);

   if ((ret = ldap_sasl_bind_s(ldapconn, binddn, LDAP_SASL_SIMPLE, &cred, NULL, NULL, &msgidp) != LDAP_SUCCESS))
   {
      ldap_get_option(ldapconn, LDAP_OPT_DIAGNOSTIC_MESSAGE, (void*)&errmsg);
      printf("LDAP Server bind error: %s\n", ldap_err2string(ret));
      printf("diagnose error: %s\n", errmsg);
      ldap_memfree(errmsg);
      return NULL;
   }
   printf("LDAP Server connection OK\n");
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
int ldapdisconnect(LDAP * ld)
{

   int rc;

   rc = ldap_unbind_ext(ld, NULL, NULL);
   if (rc != LDAP_SUCCESS) {
      return 1;
   }

   return 0;
}

int main(int argc, char **argv)
{
   LDAP *ld;
   /* all and extended attributes required */
   const char *attrs[] = { "+", "*", extensionTLSattrib, extensionSASLattrib, NULL };
   const char *filter = "(objectClass=*)";
   LDAPMessage *msg = NULL;
   LDAPMessage *entry;
   BerElement *berdata;
   struct berval **values;
   char *attr;
   int rc, i;
   bool have_start_tls = false;
   bool test_start_tls = false;

   if (argc < 4) {
      printf("Usage: %s <uri> <binddn> <bindpass> ", argv[0]);
#ifdef HAVE_LDAP_START_TLS
      printf ("[starttls]");
#endif
      printf ("\n");
      exit(1);
   }

#ifdef HAVE_LDAP_START_TLS
   if (argc > 4 && strcmp(argv[4], "starttls") == 0) {
      printf ("> Will test with start tls option.\n");
      test_start_tls = true;
   }
#endif

   ld = ldapconnect(argv[1], argv[2], argv[3], test_start_tls);
   if (ld) {
      i = 0;
      ldap_set_option(NULL, LDAP_OPT_DEBUG_LEVEL, &i);
      rc = ldap_search_ext_s(ld, "", LDAP_SCOPE_BASE, filter, (char **)attrs,
            0, NULL, NULL, NULL, LDAP_NO_LIMIT, &msg);

      switch (rc){
         case LDAP_SUCCESS:
            entry = ldap_first_entry(ld, msg);
            if (entry != NULL) {
               printf("\nRootDSE:\n");
               for (attr = ldap_first_attribute(ld, entry, &berdata);
                     attr;
                     attr = ldap_next_attribute(ld, entry, berdata)){
                  if ((values = ldap_get_values_len(ld, entry, attr)) != NULL) {
                     for (i = 0; values[i] != NULL; i++) {
                        printf ("\t %s: %s\n", attr, values[i]->bv_val);
                        if (strcmp(values[i]->bv_val, extensionStartTLS_OID) == 0 && strcmp(attr, extensionTLSattrib) == 0)
                        {
                           have_start_tls = true;
                        }
                     }
                  }
                  ldap_value_free_len(values);
                  ldap_memfree(attr);
               }
            }
         default:
            ldap_msgfree(msg);
      }

      ldapdisconnect(ld);
      printf ("\nTest successful!\n");
#ifdef HAVE_LDAP_START_TLS
      if (have_start_tls && !test_start_tls)
      {
         printf ("\nIt seems your LDAP server supports Start TLS Extension, so you could use 'starttls'\n");
         printf ("parameter to verify if you can use it in your configuration.\n\n");
      }
#endif
   } else {
      printf("\nTest failed. Please check the log above!\n");
   }
   exit(0);
}
