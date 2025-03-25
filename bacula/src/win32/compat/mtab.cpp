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

#include "bacula.h"
#include "compat.h"
#include "mtab.h"
#include "ms_atl.h"
#include <objbase.h>
#undef setlocale
#include <string>
using namespace std;

#define dbglvl_snap DT_VOLUME|50

/* empty namespace to avoid collisions */
namespace {
    // Append a backslash to the current string
    wstring AppendBackslash(wstring str)
    {
        if (str.length() == 0) {
            return wstring(L"\\");
        }
        if (str[str.length() - 1] == L'\\') {
            return str;
        }
        return str.append(L"\\");
    }

    // Get the unique volume name for the given path
    wstring GetUniqueVolumeNameForPath(wstring path, wstring &rootPath)
    {
        if (path.length() <= 0) {
        //Dmsg0(50, "Failed path.len <= 0\n");
        return L"";
        }

        // Add the backslash termination, if needed
        path = AppendBackslash(path);
        //Dmsg1(50, "Path=%ls\n", path.c_str());

        // Get the root path of the volume
        wchar_t volumeRootPath[MAX_PATH];
        wchar_t volumeName[MAX_PATH];
        wchar_t volumeUniqueName[MAX_PATH];

        volumeRootPath[0] = 0;
        volumeName[0] = 0;
        volumeUniqueName[0] = 0;

        if (!p_GetVolumePathNameW || !p_GetVolumePathNameW((LPCWSTR)path.c_str(), volumeRootPath, MAX_PATH)) {
        Dmsg1(50, "Failed GetVolumePathNameW path=%ls\n", path.c_str());
        return L"";
        }
        rootPath.assign(volumeRootPath);
        Dmsg1(dbglvl_snap, "VolumeRootPath=%ls\n", volumeRootPath);

        // Get the volume name alias (might be different from the unique volume name in rare cases)
        if (!p_GetVolumeNameForVolumeMountPointW || !p_GetVolumeNameForVolumeMountPointW(volumeRootPath, volumeName, MAX_PATH)) {
        Dmsg1(50, "Failed GetVolumeNameForVolumeMountPointW path=%ls\n", volumeRootPath);
        return L"";
        }
        Dmsg1(dbglvl_snap, "VolumeName=%ls\n", volumeName);

        // Get the unique volume name
        if (!p_GetVolumeNameForVolumeMountPointW(volumeName, volumeUniqueName, MAX_PATH)) {
        Dmsg1(50, "Failed GetVolumeNameForVolumeMountPointW path=%ls\n", volumeName);
        return L"";
        }
        Dmsg1(dbglvl_snap, "VolumeUniqueName=%ls\n", volumeUniqueName);
        return volumeUniqueName;
    }

    int volume_search(void *i1, void *i2)
    {
        wstring   *volname = (wstring *) i1;
        MTabEntry *vol = (MTabEntry *) i2;

        return volname->compare(vol->volumeName);
    }

    int volume_cmp(void *e1, void *e2)
    {
        MTabEntry *v1 = (MTabEntry *) e1;
        MTabEntry *v2 = (MTabEntry *) e2;
        return wcscmp(v1->volumeName, v2->volumeName);
    }
}

MTab::MTab() {
    MTabEntry *elt = NULL;
    lasterror = ERROR_SUCCESS;
    lasterror_str = "";
    nb_in_SnapshotSet = 0;
    nb_in_SuitableForSnapshot = 0;
    entries = New(rblist(elt, &elt->link));
}

MTab::~MTab() {
   if (entries) {
      MTabEntry *elt;
      foreach_rblist(elt, entries) {
         elt->destroy();
      }
      delete entries;
   }
}

/* we are looking for DRIVE_FIXED drive */
UINT MTabEntry::getDriveType()
{
   WCHAR *root = first();

   // Make sure to discard CD-ROM and network drives
   if (!root) {
      return DRIVE_UNKNOWN; /* zero */
   }

   driveType = GetDriveTypeW(root);
   return driveType;
}

