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
 * Bacula authentication and authorization plugin framework.
 *
 * Author: Radosław Korzeniewski, MMXX
 * radoslaw@korzeniewski.net, radekk@inteos.pl
 * Inteos Sp. z o.o. http://www.inteos.pl/
 */

#include "bacula.h"
#include "dird.h"
#include "dir_authplugin.h"

const int dbglvl = 500;

// borrow a constant from main dir_plugins.c
extern const char *plugin_type;

/**
 * @brief Prepares a consistent plugin name based on a differently formatted input strings.
 *        First it checks for a plugin file name which ends with '-dir.so' defined in dir_plugins.c
 *        Next it checks for plugin name colon ':' delimiter in plugin parameter string.
 *        Finally it just copies the whole string as a plugin name.
 *
 * @param plugin a plugin name source string
 * @return char* a prepared plugin name as a newly allocated string which caller has to free
 */
static char * prepare_authplugin_name(const char * plugin)
{
   char *buf = bstrdup(plugin);
   char *p;

   p = strstr(buf, plugin_type);

   if (!p){
      p = strchr(buf, ':');
   }

   if (p){
      // prepare plugin name
      *p = '\0';
   }

   return buf;
}

/**
 * @brief Checks if selected `Plugin` is defined in Authentication Plugin parameter from `param`.
 *        It is checking a plugin filename with console resource parameter string.
 *
 * @param plugin the `Plugin` class to check
 * @param param the Authentication Plugin parameter from console resource
 * @return true when it is an authentication plugin which matches request
 * @return false when it is something other
 */
static bool is_authentication_plugin(Plugin *plugin, const char *param)
{
   char *pluginfile = prepare_authplugin_name(plugin->file);
   char *pluginname = prepare_authplugin_name(param);
   bool rc = 0;

   if (pluginfile){
      if (pluginname){
         Dmsg2(dbglvl, "plugin: %s reqested: %s\n", pluginfile, pluginname);
         if (strncmp(pluginfile, pluginname, strlen(pluginname)) == 0 &&
               dirplug_func(plugin)->getPluginAuthenticationData){
            rc = true;
         }
         free(pluginname);
      }
      free(pluginfile);
   }

   return rc;
}

/**
 * @brief Generate a director auth plugin (BPAM) event.
 *        The event is directed to a single plugin only as it could include sensitive authentication
 *        data which should not be propagated to all. It is a main reason why it is a different
 *        function and we do not use `generate_plugin_event()` from dir_plugins.c
 *        It currently handle authentication plugins only, but it could change in the future.
 *
 * @param jcr job control record
 * @param pluginname a name of the plugin which should handle the event
 * @param eventType an event to generate
 * @param value a generic value passed to plugin which handles an event
 * @return bRC it could return a following values:
 *    - bRC_Max when no plugins loaded
 *    - bRC_Cancel - when job is canceled
 *    - bRC_OK - on success, propagated directly from plugin handling function
 *    - any other on error, propagated directly from plugin handling function
 */
static bRC dir_authplugin_generate_plugin_event(JCR *jcr, const char * pluginname, bDirEventsType eventType, void *value)
{
   bpContext *plugin_ctx;
   bDirEvent event;
   Plugin *plugin;
   int i = 0;
   bRC rc = bRC_OK;

   if (!b_plugin_list || !jcr || !jcr->plugin_ctx_list) {
      return bRC_Max;                  // Return if no plugins loaded.
   }

   if (jcr->is_job_canceled()) {
      return bRC_Cancel;
   }

   bpContext *plugin_ctx_list = (bpContext *)jcr->plugin_ctx_list;
   event.eventType = eventType;

   foreach_alist_index(i, plugin, b_plugin_list) {
      plugin_ctx = &plugin_ctx_list[i];
      if (!is_authentication_plugin(plugin, pluginname)) {
         continue;
      }
      if (dirplug_func(plugin)->handlePluginEvent){
         rc = dirplug_func(plugin)->handlePluginEvent(plugin_ctx, &event, value);
      }
      break;
   }

   return rc;
}

