///////////////////////////////////////////////////////////////////////////////
//
// Module Name:
//
//     xpfallback.c
//
// Abstract:
//
//     Runtime-forwarding wrappers for kernel32 functions which do not exist
//     on Windows XP/Server 2003. Each exported wrapper probes the REAL
//     kernel32.dll for the function (the result is cached in a static
//     pointer, exactly like the AddDllDirectory handling in module.c) and
//     forwards to it when found - which is always the case on Windows
//     Vista/7/8/8.1, so behaviour on VxKex's supported hosts is unchanged.
//
//     When the real kernel32 does not export the function (i.e. the process
//     is running on Windows XP/Server 2003), the wrapper calls one of the
//     Kxxp* implementation routines. Those routines are compiled into this
//     project as well as into the standalone KxXp module (they are the same
//     source files: procthread.c, tickcount.c and processor.c), so there is
//     no runtime dependency on KxXp.dll being present or loaded.
//
//     Note: this deliberately does NOT set the last error code when the
//     probe fails, unlike BasepGetDllDirectoryProcedure in module.c - our
//     fallback implementations generally succeed, and clobbering the last
//     error on success would be wrong.
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
#include "kxbasep.h"

//
// Implementation routines shared with the KxXp module.
//

DWORD WINAPI KxxpGetThreadId(
	IN	HANDLE	Thread);

DWORD WINAPI KxxpGetProcessId(
	IN	HANDLE	Process);

DWORD WINAPI KxxpGetProcessIdOfThread(
	IN	HANDLE	Thread);

DECLSPEC_NORETURN VOID WINAPI KxxpRaiseFailFastException(
	IN	PEXCEPTION_RECORD	ExceptionRecord OPTIONAL,
	IN	PCONTEXT			ContextRecord OPTIONAL,
	IN	DWORD				Flags);

ULONGLONG WINAPI KxxpGetTickCount64(VOID);

WORD WINAPI KxxpGetActiveProcessorGroupCount(VOID);

WORD WINAPI KxxpGetMaximumProcessorGroupCount(VOID);

DWORD WINAPI KxxpGetActiveProcessorCount(
	IN	WORD	GroupNumber);

DWORD WINAPI KxxpGetMaximumProcessorCount(
	IN	WORD	GroupNumber);

DWORD WINAPI KxxpGetCurrentProcessorNumber(VOID);

VOID WINAPI KxxpGetCurrentProcessorNumberEx(
	OUT	PPROCESSOR_NUMBER	ProcNumber);

//
// Probe the real kernel32 for a procedure address.
//
// On success, *ProcedureAddress receives the address of the real function.
// On failure (the function does not exist on this OS - e.g. XP/Server 2003),
// *ProcedureAddress is left untouched so the caller falls back to its own
// implementation.
//
// Thread safety: if two threads race here during first use, both perform the
// LdrGetProcedureAddress lookup and both store the identical result. This is
// benign (pointer-sized stores are atomic) and is the same pattern used by
// module.c.
//
STATIC VOID KexpProbeKernel32Procedure(
	IN	PCSTR	ProcedureName,
	IN OUT	PPVOID	ProcedureAddress)
{
	NTSTATUS Status;
	ANSI_STRING ProcedureNameAS;

	ASSERT (KexData != NULL);
	ASSUME (KexData->BaseDllBase != NULL);
	ASSERT (ProcedureName != NULL);
	ASSERT (ProcedureAddress != NULL);

	if (*ProcedureAddress) {
		return;
	}

	Status = RtlInitAnsiStringEx(&ProcedureNameAS, ProcedureName);
	if (!NT_SUCCESS(Status)) {
		return;
	}

	Status = LdrGetProcedureAddress(
		KexData->BaseDllBase,
		&ProcedureNameAS,
		0,
		ProcedureAddress);

	//
	// Failure is expected on XP/Server 2003; leave *ProcedureAddress as-is.
	//

	if (!NT_SUCCESS(Status)) {
		*ProcedureAddress = NULL;
	}
}

KXBASEAPI DWORD WINAPI Ext_GetThreadId(
	IN	HANDLE	Thread)
{
	STATIC DWORD (WINAPI *RealGetThreadId)(HANDLE) = NULL;

	KexpProbeKernel32Procedure("GetThreadId", (PPVOID) &RealGetThreadId);

	if (RealGetThreadId) {
		return RealGetThreadId(Thread);
	}

	return KxxpGetThreadId(Thread);
}

KXBASEAPI DWORD WINAPI Ext_GetProcessId(
	IN	HANDLE	Process)
{
	STATIC DWORD (WINAPI *RealGetProcessId)(HANDLE) = NULL;

	KexpProbeKernel32Procedure("GetProcessId", (PPVOID) &RealGetProcessId);

	if (RealGetProcessId) {
		return RealGetProcessId(Process);
	}

	return KxxpGetProcessId(Process);
}

KXBASEAPI DWORD WINAPI Ext_GetProcessIdOfThread(
	IN	HANDLE	Thread)
{
	STATIC DWORD (WINAPI *RealGetProcessIdOfThread)(HANDLE) = NULL;

	KexpProbeKernel32Procedure("GetProcessIdOfThread", (PPVOID) &RealGetProcessIdOfThread);

	if (RealGetProcessIdOfThread) {
		return RealGetProcessIdOfThread(Thread);
	}

	return KxxpGetProcessIdOfThread(Thread);
}

