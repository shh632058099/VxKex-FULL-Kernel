///////////////////////////////////////////////////////////////////////////////
//
// Module Name:
//
//     tickcount.c
//
// Abstract:
//
//     Implements GetTickCount64 (introduced in Windows Vista) on top of the
//     32-bit GetTickCount, which has existed since Windows XP/Server 2003
//     and wraps around approximately every 49.7 days.
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
// Lock-free monotonic 64-bit tick counter built from the 32-bit one.
//
// LastTickCount64 always holds the highest 64-bit tick count value this
// process has observed so far. On each call we take the current 32-bit
// tick count and splice it into the low 32 bits of the last known value.
// If that makes the value go backwards, the 32-bit counter must have
// wrapped since the last call, so we add a full 2^32 to account for it.
//
// This only works correctly if GetTickCount64 (or GetTickCount, indirectly
// through this function) is called at least once per ~49.7 day wrap
// period. That is true of any process that calls this function with any
// regularity, which is the expected use case for a compatibility shim.
//
ULONGLONG WINAPI Ext_GetTickCount64(VOID)
{
	static volatile LONG64 LastTickCount64 = 0;
	LONG64 Previous;
	LONG64 Candidate;
	ULONG Current32;

	Current32 = GetTickCount();

	do {
		Previous = LastTickCount64;
		Candidate = (Previous & ~0xFFFFFFFFI64) | (LONG64) Current32;

		if (Candidate < Previous) {
			// The 32-bit tick count wrapped since the last observation.
			Candidate += 0x100000000I64;
		}

		if (Candidate == Previous) {
			break;
		}
	} while (InterlockedCompareExchange64(&LastTickCount64, Candidate, Previous) != Previous);

	return (ULONGLONG) Candidate;
}
