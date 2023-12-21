/*
   Bacula(R) - The Network Backup Solution

   Copyright (C) 2000-2023 Kern Sibbald

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

#include "generic_driver.h"
#include "cloud_transfer_mgr.h"

#ifndef HAVE_OPENSSL
   #include "lib/sha2.h"
#endif

const int64_t dbglvl = DT_CLOUD|50;
const size_t block_size = 4096;
const int TIMEOUT = 0;
const int RETRY_DOWNLOAD = 0x0D;
const int MIN_RESTORE_CONTAINER_NAME_SIZE = 3;
const int MAX_RESTORE_CONTAINER_NAME_SIZE = 63;
#ifdef __cplusplus
extern "C" {
#endif

cloud_driver *BaculaCloudDriver()
{
   return New(generic_driver);
}

#ifdef __cplusplus
}
#endif
/* taken from plugins */
static ssize_t full_write(FILE *fp, char *ptr, int32_t nbytes, cancel_callback *cancel_cb=NULL)
{
   int fd = fileno(fp);
   int32_t nleft, nwritten;
   nleft = nbytes;
   bool cancel = false;
   if (cancel_cb && cancel_cb->fct) {
      cancel = cancel_cb->fct(cancel_cb->arg);
   }
   while (nleft > 0 && !cancel) {
      do {
         errno = 0;
         nwritten = fwrite(ptr, 1, nleft, fp);
      } while (nwritten == -1 && errno == EINTR);
      /*
       * If connection is non-blocking, we will get EAGAIN
       */
      if (nwritten == -1 && errno == EAGAIN) {
         fd_set fdset;
         struct timeval tv;

         FD_ZERO(&fdset);
         FD_SET((unsigned)fd, &fdset);
         tv.tv_sec = 1;
         tv.tv_usec = 0;
         select(fd + 1, NULL, &fdset, NULL, &tv);
         continue;
      }
      if (nwritten <= 0) {
         return nwritten;          /* error */
      }
      nleft -= nwritten;
      ptr += nwritten;

      if (cancel_cb && cancel_cb->fct) {
         cancel = cancel_cb->fct(cancel_cb->arg);
      }
   }
   return nbytes - nleft;
}


generic_driver::generic_driver()
{
   host_env = get_pool_memory(PM_FNAME);
   bucket_env = get_pool_memory(PM_NAME);
   access_key_env = get_pool_memory(PM_NAME);
   secret_key_env = get_pool_memory(PM_NAME);
   region_env = get_pool_memory(PM_NAME);
   protocol_env = get_pool_memory(PM_NAME);
   uri_style_env = get_pool_memory(PM_NAME);
   blob_endpoint_env = get_pool_memory(PM_NAME);
   file_endpoint_env = get_pool_memory(PM_NAME);
   queue_endpoint_env = get_pool_memory(PM_NAME);
   table_endpoint_env = get_pool_memory(PM_NAME);
   endpoint_suffix_env = get_pool_memory(PM_NAME);
   max_concurrent_uploads_env = get_pool_memory(PM_NAME);
   max_concurrent_downloads_env = get_pool_memory(PM_NAME);
   upload_limit_env = get_pool_memory(PM_NAME);
   download_limit_env = get_pool_memory(PM_NAME);
   transfer_priority_env = get_pool_memory(PM_NAME);
   transfer_retention_env = get_pool_memory(PM_NAME);
   unset_lctime_env = get_pool_memory(PM_NAME);
   debug_env = get_pool_memory(PM_NAME);
   working_path_env = get_pool_memory(PM_NAME);
   home_path_env = get_pool_memory(PM_NAME);
   objects_default_tier_env = get_pool_memory(PM_NAME);
}

generic_driver::~generic_driver()
{
   free_pool_memory(host_env);
   free_pool_memory(bucket_env);
   free_pool_memory(access_key_env);
   free_pool_memory(secret_key_env);
   free_pool_memory(region_env);
   free_pool_memory(protocol_env);
   free_pool_memory(uri_style_env);
   free_pool_memory(blob_endpoint_env);
   free_pool_memory(file_endpoint_env);
   free_pool_memory(queue_endpoint_env);
   free_pool_memory(table_endpoint_env);
   free_pool_memory(endpoint_suffix_env);
   free_pool_memory(max_concurrent_uploads_env);
   free_pool_memory(max_concurrent_downloads_env);
   free_pool_memory(upload_limit_env);
   free_pool_memory(download_limit_env);
   free_pool_memory(transfer_priority_env);
   free_pool_memory(transfer_retention_env);
   free_pool_memory(unset_lctime_env);
   free_pool_memory(debug_env);
   free_pool_memory(working_path_env);
   free_pool_memory(home_path_env);
   free_pool_memory(objects_default_tier_env);
}

