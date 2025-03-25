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
 * TOTP Director Authentication Plugin
 * Eric Bollengier Aug 2021
 */

#include <math.h>
#include "bacula.h"
#include "dir_plugins.h"
#include "dir_authplugin.h"
#include "dirpluglib.h"

#define USE_CMD_PARSER
#define USE_MAKEDIR
#include "../fd/fd_common.h"

/*
 *  At the first connection of the console, a key is generated
 *  and stored in sysconfdir/conf.d/totp/BASE32(ConsoleName)
 */

/*
 * TODO: We can send the QR code via email or telegram
 * to an email configured into the plugin command line.
 */

/****************************************************************/
/* Implementation of the Time-Based One Time Password Algorithm
 * defined in rfc6238
 *
 * The implementation relies on OpenSSL for HMAC SHA1
 * https://github.com/google/google-authenticator/wiki/Key-Uri-Format
 * otpauth://totp/Bacula:consoleA?secret=ORSXG5BR&issuer=Bacula
 *
 * qrencode -t ansiutf8 
 * dlksflkdslkflksdlfklsklkdkfslkdlfkslklfd
 * 
 * █████████████████████████████████████
 * ████ ▄▄▄▄▄ █▀ █▀▀ █▀▄▄▄▄ █ ▄▄▄▄▄ ████
 * ████ █   █ █▀ ▄ █▀ ▄▄▀▄ ██ █   █ ████
 * ████ █▄▄▄█ █▀█ █▄▄▀▀   ▄ █ █▄▄▄█ ████
 * ████▄▄▄▄▄▄▄█▄█▄█ █▄█▄█▄▀▄█▄▄▄▄▄▄▄████
 * ████ ▄ ▄▄█▄   ▄█▄▄▄▄ ▄▀▀  ▀ ▀▄█▄▀████
 * █████ ▄█▀█▄ ▀ ▀ ▄▄▄▄▄█▀█▀▀▄▀▀ ▄█▀████
 * ████▄▄▀█  ▄▄▀▄▀▄▀▄▄█▀ ▀ ▀▀▀▀▀▀▄  ████
 * ██████ ██ ▄ ▀█▀█▀█▄▄▀▄▄▀▄▄ ▄█▀▄▀█████
 * ████ ▄████▄ ▄  █▄▀█▄▀▄▀ ▀█▀▀▀▀ ▀ ████
 * ████ █ ▀█ ▄▄█▄▀ ▄▄█ ▀▄▀▀▀▄▀▀▄▀█▀█████
 * ████▄██▄█▄▄█▀█▄▄▀▀██▀▄██ ▄▄▄ █ ▀▀████
 * ████ ▄▄▄▄▄ █▄▄██▀ ▄ ▀▄▄  █▄█  ▄██████
 * ████ █   █ █  ▀█▄█▄▄▀ ▀█▄▄▄▄▄▀▄  ████
 * ████ █▄▄▄█ █ ▀▄ ▄█▀▄▄█▀▄ ▄▄ ▄▀█ █████
 * ████▄▄▄▄▄▄▄█▄█▄▄█████▄█▄█▄██▄▄▄██████
 *
 */

#ifdef HAVE_OPENSSL

#define T0       0
#define VALIDITY 30
#define DIGITS   6
#define DIGEST   EVP_sha1()

/* Generate a hash code for a key */
static uint8_t *hmac(const uint8_t *data, uint32_t datalen, uint64_t atime,
                     uint8_t *dest, uint32_t destlen)
{
   if (destlen < EVP_MAX_MD_SIZE) {
      return NULL;
   }
   return (uint8_t *)HMAC(DIGEST, data, datalen, (uint8_t *)&atime, sizeof(atime), dest, &destlen);
}

/* Generate the code from the digest */
static uint32_t totp_get_code(uint8_t *digest)
{
   uint64_t offset;
   uint32_t bin_code;

   /* dynamically choose what to return */
   offset   = digest[19] & 0x0f;

   bin_code = (digest[offset]  & 0x7f) << 24
      | (digest[offset+1] & 0xff) << 16
      | (digest[offset+2] & 0xff) <<  8
      | (digest[offset+3] & 0xff);

   return bin_code;
}

/* Truncate the result to a given number of digits */
static uint32_t mod_hotp(uint32_t bin_code, int digits)
{
    int power = pow(10, digits);
    uint32_t otp = bin_code % power;
    return otp;
}

/* The secret is in "clear" form. When we generate the code for the
 * authenticator app, it should be encoded in base32
 */
