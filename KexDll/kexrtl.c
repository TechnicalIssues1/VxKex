///////////////////////////////////////////////////////////////////////////////
//
// Module Name:
//
//     kexrtl.c
//
// Abstract:
//
//     Various useful run-time routines.
//
// Author:
//
//     vxiiduu (17-Oct-2022)
//
// Revision History:
//
//     vxiiduu              17-Oct-2022  Initial creation.
//     vxiiduu              29-Oct-2022  Fix bug in KexRtlPathFindFileName
//
///////////////////////////////////////////////////////////////////////////////

#include "buildcfg.h"
#include "kexdllp.h"

// Examples:
// C:\Windows\system32\notepad.exe -> notepad.exe
// notepad.exe -> notepad.exe
// dir1\dir2\notepad.exe -> dir1\dir2\notepad.exe
//
// (As you can see, this function only works on FULL paths - otherwise,
// the output path is unchanged.)
KEXAPI NTSTATUS NTAPI KexRtlPathFindFileName(
	IN	PCUNICODE_STRING Path,
	OUT	PUNICODE_STRING FileName) PROTECTED_FUNCTION
{
	ULONG LengthWithoutLastElement;

	if (!Path) {
		return STATUS_INVALID_PARAMETER_1;
	}

	if (!FileName) {
		return STATUS_INVALID_PARAMETER_2;
	}

	//
	// If Path->Buffer contains a path with no backslashes, this function
	// will fail and set LengthWithoutLastElement to zero. This is desired
	// and that's why the return value is not checked.
	//
	RtlGetLengthWithoutLastFullDosOrNtPathElement(0, Path, &LengthWithoutLastElement);

	FileName->Buffer = Path->Buffer + LengthWithoutLastElement;
	FileName->Length = Path->Length - (USHORT) (LengthWithoutLastElement * sizeof(WCHAR));
	FileName->MaximumLength = Path->MaximumLength - (USHORT) (LengthWithoutLastElement * sizeof(WCHAR));

	return STATUS_SUCCESS;
} PROTECTED_FUNCTION_END

// Examples:
// C:\Windows\system32\notepad.exe -> C:\Windows\system32\notepad
// C:\Users\bob.smith\Videos -> C:\Users\bob.smith\Videos
// C:\Users\bob.smith\ -> C:\Users\bob.smith\
// C:\Users\bob.smith -> C:\Users\bob
// file.txt -> file
// file -> file
// file. -> file
// .file -> <empty string>
//
// Returns STATUS_SUCCESS if the extension was successfully removed,
// or STATUS_NOT_FOUND if no extension was removed.

KEXAPI NTSTATUS NTAPI KexRtlPathRemoveExtension(
	IN	PCUNICODE_STRING	Path,
	OUT	PUNICODE_STRING		PathWithoutExtension) PROTECTED_FUNCTION
{
	NTSTATUS Status;
	UNICODE_STRING Stops;
	USHORT PrefixLength;
	
	if (!Path || !PathWithoutExtension || Path->Length == 0) {
		return STATUS_INVALID_PARAMETER;
	}

	*PathWithoutExtension = *Path;

	RtlInitConstantUnicodeString(&Stops, L".\\/");
	Status = RtlFindCharInUnicodeString(
		RTL_FIND_CHAR_IN_UNICODE_STRING_START_AT_END,
		PathWithoutExtension,
		&Stops,
		&PrefixLength);

	if (NT_SUCCESS(Status)) {
		if (PathWithoutExtension->Buffer[PrefixLength / sizeof(WCHAR)] == '.') {
			PathWithoutExtension->Length = PrefixLength;
		}
	}

	return Status;
} PROTECTED_FUNCTION_END

