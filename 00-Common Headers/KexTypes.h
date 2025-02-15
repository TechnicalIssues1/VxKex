///////////////////////////////////////////////////////////////////////////////
//
// Module Name:
//
//     KexTypes.h
//
// Abstract:
//
//     Type definitions and associated macros.
//
// Author:
//
//     vxiiduu (26-Sep-2022)
//
// Environment:
//
//     Any environment.
//
// Revision History:
//
//     vxiiduu               26-Sep-2022  Initial creation.
//
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <Limits.h>

#undef CDECL
#define CDECL __cdecl
#define NORETURN __declspec(noreturn)
#define DECLSPEC_EXPORT __declspec(dllexport)
#define INLINE __inline
#define NOINLINE __declspec(noinline)
#define STATIC static
#define VOLATILE volatile

typedef PSTR *PPSTR;
typedef PCSTR *PPCSTR;
typedef PWSTR *PPWSTR;
typedef PCWSTR *PPCWSTR;
typedef va_list ARGLIST;

#define GEN_STD_TYPEDEFS(Type) \
	typedef Type *P##Type; \
	typedef Type **PP##Type; \
	typedef CONST Type *PC##Type; \
	typedef CONST Type **PPC##Type

#define TYPEDEF_TYPE_NAME(Type) Type; GEN_STD_TYPEDEFS(Type)

// PCVOID, PPVOID, PPCVOID etc
GEN_STD_TYPEDEFS(VOID);