/**
 * @brief Return Authentication Data struct from selected plugin.
 *        The Plugin should use Authentication Plugin parameter string from `param` variable
 *        to prepare approaching authentication operations (events).
 *        The returned struct (`bDirAuthenticationRegister`) will be used by Director to
 *        perform required authentication operations.
 *
 * @param jcr job control record
 * @param param a Console resource Authentication Plugin parameter string
 * @return void* a `NULL` is returned on any error and a pointer to bDirAuthenticationRegister
 *         struct (casted to void*) on success
 */
void *dir_authplugin_getauthenticationData(JCR *jcr, const char *console, const char *param)
{
   bpContext *plugin_ctx;
   Plugin *plugin;
   int i = 0;
   bDirAuthenticationRegister *data = NULL;

   if (!b_plugin_list || !jcr || !jcr->plugin_ctx_list) {
      return NULL;                  /* Return if no plugins loaded */
   }

   if (jcr->is_job_canceled()) {
      return NULL;
   }

   bpContext *plugin_ctx_list = (bpContext *)jcr->plugin_ctx_list;

   foreach_alist_index(i, plugin, b_plugin_list) {
      plugin_ctx = &plugin_ctx_list[i];
      if (!is_authentication_plugin(plugin, param)) {
         continue;
      }

      if (dirplug_func(plugin)->getPluginAuthenticationData(plugin_ctx, console, param, (void**)&data) != bRC_OK){
         // getting authdata operations failed
         return NULL;
      }

      // check if data returned are valid
      if (data->name == NULL ||
          data->data == NULL ||
          data->num == 0){
         Dmsg1(1, "Invalid bDirAuthenticationRegister data for %s\n", plugin->file);
         return NULL;
      }

      break;
   }

   return (void*)data;
}

/**
 * @brief A main function to sending a message to bconsole using a valid `bsock`.
 *
 * @param bsock the `BSOCK` object for Director <-> bconsole communication
 * @param type the message type to send
 * @param message a message string to sent
 * @return bRC bRC_OK on success, bRC_Error on any error
 */
static bRC dir_authplugin_display_message(BSOCK *bsock, const char type, const char *message)
{
   // display message
   if (!bsock->fsend("%c%s\n", type, message)) {
      Dmsg1(dbglvl, "Send auth message comm error. ERR=%s\n", bsock->bstrerror());
      return bRC_Error;
   }
   return bRC_OK;
}

/**
 * @brief Handles a response (data receive) from bconsole using a valid `bsock`.
 *
 * @param jcr job control record
 * @param bsock the `BSOCK` object for Director <-> bconsole communication
 * @param pluginname a name of the plugin which should receive an resoinse with plugin event
 * @param seqdata the `seqdata` variable for current authentication operation (basically from bDirAuthenticationData struct)
 * @return bRC bRC_OK on success, bRC_Error on any error
 */
static bRC dir_authplugin_handle_response(JCR *jcr, BSOCK *bsock, const char *pluginname, const uint32_t seqdata)
{
   bDirAuthValue value;

   // get response
   if (bsock->wait_data(180) <= 0 || bsock->recv() <= 0) {
      Dmsg1(dbglvl, "Receive auth response comm error. ERR=%s\n", bsock->bstrerror());
      bmicrosleep(5, 0);
      return bRC_Error;
   }

   // check if it is a response packet
   if (bsock->msg[0] != UA_AUTH_INTERACTIVE_RESPONSE){
      Dmsg1(dbglvl, "Receive auth response packet error. Sig=%d\n", (int)bsock->msg[0]);
      bmicrosleep(5, 0);
      return bRC_Error;
   }

   // forward response to plugin
   value.seqdata = seqdata;
   value.response = bsock->msg + 1;    // we have to omit a first character which is a packet mark
   return dir_authplugin_generate_plugin_event(jcr, pluginname, bDirEventAuthenticationResponse, (void*)&value);
}

