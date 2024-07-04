/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2000-2023 Bacula Systems SA
   All rights reserved.

   The main author of Bacula is Kern Sibbald, with contributions from many
   others, a complete list can be found in the file AUTHORS.

   Licensees holding a valid Bacula Systems SA license may use this file
   and others of this release in accordance with the proprietary license
   agreement provided in the LICENSE file.  Redistribution of any part of
   this release is not permitted.

   Bacula® is a registered trademark of Kern Sibbald.
*/

/* Quick code to list open handler on windows */

#include <windows.h>
#include <winternl.h>
#include <stdio.h>
#include <ntstatus.h>

// Define NtQuerySystemInformation and SYSTEM_HANDLE_INFORMATION structures
typedef NTSTATUS(WINAPI* PNtQuerySystemInformation)(
    SYSTEM_INFORMATION_CLASS SystemInformationClass,
    PVOID SystemInformation,
    ULONG SystemInformationLength,
    PULONG ReturnLength);

typedef struct _SYSTEM_HANDLE_TABLE_ENTRY_INFO {
    USHORT UniqueProcessId;
    USHORT HandleValue;
    ULONG ObjectTypeIndex;
    ULONG HandleAttributes;
    ULONG Object;
    ULONG GrantedAccess;
} SYSTEM_HANDLE_TABLE_ENTRY_INFO, * PSYSTEM_HANDLE_TABLE_ENTRY_INFO;

typedef NTSTATUS(WINAPI* PNtQueryObject)(
    HANDLE Handle,
    OBJECT_INFORMATION_CLASS ObjectInformationClass,
    PVOID ObjectInformation,
    ULONG ObjectInformationLength,
    PULONG ReturnLength);

static PNtQueryObject pNtQueryObject;
static PNtQuerySystemInformation pNtQuerySystemInformation;
void init()
{
    HMODULE hNtdll = LoadLibraryA("ntdll.dll");
    if (!hNtdll) {
        printf("Failed to load ntdll.dll\n");
        return;
    }

    pNtQueryObject = (PNtQueryObject)GetProcAddress(hNtdll, "NtQueryObject");
    if (!pNtQueryObject) {
        printf("Failed to get NtQueryObject address\n");
    }

    pNtQuerySystemInformation = (PNtQuerySystemInformation)GetProcAddress(hNtdll, "NtQuerySystemInformation");
    if (pNtQuerySystemInformation == NULL) {
        printf("Failed to get NtQuerySystemInformation address\n");
    }
    FreeLibrary(hNtdll);

    if (!pNtQuerySystemInformation || !pNtQueryObject) {
       exit(1);
    }
}


void PrintHandleName(HANDLE handle) {

    ULONG returnLength;
    ULONG bufferSize = 0x1000;
    POBJECT_NAME_INFORMATION objectNameInfo = (POBJECT_NAME_INFORMATION)malloc(bufferSize);

    NTSTATUS status = pNtQueryObject(handle, (OBJECT_INFORMATION_CLASS)ObjectNameInformation, objectNameInfo, bufferSize, &returnLength);
    if (status == STATUS_INFO_LENGTH_MISMATCH) {
        objectNameInfo = (POBJECT_NAME_INFORMATION)realloc(objectNameInfo, returnLength);
        status = pNtQueryObject(handle, (OBJECT_INFORMATION_CLASS)ObjectNameInformation, objectNameInfo, returnLength, &returnLength);
    }

    if (status == 0) {
        printf("Handle Name: %S\n", objectNameInfo->Name.Buffer);
    } else {
        printf("NtQueryObject failed with status 0x%lX\n", status);
    }

    free(objectNameInfo);
}

int main() {
   init();

    ULONG handleInfoSize = 0x10000;
    PSYSTEM_HANDLE_INFORMATION handleInfo = (PSYSTEM_HANDLE_INFORMATION)malloc(handleInfoSize);
    NTSTATUS status;

    while ((status = pNtQuerySystemInformation(SystemHandleInformation, handleInfo, handleInfoSize, &handleInfoSize)) == STATUS_INFO_LENGTH_MISMATCH) {
        handleInfo = (PSYSTEM_HANDLE_INFORMATION)realloc(handleInfo, handleInfoSize);
    }

    if (status != 0) {
        printf("NtQuerySystemInformation failed with status 0x%lX\n", status);
        free(handleInfo);
        return 1;
    }

    DWORD currentProcessId = GetCurrentProcessId();
    printf("Open handles inherited by process %ld:\n", currentProcessId);

    for (ULONG i = 0; i < handleInfo->Count; i++) {
        if (handleInfo->Handle[i].OwnerPid == currentProcessId) {
	    HANDLE handle = (HANDLE)(ULONG_PTR)handleInfo->Handle[i].HandleValue;
            printf("Handle: 0x%p\n", handle);
	    PrintHandleName(handle);
        }
    }

    free(handleInfo);
    return 0;
}
