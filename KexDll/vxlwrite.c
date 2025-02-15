#include "buildcfg.h"
#include "kexdllp.h"

#pragma warning(disable:4244)	// conversion from ULONG to USHORT
#pragma warning(disable:4018)	// signed/unsigned mismatch

NTSTATUS CDECL VxlWriteLogEx(
	IN		VXLHANDLE		LogHandle,
	IN		PCWSTR			SourceComponent OPTIONAL,
	IN		PCWSTR			SourceFile OPTIONAL,
	IN		ULONG			SourceLine,
	IN		PCWSTR			SourceFunction OPTIONAL,
	IN		VXLSEVERITY		Severity,
	IN		PCWSTR			Format,
	IN		...) PROTECTED_FUNCTION
{
	NTSTATUS Status;
	ARGLIST ArgList;
	PVXLLOGFILEENTRY FileEntry;
	ULONG FileEntryCb;
	PTEB Teb;
	IO_STATUS_BLOCK IoStatusBlock;
	LONGLONG EndOfFileOffset;

	//
	// param validation
	//

	if (!LogHandle || !Format) {
		return STATUS_INVALID_PARAMETER;
	}

	if (Severity < 0 || Severity > LogSeverityMaximumValue) {
		return STATUS_INVALID_PARAMETER;
	}

	//
	// assign default values to optional parameters
	//

	if (!SourceComponent) {
		SourceComponent = L"";
	}

	if (!SourceFile) {
		SourceFile = L"";
	}

	if (!SourceFunction) {
		SourceFunction = L"";
	}

	Status = STATUS_SUCCESS;
	FileEntryCb = sizeof(VXLLOGFILEENTRY);
	Teb = NtCurrentTeb();

	va_start(ArgList, Format);

	try {
		HRESULT Result;
		SIZE_T TextCchSizeT;
		ULONG TextCch;
		PWSTR DoubleNewLine;

		//
		// find out how many text characters in the log entry
		//

		Result = StringCchVPrintfBufferLength(&TextCchSizeT, Format, ArgList);
		if (FAILED(Result)) {
			// must be because of an invalid format string
			Status = STATUS_INVALID_PARAMETER;
			leave;
		}

		TextCch = (ULONG) TextCchSizeT;

		//
		// allocate memory for log entry and format text into the buffer
		//

		FileEntryCb += TextCch * sizeof(WCHAR);

		if (FileEntryCb > 0xFFFF) {
			Status = STATUS_BUFFER_TOO_SMALL;
			leave;
		}

		// It's important that we avoid performing any heap allocations, directly
		// or indirectly, in VxlWriteLog. This is because we want VxlWriteLog to
		// always function, even in cases of no system memory.
		FileEntry = (PVXLLOGFILEENTRY) StackAlloc(BYTE, FileEntryCb);
		RtlZeroMemory(FileEntry, FileEntryCb);

		Result = StringCchVPrintf(FileEntry->Text, TextCch, Format, ArgList);
		if (FAILED(Result)) {
			Status = STATUS_INTERNAL_ERROR;
			leave;
		}

		//
		// If this is a debug build, write the formatted log message to the
		// debugging console so the developer doesn't need to open VxlView all
		// the time.
		//

		if (KexIsDebugBuild) {
			DbgPrint("VXL (%ws): %ws\r\n", SourceComponent, FileEntry->Text);
		}

		//
		// Check for a double newline (\r\n\r\n) and replace the first \r\n
		// out of the two \r\n's with a single null character (\0), unless
		// the double newline occurs at the end of the string.
		//

		DoubleNewLine = wcsstr(FileEntry->Text, L"\r\n\r\n");

		if (DoubleNewLine && DoubleNewLine[4]) {
			*DoubleNewLine = '\0';
			
			RtlMoveMemory(
				DoubleNewLine + 1,
				DoubleNewLine + 4,
				(TextCch - (DoubleNewLine + 4 - FileEntry->Text)) * sizeof(WCHAR));

			TextCch -= 3;
			FileEntry->TextHeaderCch = DoubleNewLine - FileEntry->Text + 1;
			FileEntry->TextCch = TextCch - FileEntry->TextHeaderCch;

			ASSERT (wcslen(FileEntry->Text + FileEntry->TextHeaderCch) == FileEntry->TextCch - 1);

			// recalculate new size of log file entry, since we reduced its
			// size by 3 characters
			FileEntryCb -= 3 * sizeof(WCHAR);
		} else {
			FileEntry->TextHeaderCch = TextCch;
			// FileEntry->TextCch remains 0 since it was zeroed at allocation
		}

		ASSERT (wcslen(FileEntry->Text) == FileEntry->TextHeaderCch - 1);
		ASSERT (FileEntry->TextCch + FileEntry->TextHeaderCch <= TextCch);

		//
		// Fill out remaining fields in the log file entry that do not require
		// interacting with the log file header.
		//

		KexNtQuerySystemTime((PLONGLONG) &FileEntry->Time64);
		FileEntry->ProcessId = (ULONG) Teb->ClientId.UniqueProcess;
		FileEntry->ThreadId = (ULONG) Teb->ClientId.UniqueThread;
		FileEntry->Severity = Severity;
		FileEntry->SourceLine = SourceLine;

		Status = STATUS_SUCCESS;
	} except (EXCEPTION_EXECUTE_HANDLER) {
		Status = GetExceptionCode();
	}

	va_end(ArgList);

	if (Status != STATUS_SUCCESS) {
		return Status;
	}

	RtlAcquireSRWLockExclusive(&LogHandle->Lock);

	try {
		//
		// fill out source component, file, and function indices
		//

		Status = VxlpFindOrCreateSourceComponentIndex(
			LogHandle,
			SourceComponent,
			&FileEntry->SourceComponentIndex);

		if (!NT_SUCCESS(Status)) {
			leave;
		}

		Status = VxlpFindOrCreateSourceFileIndex(
			LogHandle,
			SourceFile,
			&FileEntry->SourceFileIndex);

		if (!NT_SUCCESS(Status)) {
			leave;
		}

		Status = VxlpFindOrCreateSourceFunctionIndex(
			LogHandle,
			SourceFunction,
			&FileEntry->SourceFunctionIndex);

		if (!NT_SUCCESS(Status)) {
			leave;
		}

		//
		// update severity count
		//

		++LogHandle->Header->EventSeverityTypeCount[Severity];

		//
		// write the actual log entry to the file
		//

		// Passing -1 causes the write to occur at the end of the file.
		EndOfFileOffset = -1;

		Status = NtWriteFile(
			LogHandle->FileHandle,
			NULL,
			NULL,
			NULL,
			&IoStatusBlock,
			FileEntry,
			FileEntryCb,
			&EndOfFileOffset,
			NULL);
	} except (EXCEPTION_EXECUTE_HANDLER) {
		Status = GetExceptionCode();
	}

	RtlReleaseSRWLockExclusive(&LogHandle->Lock);
	return Status;
} PROTECTED_FUNCTION_END