static bool totp(uint8_t *key, uint32_t kl, uint64_t atime, int digits, uint32_t *code)
{
   uint8_t *digest;
   uint8_t dest[EVP_MAX_MD_SIZE];

   /* converts atime to big endian if system is little endian */
   uint32_t endianness = 0xbacbac01;
   if ((*(const uint8_t *)&endianness) == 0x01)
   {
      atime = ((atime & 0x00000000ffffffff) << 32) | ((atime & 0xffffffff00000000) >> 32);
      atime = ((atime & 0x0000ffff0000ffff) << 16) | ((atime & 0xffff0000ffff0000) >> 16);
      atime = ((atime & 0x00ff00ff00ff00ff) <<  8) | ((atime & 0xff00ff00ff00ff00) >>  8);
   }
   
   /* Get the digest of the message using the provided key and the timestamp */
   digest = (uint8_t *)hmac(key, kl, atime, dest, sizeof(dest));
   if (!digest) {
      return false;
   }

   /* Get the code from the hash. The portion used will change
    * dynamically
    */
   uint32_t dbc = totp_get_code(digest);

   /* calculate the mod_k of the code to get the correct number */
   *code = mod_hotp(dbc, digits);

   return true;
}

/* Generate a TOTP code from a password */
static bool totp_from_secret(uint8_t *k, size_t keylen, uint32_t *code)
{
   time_t t = floor((time(NULL) - T0) / VALIDITY);
   if (totp(k, keylen, t, DIGITS, code)) {
      return true;
   }
   return false;
}

/* Read a secret from a file */
static bool totp_read_secretfile(const char *fname, char *secret, uint32_t len, uint32_t *slen)
{
   uint32_t nb;
   FILE *fp = bfopen(fname, "r");
   if (!fp) {
      return false;
   }
   nb = fread(secret, 1, len-1, fp);
   secret[nb] = 0;
   *slen = nb;
   fclose(fp);
   return true;
}

/* Generate a TOTP code from a secretfile */
static bool totp_from_secretfile(const char *fname, uint32_t *code)
{
   uint32_t nb;
   uint8_t buf[512];
   if (!totp_read_secretfile(fname, (char *)buf, sizeof(buf), &nb)) {
      return false;
   }

   return totp_from_secret(buf, nb, code);
}

#else

static uint32_t totp_from_secret(uint8_t *k, size_t keylen, uint32_t *code)
{
   Dmsg0(10, "TOTP not available without OpenSSL\n");
   return false;
}

static uint32_t totp_from_secretfile(const char *fname, uint32_t *code)
{
   Dmsg0(10, "TOTP not available without OpenSSL\n");
   return false;
}

#endif  // HAVE_OPENSSL

/* buf len should be 1.6*strlen(secret) +  50 */
static char *totp_get_url(const char *name, const char *secret, int len, POOLMEM **ret)
{
   POOL_MEM buf;
   buf.check_size(len * 8 / 5 + 1);

   if (bin_to_base32((uint8_t*)secret, len, buf.c_str(), buf.size()) < 0) {
      return NULL;
   }

   Mmsg(ret, "otpauth://totp/Bacula:%s?secret=%s&issuer=Bacula",
        name, buf.c_str());
   return *ret;
}

#ifdef TEST_PROGRAM
#include "lib/unittests.h"

int main(int argc, char **argv)
{
   Unittests t("totp");
   uint32_t code;
   uint8_t *secret = (uint8_t*)"test1";
   POOL_MEM b;
   ok(totp_from_secret(secret, strlen((char *)secret), &code), "get code");
   Dmsg1(0, "%d\n", code);
   Dmsg1(0, "%s\n", totp_get_url("console", (char *)secret, strlen((char *)secret), b.handle()));
   return report();
}
#endif

/****************************************************************/

#ifdef __cplusplus
extern "C" {
#endif

#define PLUGIN_LICENSE      BPLUGIN_LICENSE
#define PLUGIN_AUTHOR       "Eric Bollengier"
#define PLUGIN_DATE         "August 2021"
#define PLUGIN_VERSION      "1"
#define PLUGIN_DESCRIPTION  "Director TOTP Plugin"

/* Forward referenced functions */
static bRC newPlugin(bpContext *ctx);
static bRC freePlugin(bpContext *ctx);
static bRC getPluginValue(bpContext *ctx, pDirVariable var, void *value);
static bRC setPluginValue(bpContext *ctx, pDirVariable var, void *value);
static bRC handlePluginEvent(bpContext *ctx, bDirEvent *event, void *value);
static bRC getAuthenticationData(bpContext *ctx, const char *console, const char *param, void **data);
static bRC getAuthorizationData(bpContext *ctx, const char *console, const char *param, void **data);

/* Plugin compile time variables */
#define PLUGINPREFIX                "totp:"
#define PLUGIN_NAME                 "totp"

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
   getAuthorizationData
};

