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
 * @file pluginlib.cpp
 * @author Radosław Korzeniewski (radoslaw@korzeniewski.net)
 * @brief Common definitions and utility functions for Inteos plugins.
 * @version 2.2.0
 * @date 2021-04-26
 *
 * Common definitions and utility functions for Inteos plugins.
 * Functions defines a common framework used in our utilities and plugins.
 * Author: Radosław Korzeniewski, radekk@inteos.pl, Inteos Sp. z o.o.
 */

#include "pluginlib.h"


/* Events that are passed to plugin
typedef enum {
  bEventJobStart                        = 1,
  bEventJobEnd                          = 2,
  bEventStartBackupJob                  = 3,
  bEventEndBackupJob                    = 4,
  bEventStartRestoreJob                 = 5,
  bEventEndRestoreJob                   = 6,
  bEventStartVerifyJob                  = 7,
  bEventEndVerifyJob                    = 8,
  bEventBackupCommand                   = 9,
  bEventRestoreCommand                  = 10,
  bEventEstimateCommand                 = 11,
  bEventLevel                           = 12,
  bEventSince                           = 13,
  bEventCancelCommand                   = 14,
  bEventVssBackupAddComponents          = 15,
  bEventVssRestoreLoadComponentMetadata = 16,
  bEventVssRestoreSetComponentsSelected = 17,
  bEventRestoreObject                   = 18,
  bEventEndFileSet                      = 19,
  bEventPluginCommand                   = 20,
  bEventVssBeforeCloseRestore           = 21,
  bEventVssPrepareSnapshot              = 22,
  bEventOptionPlugin                    = 23,
  bEventHandleBackupFile                = 24,
  bEventComponentInfo                   = 25
} bEventType;
*/

const char *eventtype2str(bEvent *event){
   switch (event->eventType){
      case bEventJobStart:
         return "bEventJobStart";
      case bEventJobEnd:
         return "bEventJobEnd";
      case bEventStartBackupJob:
         return "bEventStartBackupJob";
      case bEventEndBackupJob:
         return "bEventEndBackupJob";
      case bEventStartRestoreJob:
         return "bEventStartRestoreJob";
      case bEventEndRestoreJob:
         return "bEventEndRestoreJob";
      case bEventStartVerifyJob:
         return "bEventStartVerifyJob";
      case bEventEndVerifyJob:
         return "bEventEndVerifyJob";
      case bEventBackupCommand:
         return "bEventBackupCommand";
      case bEventRestoreCommand:
         return "bEventRestoreCommand";
      case bEventEstimateCommand:
         return "bEventEstimateCommand";
      case bEventLevel:
         return "bEventLevel";
      case bEventSince:
         return "bEventSince";
      case bEventCancelCommand:
         return "bEventCancelCommand";
      case bEventVssBackupAddComponents:
         return "bEventVssBackupAddComponents";
      case bEventVssRestoreLoadComponentMetadata:
         return "bEventVssRestoreLoadComponentMetadata";
      case bEventVssRestoreSetComponentsSelected:
         return "bEventVssRestoreSetComponentsSelected";
      case bEventRestoreObject:
         return "bEventRestoreObject";
      case bEventEndFileSet:
         return "bEventEndFileSet";
      case bEventPluginCommand:
         return "bEventPluginCommand";
      case bEventVssBeforeCloseRestore:
         return "bEventVssBeforeCloseRestore";
      case bEventVssPrepareSnapshot:
         return "bEventVssPrepareSnapshot";
      case bEventOptionPlugin:
         return "bEventOptionPlugin";
      case bEventHandleBackupFile:
         return "bEventHandleBackupFile";
      case bEventComponentInfo:
         return "bEventComponentInfo";
      default:
         return "Unknown";
   }
}


/*
 * Return the real size of the disk based on the size suffix.
 *
 * in:
 *    disksize - the numeric value of the disk size to compute
 *    suff - the suffix for a disksize value
 * out:
 *    uint64_t - the size of the disk computed with suffix
 */
