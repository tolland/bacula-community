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

#ifndef BEE_MTAB_ENTRY_H
#define BEE_MTAB_ENTRY_H

#include "winapi.h"

#define bwcsdup(str) wcscpy((WCHAR *)bmalloc((wcslen(str)+1)*sizeof(WCHAR)),(str))
#define safe_bfree(buf) if (buf) { bfree(buf); buf = NULL; }

/* The MTabEntry class is representing a mounted volume,
 * it associates a volume name with mount paths and a device name
 */
class MTabEntry: public SMARTALLOC {
public:
   WCHAR *volumeName;       // The uuid of the volume
                            // "\\?\Volume{086fc09e-7781-4076-9b2e-a4ff5c6b52c7}\"
   WCHAR *mountPaths;       // list of mount point (if any) separated by zero
                            // "C:\\\0C:\\mnt\\\0\0"
   WCHAR *deviceName;       // \\Device\\HarddiskVolume2
   WCHAR *shadowCopyName;   // \\?\GLOBALROOT\Device\HarddiskVolumeShadowCopy216
   bool   in_SnapshotSet;
   bool   can_Snapshot;
   UINT   driveType;
   rblink link;

   MTabEntry() {
      mountPaths = NULL;
      volumeName = NULL;
      deviceName = NULL;
      in_SnapshotSet = false;
      shadowCopyName = NULL;
      can_Snapshot = false;
      driveType = 0;
   };

   MTabEntry(WCHAR *DeviceName, WCHAR *VolumeName) {
      int last = wcslen(VolumeName);
      if (VolumeName[last - 1] == L'\\') {
         volumeName = bwcsdup(VolumeName);
      } else {                         /* \\ + \0 */
         volumeName = (WCHAR *)bmalloc((last+1)*sizeof(WCHAR));
         wcscpy(volumeName, VolumeName);
         volumeName[last] = L'\\';
         volumeName[last+1] = L'\0';
      }
      mountPaths = NULL;
      in_SnapshotSet = false;
      deviceName = bwcsdup(DeviceName);
      shadowCopyName = NULL;
      driveType = 0;
      can_Snapshot = false;
      get_paths();
   };

   ~MTabEntry() {
      destroy();
   };

   void destroy() {
      safe_bfree(mountPaths);
      safe_bfree(volumeName);
      safe_bfree(deviceName);
      safe_bfree(shadowCopyName);
   };

   /* Return  the drive type (cdrom, fixed, network, ...) */
   UINT getDriveType();

   /* Check if the current volume can be snapshoted and initialize "can_Snapshot" */
   bool initIsSuitableForSnapshot();

   /* Return true if the current volume can be snapshoted (ie not CDROM or fat32) */
   bool isSuitableForSnapshot();

   void setInSnapshotSet() {
      Dmsg1(050, "Marking %ls for the SnapshotSet\n", mountPaths);
      in_SnapshotSet = true;
   }

   void debug_paths() {
      WCHAR *p;
      /*  Display the paths in the list. */
      if (mountPaths != NULL) {
         Dmsg2(DT_VOLUME|10, "Device: [%ls], Volume: [%ls]\n", deviceName, volumeName);
         for ( p = mountPaths;  p[0] != L'\0'; p += wcslen(p) + 1) {
            Dmsg1(DT_VOLUME|10, "  %ls\n", p);
         }
      }
   };

   /* Compute the path list associated with the current volume */
   bool get_paths() {
      DWORD  count = MAX_PATH + 1;
      bool   ret = false;

      for (;;) {
         //  Allocate a buffer to hold the paths.
         mountPaths = (WCHAR*) malloc(count * sizeof(WCHAR));

         //  Obtain all of the paths
         //  for this volume.
         ret = GetVolumePathNamesForVolumeNameW(volumeName, mountPaths, 
                                                count, &count);
         if (ret) {
            break;
         }

         if (GetLastError() != ERROR_MORE_DATA) {
            break;
         }

         //  Try again with the
         //  new suggested size.
         free(mountPaths);
         mountPaths = NULL;
      }
      debug_paths();
      return ret;
   };

   /* Return the first mount point */
   WCHAR *first() {
      return mountPaths;
   };

   /* Return the next mount point */
   WCHAR *next(WCHAR *prev) {
      if (prev == NULL || prev[0] == L'\0') {
         return NULL;
      }

      prev += wcslen(prev) + 1;

      return (prev[0] == L'\0') ? NULL : prev;
   };
};

/* Class to handle all volumes of the system, it contains
 * a list of all current volumes (MTabEntry)
 */
class MTab: public SMARTALLOC {
public:
   DWORD       lasterror;
   const char *lasterror_str;
   rblist     *entries;         /* MTabEntry */
   int         nb_in_SnapshotSet;
   int         nb_in_SuitableForSnapshot;

   MTab();
   ~MTab(); 

   /* Get a Volume by name */
   MTabEntry *search(char *file);

   /* Try to add a volume to the current snapshotset */
   bool addInSnapshotSet(char *file);

   /* Fill the "entries" list will all detected volumes of the system */
   bool load_volumes();

   /* Dump for debugging at the given debug level */
   void dump(const char* file, int lineno, int level, const char* prefix);

};

#endif /* BEE_MTAB_ENTRY_H */