bool generic_driver::init(CLOUD *cloud, POOLMEM *&err) {
   /* File I/O buffer */
   buf_len = DEFAULT_BLOCK_SIZE;
   if (cloud) {
      host_name = cloud->host_name;
      bucket_name = cloud->bucket_name;
      access_key = cloud->access_key;
      secret_key = cloud->secret_key;
      region = cloud->region;
      protocol = cloud->protocol;
      uri_style = cloud->uri_style;
      blob_endpoint = cloud->blob_endpoint;
      file_endpoint = cloud->file_endpoint;
      queue_endpoint = cloud->queue_endpoint;
      table_endpoint = cloud->table_endpoint;
      endpoint_suffix = cloud->endpoint_suffix;
      max_concurrent_uploads = cloud->max_concurrent_uploads;
      max_concurrent_downloads = cloud->max_concurrent_downloads;
      upload_limit.set_bwlimit(cloud->upload_limit);
      download_limit.set_bwlimit(cloud->download_limit);
      objects_default_tier = cloud->objects_default_tier;

      driver_command = cloud->driver_command;


      pm_strcpy(host_env, "CLOUD_HOST=");
      pm_strcat(host_env, NPRTB(host_name));
      envs[0] = host_env;

      pm_strcpy(bucket_env, "CLOUD_BUCKET=");
      pm_strcat(bucket_env, NPRTB(bucket_name));
      envs[1] = bucket_env;

      pm_strcpy(access_key_env, "CLOUD_ACCESS_KEY=");
      pm_strcat(access_key_env, NPRTB(access_key));
      envs[2] = access_key_env;

      pm_strcpy(secret_key_env, "CLOUD_SECRET_KEY=");
      pm_strcat(secret_key_env, NPRTB(secret_key));
      envs[3] = secret_key_env;

      pm_strcpy(region_env, "CLOUD_REGION=");
      pm_strcat(region_env, NPRTB(region));
      envs[4] = region_env;

      pm_strcpy(protocol_env, "CLOUD_PROTOCOL=");
      pm_strcat(protocol_env, (protocol==0)?"https":"http");
      envs[5] = protocol_env;

      pm_strcpy(uri_style_env, "CLOUD_URI_TYPE=");
      pm_strcat(uri_style_env, (uri_style==0)?"virtual":"path");
      envs[6] = uri_style_env;

      pm_strcpy(blob_endpoint_env, "CLOUD_BLOB_ENDPOINT=");
      pm_strcat(blob_endpoint_env, NPRTB(blob_endpoint));
      envs[7] = blob_endpoint_env;

      pm_strcpy(file_endpoint_env, "CLOUD_FILE_ENDPOINT=");
      pm_strcat(file_endpoint_env, NPRTB(file_endpoint));
      envs[8] = file_endpoint_env;

      pm_strcpy(queue_endpoint_env, "CLOUD_QUEUE_ENDPOINT=");
      pm_strcat(queue_endpoint_env, NPRTB(queue_endpoint));
      envs[9] = queue_endpoint_env;

      pm_strcpy(table_endpoint_env, "CLOUD_TABLE_ENDPOINT=");
      pm_strcat(table_endpoint_env, NPRTB(table_endpoint));
      envs[10] = table_endpoint_env;

      pm_strcpy(endpoint_suffix_env, "CLOUD_ENDPOINT_SUFFIX=");
      pm_strcat(endpoint_suffix_env, NPRTB(endpoint_suffix));
      envs[11] = endpoint_suffix_env;

      char b[33];
      sprintf(b, "%d", max_concurrent_uploads);
      pm_strcpy(max_concurrent_uploads_env, "CLOUD_MAX_CONCURRENT_UPLOADS=");
      pm_strcat(max_concurrent_uploads_env, b);
      envs[12] = max_concurrent_uploads_env;

      sprintf(b, "%d", max_concurrent_downloads);
      pm_strcpy(max_concurrent_downloads_env, "CLOUD_MAX_CONCURRENT_DOWNLOADS=");
      pm_strcat(max_concurrent_downloads_env, b);
      envs[13] = max_concurrent_downloads_env;

      sprintf(b, "%lld", (long long) cloud->upload_limit);
      pm_strcpy(upload_limit_env, "CLOUD_UPLOAD_LIMIT=");
      pm_strcat(upload_limit_env, b);
      envs[14] = upload_limit_env;

      sprintf(b, "%lld", (long long) cloud->download_limit);
      pm_strcpy(download_limit_env, "CLOUD_DOWNLOAD_LIMIT=");
      pm_strcat(download_limit_env, b);
      envs[15] = download_limit_env;

      sprintf(b, "%d", cloud->transfer_priority);
      pm_strcpy(transfer_priority_env, "CLOUD_TRANSFER_PRIORITY=");
      pm_strcat(transfer_priority_env, b);
      envs[16] = transfer_priority_env;

      uint32_t transfer_retention_days = cloud->transfer_retention / (3600 * 24);
      /* at least 1 day retention period */
      if (transfer_retention_days <= 0) {
         transfer_retention_days = 1;
      }

      sprintf(b, "%d", transfer_retention_days);
      pm_strcpy(transfer_retention_env, "CLOUD_TRANSFER_RETENTION_DAYS=");
      pm_strcat(transfer_retention_env, b);
      envs[17] = transfer_retention_env;

      pm_strcpy(debug_env, "CLOUD_DEBUG=");
      if (chk_dbglvl(dbglvl)) pm_strcat(debug_env, "TRUE");
      envs[18] = debug_env;

      pm_strcpy(working_path_env, "CLOUD_WORKING_PATH=");
      pm_strcat(working_path_env, working_directory);
      envs[19] = working_path_env;

      pm_strcpy(home_path_env, "HOME="); /* child default home location */
      pm_strcat(home_path_env, working_directory);
      envs[20] = home_path_env;

     sprintf(b, "%d", objects_default_tier);
      pm_strcpy(objects_default_tier_env, "CLOUD_OBJECTS_DEFAULT_TIER="); /* child default home location */
      pm_strcat(objects_default_tier_env, b);
      envs[21] = objects_default_tier_env;

      if (driver_command && strstr(driver_command, "was_cloud_driver") != NULL) {
         pm_strcpy(unset_lctime_env, "LC_TIME=");
         envs[22] = unset_lctime_env;
      } else {
         envs[22] = NULL;
      }
      envs[23] = NULL;

      return true;
   }
   return false;
}

/* helper to handler close_bpipe error code */
/* be.code(stat) extracts the actual error code returned by the driver script */
/* the pipe-specific errors are concated to the err string */
int handle_error(int stat, POOLMEM *&err)
{
   berrno be;
   if (be.code(stat) != 0) {
      err = strip_trailing_junk(err);
      pm_strcat(err, " ");
      pm_strcat(err, be.bstrerror(stat));
   }
   return be.code(stat);
}

/*
Main contact point with the script. The script is always called with the following parameters
in this exact order :
fct_name,
volume_name,
cache_name,
partname ("part.x").

Depending on fct_name, some parameters can be NULL ("*None*").

call_fct Control I/Os with the script and calls read_cb and write_cb in return.

read_cb : is called when data from the script is available for read. First argument is the data buffer ready
for read. Second argument is the size of the data. The last argument is the arg parameter that's been passed at cb creation.
A typical read_cb function will parse the data buffer for some information/error message or copy the data buffer to file.

write_cb : is called when the script is ready to write data. First argument is the data buffer to fill with data.
Second argument is the max size of the data. The last argument is the arg parameter that's been passed at cb creation.
The write_cb function must return the actual written size in the data buffer within the write_cb function, 0 if the file is to be closed (eof)
or -1 in case of error (in this case, data buffer contains the error text).

A typical write_cb function will fill the data buffer with data and return the actual data size written.

The return value is the script return value. 0 is success. But err must be tested separately for error message.
*/
int generic_driver::call_fct(const char* fct_name,
                              const char* volume_name,
                              int part_number,
                              read_callback *read_cb,
                              write_callback *write_cb,
                              cancel_callback *cancel_cb,
                              POOLMEM *&err,
                              const char* cache_path_name)
{
   POOL_MEM cmd(PM_FNAME);
   if (cache_path_name) {
      Mmsg(cmd, "%s %s %s part.%d %s",
         driver_command, fct_name, NPRT(volume_name), part_number, cache_path_name);
   } else {
      Mmsg(cmd, "%s %s %s part.%d",
         driver_command, fct_name, NPRT(volume_name), part_number);
   }

   return call_fct(cmd.addr(), read_cb, write_cb, cancel_cb, err);
}

int generic_driver::call_fct(const char* fct_name,
                              const char* volume_name,
                              const char* part_name,
                              read_callback *read_cb,
                              write_callback *write_cb,
                              cancel_callback *cancel_cb,
                              POOLMEM *&err)
{
   POOL_MEM cmd(PM_FNAME);
   Mmsg(cmd, "%s %s %s %s",
      driver_command, fct_name, NPRT(volume_name), NPRT(part_name));

   return call_fct(cmd.addr(), read_cb, write_cb, cancel_cb, err);
}

