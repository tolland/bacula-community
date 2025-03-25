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
// vss.cpp -- Interface to Volume Shadow Copies (VSS)
//
// Copyright transferred from MATRIX-Computer GmbH to
//   Kern Sibbald by express permission.
//
// Author          : Thorsten Engel
// Created On      : Fri May 06 21:44:00 2005


#ifdef WIN32_VSS
#include "bacula.h"
#include "compat.h"
#include "ms_atl.h"
#include <objbase.h>
#undef setlocale
#include <string>
using namespace std;

#include "vss.h"

#define dbglvl_snap DT_VOLUME|50

wstring GetUniqueVolumeNameForPath(wstring path, wstring &rootPath);

static int volume_search(void *i1, void *i2)
{
   wstring   *volname = (wstring *) i1;
   MTabEntry *vol = (MTabEntry *) i2;

   return volname->compare(vol->volumeName);
}

BOOL VSSPathConverter();
BOOL VSSPathConvert(const char *szFilePath, char *szShadowPath, int nBuflen);
BOOL VSSPathConvertW(const wchar_t *szFilePath, wchar_t *szShadowPath, int nBuflen);

// {b5946137-7b9f-4925-af80-51abd60b20d5}

static const GUID VSS_SWPRV_ProviderID =
   { 0xb5946137, 0x7b9f, 0x4925, { 0xaf, 0x80, 0x51, 0xab, 0xd6, 0x0b, 0x20, 0xd5 } };

static pthread_once_t key_vss_once = PTHREAD_ONCE_INIT;
static pthread_key_t vssclient_key;

static void create_vss_key()
{
   int status = pthread_key_create(&vssclient_key, NULL);
   if (status != 0) {
      berrno be;
      Pmsg1(000, _("pthread key create failed: ERR=%s\n"),
            be.bstrerror(status));
      ASSERT2(0, "pthread_key_create failed");
   }
   SetVSSPathConvert(VSSPathConverter, VSSPathConvert, VSSPathConvertW);
}

/* TODO: Use the JCR variable to get the VSSClient pointer
 * the JCR FileDaemon part is not known in the VSS library
 */
static void store_vssclient_in_tsd(VSSClient *cl)
{
   int status = pthread_once(&key_vss_once, create_vss_key);
   if (status != 0) {
      berrno be;
      Pmsg1(000, _("pthread key create failed: ERR=%s\n"),
            be.bstrerror(status));
      ASSERT2(0, "pthread_once failed");
   }

   status = pthread_setspecific(vssclient_key, (void *)cl);
   if (status != 0) {
      berrno be;
      Jmsg1(NULL, M_ABORT, 0, _("pthread_setspecific failed: ERR=%s\n"),
            be.bstrerror(status));
   }
}

static VSSClient *get_vssclient_from_tsd()
{
   return (VSSClient *)pthread_getspecific(vssclient_key);
}

void
VSSCleanup(VSSClient *pVSSClient)
{
   store_vssclient_in_tsd(NULL);
   if (pVSSClient) {
      delete pVSSClient;
   }
}

/*
 * May be called multiple times
 */
VSSClient *VSSInit()
{
   VSSClient *pVSSClient = NULL;
   /* decide which vss class to initialize */
   if (g_MajorVersion == 5) {
      switch (g_MinorVersion) {
      case 1:
         pVSSClient = new VSSClientXP();
         break;
      case 2:
         pVSSClient = new VSSClient2003();
         break;
      }
   /* Vista or Longhorn or later */
   } else if (g_MajorVersion >= 6) {
      pVSSClient = new VSSClientVista();
   }
   store_vssclient_in_tsd(pVSSClient);
   return pVSSClient;
}

BOOL VSSPathConverter()
{
   if (get_vssclient_from_tsd() == NULL) {
      return false;
   }
   return true;
}

BOOL
VSSPathConvert(const char *szFilePath, char *szShadowPath, int nBuflen)
{
   VSSClient *pVSSClient = get_vssclient_from_tsd();
   if (pVSSClient) {
      return pVSSClient->GetShadowPath(szFilePath, szShadowPath, nBuflen);
   } else {
      return false;
   }
}

