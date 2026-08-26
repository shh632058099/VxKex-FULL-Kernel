///////////////////////////////////////////////////////////////////////////////
//
// Module Name:
//
//     processor.c
//
// Abstract:
//
//     Implements the processor group query APIs (introduced in Windows 7,
//     which introduced processor groups) as trivial single-group answers,
//     since Windows XP/Server 2003 predate the concept entirely and every
//     logical processor is implicitly in one group.
//
//     GetCurrentProcessorNumber(Ex) is implemented as a fixed stub - see the
//     comment above it for why, and CLAUDE_CONTEXT.md for the follow-up.
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

#ifndef ALL_PROCESSOR_GROUPS
#define ALL_PROCESSOR_GROUPS 0xFFFF
#endif

WORD WINAPI KxxpGetActiveProcessorGroupCount(VOID)
{
	return 1;
}

WORD WINAPI KxxpGetMaximumProcessorGroupCount(VOID)
{
	return 1;
}

DWORD WINAPI KxxpGetActiveProcessorCount(
	IN	WORD	GroupNumber)
{
	SYSTEM_INFO SystemInfo;

	if (GroupNumber != 0 && GroupNumber != ALL_PROCESSOR_GROUPS) {
		SetLastError(ERROR_INVALID_PARAMETER);
		return 0;
	}

	GetSystemInfo(&SystemInfo);
	return SystemInfo.dwNumberOfProcessors;
}

DWORD WINAPI KxxpGetMaximumProcessorCount(
	IN	WORD	GroupNumber)
{
	// On a system with only one (implicit) processor group, "active" and
	// "maximum" processor counts are the same thing - Windows XP/Server
	// 2003 have no concept of processors becoming available at runtime
	// beyond what GetSystemInfo already reports.
	return KxxpGetActiveProcessorCount(GroupNumber);
}

//
// GetCurrentProcessorNumber(Ex) has no clean pre-Vista equivalent: there is
// no documented, portable way to ask the NT kernel which logical processor
// the calling thread is currently running on without either an inline-asm
// trick (whose safety we can't verify without a real multi-core test rig)
// or an undocumented KUSER_SHARED_DATA/KPCR layout assumption. Rather than
// guess, this reports "processor 0" unconditionally, which is a safe lie:
// callers that use this purely as a cache/contention-avoidance hint
// (the overwhelmingly common use) degrade to "no hint", not to incorrect
// behaviour. Callers that require a real answer are not well served by
// this stub - see CLAUDE_CONTEXT.md.
//
DWORD WINAPI KxxpGetCurrentProcessorNumber(VOID)
{
	return 0;
}

VOID WINAPI KxxpGetCurrentProcessorNumberEx(
	OUT	PPROCESSOR_NUMBER	ProcNumber)
{
	ProcNumber->Group = 0;
	ProcNumber->Number = 0;
	ProcNumber->Reserved = 0;
}