int generic_driver::call_fct(char* cmd,
                              read_callback *read_cb,
                              write_callback *write_cb,
                              cancel_callback *cancel_cb,
                              POOLMEM *&err)
{
   BPIPE *bpipe;
   POOL_MEM res(PM_BSOCK);
   bool cancel;

   /* recheck debug level */
   pm_strcpy(debug_env, "CLOUD_DEBUG=");
   if (chk_dbglvl(dbglvl)) pm_strcat(debug_env, "TRUE");
   envs[18] = debug_env;

   if (chk_dbglvl(dbglvl)) {
      Dmsg1(dbglvl, "cmd=%s\n", cmd);
      for (int i = 0; envs[i] ; i++) {
         if (strncmp(envs[i], "CLOUD_SECRET_KEY=", 17) == 0) {
            Dmsg0(dbglvl, " CLOUD_SECRET_KEY=xxx\n");
         } else {
            Dmsg1(dbglvl, " %s\n", envs[i]);
         }
      }
   }

   bpipe = open_bpipe(cmd, TIMEOUT, "rwe",envs);
   if (!bpipe) {
      Mmsg(err, _("%s. error in open_bpipe.\n"), cmd);
      return false;
   }

   cancel = (cancel_cb && cancel_cb->fct && cancel_cb->fct(cancel_cb->arg));

   while(!cancel) {
      int rfd = bpipe->rfd ? fileno(bpipe->rfd):-1;
      int wfd = bpipe->wfd ? fileno(bpipe->wfd):-1;
      int efd = bpipe->efd ? fileno(bpipe->efd):-1;

      fd_set rfds;
      FD_ZERO(&rfds);
      if (rfd >= 0) FD_SET((unsigned)rfd, &rfds);
      if (efd >= 0) FD_SET((unsigned)efd, &rfds);
      fd_set wfds;
      FD_ZERO(&wfds);
      if (write_cb && wfd >= 0) FD_SET((unsigned)wfd, &wfds);
      struct timeval tv;
      tv.tv_sec = 1;
      tv.tv_usec = 0;

      int maxfd = rfd>wfd?(efd>rfd?efd:rfd):(efd>wfd?efd:wfd);
      int ret = select(maxfd+1, &rfds, &wfds, NULL, &tv);
      if (ret == 0) { /* timeout */
         if (FD_ISSET(efd, &rfds)) {
            size_t size = read(efd, res.addr(), res.size()-1);
            if (size > 0) {
               res.addr()[size] = '\0';
               Mmsg(err, _("%s. %s\n"), cmd, res.c_str());
            }
         }
      } else if (ret < 0) { /* error */
         berrno be;
         Mmsg(err, _("%s. error in select. %s\n"), cmd, be.bstrerror(errno));
         return handle_error(close_bpipe(bpipe), err);
      }  else if (FD_ISSET(rfd, &rfds)) {
        /* read 1 line */
         ssize_t size = read(rfd, res.addr(), res.size()-1);
         if (size == -1) {
            /* error */
            berrno be;
            Mmsg(err, _("%s. fgets error. %s\n"), cmd, be.bstrerror(ferror(bpipe->rfd)));
            return handle_error(close_bpipe(bpipe), err);
         } else if (size == 0) {
            return handle_error(close_bpipe(bpipe), err);
         } else {
            res.addr()[size] = '\0';   /* Outside from the data stream, but it might be displayed in a command */
            size_t unsigned_size = size;
            if (download_limit.use_bwlimit()) download_limit.control_bwlimit(size);
            if (read_cb && read_cb->fct){
               /* process the current line*/
               size_t processed = read_cb->fct(res.addr(), size, read_cb->arg);
               if (processed != unsigned_size) {
                  if (errno!=0) {
                     berrno be;
                     Mmsg(err, _("%s. read_cb error. %s\n"), cmd, be.bstrerror(errno));
                  } else {
                     Mmsg(err, _("%s. read_cb error. %s\n"), cmd, res.c_str());
                  }
                  return handle_error(close_bpipe(bpipe), err);
               }
            }
         }
      } else if (FD_ISSET(efd, &rfds)) {
         size_t size = read(efd, res.addr(), res.size()-1);
         if (size > 0) {
            res.addr()[size] = '\0';
            Mmsg(err, _("%s. %s\n"), cmd, res.c_str());
            return handle_error(close_bpipe(bpipe), err);
         }
      } else if (FD_ISSET(wfd, &wfds)) {
         int f=0;
         size_t size = write_cb->fct(res.addr(), res.size()-1, write_cb->arg, f);
         if (upload_limit.use_bwlimit()) upload_limit.control_bwlimit(size);
         if (f & WRITE_CB_EOF) {
            full_write(bpipe->wfd, res.addr(), size, cancel_cb);
            /* end of file */
            if (close_wpipe(bpipe)==0) {
               return 1;
            }
         } else if (f & WRITE_CB_ERR) {
            /* error */
            Mmsg(err, _("%s. fgets error. %s"), cmd, res.c_str());
            return handle_error(close_bpipe(bpipe), err);
         } else {
            size_t processed = full_write(bpipe->wfd, res.addr(), size, cancel_cb);
            if (processed != size) {
               if (errno!=0) {
                  berrno be;
                  Mmsg(err, _("%s. write to pipe error. %s\n"), cmd, be.bstrerror(errno));
               } else {
                  Mmsg(err, _("%s. write to pipe error. %s\n"), cmd, res.c_str());
               }
               return handle_error(close_bpipe(bpipe), err);
            }
         }
      }
      // update cancel status
      cancel = (cancel_cb && cancel_cb->fct && cancel_cb->fct(cancel_cb->arg));
   }

   if (cancel) {
      Mmsg(err, _("%s. cancelled by job.\n"), cmd);
      handle_error(close_bpipe(bpipe), err);
      return -1;
   }

   return 0;
}

bool generic_driver::term(POOLMEM *&err) {
   return true;
}

bool generic_driver::start_of_job(POOLMEM *&err) {
   if (err) {
      Mmsg(err, _("Using driver %s Bucket=%s"), driver_command, bucket_name);
   }
   return true;
}

bool generic_driver::end_of_job(POOLMEM *&err) {
   return true;
}

size_t copy_cache_part_to_cloud_read_cb(char *res, size_t sz, void* arg)
{
   transfer *xfer = (transfer*)arg;
   bool wrong_string = false;

   if (xfer) {
      char *size=strstr(res, "size:"), *mtime=strstr(res, "mtime:");
      if (size && mtime) {
         xfer->m_res_mtime = str_to_uint64(&(mtime[6]));
         xfer->m_res_size = str_to_uint64(&(size[5]));
      } else {
         wrong_string = true;
      }
      return wrong_string ? 0:sz;
   }
   return 0;
}