/* Return true if the current volume can be snapshoted:
 * if it is a FIXED disk with a Filesystem that support snapshot
 * (ie not CDROM or fat32)
 */
bool MTabEntry::isSuitableForSnapshot()
{
   return can_Snapshot;
}

bool MTabEntry::initIsSuitableForSnapshot()
{
   DWORD componentlength, fsflags;
   WCHAR fstype[50];
   WCHAR *root = first();
   UINT oldmode;
   BOOL result;

   // Make sure to discard CD-ROM and network drives
   if (!root) {
      Dmsg1(dbglvl_snap, "No mount point for %ls\n", volumeName);
      goto bail_out;
   }

   if (getDriveType() != DRIVE_FIXED) {
      Dmsg2(dbglvl_snap, "Invalid disk type %d for %ls\n", driveType, root);
      goto bail_out;
   }

   /* From fstype.c, except that we have WCHAR instead of char */
   /* We don't want any popups if there isn't any media in the drive */
   oldmode = SetErrorMode(SEM_FAILCRITICALERRORS);
   result = GetVolumeInformationW(root, NULL, 0, NULL,
                                  &componentlength, &fsflags, fstype, ARRAYSIZE(fstype));
   SetErrorMode(oldmode);

   if (result) {
      /* Windows returns NTFS, FAT, etc.  Make it lowercase to be consistent with other OSes */
      Dmsg1(dbglvl_snap, "fstype=%ls\n", fstype);
      if (!_wcsicmp(fstype, L"ntfs")) {
         can_Snapshot = true;
      }
      if (!_wcsicmp(fstype, L"refs")) {
         can_Snapshot = true;
      }
      if (!_wcsicmp(fstype, L"csvfs")) {
         can_Snapshot = true;
      }
   }
bail_out:
   Dmsg2(dbglvl_snap, "%ls is %s suitable for VSS snapshot\n", root, can_Snapshot?"":"not");
   return can_Snapshot;
}

/* Find a volume for a specific path
 *  "C:\\" or "c:\\mnt\\" -> \\?\Volume{086fc09e-7781-4076-9b2e-a4ff5c6b52c7}\
 */
MTabEntry *MTab::search(char *p)
{
   wstring volume;
   wstring path;
   wstring rootPath;

   POOLMEM* pwszBuf = get_pool_memory(PM_FNAME);
   wutf8_path_2_wchar(&pwszBuf, p);
   path.assign((wchar_t *)pwszBuf);
   volume = GetUniqueVolumeNameForPath(path, rootPath);

   MTabEntry *elt = (MTabEntry *)entries->search(&volume, volume_search);
   free_pool_memory(pwszBuf);

   if (!elt) {
      Dmsg1(000, "Unable to find %ls in volume list\n", volume.c_str());
   }

   return elt;
}

bool MTab::addInSnapshotSet(char *p)
{
   MTabEntry *elt = search(p);
   if (elt) {
      if (!elt->in_SnapshotSet && elt->isSuitableForSnapshot()) {
         nb_in_SnapshotSet++;
         elt->setInSnapshotSet();
      }
   }
   return nb_in_SnapshotSet == nb_in_SuitableForSnapshot;
}
void MTab::dump(const char* file, int lineno, int lvl, const char *prefix)
{
   MTabEntry *elt;
   WCHAR mountPaths[MAX_PATH];
   if (chk_dbglvl(lvl)) d_msg(file, lineno, lvl, "%s suitable for snapshot %d/%d/%d\n", prefix, nb_in_SnapshotSet, nb_in_SuitableForSnapshot, entries->size());
   foreach_rblist(elt, entries) {
      // replace intermediat L'\0' in mountPaths by '+' for display
      mountPaths[0]=L'\0';
      if (elt->mountPaths != NULL) {
         WCHAR *s = elt->mountPaths;
         WCHAR *d = mountPaths;
         while (*s != L'\0' && d-mountPaths<(int)sizeof(mountPaths)) {
            *d++ = *s++;
            if (*s == L'\0' && s[1] != L'\0') {
               *d++ = L'+';
               s++;
            }
         }
         *d = L'\0';
      }
      if (chk_dbglvl(lvl)) d_msg(file, lineno, lvl, "%s Vol=%ls dev=%ls mounts=%ls snap=%ls active=%c\n", prefix, elt->volumeName, elt->deviceName, mountPaths, elt->shadowCopyName, elt->in_SnapshotSet?'Y':'N');
   }
}

