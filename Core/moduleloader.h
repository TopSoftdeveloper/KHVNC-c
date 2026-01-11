
#ifndef __MODULELOADER_H__
#define __MODULELOADER_H__

#if defined (_MSC_VER) && (_MSC_VER >= 1020)
#pragma once
#endif


#include "common.h"

#ifdef __cplusplus
extern "C"
{
#endif

	typedef struct _LOADEDMODULE
	{
		PIMAGE_NT_HEADERS	pHeaders;
		PBYTE				pCodeBase;
		HINSTANCE* pModules;
		INT					nModules;
		BOOL				fInitialized;
		BOOL				fRelocated;
		DWORD				dwPageSize;
	} LOADEDMODULE, * PLOADEDMODULE;

	typedef PVOID HLOADEDMODULE;

	PLOADEDMODULE _LoadModuleFromInstance(__in HINSTANCE hInstance);

	__checkReturn __success(return != NULL) PLOADEDMODULE LoadModuleFromMemory(__in_bcount(dwSize) PVOID lpData, __in DWORD dwSize);

	FARPROC _GetProcAddress(__in_opt PLOADEDMODULE pModule, __in_z_opt LPCSTR FuncName);

	void FreeLibraryResources(__in_opt PLOADEDMODULE pModule);

	__checkReturn __success(return == TRUE) BOOL IsPEHeaderValid(__in PVOID lpData, __in DWORD dwSize);

	PVOID _FindResource(__in_opt PLOADEDMODULE module, __in_z LPCTSTR name, __in_z_opt LPCTSTR type);

	LPVOID _LoadResource(__in PLOADEDMODULE module, __in PVOID resource);

	DWORD _SizeofResource(__in_opt PLOADEDMODULE module, __in_opt PVOID resource);

	// Section data from file header
	typedef struct _SECTIONDATA
	{
		LPVOID pAddress;
		LPVOID pAlignedAddress;
		uintptr_t  Size;
		DWORD  dwCharacteristics;
		BOOL   fLast;
	} SECTIONDATA, * PSECTIONDATA;


#define ARCHITECTURE_TYPE_X86 0x00000000
#define ARCHITECTURE_TYPE_X64 0x00000001

	// MIN/MAX of address aligned
#define MIN_ALIGNED(address, alignment) (LPVOID)((uintptr_t)(address) & ~((alignment) - 1))
#define MAX_ALIGNED(value, alignment) (((value) + (alignment) - 1) & ~((alignment) - 1))

	typedef BOOL(WINAPI * DllEntryProc)(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved);

#define DEFAULT_LANGUAGE        MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL)

#ifdef __cplusplus
}
#endif


#endif /* __MODULELOADER_H__*/