BOOL
VSSPathConvertW(const wchar_t *szFilePath, wchar_t *szShadowPath, int nBuflen)
{
   VSSClient *pVSSClient = get_vssclient_from_tsd();
   if (pVSSClient) {
      return pVSSClient->GetShadowPathW(szFilePath, szShadowPath, nBuflen);
   } else {
      return false;
   }
}

// Constructor
VSSClient::VSSClient()
{
    m_pAlistWriterState = New(alist(10, not_owned_by_alist));
    m_pAlistWriterInfoText = New(alist(10, owned_by_alist));
    m_uidCurrentSnapshotSet = GUID_NULL;
}

// Destructor
VSSClient::~VSSClient()
{
   // Release the IVssBackupComponents interface
   // WARNING: this must be done BEFORE calling CoUninitialize()
   if (m_pVssObject) {
//      m_pVssObject->Release();
      m_pVssObject = NULL;
   }

   DestroyWriterInfo();
   delete m_pAlistWriterState;
   delete m_pAlistWriterInfoText;

   // Call CoUninitialize if the CoInitialize was performed successfully
   if (m_bCoInitializeCalled) {
      CoUninitialize();
   }

   delete m_VolumeList;
}

bool VSSClient::InitializeForBackup(JCR *jcr)
{
   //return Initialize (VSS_CTX_BACKUP);
   m_jcr = jcr;
   return Initialize(0);
}


bool VSSClient::InitializeForRestore(JCR *jcr)
{
   m_metadata = NULL;
   m_jcr = jcr;
   return Initialize(0, true/*=>Restore*/);
}

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

bool VSSClient::GetShadowPath(const char *szFilePath, char *szShadowPath, int nBuflen)
{
   Dmsg1(dbglvl_snap, "GetShadowPath(%s)\n", szFilePath);

   if (m_bDuringRestore) {
      return false;
   }

   if (!m_bBackupIsInitialized) {
      Jmsg0(m_jcr, M_FATAL, 0, "Backup is not Initialized\n");
      return false;
   }

   wstring path, rootPath, volume;
   POOLMEM* pwszBuf = get_pool_memory(PM_FNAME);

   wutf8_path_2_wchar(&pwszBuf, szFilePath);
   path.assign((wchar_t *)pwszBuf);

   /* TODO: Have some cache here? */
   volume = GetUniqueVolumeNameForPath(path, rootPath);

   MTabEntry *vol = (MTabEntry *)m_VolumeList->entries->search(&volume,volume_search);
   free_pool_memory(pwszBuf);

   if (vol && vol->shadowCopyName) {
      if (WideCharToMultiByte(CP_UTF8,0,vol->shadowCopyName,-1,szShadowPath,nBuflen-1,NULL,NULL)) {
         nBuflen -= (int)strlen(szShadowPath);

         bstrncat(szShadowPath, "\\", nBuflen);
         nBuflen -= 1;
        //Dmsg4(200,"szFilePath=%s rootPath=%ls len(rootPath)=%d nBuflen=%d\n",
        //      szFilePath, rootPath.c_str(), rootPath.length(), nBuflen);

         /* here we skip C:, we skip volume root */
         /* TODO: I'm not 100% sure that rootPath.lenght() WCHAR means X CHAR 
          * The main goal here is to convert
          * c:/tmp/mounted/test -> \\?\Device\HardDiskSnapshot10\test
          *
          * So, we skip c:/tmp/mounted/ from the base file.
          */
         if (strlen(szFilePath) > rootPath.length()) {
            bstrncat(szShadowPath, szFilePath+rootPath.length(), nBuflen);
         }
         Dmsg2(dbglvl_snap, "GetShadowPath(%s) -> %s\n", szFilePath, szShadowPath);
         return true;
      }
   }

   bstrncpy(szShadowPath, szFilePath, nBuflen);
   Dmsg2(dbglvl_snap, "GetShadowPath(%s) -> %s\n", szFilePath, szShadowPath);
   errno = EINVAL;
   return false;
}

/*
 * c:/tmp   ->   \\?\GLOBALROOT\Device\HarddiskVolumeShadowCopy15\tmp
 * c:/      ->   \\?\GLOBALROOT\Device\HarddiskVolumeShadowCopy15
 */
