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
 * Inspired by vtape.h
 */

#ifndef __FILE_DEV_
#define __FILE_DEV_

class file_dev : public DEVICE {
private:
   bool modify_fattr(const char *vol_name, int attr, bool set, POOLMEM **error);
   bool check_for_attr(const char *vol_name, int attr);
   bool set_fattr(const char *vol_name, int attr, POOLMEM **error);
   bool clear_fattr(const char *vol_name, int attr, POOLMEM **error);
   bool append_open_needed(const char *vol_name);
   bool is_attribute_supported(int attr);

protected:
   virtual void get_volume_fpath(const char *vol_name, POOLMEM **buf);

public:

   file_dev() { };
   ~file_dev() { m_fd = -1; };
   bool is_eod_valid(DCR *dcr);
   bool eod(DCR *dcr);
   bool open_device(DCR *dcr, int omode);
   const char *print_type();
   virtual int device_specific_init(JCR *jcr, DEVRES *device);
   bool set_append_only(const char *vol_name, POOLMEM **error);
   bool clear_append_only(const char *vol_name, POOLMEM **error);
   bool set_immutable(const char *vol_name, POOLMEM **error);
   bool clear_immutable(const char *vol_name, POOLMEM **error);
   bool check_volume_protection_time(const char *vol_name);
   bool check_for_immutable(const char *vol_name);
   bool check_for_read_only(int fd, const char *vol_name);
   bool get_os_device_freespace();
   bool is_fs_nearly_full(uint64_t threshold);
   int set_writable(int fd, const char *vol_name, POOLMEM **error);
   int set_readonly(int fd, const char *vol_name, POOLMEM **error);
   int set_atime(int fd, const char *vol_name, btime_t val, POOLMEM **error);
   int use_protect();
};

#endif /* __FILE_DEV_ */