uint64_t pluglib_size_suffix(int disksize, char suff)
{
   uint64_t size;

   switch (suff){
      case 'G':
         size = (uint64_t)disksize * 1024 * 1048576;
         break;
      case 'M':
         size = (uint64_t)disksize * 1048576;
         break;
      case 'T':
         size = (uint64_t)disksize * 1048576 * 1048576;
         break;
      case 'K':
      case 'k':
         size = (uint64_t)disksize * 1024;
         break;
      default:
         size = disksize;
   }
   return size;
}

/*
 * Return the real size of the disk based on the size suffix.
 *    This version uses a floating point numbers (double) for computation.
 *
 * in:
 *    disksize - the numeric value of the disk size to compute
 *    suff - the suffix for a disksize value
 * out:
 *    uint64_t - the size of the disk computed with suffix
 */
uint64_t pluglib_size_suffix(double disksize, char suff)
{
   uint64_t size;

   switch (suff){
      case 'G':
         size = disksize * 1024.0 * 1048576.0;
         break;
      case 'M':
         size = disksize * 1048576.0;
         break;
      case 'T':
         size = disksize * 1048576.0 * 1048576.0;
         break;
      case 'K':
      case 'k':
         size = disksize * 1024.0;
         break;
      default:
         size = disksize;
   }
   return size;
}

/*
 * Creates a path hierarchy on local FS.
 *  It is used for local restore mode to create a required directory.
 *  The functionality is similar to 'mkdir -p'.
 *
 * TODO: make a support for relative path
 * TODO: check if we can use findlib/makepath implementation instead
 *
 * in:
 *    bpContext - for Bacula debug and jobinfo messages
 *    path - a full path to create, does not check if the path is relative,
 *           could fail in this case
 * out:
 *    bRC_OK - path creation was successful
 *    bRC_Error - on any error
 */
bRC pluglib_mkpath(bpContext* ctx, char* path, bool isfatal)
{
#ifdef PLUGINPREFIX
#define _OLDPREFIX   PLUGINPREFIX
#endif
#define PLUGINPREFIX    "pluglibmkpath:"
   struct stat statp;
   POOL_MEM dir(PM_FNAME);
   char *p, *q;

   if (!path){
      return bRC_Error;
   }
   if (stat(path, &statp) == 0){
      if (S_ISDIR(statp.st_mode)){
         return bRC_OK;
      } else {
         DMSG(ctx, DERROR, "Path %s is not directory\n", path);
         JMSG(ctx, isfatal ? M_FATAL : M_ERROR, "Path %s is not directory\n", path);
         return bRC_Error;
      }
   }
   DMSG(ctx, DDEBUG, "mkpath verify dir: %s\n", path);
   pm_strcpy(dir, path);
   p = dir.addr() + 1;
   while (*p && (q = strchr(p, (int)PathSeparator)) != NULL){
      *q = 0;
      DMSG(ctx, DDEBUG, "mkpath scanning(1): %s\n", dir.c_str());
      if (stat(dir.c_str(), &statp) == 0){
         *q = PathSeparator;
         p = q + 1;
         continue;
      }
      DMSG0(ctx, DDEBUG, "mkpath will create dir(1).\n");
      if (mkdir(dir.c_str(), 0750) < 0){
         /* error */
         berrno be;
         DMSG2(ctx, DERROR, "Cannot create directory %s Err=%s\n", dir.c_str(), be.bstrerror());
         JMSG2(ctx, isfatal ? M_FATAL : M_ERROR, "Cannot create directory %s Err=%s\n", dir.c_str(), be.bstrerror());
         return bRC_Error;
      }
      *q = PathSeparator;
      p = q + 1;
   }
   DMSG0(ctx, DDEBUG, "mkpath will create dir(2).\n");
   if (mkdir(path, 0750) < 0){
      /* error */
      berrno be;
      DMSG2(ctx, DERROR, "Cannot create directory %s Err=%s\n", path, be.bstrerror());
      JMSG2(ctx, isfatal ? M_FATAL : M_ERROR, "Cannot create directory %s Err=%s\n", path, be.bstrerror());
      return bRC_Error;
   }
   DMSG0(ctx, DDEBUG, "mkpath finish.\n");
#ifdef _OLDPREFIX
#define PLUGINPREFIX    _OLDPREFIX
#undef _OLDPREFIX
#else
#undef PLUGINPREFIX
#endif
   return bRC_OK;
}