/**
 * @brief Perform a final BPAM authenticate with a Plugin.
 *
 * @param jcr job control record
 * @param bsock the `BSOCK` object for Director <-> bconsole communication
 * @param pluginname a name of the plugin which should handle authentication
 * @return bRC bRC_OK on success - authentication OK, bRC_Error on any error
 */
bRC dir_authplugin_authenticate(JCR *jcr, BSOCK *bsock, const char *pluginname)
{
   // sanity check
   if (!jcr || !bsock || !pluginname){
      Dmsg3(1, "Invalid input parameters: %p %p %p %p\n", jcr, bsock, pluginname);
      return bRC_Error;
   }

   if (dir_authplugin_display_message(bsock, UA_AUTH_INTERACTIVE_FINISH, "") != bRC_OK){
      return bRC_Error;
   }

   return dir_authplugin_generate_plugin_event(jcr, pluginname, bDirEventAuthenticate, NULL);
}

/**
 * @brief Perform BPAM authentication interaction with a user/bconsole nased on authentication data operation
 *
 * @param jcr job control record
 * @param bsock the `BSOCK` object for Director <-> bconsole communication
 * @param pluginname a name of the plugin which should handle authentication interaction
 * @param data a pointer to the bDirAuthenticationData single authentication operation
 * @param pluginall if set to true then we are called from a recursive bDirAuthenticationOperationPluginAll
 * @return bRC bRC_OK on success - operation OK, bRC_Error on any error
 */
bRC dir_authplugin_do_interaction(JCR *jcr, BSOCK *bsock, const char *pluginname, void *data, bool pluginall)
{
   bDirAuthenticationData *authData = (bDirAuthenticationData *)data;
   bDirAuthValue value;

   // sanity check
   if (!jcr || !bsock || !pluginname || !data){
      Dmsg4(1, "Invalid input parameters: %p %p %p %p\n", jcr, bsock, pluginname, data);
      return bRC_Error;
   }

   switch (authData->operation){
      case bDirAuthenticationOperationPlugin:
         // ask plugin about operation to execute
         value.seqdata = authData->seqdata;
         if (dir_authplugin_generate_plugin_event(jcr, pluginname, bDirEventAuthenticationQuestion, (void *)&value) != bRC_OK){
            // error
            Dmsg1(dbglvl, "Error getting question from plugin: %s\n", pluginname);
            return bRC_Error;
         }
         return dir_authplugin_do_interaction(jcr, bsock, pluginname, (void*)value.authdata, pluginall);

      case bDirAuthenticationOperationPluginAll:
         // we will handle all interaction if no bDirAuthenticationOperationPluginAll was generated before
         if (!pluginall){
            while (dir_authplugin_generate_plugin_event(jcr, pluginname, bDirEventAuthenticationQuestion, (void *)&value) == bRC_OK){
               // do interaction with operation from plugin and mark it is during pluginall operation
               if (dir_authplugin_do_interaction(jcr, bsock, pluginname, (void*)value.authdata, true) != bRC_OK){
                  return bRC_Error;
               }
            }
         } else {
            // we do not support bDirAuthenticationOperationPluginAll during bDirAuthenticationOperationPluginAll operation
            return bRC_Error;
         }
         break;

      case bDirAuthenticationOperationMessage:
         // display message
         return dir_authplugin_display_message(bsock, UA_AUTH_INTERACTIVE_MESSAGE, authData->question);

      case bDirAuthenticationOperationPlain:
         // display message
         if (dir_authplugin_display_message(bsock, UA_AUTH_INTERACTIVE_PLAIN, authData->question) != bRC_OK){
            return bRC_Error;
         }
         return dir_authplugin_handle_response(jcr, bsock, pluginname, authData->seqdata);

      case bDirAuthenticationOperationHidden:
         // display message
         if (dir_authplugin_display_message(bsock, UA_AUTH_INTERACTIVE_HIDDEN, authData->question) != bRC_OK){
            return bRC_Error;
         }
         return dir_authplugin_handle_response(jcr, bsock, pluginname, authData->seqdata);

      case bDirAuthenticationOperationAuthenticate:
         return bRC_OK;

      default:
         return bRC_Error;
   }

   return bRC_OK;
}