struct copy_cache_part_to_cloud_write_cb_arg {
   FILE *file;
   SHA512_CTX* sha512;
   transfer *xfer;
};

size_t copy_cache_part_to_cloud_write_cb(char *res, size_t block_size, void* arg, int& flags)
{
   flags = 0;
   ssize_t ret = 0;
   copy_cache_part_to_cloud_write_cb_arg *_arg = (copy_cache_part_to_cloud_write_cb_arg*)arg;
   if (_arg) {
      FILE *file = _arg->file;
      if (file) {
         ret = read(fileno(file), res, block_size);

         if (ret == -1){
            /* error */
            berrno be;
            Mmsg(res, "write_read_cb ERR %s", be.bstrerror(ferror(file)));
            flags|= WRITE_CB_ERR;
            ret = strlen(res);
         } else if (ret == 0) {
            /* end of file */
            flags|= WRITE_CB_EOF;
         } else { /* res is filed with ret new bytes */
            if (_arg->sha512) {
               SHA512_Update(_arg->sha512, res, ret);
            }
            if (_arg->xfer) {
               _arg->xfer->increment_processed_size(ret);
            }
         }
      }
   }
   return ret;
}

bool copy_cache_part_to_cloud_cancel_cb(void* arg)
{
   transfer *xfer = (transfer*)arg;
   if (xfer) return xfer->is_canceled();
   return false;
}

bool generic_driver::copy_cache_part_to_cloud(transfer *xfer)
{
   bool use_cache_file_path = (strstr(driver_command, "was_cloud_driver") != NULL);

   SHA512_CTX sha512;
   SHA512_Init(&sha512);
   read_callback rcb;
   rcb.fct = &copy_cache_part_to_cloud_read_cb;
   rcb.arg = (void*)xfer;

   cancel_callback ccb;
   ccb.fct = &copy_cache_part_to_cloud_cancel_cb;
   ccb.arg = (void*)xfer;

   /* when not passing the cache file path, stream it to the driver stdind */
   if (!use_cache_file_path) {
   /* copy to const_cast */
      char *fname = bstrdup(xfer->m_cache_fname);
      FILE *file=bfopen(fname, "rb");
      if (!file) {
         berrno be;
         Mmsg2(xfer->m_message, "Could not open output file %s. ERR=%s\n",
               fname, be.bstrerror());
         free(fname);
         return false;
      }
      write_callback wcb;
      wcb.fct = &copy_cache_part_to_cloud_write_cb;
      copy_cache_part_to_cloud_write_cb_arg arg;
      arg.file = file;
      if (getenv("GENERATE_CLOUD_HASH")) {
         arg.sha512 = &sha512;
      } else {
         arg.sha512 = NULL;
      }
      arg.xfer = xfer;
      wcb.arg = (void*)&arg;

      uint32_t retry = max_upload_retries;
      int ret=-1; /* so we pass the first while */
      while ((ret!=0) && (retry>0)) {
         /* when the driver decide to retry, it must reset the processed size */
         xfer->reset_processed_size();
         SHA512_Init(&sha512);
         if (retry < max_upload_retries) {
            if (xfer->m_message) {
               Dmsg3(dbglvl, "%s retry #%d err=%d\n", xfer->m_message, (max_upload_retries-retry), ret);
            } else {
               Dmsg3(dbglvl, "generic_driver::copy_cache_part_to_cloud part.%d retry #%d err=%d\n", xfer->m_part, (max_upload_retries-retry), ret);
            }
            /* And we clean the cloud part, in case the cloud target is a mess */
            call_fct("delete", xfer->m_volume_name, xfer->m_part, NULL, NULL, &ccb, xfer->m_message, NULL);
         }
         ret=call_fct("upload",xfer->m_volume_name, xfer->m_part, &rcb, &wcb, &ccb, xfer->m_message, NULL);
         if (ret!=0) {
            xfer->inc_retry();
         }
         --retry;
      }

      if (fclose(file) != 0) {
         berrno be;
         Mmsg2(xfer->m_message, "Could not close output file %s. ERR=%s\n",
               fname, be.bstrerror());
         free(fname);
         return false;
      }
      if (getenv("GENERATE_CLOUD_HASH")) {
         SHA512_Final(xfer->m_hash64, &sha512);
      } else {
         bmemzero(xfer->m_hash64, 64);
      }

      free(fname);
      return (ret==0);

   } else {
      uint32_t retry = max_upload_retries;
      int ret=-1;
      while ((ret!=0) && (retry>0)) {
         /* when the driver decide to retry, it must reset the processed size */
         xfer->reset_processed_size();
         SHA512_Init(&sha512);
         if (retry < max_upload_retries) {
            if (xfer->m_message) {
               Dmsg3(dbglvl, "%s retry #%d err=%d\n", xfer->m_message, (max_upload_retries-retry), ret);
            } else {
               Dmsg3(dbglvl, "generic_driver::copy_cache_part_to_cloud part.%d retry #%d err=%d\n", xfer->m_part, (max_upload_retries-retry), ret);
            }
            /* And we clean the cloud part, in case the cloud target is a mess */
            call_fct("delete", xfer->m_volume_name, xfer->m_part, NULL, NULL, &ccb, xfer->m_message, NULL);
         }
         ret=call_fct("upload",xfer->m_volume_name, xfer->m_part, &rcb, NULL, &ccb, xfer->m_message, xfer->m_cache_fname);
         if (ret!=0) {
            xfer->inc_retry();
         }

         --retry;
      }
      if (getenv("GENERATE_CLOUD_HASH") && (ret==0)) {
         /* compute SHA512 */
         char *fname = bstrdup(xfer->m_cache_fname);
         FILE *file=bfopen(fname, "rb");
         if (!file) {
            berrno be;
            Mmsg2(xfer->m_message, "Could not open output file %s. ERR=%s\n",
                  fname, be.bstrerror());
            free(fname);
            return false;
         }
         POOLMEM *res = get_memory(block_size+1);
         ssize_t size = block_size;
         while( size == block_size) {
            size = fread(res, 1, block_size, file);
            SHA512_Update(&sha512, res, size);
         }
         free_pool_memory(res);
         if (fclose(file) != 0) {
            berrno be;
            Mmsg2(xfer->m_message, "Could not close output file %s. ERR=%s\n",
                  fname, be.bstrerror());
            free(fname);
            return false;
         }
         free(fname);
         SHA512_Final(xfer->m_hash64, &sha512);
      } else {
         bmemzero(xfer->m_hash64, 64);
      }
      return (ret==0);
   }
   return false;
}


struct move_cloud_part_read_cb_arg {
   POOLMEM *msg;
};

