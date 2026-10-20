///////////////////////////////////////////////////////////////////////////////
//
// Module Name:
//
//     stubs.c
//
// Abstract:
//
//     Forwarder stubs that do nothing except for calling the original function.
//     These exist because some stupid software such as Chromium is not
//     compatible with export forwarders.
//
// Author:
//
//     vxiiduu (09-Mar-2024)
//
// Environment:
//
//     Win32
//
// Revision History:
//
//     vxiiduu              09-Mar-2024  Initial creation.
//
///////////////////////////////////////////////////////////////////////////////

#include "buildcfg.h"
#include "kxbasep.h"

KXBASEAPI BOOL WINAPI Stub_DuplicateHandle(
	IN	HANDLE		SourceProcessHandle,
	IN	HANDLE		SourceHandle,
	IN	HANDLE		TargetProcessHandle,
	OUT	PHANDLE		TargetHandle,
	IN	ACCESS_MASK	DesiredAccess,
	IN	BOOL		Inherit,
	IN	ULONG		Options)
{
	return DuplicateHandle(
		SourceProcessHandle,
		SourceHandle,
		TargetProcessHandle,
		TargetHandle,
		DesiredAccess,
		Inherit,
		Options);
}

static VOID NTAPI KxBasepRtlGetDeviceFamilyInfoEnum(
	OUT	PULONGLONG	UAPInfo OPTIONAL,
	OUT	PULONG		DeviceFamily OPTIONAL,
	OUT	PULONG		DeviceForm OPTIONAL)
{
	if (UAPInfo != NULL) {
		*UAPInfo = (NtCurrentPeb()->OSBuildNumber << 16) + 3570;
	}

	if (DeviceFamily != NULL) {
		*DeviceFamily = DEVICEFAMILYINFOENUM_DESKTOP;
	}

	if (DeviceForm != NULL) {
		*DeviceForm = DEVICEFAMILYDEVICEFORM_DESKTOP;
	}
}

KXBASEAPI FARPROC WINAPI Stub_GetProcAddress(
	IN	HMODULE		ModuleHandle,
	IN	PCSTR		ProcedureName)
{
	if ((KexData->Flags & KEXDATA_FLAG_CHROMIUM) &&
		ModuleHandle == (HMODULE) KexData->SystemDllBase &&
		ProcedureName != NULL &&
		StringEqualA(ProcedureName, "RtlGetDeviceFamilyInfoEnum")) {

		KexLogDebugEvent(L"Returning RtlGetDeviceFamilyInfoEnum compatibility shim for Chromium");
		return (FARPROC) KxBasepRtlGetDeviceFamilyInfoEnum;
	}

	return GetProcAddress(
		ModuleHandle,
		ProcedureName);
}