/**
 * @brief It splits a string `str` into a separate substrings split on `sep` character.
 *
 * @param str a string to split
 * @param sep split separator character
 * @return alist* newly allocated list of splitted substrings
 */
alist * plugutil_str_split_to_alist(const char * str, const char sep)
{
   alist * list = New(alist(5, true));
   plugutil_str_split_to_alist(*list, str, sep);
   return list;
}

/**
 * @brief It splits a string `str` into a separate substrings split on `sep` character.
 *
 * @param list a list used to populate split results
 * @param str a string to split
 * @param sep split separator character
 */
void plugutil_str_split_to_alist(alist  *list, const char * str, const char sep)
{
   plugutil_str_split_to_alist(*list, str, sep);
}

/**
 * @brief It splits a string `str` into a separate substrings split on `sep` character. Leading and trailing spaces are removed.
 *
 * @param list a list used to populate split results
 * @param str a string to split
 * @param sep split separator character
 */
void plugutil_str_split_to_alist(alist &list, const char * str, const char sep)
{
   if (str != NULL && strlen(str) > 0) {
      POOL_MEM buf(PM_NAME);
      const char * p = str;
      const char * q;

      do {
         // search for separator char - sep
         q = strchr(p, sep);
         if (q == NULL){
            // copy whole string from p to buf
            pm_strcpy(buf, p);
         } else {
            // copy string from p up to q
            pm_memcpy(buf, p, q - p + 1);
            buf.c_str()[q - p] = '\0';
            p = q + 1;     // next element
         }
         /* The following cleanup has been added to split strings such as
          * item1: value  => 'item1', 'value'  and not 'item1', ' value'
          */
         strip_leading_space(buf.c_str());
         strip_trailing_junk(buf.c_str());

         // in buf we have splitted string part
         const char * s = bstrdup(buf.c_str());
         list.append((void*)s);
      } while (q != NULL);
   }
}

/*
 * Render an external tool parameter for string value.
 *
 * in:
 *    bpContext - for Bacula debug and jobinfo messages
 *    param - a pointer to the param variable where we will render a parameter
 *    pname - a name of the parameter to compare
 *    fmt - a low-level parameter name
 *    name - a name of the parameter from parameter list
 *    value - a value to render
 * out:
 *    True if parameter was rendered
 *    False if it was not the parameter required
 */
bool render_param(POOLMEM **param, const char *pname, const char *fmt, const char *name, const char *value)
{
   if (bstrcasecmp(name, pname)){
      if (!*param){
         *param = get_pool_memory(PM_NAME);
         Mmsg(*param, " -%s '%s' ", fmt, value);
         DMsg1(DDEBUG, "render param:%s\n", *param);
      }
      return true;
   }
   return false;
}

/*
 * Render an external tool parameter for string value.
 *
 * in:
 *    bpContext - for Bacula debug and jobinfo messages
 *    param - a pointer to the param variable where we will render a parameter
 *    pname - a name of the parameter to compare
 *    fmt - a low-level parameter name
 *    name - a name of the parameter from parameter list
 *    value - a value to render
 * out:
 *    True if parameter was rendered
 *    False if it was not the parameter required
 */
bool render_param(POOLMEM **param, const char *pname, const char *fmt, const char *name, const int value)
{
   if (bstrcasecmp(name, pname)){
      if (!*param){
         *param = get_pool_memory(PM_NAME);
         Mmsg(*param, " -%s %d ", value);
         DMsg1(DDEBUG, "render param:%s\n", *param);
      }
      return true;
   }
   return false;
}

/**
 * @brief
 *
 * @param param
 * @param pname
 * @param name
 * @param value
 * @return true
 * @return false
 */
bool parse_param(POOL_MEM &param, const char *pname, const char *name, const char *value)
{
   if (bstrcasecmp(name, pname)){
      pm_strcpy(param, value);
      DMsg1(DDEBUG, "parse param:%s\n", param.c_str());
      return true;
   }
   return false;
};