static bDirAuthenticationData totpquestions0[] =
{
   // operation; question; data;
   {bDirAuthenticationOperationPlain, "Code:", 0}
};

static bDirAuthenticationRegister totpregister0 =
{
   .name = PLUGIN_NAME,
   .welcome = "Code Authentication",
   .num = 1,
   .data = totpquestions0,
   .nsTTL = 0,
};

// TODO: Use the location of bacula-dir or a user defined variable to set it
#ifdef SYSCONFDIR
#define KEYDIR SYSCONFDIR "/conf.d/totp/"
#else
#define KEYDIR "/tmp/key/"
#endif

class totp_api : public SMARTALLOC
{
public:
   POOLMEM *code;
   POOLMEM *keyname;
   uint32_t input_code;
   char *keydir;
   char *sendcommand;
   bool gen_qrcode;
   bDirAuthenticationRegister totpregisterQR;
   totp_api() :
      code(NULL),
      keyname(NULL),
      input_code(0),
      keydir(NULL),
      sendcommand(NULL),
      gen_qrcode(true)
      {
         code = get_pool_memory(PM_FNAME);
         keyname = get_pool_memory(PM_FNAME);
         *code = *keyname = 0;

         // The bDirAuthenticationRegister struct is used to display
         // the QR code at the first connection
         totpregisterQR.name = PLUGIN_NAME;
         totpregisterQR.welcome = NULL; // To fill during the session
         totpregisterQR.num = 1;
         totpregisterQR.data = totpquestions0;
         totpregisterQR.nsTTL = 0;
      };

   ~totp_api() {
      free_and_null_pool_memory(code);
      free_and_null_pool_memory(keyname);
      bfree_and_null(keydir);
      bfree_and_null(sendcommand);
   };

   POOLMEM *edit_codes(POOLMEM *&omsg, const char *imsg, const char *name, const char *qrcode_file) {
      const char *p;
      const char *str;
      char add[50];
      extern char my_name[];    /* From libbac */

      *omsg = 0;
      for (p=imsg; *p; p++) {
         if (*p == '%') {
            switch (*++p) {
            case '%':
               str = "%";
               break;
            case 'a':
               str = qrcode_file;
               break;
            case 'd':
               str = my_name;            /* Director's name */
               break;
            case 'c':
               str = name;               /* Console name */
               break;
            case 'P':
               edit_uint64(getpid(), add);
               str = add;
               break;
            default:
               add[0] = '%';
               add[1] = *p;
               add[2] = 0;
               str = add;
               break;
            }
         } else {
            add[0] = *p;
            add[1] = 0;
            str = add;
         }
         pm_strcat(omsg, str);
      }
      return omsg;
   };

   bool parse_param(const char *param)
   {
      cmd_parser parser;
      bfree_and_null(keydir);
      bfree_and_null(sendcommand);
      gen_qrcode = true;

      if (!param) {
         return true;
      }
      if (parser.parse_cmd(param) == bRC_OK) {
         for(int i = 1; i < parser.argc ; i++) {
            if (strcasecmp(parser.argk[i], "no_qrcode") == 0) {
               gen_qrcode = false;

            } else if (!parser.argv[i]) {
               Dmsg0(0, "Incorrect configuration for totp plugin.\n");

            } else if (strcasecmp(parser.argk[i], "keydir") == 0) {
               keydir = bstrdup(parser.argv[i]);

            } else if (strcasecmp(parser.argk[i], "sendcommand") == 0) {
               sendcommand = bstrdup(parser.argv[i]);
               gen_qrcode = false;

            } else {
               Dmsg0(0, "Unknown parameter for totp plugin\n"); 
            }
         }
      } else {
         Dmsg0(0, "Unable to decode totp command line\n");
         return false;
      }
      return true;
   }

