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
 * Inspired by vtape.h
 */

#ifndef DEDUP_DEV_H
#define DEDUP_DEV_H

/* enum loadable dedup drivers */
enum {
   D_LEGACY_DRIVER  = 1,
   D_DEDUP2_DRIVER  = 2
};

class dedup_dev : public file_dev {
public:
   DedupEngine *dedupengine;
   dedup_dev(JCR *jcr, DEVRES *device): dedupengine(NULL) { };
   virtual ~dedup_dev() {};
   int device_specific_init(JCR*, DEVRES*);
   bool truncate(DCR *dcr);
   bool open_device(DCR *dcr, int omode);
   const char *print_type();
   virtual void dbg_print(FILE *fp);

   void updateVolCatExtraBytes(uint64_t);
   boffset_t get_adata_size(DCR *dcr);
   bool setup_dedup_rehydration_interface(DCR *dcr);
   void free_dedup_rehydration_interface(DCR *dcr);
   GetMsg *get_msg_queue(JCR *jcr, BSOCK *sock, int32_t bufsize);
   void dedup_get_status(STATUS_PKT *sp, int options);
   const char *dedup_get_dedupengine_name() { return device->dedup->hdr.name; };
   void term(DCR *dcr);
   virtual bool dedup_cmd(JCR *jcr);
   virtual void *dedup_get_dedupengine() { return (void*)dedupengine; };
   virtual bool is_fs_nearly_full(uint64_t threshold);
};

#endif  /* DEDUP_DEV_H */
