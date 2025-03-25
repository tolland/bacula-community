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
 * Routines for writing Cloud drivers
 *
  * Written by Norbert Bizet, June MMXVIII
 *
 */
#ifndef GENERIC_CLOUD_DRIVER_H
#define GENERIC_CLOUD_DRIVER_H

#include "cloud_driver.h"

class generic_driver: public cloud_driver {

protected:

   uint32_t buf_len;
   char *host_name;
   char *bucket_name;
   char *access_key;
   char *secret_key;
   char *region;
   int32_t protocol;
   int32_t uri_style;
   char *driver_command;
   char *blob_endpoint;
   char *file_endpoint;
   char *queue_endpoint;
   char *table_endpoint;
   char *endpoint_suffix;
   uint32_t max_concurrent_uploads;
   uint32_t max_concurrent_downloads;
   uint32_t objects_default_tier;
   /* 
      read_cb_type : returns the number of proceed bytes from char*. 
      char* is the data buffer. 
      size_t is the buffer size. 
      void* is arg (opaque handler)
   */
   typedef size_t (read_cb_type)(char*, size_t, void*);
   typedef struct {
      read_cb_type* fct;
      void *arg;
   } read_callback;

   /* 
      write_cb_type : returns the number of proceed bytes from char*. 
      char* is the data buffer. 
      size_t is the buffer size. 
      void* is arg (opaque handler)
      int& is the cb file status.
   */
   #define WRITE_CB_EOF 1
   #define WRITE_CB_ERR 2
   typedef size_t (write_cb_type)(char*, size_t, void*, int&);
   typedef struct {
      write_cb_type* fct;
      void *arg;
   } write_callback;

   /* accessors with difference prototypes */
   int call_fct( const char* fct_name,
                  const char* volume_name,
                  int part_number,
                  read_callback *read_cb,
                  write_callback *write_cb,
                  cancel_callback *cancel_cb,
                  POOLMEM *&err,
                  const char* cache_path_name = NULL
                  );
   int call_fct(const char* fct_name,
                  const char* volume_name,
                  const char* part_name,
                  read_callback *read_cb,
                  write_callback *write_cb,
                  cancel_callback *cancel_cb,
                  POOLMEM *&err);

   /* actual working function */
   int call_fct( char* cmd,
                  read_callback *read_cb,
                  write_callback *write_cb,
                  cancel_callback *cancel_cb,
                  POOLMEM *&err);

public:
   generic_driver();
   virtual ~generic_driver();
   bool copy_cache_part_to_cloud(transfer *xfer);
   bool move_cloud_part(const char *VolumeName, uint32_t apart , const char *to, cancel_callback *cancel_cb, POOLMEM *&err, int& exists);
   int copy_cloud_part_to_cache(transfer *xfer);
   bool restore_cloud_object(transfer *xfer, const char *cloud_fname);
   bool is_waiting_on_server(transfer *xfer);
   bool truncate_cloud_volume(const char *volume_name, ilist *trunc_parts, cancel_callback *cancel_cb, POOLMEM *&err);
   bool clean_cloud_volume(const char *VolumeName, cleanup_cb_type *cb, cleanup_ctx_type *ctx, cancel_callback *cancel_cb, POOLMEM *&err);
   bool init(CLOUD *cloud, POOLMEM *&err);
   bool term(POOLMEM *&err);
   bool start_of_job(POOLMEM *&err);
   bool end_of_job(POOLMEM *&err);
   bool get_cloud_volume_parts_list(const char* volume_name, ilist *parts, cancel_callback *cancel_cb, POOLMEM *&err);
   bool get_cloud_volumes_list(alist *volumes, cancel_callback *cancel_cb, POOLMEM *&err);

private:
   POOLMEM *access_key_env;
   POOLMEM *secret_key_env;
   POOLMEM *bucket_env;
   POOLMEM *host_env;
   POOLMEM *region_env;
   POOLMEM *protocol_env;
   POOLMEM *uri_style_env;
   POOLMEM *blob_endpoint_env;
   POOLMEM *file_endpoint_env;
   POOLMEM *queue_endpoint_env;
   POOLMEM *table_endpoint_env;
   POOLMEM *endpoint_suffix_env;
   POOLMEM *max_concurrent_uploads_env;
   POOLMEM *max_concurrent_downloads_env;
   POOLMEM *upload_limit_env;
   POOLMEM *download_limit_env;
   POOLMEM *transfer_priority_env;
   POOLMEM *transfer_retention_env;
   POOLMEM *unset_lctime_env;
   POOLMEM *debug_env;
   POOLMEM *working_path_env;
   POOLMEM *home_path_env;
   POOLMEM *objects_default_tier_env;
   char *envs[24];
};

#endif /* GENERIC_CLOUD_DRIVER_H*/