KEXAPI BOOLEAN NTAPI KexRtlPathReplaceIllegalCharacters(
	IN		PCUNICODE_STRING	Path,
	OUT		PUNICODE_STRING		SanitizedPath,
	IN		WCHAR				ReplacementCharacter OPTIONAL,
	IN		BOOLEAN				AllowPathSeparators) PROTECTED_FUNCTION
{
	PCWSTR PathEnd;
	BOOLEAN AtLeastOneCharacterWasReplaced;

	ASSERT (Path != NULL);
	ASSERT (Path->Length != 0);
	ASSERT (Path->Buffer != NULL);
	ASSERT (SanitizedPath != NULL);

	if (!Path || !Path->Length || !Path->Buffer || !SanitizedPath) {
		return FALSE;
	}

	if (!ReplacementCharacter) {
		ReplacementCharacter = '_';
	}

	*SanitizedPath = *Path;
	AtLeastOneCharacterWasReplaced = FALSE;
	PathEnd = KexRtlEndOfUnicodeString(Path);

	until (SanitizedPath->Buffer == PathEnd) {
		switch (*SanitizedPath->Buffer) {
		case '<':
		case '>':
		case ':':
		case '"':
		case '|':
		case '?':
		case '*':
			*SanitizedPath->Buffer = ReplacementCharacter;
			AtLeastOneCharacterWasReplaced = TRUE;
			break;
		case '/':
		case '\\':
			unless (AllowPathSeparators) {
				*SanitizedPath->Buffer = ReplacementCharacter;
				AtLeastOneCharacterWasReplaced = TRUE;
			}
			break;
		}

		SanitizedPath->Buffer++;
	}

	SanitizedPath->Buffer = Path->Buffer;

	return AtLeastOneCharacterWasReplaced;
} PROTECTED_FUNCTION_END_BOOLEAN

KEXAPI NTSTATUS NTAPI KexRtlGetProcessImageBaseName(
	OUT	PUNICODE_STRING	FileName) PROTECTED_FUNCTION
{
	return KexRtlPathFindFileName(&NtCurrentPeb()->ProcessParameters->ImagePathName, FileName);
} PROTECTED_FUNCTION_END