//
// The real RaiseFailFastException never returns. Our fallback doesn't either.
// The infinite loop after the forwarded call exists only to satisfy the
// compiler that this DECLSPEC_NORETURN function cannot return if the real
// implementation were ever to return unexpectedly.
//
KXBASEAPI DECLSPEC_NORETURN VOID WINAPI Ext_RaiseFailFastException(
	IN	PEXCEPTION_RECORD	ExceptionRecord OPTIONAL,
	IN	PCONTEXT			ContextRecord OPTIONAL,
	IN	DWORD				Flags)
{
	STATIC VOID (WINAPI *RealRaiseFailFastException)(PEXCEPTION_RECORD, PCONTEXT, DWORD) = NULL;

	KexpProbeKernel32Procedure("RaiseFailFastException", (PPVOID) &RealRaiseFailFastException);

	if (RealRaiseFailFastException) {
		RealRaiseFailFastException(ExceptionRecord, ContextRecord, Flags);
		for (;;) {
			;
		}
	}

	KxxpRaiseFailFastException(ExceptionRecord, ContextRecord, Flags);
}

KXBASEAPI ULONGLONG WINAPI Ext_GetTickCount64(VOID)
{
	STATIC ULONGLONG (WINAPI *RealGetTickCount64)(VOID) = NULL;

	KexpProbeKernel32Procedure("GetTickCount64", (PPVOID) &RealGetTickCount64);

	if (RealGetTickCount64) {
		return RealGetTickCount64();
	}

	return KxxpGetTickCount64();
}

KXBASEAPI WORD WINAPI Ext_GetActiveProcessorGroupCount(VOID)
{
	STATIC WORD (WINAPI *RealGetActiveProcessorGroupCount)(VOID) = NULL;

	KexpProbeKernel32Procedure("GetActiveProcessorGroupCount", (PPVOID) &RealGetActiveProcessorGroupCount);

	if (RealGetActiveProcessorGroupCount) {
		return RealGetActiveProcessorGroupCount();
	}

	return KxxpGetActiveProcessorGroupCount();
}

KXBASEAPI WORD WINAPI Ext_GetMaximumProcessorGroupCount(VOID)
{
	STATIC WORD (WINAPI *RealGetMaximumProcessorGroupCount)(VOID) = NULL;

	KexpProbeKernel32Procedure("GetMaximumProcessorGroupCount", (PPVOID) &RealGetMaximumProcessorGroupCount);

	if (RealGetMaximumProcessorGroupCount) {
		return RealGetMaximumProcessorGroupCount();
	}

	return KxxpGetMaximumProcessorGroupCount();
}

KXBASEAPI DWORD WINAPI Ext_GetActiveProcessorCount(
	IN	WORD	GroupNumber)
{
	STATIC DWORD (WINAPI *RealGetActiveProcessorCount)(WORD) = NULL;

	KexpProbeKernel32Procedure("GetActiveProcessorCount", (PPVOID) &RealGetActiveProcessorCount);

	if (RealGetActiveProcessorCount) {
		return RealGetActiveProcessorCount(GroupNumber);
	}

	return KxxpGetActiveProcessorCount(GroupNumber);
}

KXBASEAPI DWORD WINAPI Ext_GetMaximumProcessorCount(
	IN	WORD	GroupNumber)
{
	STATIC DWORD (WINAPI *RealGetMaximumProcessorCount)(WORD) = NULL;

	KexpProbeKernel32Procedure("GetMaximumProcessorCount", (PPVOID) &RealGetMaximumProcessorCount);

	if (RealGetMaximumProcessorCount) {
		return RealGetMaximumProcessorCount(GroupNumber);
	}

	return KxxpGetMaximumProcessorCount(GroupNumber);
}

KXBASEAPI DWORD WINAPI Ext_GetCurrentProcessorNumber(VOID)
{
	STATIC DWORD (WINAPI *RealGetCurrentProcessorNumber)(VOID) = NULL;

	KexpProbeKernel32Procedure("GetCurrentProcessorNumber", (PPVOID) &RealGetCurrentProcessorNumber);

	if (RealGetCurrentProcessorNumber) {
		return RealGetCurrentProcessorNumber();
	}

	return KxxpGetCurrentProcessorNumber();
}

KXBASEAPI VOID WINAPI Ext_GetCurrentProcessorNumberEx(
	OUT	PPROCESSOR_NUMBER	ProcNumber)
{
	STATIC VOID (WINAPI *RealGetCurrentProcessorNumberEx)(PPROCESSOR_NUMBER) = NULL;

	KexpProbeKernel32Procedure("GetCurrentProcessorNumberEx", (PPVOID) &RealGetCurrentProcessorNumberEx);

	if (RealGetCurrentProcessorNumberEx) {
		RealGetCurrentProcessorNumberEx(ProcNumber);
		return;
	}

	KxxpGetCurrentProcessorNumberEx(ProcNumber);
}
