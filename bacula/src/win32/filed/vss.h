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
/*                               -*- Mode: C -*-
 * vss.h --
 */
//
// Copyright transferred from MATRIX-Computer GmbH to
//   Kern Sibbald by express permission.
/*
 *
 * Author          : Thorsten Engel
 * Created On      : Fri May 06 21:44:00 2006 
 */

#ifndef __VSS_H_
#define __VSS_H_

#ifndef b_errno_win32
#define b_errno_win32 (1<<29)
#endif
 
#ifdef WIN32_VSS

#include "mtab.h"

// some forward declarations
struct IVssAsync;

class VSSClient
{
public:
    VSSClient();
    virtual ~VSSClient();

    // Backup Process
    bool InitializeForBackup(JCR *jcr);
    bool InitializeForRestore(JCR *jcr);
    virtual bool CreateSnapshots(alist *mount_points) = 0;
    virtual bool CloseBackup() = 0;
    virtual bool CloseRestore() = 0;
    virtual WCHAR *GetMetadata() = 0;
    virtual const char* GetDriverName() = 0;
    bool GetShadowPath  (const char* szFilePath, char* szShadowPath, int nBuflen);
    bool GetShadowPathW (const wchar_t* szFilePath, wchar_t* szShadowPath, int nBuflen); /* nBuflen in characters */

    const size_t GetWriterCount();
    const char* GetWriterInfo(int nIndex);
    const int   GetWriterState(int nIndex);
    void DestroyWriterInfo();
    void AppendWriterInfo(int nState, const char* pszInfo);
    const bool  IsInitialized() { return m_bBackupIsInitialized; };
    IUnknown *GetVssObject() { return m_pVssObject; };
    MTab* GetVolumeList() { return m_VolumeList; };

private:
    virtual bool Initialize(DWORD dwContext, bool bDuringRestore = FALSE) = 0;
    virtual bool WaitAndCheckForAsyncOperation(IVssAsync*  pAsync) = 0;
    virtual void QuerySnapshotSet(GUID snapshotSetID) = 0;

protected:
    /* On windows, we control the Compiler version and we can use C++11
       in-member class initializers */
    JCR       *m_jcr = NULL;

    DWORD      m_dwContext = 0;

    IUnknown*  m_pVssObject = NULL;
    GUID       m_uidCurrentSnapshotSet = GUID_NULL;

    MTab      *m_VolumeList = NULL;

    alist     *m_pAlistWriterState = NULL;
    alist     *m_pAlistWriterInfoText = NULL;

    bool       m_bCoInitializeCalled = false;
    bool       m_bCoInitializeSecurityCalled = false;
    bool       m_bDuringRestore = false;  /* true if we are doing a restore */
    bool       m_bBackupIsInitialized = false;
    bool       m_bWriterStatusCurrent = false;

    WCHAR     *m_metadata = NULL;

    void       CreateVSSVolumeList();
    void       DeleteVSSVolumeList();
};

class VSSClientXP:public VSSClient
{
public:
   VSSClientXP();
   virtual ~VSSClientXP();
   virtual bool CreateSnapshots(alist *mount_points);
   virtual bool CloseBackup();
   virtual bool CloseRestore();
   virtual WCHAR *GetMetadata();
#ifdef _WIN64
   virtual const char* GetDriverName() { return "Win64 VSS"; };
#else
   virtual const char* GetDriverName() { return "Win32 VSS"; };
#endif
private:
   virtual bool Initialize(DWORD dwContext, bool bDuringRestore);
   virtual bool WaitAndCheckForAsyncOperation(IVssAsync* pAsync);
   virtual void QuerySnapshotSet(GUID snapshotSetID);
   bool CheckWriterStatus();   
};

class VSSClient2003:public VSSClient
{
public:
   VSSClient2003();
   virtual ~VSSClient2003();
   virtual bool CreateSnapshots(alist *mount_points);
   virtual bool CloseBackup();   
   virtual bool CloseRestore();
   virtual WCHAR *GetMetadata();
#ifdef _WIN64
   virtual const char* GetDriverName() { return "Win64 VSS"; };
#else
   virtual const char* GetDriverName() { return "Win32 VSS"; };
#endif
private:
   virtual bool Initialize(DWORD dwContext, bool bDuringRestore);
   virtual bool WaitAndCheckForAsyncOperation(IVssAsync*  pAsync);
   virtual void QuerySnapshotSet(GUID snapshotSetID);
   bool CheckWriterStatus();
};

class VSSClientVista:public VSSClient
{
public:
   VSSClientVista();
   virtual ~VSSClientVista();
   virtual bool CreateSnapshots(alist *mount_points);
   virtual bool CloseBackup();   
   virtual bool CloseRestore();
   virtual WCHAR *GetMetadata();
#ifdef _WIN64
   virtual const char* GetDriverName() { return "Win64 VSS"; };
#else
   virtual const char* GetDriverName() { return "Win32 VSS"; };
#endif
private:
   virtual bool Initialize(DWORD dwContext, bool bDuringRestore);
   virtual bool WaitAndCheckForAsyncOperation(IVssAsync*  pAsync);
   virtual void QuerySnapshotSet(GUID snapshotSetID);
   bool CheckWriterStatus();
};


BOOL VSSPathConvert(const char *szFilePath, char *szShadowPath, int nBuflen);
BOOL VSSPathConvertW(const wchar_t *szFilePath, wchar_t *szShadowPath, int nBuflen);

#endif /* WIN32_VSS */

#endif /* __VSS_H_ */
