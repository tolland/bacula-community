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
 * Sample Plugin program
 *
 *  Kern Sibbald, October 2007
 */
#include "bacula.h"
#include "dir_plugins.h"
#include "dir_authplugin.h"
#include "dirpluglib.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PLUGIN_LICENSE      "AGPLv3"
#define PLUGIN_AUTHOR       "Radosław Korzeniewski"
#define PLUGIN_DATE         "September 2020"
#define PLUGIN_VERSION      "1"
#define PLUGIN_DESCRIPTION  "Test Director Auth API Plugin"

/* Forward referenced functions */
static bRC newPlugin(bpContext *ctx);
static bRC freePlugin(bpContext *ctx);
static bRC getPluginValue(bpContext *ctx, pDirVariable var, void *value);
static bRC setPluginValue(bpContext *ctx, pDirVariable var, void *value);
static bRC handlePluginEvent(bpContext *ctx, bDirEvent *event, void *value);
static bRC getAuthenticationData(bpContext *ctx, const char *console, const char *param, void **data);
static bRC getAuthorizationData(bpContext *ctx, const char *console, const char *param, void **data);

/* Plugin compile time variables */
#define PLUGINPREFIX                "authapi:"
#define PLUGIN_NAME                 "test-authentication-api"

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
   newPlugin,                         /* new plugin instance */
   freePlugin,                        /* free plugin instance */
   getPluginValue,
   setPluginValue,
   handlePluginEvent,
   getAuthenticationData,
   getAuthorizationData,
};

static bDirAuthenticationData testquestions0[] =
{
   // operation; question; data;
   {bDirAuthenticationOperationLogin, "Username:", 0},
   {bDirAuthenticationOperationPassword, "Password:", 1},
};

static bDirAuthenticationRegister testregister0 =
{
   .name = PLUGIN_NAME,
   .welcome = "This is a test authplugin API Plugin. Use root/root to login.",
   .num = 2,
   .data = testquestions0,
   .nsTTL = 0,
};

static bDirAuthenticationData testquestions1[] =
{
   // operation; question; data;
   {bDirAuthenticationOperationLogin, "Username:", 0},
   {bDirAuthenticationOperationPlugin, NULL, 1},
   {bDirAuthenticationOperationPlain, "Response:", 2},
};

static bDirAuthenticationData testquestions1_msg =
{
   bDirAuthenticationOperationMessage, NULL, 0
};

static bDirAuthenticationRegister testregister1 =
{
   .name = PLUGIN_NAME,
   .welcome = "This is a test authplugin API Plugin. Use bacula username to login.",
   .num = 3,
   .data = testquestions1,
   .nsTTL = 0,
};

struct test_api : public SMARTALLOC
{
#if __cplusplus >= 201103L
   POOL_MEM username{PM_NAME};
   POOL_MEM password{PM_NAME};
   POOL_MEM challenge{PM_NAME};
   POOL_MEM challenge_str{PM_NAME};
   int mode{0};
#else
   POOL_MEM username;
   POOL_MEM password;
   POOL_MEM challenge;
   POOL_MEM challenge_str;
   int mode;
   test_api() :
      username(PM_NAME),
      password(PM_NAME),
      challenge(PM_NAME),
      challenge_str(PM_NAME),
      mode(0)
   {}
#endif
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
   test_api *self = New (test_api);
   DMSG0(ctx, DINFO, "newPlugin\n");
   ctx->pContext = self;
   return bRC_OK;
}

static bRC freePlugin(bpContext *ctx)
{
   DMSG0(ctx, DINFO, "freePlugin\n");
   if (ctx->pContext) {
      test_api *self = (test_api *)ctx->pContext;
      delete self;
   }
   return bRC_OK;
}

static bRC getPluginValue(bpContext *ctx, pDirVariable var, void *value)
{
   DMSG1(ctx, DINFO, "plugin: getPluginValue var=%d\n", var);
   return bRC_OK;
}

static bRC setPluginValue(bpContext *ctx, pDirVariable var, void *value)
{
   DMSG1(ctx, DINFO, "plugin: setPluginValue var=%d\n", var);
   return bRC_OK;
}

static bRC handlePluginEvent(bpContext *ctx, bDirEvent *event, void *value)
{
   bDirAuthValue *pvalue;
   test_api *self = (test_api *)ctx->pContext;
   int r;

   switch (event->eventType) {
   case bDirEventAuthenticationQuestion:
      pvalue = (bDirAuthValue *)value;
      switch (self->mode)
      {
      case 1:
         r = rand();
         Mmsg(self->challenge, "%d", r);
         Mmsg(self->challenge_str, "You should use %s as a response.", self->challenge.c_str());
         testquestions1_msg.question = self->challenge_str.c_str();
         pvalue->authdata = &testquestions1_msg;
         break;
      default:
         break;
      }
      break;

   case bDirEventAuthenticationResponse:
      pvalue = (bDirAuthValue *)value;
      DMSG_EVENT_STR(event, pvalue->response);
      switch (self->mode)
      {
      case 0:
         switch (pvalue->seqdata)
         {
         case 0:
            pm_strcpy(self->username, pvalue->response);
            break;
         case 1:
            pm_strcpy(self->password, pvalue->response);
            break;
         default:
            break;
         }
      case 1:
         switch (pvalue->seqdata)
         {
         case 0:
            pm_strcpy(self->username, pvalue->response);
            break;
         case 2:
            pm_strcpy(self->password, pvalue->response);
            break;
         default:
            break;
         }
      }
      break;

   case bDirEventAuthenticate:
      DMSG_EVENT_PTR(event, value);
      switch (self->mode)
      {
      case 0:
         if (!bstrcmp(self->username.c_str(), "root") || !bstrcmp(self->password.c_str(), "root")) {
            return bRC_Error;
         }
         break;
      case 1:
         if (!bstrcmp(self->username.c_str(), "bacula") || !bstrcmp(self->password.c_str(), self->challenge.c_str())) {
            return bRC_Error;
         }
         break;
      default:
         return bRC_Error;
      }
      break;

   default:
      break;
   }
   return bRC_OK;
}

static bRC getAuthenticationData(bpContext *ctx, const char *console, const char *param, void **data)
{
   test_api *self = (test_api *)ctx->pContext;
   bDirAuthenticationRegister **padata = (bDirAuthenticationRegister **)data;

   self->mode = 0;   // this is a default

   DMSG2(ctx, DINFO, "registering with: console=%s %s\n", console, NPRT(param));
   sscanf(param, PLUGIN_NAME ":%d", &self->mode);
   switch (self->mode)
   {
   case 1:
      DMSG0(ctx, DINFO, "testregister1\n");
      *padata = &testregister1;
      break;
   default:
      DMSG0(ctx, DINFO, "testregister0\n");
      *padata = &testregister0;
      break;
   }

   return bRC_OK;
}

static bRC getAuthorizationData(bpContext *ctx, const char *console, const char *param, void **data)
{

   return bRC_OK;
}

#ifdef __cplusplus
}
#endif