size_t move_cloud_part_read_cb(char *res, size_t size, void* arg)
{
   move_cloud_part_read_cb_arg*_arg = (move_cloud_part_read_cb_arg*)arg;
   if (_arg) {
      Mmsg(_arg->msg, "%s", res);
   }
   return size;
}

bool generic_driver::move_cloud_part(const char *VolumeName, uint32_t apart , const char *to, cancel_callback *cancel_cb, POOLMEM *&err, int& exists)
{
   /* retrieve the output message into err */
   read_callback rcb;
   rcb.fct = &move_cloud_part_read_cb;
   move_cloud_part_read_cb_arg arg;
   arg.msg = err;
   rcb.arg = (void*)&arg;

   int ret = call_fct("move", VolumeName, (int)apart, &rcb, NULL, cancel_cb, err, to);
   /* 0 = OK (either because the part has been moved or because it doesn't exists) */
   if (ret == 0) {
      /* copied part is return by the read callback */
      exists = strlen(err);
      return true;
   }

   return false;
}


struct copy_cloud_part_to_cache_read_cb_arg {
   FILE *file;
   SHA512_CTX* sha512;
   transfer *xfer;
};

size_t copy_cloud_part_to_cache_read_cb(char *res, size_t size, void* arg)
{
   copy_cloud_part_to_cache_read_cb_arg *_arg = (copy_cloud_part_to_cache_read_cb_arg*)arg;
   if (_arg) {
      if (_arg->file) {
         if (_arg->sha512) {
            SHA512_Update(_arg->sha512, res, size);
         }
         if (_arg->xfer) {
            _arg->xfer->increment_processed_size(size);
         }
         return full_write(_arg->file, res, size);
      }
   }
   return 0;
}

bool copy_cloud_part_to_cache_cancel_cb(void* arg)
{
   transfer *xfer = (transfer*)arg;
   if (xfer) return xfer->is_canceled();
   return false;
}

int generic_driver::copy_cloud_part_to_cache(transfer *xfer)
{
   SHA512_CTX sha512;
   SHA512_Init(&sha512);
   cancel_callback ccb;
   ccb.fct = &copy_cloud_part_to_cache_cancel_cb;
   ccb.arg = (void*)xfer;

   if (xfer) {
      /* Azure restores on a different bucket, so the restore bucket is the one to be checked */
      /* swap bucket to restore or swap it back to the original */
      if (strlen(xfer->m_restore_bucket) != 0) {
         pm_strcpy(bucket_env, "CLOUD_BUCKET=");
         pm_strcat(bucket_env, NPRTB(xfer->m_restore_bucket));
         envs[1] = bucket_env;
      } else {
         pm_strcpy(bucket_env, "CLOUD_BUCKET=");
         pm_strcat(bucket_env, NPRTB(bucket_name));
         envs[1] = bucket_env;
      }
   }

   bool use_cache_file_path = (strstr(driver_command, "was_cloud_driver") != NULL);
   if (!use_cache_file_path) {
      /* Create the cache file */
      /* copy to const_cast */
      char *fname = bstrdup(xfer->m_cache_fname);
      /* create the folder */
      char *p = fname;
      while (IsPathSeparator(*p)) {
         p++;
      }
      while ((p = first_path_separator(p))) {
         char save_p;
         save_p = *p;
         *p = 0;
         mkdir(fname, 0740);
         *p = save_p;
         while (IsPathSeparator(*p)) {
            p++;
         }
      }

      FILE *file=bfopen(fname, "wb");
      if (!file) {
         berrno be;
         Mmsg2(xfer->m_message, "Could not open output file %s. ERR=%s\n",
               fname, be.bstrerror());
         free(fname);
         return CLOUD_DRIVER_COPY_PART_TO_CACHE_ERROR;
      }

      read_callback rcb;
      rcb.fct = &copy_cloud_part_to_cache_read_cb;
      copy_cloud_part_to_cache_read_cb_arg arg;
      arg.file = file;
      if (getenv("GENERATE_CLOUD_HASH")) {
         arg.sha512 = &sha512;
      } else {
         arg.sha512 = NULL;
      }
      arg.xfer = xfer;
      rcb.arg = (void*)&arg;
      int ret=call_fct("download",xfer->m_volume_name, xfer->m_part, &rcb, NULL, &ccb, xfer->m_message, NULL);

      if (fclose(file) != 0) {
         berrno be;
         Mmsg2(xfer->m_message, "Could not close output file %s. ERR=%s\n",
               fname, be.bstrerror());
         free(fname);
         return CLOUD_DRIVER_COPY_PART_TO_CACHE_ERROR;
      }
      if (getenv("GENERATE_CLOUD_HASH")) {
         SHA512_Final(xfer->m_hash64, &sha512);
      } else {
         bmemzero(xfer->m_hash64, 64);
      }

      free(fname);

      if (ret==RETRY_DOWNLOAD)
      {
         /* reset xfer->m_message (MT9511)
         Normally we dont do it in order to report as many errors as possible but 
         retry is detected based on the return code (0x0D).
         This code might be interpreted as an error thru bpipe ("Unknown error during program execvp" in berror.c), 
         so to keep the log clean, we reset the error message in this very specific case. */
         if (xfer && xfer->m_message) {
            Dmsg3(dbglvl, "generic_driver::RETRY_DOWNLOAD purgin xfer message '%s' %s\\part%d.\n", xfer->m_message, xfer->m_volume_name, xfer->m_part);
            *(xfer->m_message)='\0';
         }
         return CLOUD_DRIVER_COPY_PART_TO_CACHE_RETRY;
      }

      return (ret==0) ? CLOUD_DRIVER_COPY_PART_TO_CACHE_OK:CLOUD_DRIVER_COPY_PART_TO_CACHE_ERROR;

   } else { /* Azure is assumed here. Make sure to adjust logic if other provider use direct write to cache*/
      int ret=call_fct("download",xfer->m_volume_name, xfer->m_part, NULL, NULL, &ccb, xfer->m_message, xfer->m_cache_fname);
      if (getenv("GENERATE_CLOUD_HASH") && (ret==0)) {
         /* compute SHA512 */
         char *fname = bstrdup(xfer->m_cache_fname);
         FILE *file=bfopen(fname, "rb");
         if (!file) {
            berrno be;
            Mmsg2(xfer->m_message, "Could not open output file %s. ERR=%s\n",
                  fname, be.bstrerror());
            free(fname);
            return CLOUD_DRIVER_COPY_PART_TO_CACHE_ERROR;
         }
         POOLMEM *res = get_memory(block_size+1);
         ssize_t size = block_size;
         while( size == block_size) {
            size = fread(res, 1, block_size, file);
            SHA512_Update(&sha512, res, size);
         }
         free_pool_memory(res);
         if (fclose(file) != 0) {
            berrno be;
            Mmsg2(xfer->m_message, "Could not close output file %s. ERR=%s\n",
                  fname, be.bstrerror());
            free(fname);
            return CLOUD_DRIVER_COPY_PART_TO_CACHE_ERROR;
         }
         free(fname);
         SHA512_Final(xfer->m_hash64, &sha512);
      } else {
         bmemzero(xfer->m_hash64, 64);
      }

      if (ret==RETRY_DOWNLOAD) {
         /* the python driver will write the restoration bucket name into the cache file */
         /* retrieve the restore bucket name */
         char *fname = bstrdup(xfer->m_cache_fname);
         FILE *file=bfopen(fname, "rb");
         if (!file) {
            berrno be;
            Mmsg2(xfer->m_message, "Could not open output file %s. ERR=%s\n",
                  fname, be.bstrerror());
            free(fname);
            return CLOUD_DRIVER_COPY_PART_TO_CACHE_ERROR;
         }

         fseek(file, 0L, SEEK_END);
         ssize_t size = ftell(file);
         fseek(file, 0L, SEEK_SET);

         /*Azure Container names must be from 3 through 63 characters long.*/
         if (size < MIN_RESTORE_CONTAINER_NAME_SIZE ||
             size > MAX_RESTORE_CONTAINER_NAME_SIZE) {
            /* if we're out-of-bound, something is wrong: let's abort */
            Mmsg1(xfer->m_message, "restore bucket name length %d is out of range. Aborting.\n", size);
            free(fname);
            return CLOUD_DRIVER_COPY_PART_TO_CACHE_ERROR;
         } else {
            char restore_bucket_name[MAX_RESTORE_CONTAINER_NAME_SIZE+1];
            ssize_t ret = fread(restore_bucket_name, 1, size, file);
            if (ret != size) {
               berrno be;
               Mmsg2(xfer->m_message, "Could not close output file %s. ERR=%s\n",
                     fname, be.bstrerror());
               free(fname);
               return CLOUD_DRIVER_COPY_PART_TO_CACHE_ERROR;
            }
            restore_bucket_name[size] = '\0';
            if (fclose(file) != 0) {
               berrno be;
               Mmsg2(xfer->m_message, "Could not close output file %s. ERR=%s\n",
                     fname, be.bstrerror());
               free(fname);
               return CLOUD_DRIVER_COPY_PART_TO_CACHE_ERROR;
            }
            free(fname);
            xfer->set_restore_bucket(restore_bucket_name);
            return CLOUD_DRIVER_COPY_PART_TO_CACHE_RETRY;
         }
      }

      return (ret==0) ? CLOUD_DRIVER_COPY_PART_TO_CACHE_OK:CLOUD_DRIVER_COPY_PART_TO_CACHE_ERROR;
   }
   return CLOUD_DRIVER_COPY_PART_TO_CACHE_ERROR;
}