/* Load the list volumes that exists on the Windows host
 * with tuples like :
 *    VolumeName='\\\\?\\Volume{086fc09e-7781-4076-9b2e-a4ff5c6b52c7}\\'
 *    DeviceName='\\Device\\HarddiskVolume2'
 */
bool MTab::load_volumes()
{
   DWORD  count                = 0;
   WCHAR  DeviceName[MAX_PATH] = L"";
   HANDLE FindHandle           = INVALID_HANDLE_VALUE;
   size_t Index                = 0;
   bool   Success              = FALSE;
   WCHAR  VolumeName[MAX_PATH] = L"";

   Dmsg0(dbglvl_snap, "Filling MTAB\n");


   //  Enumerate all volumes in the system.
   FindHandle = FindFirstVolumeW(VolumeName, ARRAYSIZE(VolumeName));

   if (FindHandle == INVALID_HANDLE_VALUE) {
      lasterror = GetLastError();
      return false;
   }

   for (;;) {
      //  Skip the \\?\ prefix and remove the trailing backslash.
      Index = wcslen(VolumeName) - 1;

      if (VolumeName[0]     != L'\\' ||
          VolumeName[1]     != L'\\' ||
          VolumeName[2]     != L'?'  ||
          VolumeName[3]     != L'\\' ||
          VolumeName[Index] != L'\\')
      {
         lasterror = ERROR_BAD_PATHNAME;
         lasterror_str = "FindFirstVolumeW/FindNextVolumeW returned a bad path";
         Dmsg1(000, "FindFirstVolumeW/FindNextVolumeW returned a bad path %ls\n", VolumeName);
         break;
      }

      //
      //  QueryDosDeviceW does not allow a trailing backslash,
      //  so temporarily remove it.
      VolumeName[Index] = L'\0';

      count = QueryDosDeviceW(&VolumeName[4], DeviceName,
                              ARRAYSIZE(DeviceName));

      VolumeName[Index] = L'\\';

      if (count == 0) {
         lasterror = GetLastError();
         Dmsg1(000, "QueryDosDeviceW failed with error code %d\n", lasterror);
         break;
      }

      // VolumeName='\\\\?\\Volume{086fc09e-7781-4076-9b2e-a4ff5c6b52c7}\\'
      // DeviceName='\\Device\\HarddiskVolume2'
      MTabEntry *entry = New(MTabEntry(DeviceName, VolumeName));
      if (entry->initIsSuitableForSnapshot()) {
         nb_in_SuitableForSnapshot++;
      }
      entries->insert(entry, volume_cmp);
      //
      //  Move on to the next volume.
      Success = FindNextVolumeW(FindHandle, VolumeName, ARRAYSIZE(VolumeName));

      if (!Success) {
         lasterror = GetLastError();
         if (lasterror != ERROR_NO_MORE_FILES) {
            Dmsg1(000, "FindNextVolumeW failed with error code %d\n", lasterror);
            break;
         }

         //  Finished iterating
         //  through all the volumes.
         lasterror = ERROR_SUCCESS;
         break;
      }
   }

   FindVolumeClose(FindHandle);
   FindHandle = INVALID_HANDLE_VALUE;

   return true;
}
