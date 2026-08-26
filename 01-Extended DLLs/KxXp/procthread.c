///////////////////////////////////////////////////////////////////////////////
//
// Module Name:
//
//     procthread.c
//
// Abstract:
//
//     Implements a handful of process/thread information APIs that were
//     introduced after Windows XP/Server 2003 (mostly in Vista) and are
//     therefore missing on those targets. Each one is a thin wrapper around
//     an NT native API that has existed since NT4, so there is nothing
//     version-specific being relied upon here.
//
// Author:
//
//     Claude (25-Aug-2026)
//
// Environment:
//
//     Win32
//
// Revision History:
//
//     Claude                25-Aug-2026  Initial creation.
//
///////////////////////////////////////////////////////////////////////////////

#include "buildcfg.h"
#include <KexComm.h>

//
// GetThreadId, GetProcessId and GetProcessIdOfThread were all introduced in
// Windows Vista/Server 2008. They are simple accessors for information that
// NtQueryInformationThread/Process has always been able to return.
//

DWORD WINAPI KxxpGetThreadId(
	IN	HANDLE	Thread)
{
	THREAD_BASIC_INFORMATION Tbi;
	NTSTATUS Status;

	Status = NtQueryInformationThread(Thread, ThreadBasicInformation, &Tbi, sizeof(Tbi), NULL);

	if (!NT_SUCCESS(Status)) {
		SetLastError(RtlNtStatusToDosError(Status));
		return 0;
	}

	return (DWORD) (ULONG_PTR) Tbi.ClientId.UniqueThread;
}

DWORD WINAPI KxxpGetProcessId(
	IN	HANDLE	Process)
{
	PROCESS_BASIC_INFORMATION Pbi;
	NTSTATUS Status;

	Status = NtQueryInformationProcess(Process, ProcessBasicInformation, &Pbi, sizeof(Pbi), NULL);

	if (!NT_SUCCESS(Status)) {
		SetLastError(RtlNtStatusToDosError(Status));
		return 0;
	}

	return (DWORD) Pbi.UniqueProcessId;
}

DWORD WINAPI KxxpGetProcessIdOfThread(
	IN	HANDLE	Thread)
{
	THREAD_BASIC_INFORMATION Tbi;
	NTSTATUS Status;

	Status = NtQueryInformationThread(Thread, ThreadBasicInformation, &Tbi, sizeof(Tbi), NULL);

	if (!NT_SUCCESS(Status)) {
		SetLastError(RtlNtStatusToDosError(Status));
		return 0;
	}

	return (DWORD) (ULONG_PTR) Tbi.ClientId.UniqueProcess;
}

//
// RaiseFailFastException (Vista+) is meant to terminate the process
// immediately and uncatchably - it's used by /GS cookie checks, CFG
// violations, heap corruption detection, etc. The real implementation also
// notifies any registered vectored exception handlers with the
// FAIL_FAST_GENERATE_EXCEPTION_ADDRESS behaviour before terminating; that
// part is intentionally NOT implemented here; a caller relying on it seeing
// their VEH run before the process dies will not get that. What every
// caller actually depends on - that the process dies immediately and this
// call never returns - is preserved. See CLAUDE_CONTEXT.md for follow-up.
//
DECLSPEC_NORETURN
VOID WINAPI KxxpRaiseFailFastException(
	IN	PEXCEPTION_RECORD	ExceptionRecord OPTIONAL,
	IN	PCONTEXT			ContextRecord OPTIONAL,
	IN	DWORD				Flags)
{
	UNREFERENCED_PARAMETER(ExceptionRecord);
	UNREFERENCED_PARAMETER(ContextRecord);
	UNREFERENCED_PARAMETER(Flags);

	if (IsDebuggerPresent()) {
		DebugBreak();
	}

	// 0xC0000409 is STATUS_STACK_BUFFER_OVERRUN, the same code real
	// RaiseFailFastException terminates with.
	TerminateProcess(GetCurrentProcess(), 0xC0000409);

	// TerminateProcess on the current process does not return, but the
	// compiler doesn't know that for a function pointer call, so loop.
	for (;;) {
		;
	}
}
