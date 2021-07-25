//===============================================================================================//
// Copyright (c) 2012, Stephen Fewer of Harmony Security (www.harmonysecurity.com)
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without modification, are permitted
// provided that the following conditions are met:
//
//     * Redistributions of source code must retain the above copyright notice, this list of
// conditions and the following disclaimer.
//
//     * Redistributions in binary form must reproduce the above copyright notice, this list of
// conditions and the following disclaimer in the documentation and/or other materials provided
// with the distribution.
//
//     * Neither the name of Harmony Security nor the names of its contributors may be used to
// endorse or promote products derived from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR
// IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
// FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//===============================================================================================//
#ifndef _REFLECTIVEDLLINJECTION_REFLECTIVELOADER_H
#define _REFLECTIVEDLLINJECTION_REFLECTIVELOADER_H
//===============================================================================================//
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <intrin.h>

#include "ReflectiveDLLInjection.h"

typedef HMODULE (WINAPI * LOADLIBRARYA)( LPCSTR );
typedef FARPROC (WINAPI * GETPROCADDRESS)( HMODULE, LPCSTR );
typedef LPVOID  (WINAPI * VIRTUALALLOC)( LPVOID, SIZE_T, DWORD, DWORD );
typedef LPVOID  (WINAPI * VIRTUALPROTECT)( LPVOID, SIZE_T, DWORD, PDWORD );
typedef LPVOID  (WINAPI * VIRTUALFREE)( LPVOID, SIZE_T, DWORD );
typedef DWORD  (NTAPI * NTFLUSHINSTRUCTIONCACHE)( HANDLE, PVOID, ULONG );

#define KERNEL32DLL_HASH               0x29cdd463
#define NTDLLDLL_HASH                  0x145370bb

#define LOADLIBRARYA_HASH              0x53b2070f
#define GETPROCADDRESS_HASH            0xf8f45725
#define VIRTUALALLOC_HASH              0x03285501
#define VIRTUALPROTECT_HASH            0x820621f3
#define VIRTUALFREE_HASH               0x3a9acc72
#define NTFLUSHINSTRUCTIONCACHE_HASH   0x24f8dd09

#ifdef _WIN64
typedef BOOLEAN (NTAPI * RTLADDFUNCTIONTABLE)( PVOID, DWORD, DWORD64 );
#define RTLADDFUNCTIONTABLE_HASH       0x38791528
#endif

#define IMAGE_REL_BASED_ARM_MOV32A     5
#define IMAGE_REL_BASED_ARM_MOV32T     7

#define ARM_MOV_MASK                   (DWORD)(0xFBF08000)
#define ARM_MOV_MASK2                  (DWORD)(0xFBF08F00)
#define ARM_MOVW                       0xF2400000
#define ARM_MOVT                       0xF2C00000

#define HASH_KEY                       13

#ifndef REFLECTIVE_LOADER_SYM
#define REFLECTIVE_LOADER_SYM ReflectiveLoader
#endif

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

#define REFLECTIVE_LOADER_SYMNAME TOSTRING(REFLECTIVE_LOADER_SYM)

#define IMAGE_DOS_SIGNATURE_ALT  0x4548
#define IMAGE_NT_SIGNATURE_ALT   0x0B15B00B5

//===============================================================================================//

#define FNV_PRIME_32    16777619
#define FNV_OFFSET_32   2166136261

__forceinline static
DWORD symhash(const unsigned char *s) {
   register DWORD h = FNV_OFFSET_32;

   if (!s || !s[0])
      return h;

   /*
      Workaround stdcall namings
      _BLABLA@1234 -> BLABLA
   */
   if (s[0] == '_')
      s ++;

   while (s[0] && s[0] != '@') {
      h = h ^ s[0];
      h = h * FNV_PRIME_32;
      s ++;
   }

   return h;
}

__forceinline static
DWORD hashmodname(const unsigned char *s, DWORD dwLength) {
   register DWORD h = FNV_OFFSET_32;

   while (dwLength --) {
      unsigned char c = (*s ++);

      if (!c)
         continue;

      if (c >= 'a')
         c -= 0x20;

      h ^= c;
      h *= FNV_PRIME_32;
   }

   return h;
}

//===============================================================================================//
typedef struct _UNICODE_STR
{
  USHORT Length;
  USHORT MaximumLength;
  PWSTR pBuffer;
} UNICODE_STR, *PUNICODE_STR;

// WinDbg> dt -v ntdll!_LDR_DATA_TABLE_ENTRY
//__declspec( align(8) )
typedef struct _LDR_DATA_TABLE_ENTRY
{
   //LIST_ENTRY InLoadOrderLinks; // As we search from PPEB_LDR_DATA->InMemoryOrderModuleList we dont use the first entry.
   LIST_ENTRY InMemoryOrderModuleList;
   LIST_ENTRY InInitializationOrderModuleList;
   PVOID DllBase;
   PVOID EntryPoint;
   ULONG SizeOfImage;
   UNICODE_STR FullDllName;
   UNICODE_STR BaseDllName;
   ULONG Flags;
   SHORT LoadCount;
   SHORT TlsIndex;
   LIST_ENTRY HashTableEntry;
   ULONG TimeDateStamp;
} LDR_DATA_TABLE_ENTRY, *PLDR_DATA_TABLE_ENTRY;