   /* Generate a filename for the current console */
   bool compute_keyfile(const char *name, POOLMEM **key)
   {
      if (!keydir) {
         keydir = bstrdup(KEYDIR);
      }
      int len = strlen(keydir) + 1;
      int nlen = strlen(name);
      *key = check_pool_memory_size(*key, len + nlen * 5 / 8 + 10);
      Mmsg(key, "%s/", keydir);
      if (bin_to_base32((uint8_t *)name, nlen, *key + len, sizeof_pool_memory(code) - (len + 1)) < 0) {
         Dmsg1(10, "Unable to encode %s to base32\n", name);
         *key[0] = 0;
         return false;
      }
      Dmsg1(200, "keyname=%s\n", *key);
      return true;
   };

   /* Check if we have a key for this console */
   bool has_key(const char *name) {
      if (!compute_keyfile(name, &keyname)) {
         return false;
      }
      if (access(keyname, R_OK) < 0) {
         return false;
      }
      return true;
   };

   bool getQRCode(const char *name, char *k64, POOLMEM **ret)
   {
      char urlfile[128];
      POOL_MEM tmp;

      if (totp_get_url(name, k64, strlen(k64), ret) == NULL) {
         Dmsg0(DINFO, "Unable to generate the totp url from the key\n");
         return false;
      }

      // Write URL to disk to generate the QR code
      bstrncpy(urlfile, "/tmp/key.XXXXXX", sizeof(urlfile));
      int fpt = mkstemp(urlfile);
      if (fpt < 0) {
         berrno be;
         Dmsg1(DINFO, "Unable to create a new key. ERR=%s\n", be.bstrerror());
         return false;
      }

      FILE *fp = fdopen(fpt, "w");
      fprintf(fp, "%s", *ret);
      fclose(fp);

      // Call qrencode to generate the QR code, else display the URL
      Mmsg(tmp, "sh -c 'cat \"%s\" | qrencode -t ansiutf8'", urlfile);
      if (run_program_full_output (tmp.c_str(), 0, *ret, NULL) != 0) {
         berrno be;
         Dmsg1(DINFO, "Unable to call qrencode on a new key. ERR=%s\n", be.bstrerror());
         if (totp_get_url(name, k64, strlen(k64), ret)) {
            pm_strcat(ret, _("\nUse this URL into your TOTP client and close this screen.\n"));

         } else {
            // Should not happen, the first call to totp_get_url() was OK
            Dmsg0(DINFO, "Unable to generate the totp url from the key\n");
            return false;
         }

      } else {
         Mmsg(tmp, "\n%s\nScan the QR code into your TOTP client and close this screen.\n", *ret);
         pm_strcpy(ret, tmp.c_str());
      }
      unlink(urlfile);

      return true;
   };

   bool sendQRCode(const char *name, char *k64, POOLMEM **ret)
   {
      bool rcode=false;
      char urlfile[128];
      char pngfile[128];
      POOL_MEM tmp, tmp2;

      if (totp_get_url(name, k64, strlen(k64), ret) == NULL) {
         Dmsg0(DINFO, "Unable to generate the totp url from the key\n");
         return false;
      }

      // Write URL to disk to generate the QR code
      bstrncpy(urlfile, "/tmp/key.XXXXXX", sizeof(urlfile));
      int fpt = mkstemp(urlfile);
      if (fpt < 0) {
         berrno be;
         Dmsg1(0, "Unable to create a new key. ERR=%s\n", be.bstrerror());
         return false;
      }

      FILE *fp = fdopen(fpt, "w");
      fprintf(fp, "%s", *ret);
      fclose(fp);

      // Write png to disk to generate the QR code
      bstrncpy(pngfile, "/tmp/qrcode.XXXXXX.png", sizeof(pngfile));
      fpt = mkstemps(pngfile, 4);
      if (fpt < 0) {
         berrno be;
         Dmsg1(0, "Unable to create a new key. ERR=%s\n", be.bstrerror());
         unlink(urlfile);
         return false;
      }

      // Call qrencode to generate the QR code, else display the URL
      Mmsg(tmp, "sh -c 'cat \"%s\" | qrencode -s 10 -t png -o \"%s\"'", urlfile, pngfile);
      if (run_program_full_output (tmp.c_str(), 0, *ret, NULL) != 0) {
         Dmsg0(0, "Unable to generate the totp png file from the key\n");
         goto bail_out;
      }

      if (sendcommand) {
         edit_codes(tmp.addr(), sendcommand, name, pngfile);

         if (run_program_full_output (tmp.c_str(), 10, *ret, NULL) != 0) {
            Dmsg1(0, "Unable to call the mail program to send the totp key %s\n", *ret);
            goto bail_out;
         }
      } else {
         pm_strcpy(ret, pngfile);
      }
      rcode = true;

   bail_out:
      // Run the email command
      unlink(urlfile);
      /* cleanup if we are in error or if we have send the file */
      if (!rcode || sendcommand) {
         unlink(pngfile);
      }
      return rcode;
   };

