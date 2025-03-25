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
 * @file pluginlib.h
 * @author Radosław Korzeniewski (radoslaw@korzeniewski.net)
 * @brief Common definitions and utility functions for Inteos plugins.
 * @version 2.2.0
 * @date 2021-04-26
 *
 * Common definitions and utility functions for Inteos plugins.
 * Functions defines a common framework used in our utilities and plugins.
 * Author: Radosław Korzeniewski, radekk@inteos.pl, Inteos Sp. z o.o.
 */

#ifndef _PLUGINLIB_H_
#define _PLUGINLIB_H_

#include <sys/stat.h>
#include <sys/types.h>
#include <ctype.h>

#include "bacula.h"
#include "lib/ini.h"
#include "fd_plugins.h"

/* Pointers to Bacula functions used in plugins */
extern bFuncs *bfuncs;
extern bInfo *binfo;

// Plugin linking time variables
extern const char *PLUGINPREFIX;
extern const char *PLUGINNAME;

/* module definition */
#ifndef PLUGMODULE
#define PLUGMODULE   "PluginLib::"
#endif

#define _STR(x) __STR(x)
#define __STR(x) #x

#ifdef VERSIONGIT
   #define VERSIONGIT_STR  _STR(VERSIONGIT)
#else
   #define VERSIONGIT_STR  "/unknown"
#endif

/* size of different string or query buffers */
#define BUFLEN       4096
#define BIGBUFLEN    65536

/* debug and messages functions */
#define JMSG0(ctx,type,msg) \
   if (ctx) bfuncs->JobMessage ( ctx, __FILE__, __LINE__, type, 0, "%s " msg, PLUGINPREFIX );
#define JMSG1 JMSG
#define JMSG(ctx,type,msg,var) \
   if (ctx) bfuncs->JobMessage ( ctx, __FILE__, __LINE__, type, 0, "%s " msg, PLUGINPREFIX, var );
#define JMSG2(ctx,type,msg,var1,var2) \
   if (ctx) bfuncs->JobMessage ( ctx, __FILE__, __LINE__, type, 0, "%s " msg, PLUGINPREFIX, var1, var2 );
#define JMSG3(ctx,type,msg,var1,var2,var3) \
   if (ctx) bfuncs->JobMessage ( ctx, __FILE__, __LINE__, type, 0, "%s " msg, PLUGINPREFIX, var1, var2, var3 );
#define JMSG4(ctx,type,msg,var1,var2,var3,var4) \
   if (ctx) bfuncs->JobMessage ( ctx, __FILE__, __LINE__, type, 0, "%s " msg, PLUGINPREFIX, var1, var2, var3, var4 );

#define DMSG0(ctx,level,msg) \
   if (ctx) bfuncs->DebugMessage ( ctx, __FILE__, __LINE__, level, "%s " msg, PLUGINPREFIX );
#define DMSG1 DMSG
#define DMSG(ctx,level,msg,var) \
   if (ctx) bfuncs->DebugMessage ( ctx, __FILE__, __LINE__, level, "%s " msg, PLUGINPREFIX, var );
#define DMSG2(ctx,level,msg,var1,var2) \
   if (ctx) bfuncs->DebugMessage ( ctx, __FILE__, __LINE__, level, "%s " msg, PLUGINPREFIX, var1, var2 );
#define DMSG3(ctx,level,msg,var1,var2,var3) \
   if (ctx) bfuncs->DebugMessage ( ctx, __FILE__, __LINE__, level, "%s " msg, PLUGINPREFIX, var1, var2, var3 );
#define DMSG4(ctx,level,msg,var1,var2,var3,var4) \
   if (ctx) bfuncs->DebugMessage ( ctx, __FILE__, __LINE__, level, "%s " msg, PLUGINPREFIX, var1, var2, var3, var4 );
#define DMSG6(ctx,level,msg,var1,var2,var3,var4,var5,var6) \
   if (ctx) bfuncs->DebugMessage ( ctx, __FILE__, __LINE__, level, "%s " msg, PLUGINPREFIX, var1, var2, var3, var4, var5, var6 );
#define DMSG7(ctx,level,msg,var1,var2,var3,var4,var5,var6,var7)             \
   if (ctx) bfuncs->DebugMessage ( ctx, __FILE__, __LINE__, level, "%s " msg, PLUGINPREFIX, var1, var2, var3, var4, var5, var6, var7);

/* fixed debug level definitions */
#define D1  1                    /* debug for every error */
#define DERROR D1
#define D2  10                   /* debug only important stuff */
#define DINFO  D2
#define D3  200                  /* debug for information only */
#define DDEBUG D3
#define D4  800                  /* debug for detailed information only */
#define DVDEBUG D4

#define getBaculaVar(bvar,val)   bfuncs->getBaculaValue(ctx, bvar, val);
#define checkChanges(sp)         bfuncs->checkChanges(ctx, sp);
#define getAccurateAttribs(att)  bfuncs->getAccurateAttribs(ctx, att);

/* used for sanity check in plugin functions */
#define ASSERT_CTX \
   if (!ctx || !ctx->pContext || !bfuncs) \
   { \
      return bRC_Error; \
   }

/* defines for handleEvent */
#define DMSG_EVENT_STR(event,value)       DMSG2(ctx, DINFO, "%s value=%s\n", eventtype2str(event), NPRT((char *)value));
#define DMSG_EVENT_CHAR(event,value)      DMSG2(ctx, DINFO, "%s value='%c'\n", eventtype2str(event), (char)value);
#define DMSG_EVENT_LONG(event,value)      DMSG2(ctx, DINFO, "%s value=%ld\n", eventtype2str(event), (intptr_t)value);
#define DMSG_EVENT_PTR(event,value)       DMSG2(ctx, DINFO, "%s value=%p\n", eventtype2str(event), value);