bool generic_driver::restore_cloud_object(transfer *xfer, const char *cloud_fname)
{
   (void) (xfer);
   (void) (cloud_fname);
   /* not implemented */
   return false;
}

struct is_waiting_on_server_read_cb_arg {
   POOLMEM *msg;
};

size_t is_waiting_on_server_read_cb(char *res, size_t size, void* arg)
{
   is_waiting_on_server_read_cb_arg*_arg = (is_waiting_on_server_read_cb_arg*)arg;
   if (_arg) {
      Mmsg(_arg->msg, "%s", res);
   }
   return size;
}

bool generic_driver::is_waiting_on_server(transfer *xfer)
{
   Dmsg2(dbglvl, "generic_driver::is_waiting_on_server for %spart%d.\n", xfer->m_volume_name, xfer->m_part);
   /* only Amazon and Azure support restoration */
   if (strstr(driver_command, "aws_cloud_driver") != NULL ||
       strstr(driver_command, "was_cloud_driver") != NULL) {
      Dmsg0(dbglvl, "call_fct wait_on_restore\n");

      if (xfer) {
         /* Azure restores on a different bucket, so the restore bucket is the one to be checked */
         /* swap bucket to restore or swap it back to the original */
         if (strlen(xfer->m_restore_bucket) != 0) {
            pm_strcpy(bucket_env, "CLOUD_BUCKET=");
            pm_strcat(bucket_env, NPRTB(xfer->m_restore_bucket));
            envs[1] = bucket_env;
         } else {
            pm_strcpy(bucket_env, "CLOUD_BUCKET=");
            pm_strcat(bucket_env, NPRTB(bucket_name));
            envs[1] = bucket_env;
         }
      }

      POOLMEM *output = get_memory(block_size+1);
      read_callback rcb;
      rcb.fct = &is_waiting_on_server_read_cb;
      is_waiting_on_server_read_cb_arg arg;
      arg.msg = output;
      rcb.arg = (void*)&arg;    

      int ret = call_fct("wait_on_restore",xfer->m_volume_name, xfer->m_part, &rcb, NULL, NULL, xfer->m_message, NULL);
      /* We report xfer->m_message here once */
      Dmsg3(dbglvl, "wait_on_restore returns %d. output=%s error=%s\n", ret, output, xfer->m_message);
      bool in_progress = (strcmp(output, "WOR-INPROGRESS") == 0);
      free_pool_memory(output);
      return (in_progress);
   }
   return false;
}

/* no truncate_cloud_volume_read_cb */
bool generic_driver::truncate_cloud_volume(const char *volume_name, ilist *trunc_parts, cancel_callback *cancel_cb, POOLMEM *&err) {

   if (!volume_name) {
      Mmsg(err, _("truncate_cloud_volume. No Volume name provided.\n"));
      return false;
   }
   if (!trunc_parts) {
      Mmsg(err, _("truncate_cloud_volume. No trunc_parts list provided.\n"));
      return false;
   }

   int rtn=0;
   int i;
   for (i=1; (i <= (int)trunc_parts->last_index()); i++) {
      if (!trunc_parts->get(i)) {
         continue;
      }
      rtn |= call_fct("delete", volume_name, i, NULL, NULL, cancel_cb, err);
      if ((cancel_cb && cancel_cb->fct && cancel_cb->fct(cancel_cb->arg))) {
         Mmsg(err, _("truncate_cloud_volume. cancelled by job.\n"));
         return false;
      }
   }
   return (rtn==0);
}


struct clean_cloud_volume_read_cb_arg {
   POOLMEM **remain;
   alist *parts;
   cleanup_cb_type *cb;
   cleanup_ctx_type *ctx;
};

