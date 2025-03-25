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

/* the goal is to show how to use the functions in lib/crypto.c
 * to sign a stream of data
 */

#include "bacula.h"
#include "../lib/unittests.h"

/* the goal is to show how to use the functions in lib/crypto.c
 * to sign a stream of data and verify the signature
 * the code come
 */
int test_signature()
{
   JCR *jcr = NULL;
   X509_KEYPAIR *pki_keypair_pub;
   X509_KEYPAIR *pki_keypair_priv; // these two keypaire could only be one
   uint8_t data[65536];
   uint8_t mysignature[65536];
   uint8_t mysignature2[65536];
   int ret;
   log("Test signatures");

   int len=sizeof(data);
   // Find the keys and certificates
   const char *tmp = getenv("tmp");
   if (!tmp) {
      tmp="/tmp";
   } else {
      printf("Use tmp=%s\n", tmp);
   }

   POOL_MEM filename(PM_FNAME);
   struct stat statbuf;
   Mmsg(filename, "%s/cryptokeypair.pem", tmp);

   if (stat(filename.c_str(), &statbuf)!=0) {
      fprintf(stderr, "crypto pair not found: %s\n", filename.c_str());
      ok(false, "crypto pair not found");
      return 1;
   }
   // load the keypair
   pki_keypair_pub = crypto_keypair_new();
   pki_keypair_priv = crypto_keypair_new();
   // load public certificate
   ret = crypto_keypair_load_cert(pki_keypair_pub, filename.c_str());
   ok(ret, "load public certificate");

   // load public certificate and ist private key into the other keypair
   ret = crypto_keypair_load_cert(pki_keypair_priv, filename.c_str()); // needed too initialize keyid
   ret = crypto_keypair_load_key(pki_keypair_priv, filename.c_str(), NULL, NULL);
   ok(ret, "load private key");

   // in crypto_setup_digests(bctx_t &bctx)
   crypto_digest_t signing_algorithm = CRYPTO_DIGEST_SHA256;
   DIGEST *signing_digest = crypto_digest_new(jcr, signing_algorithm);
   ok(signing_digest != NULL, "signature digest initialization");

   // call crypto_digest_update()
   int cont = 1;
   for (int i=0; cont != 0 && i<1000; i++) {
      cont = crypto_digest_update(signing_digest, data, len);
   }
   ok(cont!=0, "update");

   // in crypto_terminate_digests(bctx_t &bctx)
   SIGNATURE *sig = crypto_sign_new(jcr);
   ok(sig != NULL, "crypto_sign_new");

   ret = crypto_sign_add_signer(sig, signing_digest, pki_keypair_priv);
   ok(ret, "An error occurred while adding signer the stream");

   // Get signature size
   uint32_t size = 0;
   ret = crypto_sign_encode(sig, NULL, &size); // this is mendatory
   ok(ret, "retrieving the size of the signature");

   ok(size<=sizeof(mysignature), "check signature size");

   ret = crypto_sign_encode(sig, mysignature, &size);
   ok(ret, "signe the stream");

   // decode the "encoded" signature
   SIGNATURE *sig2 = crypto_sign_decode(jcr, mysignature, size);
   ok(sig2 != NULL, "decode the signature");

   // verify_signature(r_ctx &rctx)
   // reuse the same digest (the lazy way)
   crypto_error_t crypto_err = crypto_sign_verify(sig2, pki_keypair_pub, signing_digest);
   ok(crypto_err == CRYPTO_ERROR_NONE, "crypto_sign_verify");
   {
      // No do the same calculation on another piece of data and check
      // that the new digest dont match the previous signature
      DIGEST *signing_digest = crypto_digest_new(jcr, signing_algorithm);
      const char *lorem_ipsum = "Lorem Ipsum";
      int upd = crypto_digest_update(signing_digest, (const uint8_t *)lorem_ipsum, strlen(lorem_ipsum));
      SIGNATURE *sig = crypto_sign_new(jcr);
      int ret = crypto_sign_add_signer(sig, signing_digest, pki_keypair_priv);
      uint32_t size = 0;
      crypto_sign_encode(sig, NULL, &size); // this is required to initialize size=~122
      int enc = crypto_sign_encode(sig, mysignature2, &size);
      SIGNATURE *sig3 = crypto_sign_decode(jcr, mysignature2, size);
      crypto_error_t c_err = crypto_sign_verify(sig3, pki_keypair_pub, signing_digest);
      ok(signing_digest!=NULL && upd!=0 && ret && enc && sig2 != NULL && c_err== CRYPTO_ERROR_NONE, "second digest");
      // the line below throw a Qmsg()
      // openssl.c:79 OpenSSL digest Verify final failed: ERR=error:04091068:lib(4):func(145):reason(104)
      crypto_error_t c_err2 = crypto_sign_verify(sig2, pki_keypair_pub, signing_digest);
      ok(c_err2 == CRYPTO_ERROR_BAD_SIGNATURE, "second digest should not match first signature");
      crypto_sign_free(sig);
      crypto_sign_free(sig3);
      crypto_digest_free(signing_digest);
   }

   // Release everything
   crypto_sign_free(sig);
   sig = NULL;
   crypto_sign_free(sig2);
   sig2 = NULL;

   crypto_digest_free(signing_digest);
   crypto_keypair_free(pki_keypair_pub);
   crypto_keypair_free(pki_keypair_priv);
   return 0;
}

int main()
{
   setup_daemon_message_queue(); // a Qmsg() is thrown
   Unittests sign_test("crypto_test");
   test_signature();
   int ret = report();
   free_daemon_message_queue();  // release the Qmsg()
   return ret;
}