// WinDbg> dt -v ntdll!_PEB_LDR_DATA
typedef struct _PEB_LDR_DATA //, 7 elements, 0x28 bytes
{
   DWORD dwLength;
   DWORD dwInitialized;
   LPVOID lpSsHandle;
   LIST_ENTRY InLoadOrderModuleList;
   LIST_ENTRY InMemoryOrderModuleList;
   LIST_ENTRY InInitializationOrderModuleList;
   LPVOID lpEntryInProgress;
} PEB_LDR_DATA, * PPEB_LDR_DATA;

// WinDbg> dt -v ntdll!_PEB_FREE_BLOCK
typedef struct _PEB_FREE_BLOCK // 2 elements, 0x8 bytes
{
   struct _PEB_FREE_BLOCK * pNext;
   DWORD dwSize;
} PEB_FREE_BLOCK, * PPEB_FREE_BLOCK;

// struct _PEB is defined in Winternl.h but it is incomplete
// WinDbg> dt -v ntdll!_PEB
typedef struct __PEB // 65 elements, 0x210 bytes
{
   BYTE bInheritedAddressSpace;
   BYTE bReadImageFileExecOptions;
   BYTE bBeingDebugged;
   BYTE bSpareBool;
   LPVOID lpMutant;
   LPVOID lpImageBaseAddress;
   PPEB_LDR_DATA pLdr;
   LPVOID lpProcessParameters;
   LPVOID lpSubSystemData;
   LPVOID lpProcessHeap;
   PRTL_CRITICAL_SECTION pFastPebLock;
   LPVOID lpFastPebLockRoutine;
   LPVOID lpFastPebUnlockRoutine;
   DWORD dwEnvironmentUpdateCount;
   LPVOID lpKernelCallbackTable;
   DWORD dwSystemReserved;
   DWORD dwAtlThunkSListPtr32;
   PPEB_FREE_BLOCK pFreeList;
   DWORD dwTlsExpansionCounter;
   LPVOID lpTlsBitmap;
   DWORD dwTlsBitmapBits[2];
   LPVOID lpReadOnlySharedMemoryBase;
   LPVOID lpReadOnlySharedMemoryHeap;
   LPVOID lpReadOnlyStaticServerData;
   LPVOID lpAnsiCodePageData;
   LPVOID lpOemCodePageData;
   LPVOID lpUnicodeCaseTableData;
   DWORD dwNumberOfProcessors;
   DWORD dwNtGlobalFlag;
   LARGE_INTEGER liCriticalSectionTimeout;
   DWORD dwHeapSegmentReserve;
   DWORD dwHeapSegmentCommit;
   DWORD dwHeapDeCommitTotalFreeThreshold;
   DWORD dwHeapDeCommitFreeBlockThreshold;
   DWORD dwNumberOfHeaps;
   DWORD dwMaximumNumberOfHeaps;
   LPVOID lpProcessHeaps;
   LPVOID lpGdiSharedHandleTable;
   LPVOID lpProcessStarterHelper;
   DWORD dwGdiDCAttributeList;
   LPVOID lpLoaderLock;
   DWORD dwOSMajorVersion;
   DWORD dwOSMinorVersion;
   WORD wOSBuildNumber;
   WORD wOSCSDVersion;
   DWORD dwOSPlatformId;
   DWORD dwImageSubsystem;
   DWORD dwImageSubsystemMajorVersion;
   DWORD dwImageSubsystemMinorVersion;
   DWORD dwImageProcessAffinityMask;
   DWORD dwGdiHandleBuffer[34];
   LPVOID lpPostProcessInitRoutine;
   LPVOID lpTlsExpansionBitmap;
   DWORD dwTlsExpansionBitmapBits[32];
   DWORD dwSessionId;
   ULARGE_INTEGER liAppCompatFlags;
   ULARGE_INTEGER liAppCompatFlagsUser;
   LPVOID lppShimData;
   LPVOID lpAppCompatInfo;
   UNICODE_STR usCSDVersion;
   LPVOID lpActivationContextData;
   LPVOID lpProcessAssemblyStorageMap;
   LPVOID lpSystemDefaultActivationContextData;
   LPVOID lpSystemAssemblyStorageMap;
   DWORD dwMinimumStackCommit;
} _PEB, * _PPEB;

typedef struct
{
   WORD	offset:12;
   WORD	type:4;
} IMAGE_RELOC, *PIMAGE_RELOC;
//===============================================================================================//
#endif
//===============================================================================================//