size_t clean_cloud_volume_read_cb(char *res, size_t sz, void* arg)
{
   alist *parts = NULL;
   bool wrong_string = false;
   size_t left = sz;
   POOLMEM **remain = NULL;
   clean_cloud_volume_read_cb_arg *_arg = (clean_cloud_volume_read_cb_arg*)arg;
   if (_arg) {
      parts = _arg->parts;
      remain = _arg->remain;
   }

   if (parts) {
      char * pch = strtok (res,"\n");
      /* we enter the cb again and remaining string has not been processed */
      if (remain && strlen(*remain) != 0) {
         pm_strcat(remain, pch);
         char *name=strstr(*remain, "part");
         char *time=strstr(*remain, ",mtime:");
         if (name && time) {
            *time='\0';
            parts->append(bstrdup(name));
         } else {
            wrong_string = true;
         }
         **remain = 0;
         left -= strlen(pch)+1;
         pch = strtok (NULL,"\n");
      }
      while (!wrong_string && pch && (left > 0))
      {
         char *name=strstr(pch, "part");
         char *time=strstr(pch, ",mtime:");
         if (name && time) {
            *time='\0';
            parts->append(bstrdup(name));
         } else {
            pm_strcpy(remain, pch);
         }
         left -= strlen(pch)+1;
         pch = strtok (NULL, "\n");
      }
      return wrong_string ? 0:sz;
   }

   return 0;
}

bool generic_driver::clean_cloud_volume(const char *VolumeName, cleanup_cb_type *cb, cleanup_ctx_type *ctx, cancel_callback *cancel_cb, POOLMEM *&err)
{
   if (!VolumeName) {
      Mmsg(err, _("clean_cloud_volume. No Volume name provided.\n"));
      return false;
   }

   alist parts;

   clean_cloud_volume_read_cb_arg arg;
   arg.parts = &parts;
   POOLMEM *p= get_memory(block_size);
   *p = 0;
   arg.remain = &p;
   arg.cb = cb;
   arg.ctx = ctx;
   read_callback pcb;
   pcb.fct = &clean_cloud_volume_read_cb;
   pcb.arg = (void*)&arg;
   /* list everything in the volume VolumeName */
   int ret = call_fct("ls", VolumeName, "", &pcb, NULL, cancel_cb, err);
   free_pool_memory(*arg.remain);
   /* 1 is the generic return code for path not found, except for was driver that always return 0 */
   if (ret == 1) {
      err = strip_trailing_junk(err);
      pm_strcat(err, " Cloud volume ");
      pm_strcat(err, VolumeName);
      pm_strcat(err, " not found.\n");
      /* the volume could not be found in the cloud: it's possible to list a Volume on cloud that doesn't exist. 
         Accept it as an OK behavior */
      return true;
   }

   int rtn=0;
   char *part = NULL;
   foreach_alist(part, &parts) {
      int r = call_fct("delete", VolumeName, part, NULL, NULL, cancel_cb, err);
      if (r == 0) {
         Dmsg2(dbglvl, "clean_cloud_volume for %s: Unlink file %s.\n", VolumeName, part);
      } else {
         Dmsg4(dbglvl, "clean_cloud_volume delete %s/%s returns %d. err=%s\n", VolumeName, part, r, err);
      }
      rtn |= r;
      if ((cancel_cb && cancel_cb->fct && cancel_cb->fct(cancel_cb->arg))) {
         Mmsg(err, _("clean_cloud_volume. cancelled by job.\n"));
         return false;
      }
   }

   return (ret|rtn == 0);
}

struct get_cloud_volume_parts_list_read_cb_arg {
   POOLMEM **remain;
   ilist *parts;
};


size_t get_cloud_volume_parts_list_read_cb(char *res, size_t sz, void* arg)
{
   ilist *parts = NULL;
   bool wrong_string = false;
   size_t left = sz;
   POOLMEM **remain = NULL;
   get_cloud_volume_parts_list_read_cb_arg *_arg = (get_cloud_volume_parts_list_read_cb_arg*)arg;
   if (_arg) {
      parts = _arg->parts;
      remain = _arg->remain;
   }

   if (parts) {
      char * pch = strtok (res,"\n");
      /* we enter the cb again and remaining string has not been processed */
      if (remain && *remain && strlen(*remain) != 0) {
         pm_strcat(remain, pch);
         char *ext=strstr(*remain, "part."), *size=strstr(*remain, "size:"), *mtime=strstr(*remain, "mtime:");
         if (ext && size && mtime) {
            cloud_part *part = (cloud_part*) malloc(sizeof(cloud_part));
            part->index = str_to_uint64(&(ext[5]));
            part->mtime = str_to_uint64(&(mtime[6]));
            part->size = str_to_uint64((&(size[5])));
            /* ***FIXME*** : wbn to retrieve SHA512 from cloud */
            bmemzero(part->hash64, 64);
            parts->put(part->index, part);
         } else {
            wrong_string = true;
         }
         **remain = 0;
         left -= strlen(pch)+1;
         pch = strtok (NULL,"\n");
      }
      while (!wrong_string && pch && (left > 0))
      {
         char *ext=strstr(pch, "part."), *size=strstr(pch, "size:"), *mtime=strstr(pch, "mtime:");
         if (ext && size && mtime) {
            cloud_part *part = (cloud_part*) malloc(sizeof(cloud_part));
            part->index = str_to_uint64(&(ext[5]));
            part->mtime = str_to_uint64(&(mtime[6]));
            part->size = str_to_uint64((&(size[5])));
            /* ***FIXME*** : wbn to retrieve SHA512 from cloud */
            bmemzero(part->hash64, 64);
            parts->put(part->index, part);
         } else {
            pm_strcpy(remain, pch);
         }
         left -= strlen(pch)+1;
         pch = strtok (NULL, "\n");
      }
      return wrong_string ? 0:sz;
   }

   return 0;
}
bool generic_driver::get_cloud_volume_parts_list(const char* volume_name, ilist *parts, cancel_callback *cancel_cb, POOLMEM *&err)
{
   if (!volume_name) {
      Mmsg(err, _("get_cloud_volume_parts_list. No Volume name provided.\n"));
      return false;
   }
   if (!parts) {
      Mmsg(err, _("get_cloud_volume_parts_list. No parts list provided.\n"));
      return false;
   }

   get_cloud_volume_parts_list_read_cb_arg arg;
   arg.parts = parts;
   POOLMEM *p= get_memory(block_size);
   *p = 0;
   arg.remain = &p;
   read_callback pcb;
   pcb.fct = &get_cloud_volume_parts_list_read_cb;
   pcb.arg = (void*)&arg;
   int ret = call_fct("ls", volume_name, "part.", &pcb, NULL, cancel_cb, err);
   free_pool_memory(*arg.remain);
   /* 1 is the generic return code for path not found, except for was driver that always return 0 */
   if (ret == 1) {
      err = strip_trailing_junk(err);
      pm_strcat(err, " Cloud volume ");
      pm_strcat(err, volume_name);
      pm_strcat(err, " not found.\n");
      /* the volume could not be found in the cloud: it's possible to list a Volume on cloud that doesn't exist. 
         Accept it as an OK behavior */
      return true;
   }
   return (ret == 0);
}

struct get_cloud_volume_list_read_cb_arg {
   POOLMEM **remain;
   alist *volumes;
};