//
// NtQueryKeyValue is too annoying to use in everyday code, RtlQueryRegistryValues
// is unsafe, and RtlpNtQueryKeyValue only supports the default/unnamed key. So
// here is an API that essentially mimics the function of win32 RegGetValue.
//
//   KeyHandle - Handle to an open registry key.
//
//   ValueName - Name of the value to query.
//
//   ValueDataCb - Points to size, in bytes, of the buffer indicated by ValueData.
//                 Upon successful return, contains the size of the data retrieved
//                 from the registry.
//                 If this value is 0 before the function is called, the function
//                 will return with STATUS_INSUFFICIENT_BUFFER, not check the
//                 ValueData parameter, and place the correct buffer size required
//                 to store the requested registry data in *ValueDataCb.
//
//   ValueData - Buffer which holds the returned data. If NULL, the function will
//               fail with STATUS_INVALID_PARAMETER (unless ValueDataCb is zero).
//
//   ValueDataTypeRestrict - Indicates which data types are allowed to be returned.
//                           One or more flags from the REG_RESTRICT_* set can be
//                           passed. If the data type of the value in the registry
//                           does not match these filters, the function will return
//                           STATUS_OBJECT_TYPE_MISMATCH and *ValueDataType will
//                           contain the type of the registry data.
//
//   ValueDataType - If function returns successfully, contains the data type of the
//                   data read from the registry.
//
// If this function returns with a failure code, the buffer pointed to by ValueData
// is unmodified.
//
KEXAPI NTSTATUS NTAPI KexRtlQueryKeyValueData(
	IN		HANDLE				KeyHandle,
	IN		PCUNICODE_STRING	ValueName,
	IN OUT	PULONG				ValueDataCb,
	OUT		PVOID				ValueData OPTIONAL,
	IN		ULONG				ValueDataTypeRestrict,
	OUT		PULONG				ValueDataType OPTIONAL) PROTECTED_FUNCTION
{
	NTSTATUS Status;
	PVOID KeyValueBuffer;
	ULONG KeyValueBufferCb;
	PKEY_VALUE_PARTIAL_INFORMATION KeyValueInformation;

	//
	// Validate parameters.
	//
	if (!KeyHandle || KeyHandle == INVALID_HANDLE_VALUE) {
		return STATUS_INVALID_PARAMETER_1;
	}

	if (!ValueName) {
		return STATUS_INVALID_PARAMETER_2;
	}

	if (!ValueDataCb) {
		return STATUS_INVALID_PARAMETER_3;
	}

	if (ValueData != NULL && *ValueDataCb == 0) {
		return STATUS_INVALID_PARAMETER_MIX;
	}

	if (!ValueData && *ValueDataCb != 0) {
		return STATUS_INVALID_PARAMETER_MIX;
	}

	if (!ValueDataTypeRestrict || (ValueDataTypeRestrict & (~LEGAL_REG_RESTRICT_MASK))) {
		return STATUS_INVALID_PARAMETER_5;
	}

	//
	// First of all, check if the caller just wants to know the length
	// of buffer required.
	//

	if (*ValueDataCb == 0) {
		Status = NtQueryValueKey(
			KeyHandle,
			ValueName,
			KeyValuePartialInformation,
			NULL,
			0,
			ValueDataCb);

		if (Status == STATUS_BUFFER_TOO_SMALL) {
			*ValueDataCb -= sizeof(KEY_VALUE_PARTIAL_INFORMATION);
		}

		return Status;
	}

	//
	// Now we allocate a buffer to store the KEY_VALUE_PARTIAL_INFORMATION
	// structure in addition to any data read from the registry.
	//
	
	KeyValueBufferCb = *ValueDataCb + sizeof(KEY_VALUE_PARTIAL_INFORMATION);
	KeyValueBuffer = SafeAlloc(BYTE, KeyValueBufferCb);

	if (!KeyValueBuffer) {
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	Status = NtQueryValueKey(
		KeyHandle,
		ValueName,
		KeyValuePartialInformation,
		KeyValueBuffer,
		KeyValueBufferCb,
		ValueDataCb);

	if (!NT_SUCCESS(Status)) {
		goto Exit;
	}

	*ValueDataCb -= sizeof(KEY_VALUE_PARTIAL_INFORMATION);
	KeyValueInformation = (PKEY_VALUE_PARTIAL_INFORMATION) KeyValueBuffer;

	//
	// Now, we check the data type of the returned value to make sure it
	// is matched by the ValueDataTypeRestrict filter.
	//

	unless (ValueDataTypeRestrict & (1 << KeyValueInformation->Type)) {
		Status = STATUS_OBJECT_TYPE_MISMATCH;
		goto Exit;
	}

	//
	// Copy the result into the caller's buffer.
	//

	RtlCopyMemory(ValueData, KeyValueInformation->Data, KeyValueInformation->DataLength);

Exit:
	if (NT_SUCCESS(Status) || Status == STATUS_OBJECT_TYPE_MISMATCH) {
		if (ValueDataType) {
			*ValueDataType = KeyValueInformation->Type;
		}
	}

	SafeFree(KeyValueBuffer);
	return Status;
} PROTECTED_FUNCTION_END

//
// Query multiple values of a key.
//
// KeyHandle
//   Handle to an open registry key under which to query values.
//
// QueryTable
//   Pointer to an array of KEX_RTL_QUERY_KEY_MULTIPLE_VARIABLE_TABLE_ENTRY
//   structures which provide space to store input and output parameters to
//   the KexRtlQueryKeyValueData routine.
//
// NumberOfQueryTableElements
//   Pointer to number of elements in the array pointed to by QueryTable.
//   Upon return, the number pointed to by this parameter contains the number
//   of values successfully queried.
//
// Flags
//   Valid "Flags" parameters start with QUERY_KEY_MULTIPLE_VALUE_:
//
//   QUERY_KEY_MULTIPLE_VALUE_FAIL_FAST (1)
//     Fail and return a failure code if one of the values in the query
//     table cannot be queried. By default, on failure to query a value
//     this function will simply record failure status inside the query
//     table entry, continue to the next entry and return success once
//     all values have been queried.
//
KEXAPI NTSTATUS NTAPI KexRtlQueryKeyMultipleValueData(
	IN		HANDLE												KeyHandle,
	IN		PKEX_RTL_QUERY_KEY_MULTIPLE_VARIABLE_TABLE_ENTRY	QueryTable,
	IN OUT	PULONG												NumberOfQueryTableElements,
	IN		ULONG												Flags) PROTECTED_FUNCTION
{
	ULONG Counter;

	if (!QueryTable) {
		return STATUS_INVALID_PARAMETER_2;
	}

	if (!NumberOfQueryTableElements || *NumberOfQueryTableElements == 0) {
		return STATUS_INVALID_PARAMETER_3;
	}

	Counter = *NumberOfQueryTableElements;
	*NumberOfQueryTableElements = 0;

	if (Flags & ~(QUERY_KEY_MULTIPLE_VALUE_FAIL_FAST)) {
		return STATUS_INVALID_PARAMETER_4;
	}

	do {
		QueryTable->Status = KexRtlQueryKeyValueData(
			KeyHandle,
			&QueryTable->ValueName,
			&QueryTable->ValueDataCb,
			QueryTable->ValueData,
			QueryTable->ValueDataTypeRestrict,
			&QueryTable->ValueDataType);

		if (Flags & QUERY_KEY_MULTIPLE_VALUE_FAIL_FAST) {
			if (!NT_SUCCESS(QueryTable->Status)) {
				return STATUS_UNSUCCESSFUL;
			}
		}

		++QueryTable;
		++*NumberOfQueryTableElements;
	} while (--Counter);

	return STATUS_SUCCESS;
} PROTECTED_FUNCTION_END

//
// Check whether a string ends with another string.
// For example, you can use this to see if a filename has a particular
// extension.
//
KEXAPI BOOLEAN NTAPI KexRtlUnicodeStringEndsWith(
	IN	PCUNICODE_STRING	String,
	IN	PCUNICODE_STRING	EndsWith,
	IN	BOOLEAN				CaseInsensitive) PROTECTED_FUNCTION
{
	UNICODE_STRING EndOfString;

	//
	// Create a subset of the String that just contains the
	// end of it (with the number of characters that EndsWith
	// contains).
	//

	EndOfString.Buffer = String->Buffer + KexRtlUnicodeStringCch(String) - KexRtlUnicodeStringCch(EndsWith);
	EndOfString.Length = EndsWith->Length;
	EndOfString.MaximumLength = EndsWith->Length;

	if (EndOfString.Buffer < String->Buffer) {
		// EndsWith length greater than String length
		return FALSE;
	}

	//
	// Now perform the actual check.
	//

	return RtlEqualUnicodeString(&EndOfString, EndsWith, CaseInsensitive);
} PROTECTED_FUNCTION_END_BOOLEAN

//
// Similar to RtlFindUnicodeSubstring in Win10 NTDLL (but does not
// respect NLS).
// Returns the address of the character in Haystack where Needle starts,
// or NULL if Needle could not be found.
//
KEXAPI PWCHAR NTAPI KexRtlFindUnicodeSubstring(
	PCUNICODE_STRING	Haystack,
	PCUNICODE_STRING	Needle,
	BOOLEAN				CaseInsensitive) PROTECTED_FUNCTION
{
	ULONG LengthOfNeedle;
	ULONG LengthOfHaystack;
	PWCHAR NeedleBuffer;
	PWCHAR NeedleBufferEnd;
	PWCHAR HaystackBuffer;
	PWCHAR HaystackBufferEnd;
	PWCHAR HaystackBufferRealEnd;
	PWCHAR StartOfNeedleInHaystack;
	WCHAR NeedleFirst;

	LengthOfNeedle = Needle->Length & ~1;
	LengthOfHaystack = Haystack->Length & ~1;

	if (LengthOfNeedle > LengthOfHaystack || !LengthOfHaystack || !LengthOfNeedle) {
		return NULL;
	}

	NeedleBuffer = Needle->Buffer;
	NeedleBufferEnd = (PWCHAR) (((PBYTE) NeedleBuffer) + LengthOfNeedle);
	HaystackBuffer = Haystack->Buffer;
	HaystackBufferEnd = (PWCHAR) (((PBYTE) HaystackBuffer) + LengthOfHaystack - LengthOfNeedle);
	HaystackBufferRealEnd = (PWCHAR) (((PBYTE) HaystackBufferEnd) + LengthOfNeedle);

	if (CaseInsensitive) {
		NeedleFirst = ToUpper(*NeedleBuffer);

		while (TRUE) {
			NeedleBuffer = Needle->Buffer + 1;

			while (ToUpper(*HaystackBuffer) != NeedleFirst) {
				++HaystackBuffer; // Multiple evaluation. Can't increment inside macro

				if (HaystackBuffer > HaystackBufferEnd) {
					return NULL;
				}
			}

			StartOfNeedleInHaystack = HaystackBuffer++;

			while (ToUpper(*HaystackBuffer) == ToUpper(*NeedleBuffer)) {
				++HaystackBuffer;
				++NeedleBuffer;

				if (HaystackBuffer > HaystackBufferRealEnd) {
					break;
				} else if (NeedleBuffer >= NeedleBufferEnd) {
					return StartOfNeedleInHaystack;
				}
			}
		}
	} else {
		NeedleFirst = *NeedleBuffer;

		while (TRUE) {
			NeedleBuffer = Needle->Buffer + 1;

			while (*HaystackBuffer++ != NeedleFirst) {
				if (HaystackBuffer > HaystackBufferEnd) {
					return NULL;
				}
			}

			StartOfNeedleInHaystack = HaystackBuffer - 1;

			while (*HaystackBuffer++ == *NeedleBuffer++) {
				if (HaystackBuffer > HaystackBufferRealEnd) {
					break;
				} else if (NeedleBuffer >= NeedleBufferEnd) {
					return StartOfNeedleInHaystack;
				}
			}
		}
	}
} PROTECTED_FUNCTION_END_BOOLEAN

KEXAPI VOID NTAPI KexRtlAdvanceUnicodeString(
	OUT	PUNICODE_STRING	String,
	IN	USHORT			AdvanceCb) PROTECTED_FUNCTION
{
	String->Buffer += (AdvanceCb / sizeof(WCHAR));
	String->Length -= AdvanceCb;
	String->MaximumLength -= AdvanceCb;
} PROTECTED_FUNCTION_END_VOID

KEXAPI VOID NTAPI KexRtlRetreatUnicodeString(
	OUT	PUNICODE_STRING	String,
	IN	USHORT			RetreatCb) PROTECTED_FUNCTION
{
	String->Buffer -= (RetreatCb / sizeof(WCHAR));
	String->Length += RetreatCb;
	String->MaximumLength += RetreatCb;
} PROTECTED_FUNCTION_END_VOID

KEXAPI NTSTATUS NTAPI KexRtlShiftUnicodeString(
	IN OUT	PUNICODE_STRING	String,
	IN		USHORT			ShiftCch,
	IN		WCHAR			LeftFillCharacter OPTIONAL) PROTECTED_FUNCTION
{
	USHORT ShiftCb;
	NTSTATUS Status;

	if (!String || !ShiftCch) {
		return STATUS_INVALID_PARAMETER;
	}

	ShiftCb = ShiftCch * sizeof(WCHAR);

	if (ShiftCb > String->MaximumLength) {
		return STATUS_BUFFER_TOO_SMALL;
	}

	if (!LeftFillCharacter) {
		LeftFillCharacter = ' ';
	}

	Status = STATUS_SUCCESS;

	if (String->Length + ShiftCb > String->MaximumLength) {
		String->Length = String->MaximumLength - ShiftCb;
		Status = STATUS_BUFFER_OVERFLOW;
	}

	RtlMoveMemory(String->Buffer + ShiftCch, String->Buffer, String->Length);
	__stosw((PUSHORT) String->Buffer, LeftFillCharacter, ShiftCch);
	String->Length += ShiftCb;

	return Status;
} PROTECTED_FUNCTION_END

KEXAPI ULONG NTAPI KexRtlRemoteProcessBitness(
	IN	HANDLE	ProcessHandle)
{
	NTSTATUS Status;
	ULONG_PTR Peb32;

	if (KexRtlOperatingSystemBitness() == 32) {
		return 32;
	}

	Status = NtQueryInformationProcess(
		ProcessHandle,
		ProcessWow64Information,
		&Peb32,
		sizeof(Peb32),
		NULL);

	if (NT_SUCCESS(Status) && Peb32) {
		return 32;
	} else {
		return 64;
	}
}

//
// 1. This API will automatically change the memory protections for you.
// 2. This API will automatically deal with 32/64 bit differences. (You
//    are still restricted to the 32-bit address space if you are writing
//    from a 32-bit process to a 64-bit process.)
//
KEXAPI NTSTATUS NTAPI KexRtlWriteProcessMemory(
	IN	HANDLE		ProcessHandle,
	IN	ULONG_PTR	Destination,
	IN	PVOID		Source,
	IN	SIZE_T		Cb) PROTECTED_FUNCTION
{
	PVOID DestinationPageAddress;
	SIZE_T DestinationPageSize;
	ULONG OldProtect;
	NTSTATUS Status;

	DestinationPageAddress = (PVOID) Destination;
	DestinationPageSize = Cb;

	Status = NtProtectVirtualMemory(
		ProcessHandle,
		&DestinationPageAddress,
		&DestinationPageSize,
		PAGE_READWRITE,
		&OldProtect);

	if (!NT_SUCCESS(Status)) {
		return Status;
	}

	Status = NtWriteVirtualMemory(
		ProcessHandle,
		(PVOID) Destination,
		Source,
		Cb,
		NULL);

	NtProtectVirtualMemory(
		ProcessHandle,
		&DestinationPageAddress,
		&DestinationPageSize,
		OldProtect,
		&OldProtect);

	return Status;
} PROTECTED_FUNCTION_END

//
// Recursively create or open a directory.
//
KEXAPI NTSTATUS NTAPI KexRtlCreateDirectoryRecursive(
	OUT	PHANDLE				DirectoryHandle,
	IN	ACCESS_MASK			DesiredAccess,
	IN	POBJECT_ATTRIBUTES	ObjectAttributes,
	IN	ULONG				ShareAccess) PROTECTED_FUNCTION
{
	NTSTATUS Status;
	IO_STATUS_BLOCK IoStatusBlock;
	BOOLEAN AlreadyRetried;

	AlreadyRetried = FALSE;

	//
	// Attempt to create the directory.
	//

Retry:
	Status = NtCreateFile(
		DirectoryHandle,
		DesiredAccess | SYNCHRONIZE,
		ObjectAttributes,
		&IoStatusBlock,
		NULL,
		FILE_ATTRIBUTE_NORMAL,
		ShareAccess,
		FILE_OPEN_IF,
		FILE_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT,
		NULL,
		0);

	if (!NT_SUCCESS(Status) && !AlreadyRetried) {
		HANDLE TemporaryHandle;
		OBJECT_ATTRIBUTES NewObjectAttributes;
		UNICODE_STRING ShorterPath;
		ULONG NewLength;

		//
		// If failed, chop off the last path element and try again.
		//

		NewObjectAttributes = *ObjectAttributes;
		ShorterPath = *ObjectAttributes->ObjectName;
		NewObjectAttributes.ObjectName = &ShorterPath;

		if (!ShorterPath.Length) {
			//
			// Already chopped off all path elements, so that means the root
			// of the path must not exist.
			//

			return STATUS_OBJECT_PATH_NOT_FOUND;
		}

		Status = RtlGetLengthWithoutLastFullDosOrNtPathElement(
			0,
			&ShorterPath,
			&NewLength);

		if (!NT_SUCCESS(Status)) {
			return Status;
		}

		NewLength *= sizeof(WCHAR);
		ASSERT (NewLength < ShorterPath.Length);

		ShorterPath.Length = (USHORT) NewLength;

		Status = KexRtlCreateDirectoryRecursive(
			&TemporaryHandle,
			0,
			&NewObjectAttributes,
			0);

		if (NT_SUCCESS(Status)) {
			//
			// If we succeeded, now go back and retry creating the original
			// directory again.
			//

			NtClose(TemporaryHandle);
			AlreadyRetried = TRUE;
			goto Retry;
		}
	}

	return Status;
} PROTECTED_FUNCTION_END