bool VSSClient::GetShadowPathW(const wchar_t *szFilePath, wchar_t *szShadowPath, int nBuflen)
{
   Dmsg1(dbglvl_snap, "GetShadowPathW(%ls)\n", szFilePath);

   if (m_bDuringRestore) {
      return false;
   }

   if (!m_bBackupIsInitialized) {
      Jmsg0(m_jcr, M_FATAL, 0, "Backup is not Initialized\n");
      return false;
   }
   wstring path, rootPath, volume;
   path.assign((wchar_t *)szFilePath);
   /* TODO: Have some cache here? */
   /* In  : path = c:\tmp
    * Out : rootPath = c:\
    * Out : volume = \\?\Volume{33502cbb-c037-4bb7-80a2-8d40a4753bbd}\
    */
   volume = GetUniqueVolumeNameForPath(path, rootPath);
   MTabEntry *vol = (MTabEntry *)m_VolumeList->entries->search(&volume,volume_search);
   /* Out shadowCopyName=\\?\GLOBALROOT\Device\HarddiskVolumeShadowCopy4 */
   if (vol && vol->shadowCopyName) {
      Dmsg5(dbglvl_snap, "szFilePath=%ls rootPath=%ls len(rootPath)=%d nBuflen=%d shadowCopyName=%ls\n",
            szFilePath, rootPath.c_str(), rootPath.length(), nBuflen, vol->shadowCopyName);

Dmsg3(20, "ASX GetShadowPathW(%ls) volume=%ls vol->shadowCopyName=%ls\n", szFilePath, volume.c_str(), vol->shadowCopyName);
Dmsg5(20, "ASX szFilePath=%ls rootPath=%ls len(rootPath)=%d nBuflen=%d shadowCopyName=%ls\n",
      szFilePath, rootPath.c_str(), rootPath.length(), nBuflen, vol->shadowCopyName);

      wcsncpy(szShadowPath, vol->shadowCopyName, nBuflen);
      nBuflen -= (int)wcslen(vol->shadowCopyName);

      //Dmsg4(200, "szFilePath=%ls rootPath=%ls len(rootPath)=%d nBuflen=%d shadowCopyName=%ls\n",
      //      szFilePath, rootPath.c_str(), rootPath.length(), nBuflen, vol->shadowCopyName);

      if (wcslen(szFilePath) > rootPath.length()) {
         wcsncat(szShadowPath, L"\\", nBuflen);
         nBuflen -= 1;
         // Append the right part of szFilePath that is on the right of rootPath
         // aka "tmp" in c:/tmp
         wcsncat(szShadowPath, szFilePath+rootPath.length(), nBuflen);
      }
Dmsg2(20, "ASX GetShadowPathW(%ls) ==> %ls\n", szFilePath, szShadowPath);
      Dmsg2(dbglvl_snap, "GetShadowPathW(%ls) -> %ls\n", szFilePath, szShadowPath);
      return true;
   }

   wcsncpy(szShadowPath, szFilePath, nBuflen);
   Dmsg2(dbglvl_snap, "GetShadowPathW(%ls) -> %ls\n", szFilePath, szShadowPath);
   errno = EINVAL;
   return false;
}

const size_t VSSClient::GetWriterCount()
{
   return m_pAlistWriterInfoText->size();
}

const char* VSSClient::GetWriterInfo(int nIndex)
{
   return (char*)m_pAlistWriterInfoText->get(nIndex);
}


const int VSSClient::GetWriterState(int nIndex)
{
   void *item = m_pAlistWriterState->get(nIndex);

/* Eliminate compiler warnings */
#ifdef HAVE_VSS64
   return (int64_t)(char *)item;
#else
   return (int)(char *)item;
#endif
}

void VSSClient::AppendWriterInfo(int nState, const char* pszInfo)
{
   m_pAlistWriterInfoText->push(bstrdup(pszInfo));
   m_pAlistWriterState->push((void*)(intptr_t)nState);
}

/*
 * Note, this is called at the end of every job, so release all
 *  the items in the alists, but do not delete the alist.
 */
void VSSClient::DestroyWriterInfo()
{
   while (!m_pAlistWriterInfoText->empty()) {
      free(m_pAlistWriterInfoText->pop());
   }

   while (!m_pAlistWriterState->empty()) {
      m_pAlistWriterState->pop();
   }
}

#endif