void get_cloud_volumes_list_read_cb_append_to_volumes(char* c, alist* volumes) {
   if (volumes) {
      c = strip_trailing_slashes(c);
      char * vol = strrchr(c, '/');
      if (vol) {
         vol++;
         volumes->append(bstrdup(vol));
      } else {
         volumes->append(bstrdup(c));
      }
   }
}

size_t get_cloud_volumes_list_read_cb(char* res, size_t size, void *arg)
{
   alist *volumes = NULL;
   POOLMEM **remain = NULL;
   bool is_complete(res[size-1]=='\n');
   get_cloud_volume_list_read_cb_arg *_arg = (get_cloud_volume_list_read_cb_arg*)arg;
   if (_arg) {
      volumes = _arg->volumes;
      remain = _arg->remain;
   }

   if (volumes) {
      /* do the actual process */
      char * pch = strtok (res,"\n");
      if (remain && *remain && strlen(*remain) != 0) {
         pm_strcat(remain, pch);
         get_cloud_volumes_list_read_cb_append_to_volumes(*remain, volumes);
         pch = strtok (NULL, "\n");
         *remain = 0;
      }

      while (pch != NULL)
      {
         pm_strcpy(*remain, pch);
         pch = strtok (NULL, "\n");
         if (pch || is_complete) {
            get_cloud_volumes_list_read_cb_append_to_volumes(*remain, volumes);
         }
      }
      return size;
   }
   return 0;
}
bool generic_driver::get_cloud_volumes_list(alist *volumes, cancel_callback *cancel_cb, POOLMEM *&err)
{
   if (!volumes) {
      Mmsg(err, _("get_cloud_volumes_list. No volumes list provided.\n"));
      return false;
   }

   read_callback pcb;
   pcb.fct = &get_cloud_volumes_list_read_cb;
   get_cloud_volume_list_read_cb_arg arg;
   arg.volumes = volumes;
   POOLMEM *p= get_memory(block_size);
   *p = 0;
   arg.remain = &p;
   pcb.arg = (void*)&arg;
   int ret = call_fct("vol_ls", NULL, 0, &pcb, NULL, cancel_cb, err);
   free_pool_memory(*arg.remain);
   return (ret == 0);
}


//=================================================
#ifdef TEST_PROGRAM
int main (int argc, char *argv[])
{
   pthread_attr_t attr;

   void * start_heap = sbrk(0);
   (void)start_heap;

   setlocale(LC_ALL, "");
   bindtextdomain("bacula", LOCALEDIR);
   textdomain("bacula");
   init_stack_dump();
   my_name_is(argc, argv, "generic_driver_test");
   init_msg(NULL, NULL);
   daemon_start_time = time(NULL);
   set_thread_concurrency(150);
   lmgr_init_thread(); /* initialize the lockmanager stack */
   pthread_attr_init(&attr);

   {
      /* init a stub cloud */
      // CLOUD cloud = {
      // RES(),_("host_name"),_("norbtestbucket"),_("access_key"),_("secret_key"),
      // NULL, NULL, NULL, NULL, NULL, NULL,
      // 0, 0, 0, 0, 0, 0, 0, 0, 0, _("generic_cloud_driver")}; /*!! Change script location !!*/
      // CLOUD cloud = {
      // RES(),_("host_name"),_("norbtestbucket"),_("access_key"),_("secret_key"),
      // NULL, NULL, NULL, NULL, NULL, NULL,
      // 0, 0, 0, 0, 0, 0, 0, 0, 0, _("google_cloud_driver")}; /*!! Change script location !!*/
      CLOUD cloud = {
      RES(),_("host_name"),_("norbtestbucket"),_("access_key"),_("secret_key"),
      NULL, NULL, NULL, NULL, NULL, NULL,
      0, 0, 0, 0, 0, 0, 0, 0, 0, _("oracle_cloud_driver")}; /*!! Change script location !!*/
      /* init a driver with stub cloud */
      generic_driver dev;
      POOLMEM *err=get_pool_memory(PM_MESSAGE);
      memset(err, 0, sizeof_pool_memory(err));
      POOLMEM *tmp=get_pool_memory(PM_FNAME);
      memset(tmp, 0, sizeof_pool_memory(tmp));
      printf("init\n");
      bool res = dev.init(&cloud, err);
      if (err[0]) printf(err);
      ASSERT(res);

      /* retrieve all volumes for this cloud*/
      alist volumes;
      cancel_callback ccb;
      printf("get_cloud_volumes_list\n");
      res = dev.get_cloud_volumes_list(&volumes, &ccb, err);
      if (err[0]) printf(err);
      ASSERT(res);

      /* list all parts for each volume */
      char *vol;
      foreach_alist(vol, &volumes) {
         ilist parts;
         printf("get_cloud_volume_parts_list for volume:%s\n", vol);
         res = dev.get_cloud_volume_parts_list(vol, &parts, &ccb, err);
         if (err[0]) printf(err);
         ASSERT(res);

         /* for each found part, transfer it to /tmp cache */
         alist cache_parts;
         int max_size = parts.last_index();
         for(int index=0; index<=max_size; index++ ) {
            if (cloud_part *p = (cloud_part *)parts.get(index)) {
               /* create a stub transfer */
               Mmsg(tmp, "/tmp/%s/part.%d", vol, p->index);
               transfer xfer(p->size, NULL, tmp, vol, p->index, NULL, NULL, NULL);
               printf("copy_cloud_part_to_cache to %s\n", tmp);
               res = dev.copy_cloud_part_to_cache(&xfer);
               if (xfer.m_message[0]) printf(xfer.m_message);
               ASSERT(res);
               struct stat mstatp;
               res = (lstat(xfer.m_cache_fname, &mstatp) == 0);
               ASSERT(res);
               ASSERT((uint64_t)mstatp.st_size == p->size);
               /* save for later*/
               cache_parts.append(bstrdup(tmp));
            }
         }

         /* purge all the parts in the current volume */
         printf("truncate_cloud_volume %s\n", vol);
         res = dev.truncate_cloud_volume(vol, &parts, &ccb, err);
         if (err[0]) printf(err);
         ASSERT(res);

         /* now push all the cache files back into the cloud */
         char *c;
         foreach_alist(c, &cache_parts) {
            struct stat mstatp;
            res = (lstat(c, &mstatp) == 0);
            ASSERT(res);
            const char *ext=strstr(c, "part.");
            transfer xfer(mstatp.st_size, NULL, c, vol, atoi(&(ext[5])), NULL, NULL, NULL);
            printf("copy_cache_part_to_cloud %s\n", c);
            res = dev.copy_cache_part_to_cloud(&xfer);
            if (xfer.m_message[0]) printf(xfer.m_message);
            ASSERT(res);
         }
      }

      free_pool_memory(err);
      free_pool_memory(tmp);
   }

   printf("SUCCESS");
   return 0;

}

#endif /* TEST_PROGRAM */