/*
 * Setup XECOMMCTX parameter for boolean value.
 *
 * in:
 *    bpContext - for Bacula debug and jobinfo messages
 *    param - a pointer to the param variable where we will render a parameter
 *    pname - a name of the parameter to compare
 *    name - a name of the parameter from parameter list
 *    value - a value to render
 * out:
 *    True if parameter was rendered
 *    False if it was not the parameter required
 */
// TODO: It should be called setup_param
bool render_param(bool &param, const char *pname, const char *name, const bool value)
{
   if (bstrcasecmp(name, pname))
   {
      param = value;
      DMsg2(DDEBUG, "render param: %s=%s\n", pname, param ? "True" : "False");
      return true;
   }
   return false;
}

/**
 * @brief Renders a parameter as a "key=value" string into a prepared buffer.
 *
 * @param param the place to render to
 * @param handler the handler determines the value type
 * @param key the "key" name
 * @param val the "value" to render
 * @return true if parameter rendering ok
 * @return false on rendering error
 */
bool render_param(POOL_MEM &param, INI_ITEM_HANDLER *handler, char *key, item_value val)
{
   if (handler == ini_store_str){
      Mmsg(param, "%s=%s\n", key, val.strval);
   } else
   if (handler == ini_store_int64){
      Mmsg(param, "%s=%lld\n", key, val.int64val);
   } else
   if (handler == ini_store_bool){
      Mmsg(param, "%s=%d\n", key, val.boolval ? 1 : 0);
   } else {
      DMsg1(DERROR, "Unsupported parameter handler for: %s\n", key);
      return false;
   }
   return true;
}

/**
 * @brief Set the up param value
 *
 * @param param the param variable where we will setup a parameter
 * @param pname a name of the parameter to compare
 * @param name a name of the parameter from parameter list
 * @param value a value to setup
 * @return true if parameter was handled
 * @return false if it was not the parameter required, and param was not changed
 */
bool setup_param(int32_t &param, const char *pname, const char *name, const int32_t value)
{
   if (bstrcasecmp(name, pname))
   {
      param = value;
      DMsg2(DDEBUG, "setup param: %s=%d\n", pname, param);
      return true;
   }
   return false;
}

/**
 * @brief Set the up param value
 *
 * @param param the param variable where we will setup a parameter
 * @param pname a name of the parameter to compare
 * @param name a name of the parameter from parameter list
 * @param value a value to setup
 * @return true if parameter was handled
 * @return false if it was not the parameter required, and param was not changed
 */
bool setup_param(bool &param, const char *pname, const char *name, const bool value)
{
   if (bstrcasecmp(name, pname))
   {
      param = value;
      DMsg2(DDEBUG, "render param: %s=%s\n", pname, param ? "True" : "False");
      return true;
   }
   return false;
}

/**
 * @brief Set the up param value
 *
 * @param param the param variable where we will setup a parameter
 * @param pname a name of the parameter to compare
 * @param name a name of the parameter from parameter list
 * @param value a value to setup
 * @return true if parameter was handled
 * @return false if it was not the parameter required, and param was not changed
 */
bool setup_param(POOL_MEM &param, const char *pname, const char *name, const char *value)
{
   if (bstrcasecmp(name, pname))
   {
      pm_strcpy(param, value);
      DMsg2(DDEBUG, "setup param: %s=%s\n", pname, param.c_str());
      return true;
   }
   return false;
}

/**
 * @brief Setup parameter for boolean from string value.
 * The parameter value will be false if value start with '0' character and
 * will be true in any other case. So, when a plugin will have a following:
 *    param
 *    param=xxx
 *    param=1
 *  then a param will be set to true.
 *
 * @param param the param variable where we will render a parameter
 * @param pname a name of the parameter to compare
 * @param name a name of the parameter from parameter list
 * @param value a value to parse
 * @return true if parameter was parsed
 * @return false if it was not the parameter required
 */
bool parse_param(bool &param, const char *pname, const char *name, const char *value)
{
   if (bstrcasecmp(name, pname)){
      if (value && *value == '0'){
         param = false;
      } else {
         param = true;
      }
      DMsg2(DINFO, "%s parameter: %s\n", name, param ? "True" : "False");
      return true;
   }
   return false;
}