/* pure debug macros */
#define DMsg0(level,msg)                  Dmsg1(level, PLUGMODULE "%s: " msg, __func__)
#define DMsg1(level,msg,a1)               Dmsg2(level, PLUGMODULE "%s: " msg, __func__, a1)
#define DMsg2(level,msg,a1,a2)            Dmsg3(level, PLUGMODULE "%s: " msg, __func__, a1, a2)
#define DMsg3(level,msg,a1,a2,a3)         Dmsg4(level, PLUGMODULE "%s: " msg, __func__, a1, a2, a3)
#define DMsg4(level,msg,a1,a2,a3,a4)      Dmsg5(level, PLUGMODULE "%s: " msg, __func__, a1, a2, a3, a4)

#define BOOLSTR(b)                        (b?"True":"False")

/*
 * Common structure for key/pair values
 */
class key_pair : public SMARTALLOC
{
public:
   POOL_MEM key;
   POOL_MEM value;

   key_pair() : key(PM_NAME), value(PM_MESSAGE) {};
   key_pair(const char *k, const char *v)
   {
      pm_strcpy(key, k);
      pm_strcpy(value, v);
   };
   ~key_pair() {};
};

const char *eventtype2str(bEvent *event);
uint64_t pluglib_size_suffix(int disksize, char suff);
uint64_t pluglib_size_suffix(double disksize, char suff);
bRC pluglib_mkpath(bpContext* ctx, char* path, bool isfatal);

/*
 * Checks if plugin command points to our Plugin
 *
 * in:
 *    command - the plugin command used for backup/restore
 * out:
 *    True - if it is our plugin command
 *    False - the other plugin command
 */
inline bool isourplugincommand(const char *pluginprefix, const char *command)
{
   /* check if it is our Plugin command */
   int len = strlen(pluginprefix);
   if (len > 0 && pluginprefix[len-1] == ':') {
      len--;
   }
   if (strncmp(pluginprefix, command, len) == 0){
      /* it is our plugin prefix */
      return true;
   }
   return false;
}

/**
 * @brief Checks if fname is a part of pluginprefix namespace
 *
 * @param pluginprefix plugin namespace prefix
 * @param fname file name to check
 * @return true when it matches
 * @return false when not
 */
inline bool isourpluginfname(const char *pluginprefix, const char *fname)
{
   /* check if it is our Plugin fname */
   if (strncmp(pluginprefix, fname, strlen(pluginprefix)) == 0){
      return true;
   }
   char _fn[strlen(pluginprefix) + 2];
   _fn[0] = '/';
   _fn[1] = '\0';
   strcat(_fn, pluginprefix);
   if (strncmp(_fn, fname, strlen(_fn)) == 0){
      return true;
   }

   return false;
}

void plugutil_str_split_to_alist(alist &list, const char * str, const char sep = '.');
void plugutil_str_split_to_alist(alist * list, const char * str, const char sep = '.');
alist * plugutil_str_split_to_alist(const char * str, const char sep = '.');

/**
 * @brief Verifies if path is local except '/'
 *
 * @param path
 * @return true
 * @return false
 */
inline bool islocalpath(const char *path)
{
   bool result = path && strlen(path) > 1;
   bool spath = path[0] == '/';
#ifdef HAVE_WIN32
   bool wpath = isalpha(path[0]) && path[1] == ':' ;   // simple drive letter
#else
   bool wpath = false;
#endif
   return result && (spath || wpath);
}

/* plugin parameters manipulation */
bool render_param(POOLMEM **param, const char *pname, const char *fmt, const char *name, const char *value);
bool render_param(POOLMEM **param, const char *pname, const char *fmt, const char *name, const int value);
bool render_param(bool &param, const char *pname, const char *name, const bool value);
bool render_param(POOL_MEM &param, INI_ITEM_HANDLER *handler, char *key, item_value val);

bool parse_param(bool &param, const char *pname, const char *name, const char *value);
bool parse_param(int &param, const char *pname, const char *name, const char *value, bool *err = NULL);
bool parse_param(POOL_MEM &param, const char *pname, const char *name, const char *value);

bool setup_param(int32_t &param, const char *pname, const char *name, const int32_t value);
bool setup_param(bool &param, const char *pname, const char *name, const bool value);
bool setup_param(POOL_MEM &param, const char *pname, const char *name, const char *value);

bool parse_param_add_str(alist **list, const char *pname, const char *name, const char *value);

bool scan_parameter_str(const char * cmd, const char *prefix, POOL_MEM &param);
inline bool scan_parameter_str(const POOL_MEM &cmd, const char *prefix, POOL_MEM &param) { return scan_parameter_str(cmd.c_str(), prefix, param); }
bool scan_parameter_int(const char *cmd, const char *prefix, int &param);
inline bool scan_parameter_int(const POOL_MEM &cmd, const char *prefix, int &param) { return scan_parameter_int(cmd.c_str(), prefix, param); }

void scan_and_terminate_str(POOL_MEM &buf, int msglen);

namespace pluginlib
{
   bool parse_param_add_str(alist &list, const char *pname, const char *name, const char *value);
}  // namespace pluginlib

#endif /* _PLUGINLIB_H_ */