   /* Must call has_key() first */
   bDirAuthenticationRegister *gen_key(const char *name)
   {
      char k[24];
      char k64[2*sizeof(k)];         // Normally 33% larger

      // Generate a random string that will be used as key
      if (RAND_bytes((uint8_t*)k, sizeof(k)-1) == 0) {
         return NULL;
      }
      k[sizeof(k)-1]=0;

      bin_to_base64(k64, sizeof(k64), k, sizeof(k)-1, 1);
      
      if (!compute_keyfile(name, &keyname)) {
         return NULL;
      }

      umask(0077);
      makedir(keyname, false, 0700);
      FILE *fp = bfopen(keyname, "w");
      if (!fp) {
         berrno be;
         Dmsg2(DINFO, "Unable to create a new key %s. ERR=%s\n", keyname, be.bstrerror());
         return NULL;
      }

      fprintf(fp, "%s", k64);

      if (fclose(fp) != 0) {
         berrno be;
         Dmsg2(DINFO, "Unable to create a new key %s. ERR=%s\n", keyname, be.bstrerror());
         unlink(keyname);
         return NULL;
      }

      if (gen_qrcode) {
         if (!getQRCode(name, k64, &code)) {
            unlink(keyname);
            return NULL;
         }

      } else if (sendcommand) {
         if (!sendQRCode(name, k64, &code)) {
            unlink(keyname);
            return NULL;
         }
      } else {
         pm_strcpy(code, "\nTOTP code generated. Ask the QR Code to your Bacula Administrator.\n");
      }
      totpregisterQR.welcome = code;
      return &totpregisterQR;
   };
};

bRC loadPlugin(bDirInfo *lbinfo, bDirFuncs *lbfuncs, pDirInfo **pinfo, pDirFuncs **pfuncs)
{
#ifndef HAVE_OPENSSL
   Dmsg2(DINFO, "Unable to load TOTP plugin. Requires OpenSSL\n");
   return bRC_Error;
#endif

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
   totp_api *self = New (totp_api);
   DMSG0(ctx, DINFO, "newPlugin\n");
   ctx->pContext = self;
   return bRC_OK;
}