/**
 * @brief Setup Plugin parameter for integer from string value.
 *
 * @param param the param variable where we will render a parameter
 * @param pname a name of the parameter to compare
 * @param name a name of the parameter from parameter list
 * @param value a value to render
 * @param err a pointer to error flag when conversion was unsuccessful, optional
 * @return true
 * @return false
 */
bool parse_param(int &param, const char *pname, const char *name, const char *value, bool * err)
{
   // clear error flag when requested
   if (err != NULL) *err = false;

   if (value && bstrcasecmp(name, pname)){
      /* convert str to integer */
      long outparam = strtol(value, NULL, 10);

      if (outparam == LONG_MIN || outparam == LONG_MAX){
         // error in conversion?
         if (errno == ERANGE){
            // yes, error
            DMsg2(DERROR, "Invalid %s parameter: %s\n", name, value);
            // setup error flag
            if (err != NULL) *err = true;
            return false;
         }
      }
      param = outparam;
      DMsg2(DINFO, "%s parameter: %d\n", name, param);

      return true;
   }
   return false;
}

/*
 * Render and add a parameter for string value to alist.
 *  When alist is NULL (uninitialized) then it creates a new list to use.
 *
 * in:
 *    bpContext - for Bacula debug and jobinfo messages
 *    list - pointer to alist class to use
 *    pname - a name of the parameter to compare
 *    name - a name of the parameter from parameter list
 *    value - a value to render
 * out:
 *    True if parameter was rendered
 *    False if it was not the parameter required
 */
bool parse_param_add_str(alist **list, const char *pname, const char *name, const char *value)
{
   POOLMEM *param;

   if (list != NULL){
      if (bstrcasecmp(name, pname)){
         if (!*list){
            *list = New(alist(8, not_owned_by_alist));
         }
         param = get_pool_memory(PM_NAME);
         pm_strcpy(param, value);
         (*list)->append(param);
         DMsg2(DDEBUG, "add param: %s=%s\n", name, param);
         return true;
      }
   }
   return false;
}

/**
 * @brief Scans for `prefix` in `cmd`, when match copy remaining to `param`.
 *
 * @param cmd - command string to can for prefix and extract parameter
 * @param prefix - prefix string to check
 * @param param - when `prefix` match then copy the remaining from `cmd`
 * @return true - when prefix match in the command
 * @return false - when not
 */
bool scan_parameter_str(const char * cmd, const char *prefix, POOL_MEM &param)
{
   if (prefix != NULL)
   {
      int len = strlen(prefix);
      if (strncmp(cmd, prefix, len) == 0)
      {
         // prefix match, extract param
         pm_strcpy(param, cmd + len);
         strip_trailing_newline(param.c_str());
         return true;
      }
   }

   return false;
}

bool scan_parameter_int(const char * cmd, const char *prefix, int &param)
{
   POOL_MEM tmp;

   if (scan_parameter_str(cmd, prefix, tmp))
   {
      param = atoi(tmp.c_str());
      return true;
   }

   return false;
}

// ensure error message is terminated with newline and terminated with standard c-string nul
void scan_and_terminate_str(POOL_MEM &buf, int msglen)
{
   if (msglen >= 0){
      // we will consume at most two chars more
      buf.check_size(msglen + 2);
      bool check = msglen > 0 ? buf.c_str()[msglen - 1] != '\n' : true;
      buf.c_str()[msglen] = check * '\n';
      buf.c_str()[msglen + 1] = '\0';
   }
}

namespace pluginlib
{
   /**
    * @brief Render and add a parameter for string value to alist.
    *       In this case POOL_MEM is created.
    *
    * @param list a list to append
    * @param pname a name of the parameter to compare
    * @param name a name of the parameter from parameter list
    * @param value a value to render
    * @return true if parameter was rendered
    * @return false if it was not the parameter required
    */
   bool parse_param_add_str(alist &list, const char *pname, const char *name, const char *value)
   {
      if (bstrcasecmp(name, pname))
      {
         POOL_MEM *param = new POOL_MEM(PM_NAME);
         pm_strcpy(*param, value);
         list.append(param);
         DMsg2(DDEBUG, "add param: %s=%s\n", name, (*param).c_str());
         return true;
      }

      return false;
   }

}  // namespace pluginlib
