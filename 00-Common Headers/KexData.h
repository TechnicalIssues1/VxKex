///////////////////////////////////////////////////////////////////////////////
//
// Module Name:
//
//     KexData.h
//
// Abstract:
//
//     Contains the KEX_PROCESS_DATA structure and associated structure
//     definitions.
//
// Author:
//
//     vxiiduu (18-Apr-2022)
//
// Environment:
//
//     Any environment.
//
// Revision History:
//
//     vxiiduu               18-Apr-2022  Initial creation.
//
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <KexComm.h>
#include <NtDll.h>

#define KEXDATA_FLAG_PROPAGATED			1

#define KEX_STRONGSPOOF_SHAREDUSERDATA	1
#define KEX_STRONGSPOOF_REGISTRY		2

typedef enum _KEX_WIN_VER_SPOOF {
	WinVerSpoofNone,	// do not spoof
	WinVerSpoofWin7,	// for testing - not useful in practice
	WinVerSpoofWin8,
	WinVerSpoofWin8Point1,
	WinVerSpoofWin10,
	WinVerSpoofWin11,
	WinVerSpoofMax		// should always be the last value
} TYPEDEF_TYPE_NAME(KEX_WIN_VER_SPOOF);

//
// These variable names are present under the IFEO key for each executable
// with a KEX_ prefix. For example:
//
//   HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\
//   Image File Execution Options\application.exe\{id}\KEX_WinVerSpoof
//

typedef struct _KEX_IFEO_PARAMETERS {
	ULONG				DisableForChild;
	ULONG				DisableAppSpecific;
	KEX_WIN_VER_SPOOF	WinVerSpoof;
	ULONG				StrongVersionSpoof;
} TYPEDEF_TYPE_NAME(KEX_IFEO_PARAMETERS);

//
// A KEX_PROCESS_DATA structure is exported from KexDll under the name _KexData.
// (See KexDll.h for more information.)
//

typedef struct _KEX_PROCESS_DATA {
	ULONG					Flags;
	ULONG					InstalledVersion;			// version dword of VxKex
	KEX_IFEO_PARAMETERS		IfeoParameters;
	UNICODE_STRING			WinDir;						// e.g. C:\Windows
	UNICODE_STRING			KexDir;						// e.g. C:\Program Files\VxKex
	UNICODE_STRING			ImageBaseName;				// e.g. program.exe
	HANDLE					SrvChannel;
	PVOID					KexDllBase;
	PVOID					SystemDllBase;				// NTDLL base
} TYPEDEF_TYPE_NAME(KEX_PROCESS_DATA);