static bRC freePlugin(bpContext *ctx)
{
   DMSG0(ctx, DINFO, "freePlugin\n");
   if (ctx->pContext) {
      totp_api *self = (totp_api *)ctx->pContext;
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
   totp_api *self = (totp_api *)ctx->pContext;
   uint32_t c=999;

   switch (event->eventType) {
   case bDirEventAuthenticationQuestion:
      pvalue = (bDirAuthValue *)value;
      pvalue->authdata = totpquestions0;
      break;

   case bDirEventAuthenticationResponse:
      pvalue = (bDirAuthValue *)value;
      DMSG_EVENT_STR(event, pvalue->response);
      switch (pvalue->seqdata)
      {
      case 0:
         self->input_code = str_to_uint64((char *)pvalue->response);
         break;
      default:
         break;
      }
      break;

   case bDirEventAuthenticate:
      DMSG_EVENT_PTR(event, value);
      if (self->keyname[0] == 0) {
         return bRC_Error;
      }
      if (totp_from_secretfile(self->keyname, &c)) {
         if (self->input_code == c) {
            return bRC_OK;
         }
         Dmsg3(200, "Incorrect code for %s %ld == %ld\n", self->keyname, self->input_code, c);
      }
      return bRC_Error;
      break;

   default:
      break;
   }
   return bRC_OK;
}

static bRC getAuthenticationData(bpContext *ctx, const char *console, const char *param, void **data)
{
   totp_api *self = (totp_api *)ctx->pContext;
   bDirAuthenticationRegister **padata = (bDirAuthenticationRegister **)data;

   Dmsg2(DINFO, "console=%s param=%s\n", console, param);
   if (!self->parse_param(param)) {
      return bRC_Error;
   }
   if (self->has_key(console)) {
      *padata = &totpregister0;
   } else {
      *padata = self->gen_key(console);
   }
   return *padata ? bRC_OK : bRC_Error;
}

static bRC getAuthorizationData(bpContext *ctx, const char *console, const char *param, void **data)
{

   return bRC_OK;
}

#ifdef BTOTP_PROGRAM
static void usage(int ret)
{
      fprintf(stderr, _(
"Usage: btotp [-k /path/to/keydir] [-d100] [-c] [-r] [-u] [-q] -n name\n"
"       -d int            Set debug level\n"
"       -c                Create a key if needed\n"
"       -n name           Name of the console (or via BTOTP_NAME env)\n"
"       -u                Display otpauth URL\n"
"       -q                Display qrcode\n"
"       -r                Remove key on disk\n"
"       -k dir            Path to the keydir\n\n"));
      exit(ret);
}

int main(int argc, char **argv)
{
   setlocale(LC_ALL, "");
   bindtextdomain("bacula", LOCALEDIR);
   textdomain("bacula");
   init_stack_dump();
   lmgr_init_thread();
   totp_api totp;
   
   int ch;
   bool docreate=false, displayQR=false, displayURL=false,remove=false;
   char *name = NULL;
   my_name_is(argc, argv, "btotp");
   init_msg(NULL, NULL);

   OSDependentInit();

   while ((ch = getopt(argc, argv, "rd:ck:n:?qu")) != -1) {
      switch (ch) {
      case 'r':
         remove=true;
         break;
      case 'n':
         name=optarg;
         break;
      case 'q':
         displayQR=true;
         break;
      case 'u':
         displayURL=true;
         break;
      case 'd':                    /* debug level */
         if (*optarg == 't') {
            dbg_timestamp = true;
         } else {
            debug_level = atoi(optarg);
            if (debug_level <= 0) {
               debug_level = 1;
            }
         }
         break;
      case 'c':
         docreate=true;
         break;
      case 'k':
         totp.keydir = bstrdup(optarg);
         break;
      case '?':
      default:
         usage(0);

      }
   }
   argc -= optind;
   argv += optind;

   if (!name) {
      name = getenv("BTOTP_NAME");
   }
   
   if (argc != 0 || !name) {
      Pmsg0(0, _("Wrong number of arguments: \n"));
      usage(1);
   }

   if (!totp.has_key(name)) {
      if (docreate) {
         if (!totp.gen_key(name)) {
            Pmsg0(0, _("Unable to generate the key\n"));
            usage(2);
         }

      } else {
         Pmsg0(0, _("Unable to read the key. Use -c to create a key.\n"));
         usage(2);
      }
   }
   if (docreate) {
      printf("%s\n", totp.keyname);
      return 0;
   }
   if (remove) {
      POOL_MEM tmp;
      if (!totp.compute_keyfile(name, tmp.handle())) {
         Pmsg0(0, _("Unable to find the key file\n"));
         usage(2);
      }
      if (unlink(tmp.c_str()) < 0) {
         berrno be;
         Pmsg3(0, _("Unable to remove disk key %s for %s file. ERR=%s\n"), tmp.c_str(), name, be.bstrerror());
         usage(2);
      }
      printf("Disk storage %s for key %s has been deleted\n", tmp.c_str(), name);
      return 0;
   }
   if (displayURL || displayQR) {
      POOL_MEM tmp;
      char buf[512];
      uint32_t len;
      if (!totp.compute_keyfile(name, tmp.handle())) {
         Pmsg0(0, _("Unable to find the key file\n"));
         usage(2);
      }
      if (!totp_read_secretfile(tmp.c_str(), buf, sizeof(buf), &len)) {
         berrno be;
         Pmsg1(0, _("Unable to read the key. ERR=%s\n"), be.bstrerror());
         usage(2);
      }
      if (displayURL) {
         if (totp_get_url(name, buf, len, tmp.handle())) {
            Pmsg1(0, "\n%s\n", tmp.c_str());
         } else {
            Pmsg0(0, "Unable to generate the totp url from the key\n");
            usage(2);
         }
      }
      if (displayQR) {
         if (totp.getQRCode(name, buf, tmp.handle())) {
            Pmsg1(0, "\n%s\n", tmp.c_str());
         } else {
            Pmsg0(0, "Unable to generate the QRcode from the key\n");
            usage(2);
         }
      }
      return 0;
   }

   uint32_t c=99;
   if (totp_from_secretfile(totp.keyname, &c)) {
      printf("%06u\n", (unsigned int)c);

   } else {
      Pmsg0(0, _("Unable to generate the code from the key\n"));
      return 2;
   }
   return 0;
}
#endif // BTOTP_PROGRAM


#ifdef __cplusplus
}
#endif
