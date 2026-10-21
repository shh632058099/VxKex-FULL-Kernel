#include "buildcfg.h"
#include "kxcomp.h"

typedef struct _EDGE_APIINFO EDGE_APIINFO;
typedef struct _EDGE_APIINFO_VTABLE {
	HRESULT (STDMETHODCALLTYPE *QueryInterface)(EDGE_APIINFO *, REFIID, PVOID *);
	ULONG (STDMETHODCALLTYPE *AddRef)(EDGE_APIINFO *);
	ULONG (STDMETHODCALLTYPE *Release)(EDGE_APIINFO *);
	HRESULT (STDMETHODCALLTYPE *GetIids)(EDGE_APIINFO *, ULONG *, IID **);
	HRESULT (STDMETHODCALLTYPE *GetRuntimeClassName)(EDGE_APIINFO *, HSTRING *);
	HRESULT (STDMETHODCALLTYPE *GetTrustLevel)(EDGE_APIINFO *, TrustLevel *);
	HRESULT (STDMETHODCALLTYPE *IsTypePresent)(EDGE_APIINFO *, HSTRING, BOOLEAN *);
	HRESULT (STDMETHODCALLTYPE *IsMethodPresent)(EDGE_APIINFO *, HSTRING, HSTRING, BOOLEAN *);
	HRESULT (STDMETHODCALLTYPE *IsMethodPresentWithArity)(EDGE_APIINFO *, HSTRING, HSTRING, UINT32, BOOLEAN *);
	HRESULT (STDMETHODCALLTYPE *IsEventPresent)(EDGE_APIINFO *, HSTRING, HSTRING, BOOLEAN *);
	HRESULT (STDMETHODCALLTYPE *IsPropertyPresent)(EDGE_APIINFO *, HSTRING, HSTRING, BOOLEAN *);
	HRESULT (STDMETHODCALLTYPE *IsReadOnlyPropertyPresent)(EDGE_APIINFO *, HSTRING, HSTRING, BOOLEAN *);
	HRESULT (STDMETHODCALLTYPE *IsWriteablePropertyPresent)(EDGE_APIINFO *, HSTRING, HSTRING, BOOLEAN *);
	HRESULT (STDMETHODCALLTYPE *IsEnumNamedValuePresent)(EDGE_APIINFO *, HSTRING, HSTRING, BOOLEAN *);
	HRESULT (STDMETHODCALLTYPE *IsApiContractPresent)(EDGE_APIINFO *, HSTRING, UINT16, BOOLEAN *);
	HRESULT (STDMETHODCALLTYPE *IsApiContractPresentByMajorAndMinor)(EDGE_APIINFO *, HSTRING, UINT16, UINT16, BOOLEAN *);
} EDGE_APIINFO_VTABLE;

struct _EDGE_APIINFO {
	const EDGE_APIINFO_VTABLE *lpVtbl;
};

static const IID EdgeApiInfoIID = {
	0x997439fe, 0xf681, 0x4a11,
	{0xb4, 0x16, 0xc1, 0x3a, 0x47, 0xe8, 0xba, 0x36}
};

static HRESULT STDMETHODCALLTYPE EdgeApiInfoQueryInterface(
	EDGE_APIINFO *This, REFIID InterfaceId, PVOID *Result)
{
	if (!Result) {
		return E_POINTER;
	}
	*Result = NULL;
	if (IsEqualIID(InterfaceId, &EdgeApiInfoIID) ||
		IsEqualIID(InterfaceId, &IID_IUnknown) ||
		IsEqualIID(InterfaceId, &IID_IInspectable) ||
		IsEqualIID(InterfaceId, &IID_IAgileObject)) {
		*Result = This;
		return S_OK;
	}
	return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE EdgeApiInfoAddRef(EDGE_APIINFO *This) { return 2; }
static ULONG STDMETHODCALLTYPE EdgeApiInfoRelease(EDGE_APIINFO *This) { return 1; }

static HRESULT STDMETHODCALLTYPE EdgeApiInfoGetIids(
	EDGE_APIINFO *This, ULONG *Count, IID **Array)
{
	if (!Count || !Array) {
		return E_POINTER;
	}
	*Count = 0;
	*Array = NULL;
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE EdgeApiInfoGetRuntimeClassName(
	EDGE_APIINFO *This, HSTRING *Name)
{
	if (!Name) {
		return E_POINTER;
	}
	return WindowsCreateString(
		L"Windows.Foundation.Metadata.ApiInformation", 42, Name);
}

static HRESULT STDMETHODCALLTYPE EdgeApiInfoGetTrustLevel(
	EDGE_APIINFO *This, TrustLevel *Level)
{
	if (!Level) {
		return E_POINTER;
	}
	*Level = BaseTrust;
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE EdgeApiInfoFalse(EDGE_APIINFO *This, HSTRING Name, BOOLEAN *Value)
{
	if (!Value) { return E_POINTER; }
	*Value = FALSE;
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE EdgeApiInfoFalse2(
	EDGE_APIINFO *This, HSTRING TypeName, HSTRING MemberName, BOOLEAN *Value)
{
	if (!Value) { return E_POINTER; }
	*Value = FALSE;
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE EdgeApiInfoFalse3(
	EDGE_APIINFO *This, HSTRING TypeName, HSTRING MemberName, UINT32 Arity, BOOLEAN *Value)
{
	if (!Value) { return E_POINTER; }
	*Value = FALSE;
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE EdgeApiInfoFalseContract(
	EDGE_APIINFO *This, HSTRING ContractName, UINT16 Major, BOOLEAN *Value)
{
	if (!Value) { return E_POINTER; }
	*Value = FALSE;
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE EdgeApiInfoFalseContractMinor(
	EDGE_APIINFO *This, HSTRING ContractName, UINT16 Major, UINT16 Minor, BOOLEAN *Value)
{
	if (!Value) { return E_POINTER; }
	*Value = FALSE;
	return S_OK;
}

static const EDGE_APIINFO_VTABLE EdgeApiInfoVtable = {
	EdgeApiInfoQueryInterface,
	EdgeApiInfoAddRef,
	EdgeApiInfoRelease,
	EdgeApiInfoGetIids,
	EdgeApiInfoGetRuntimeClassName,
	EdgeApiInfoGetTrustLevel,
	EdgeApiInfoFalse,
	EdgeApiInfoFalse2,
	EdgeApiInfoFalse3,
	EdgeApiInfoFalse2,
	EdgeApiInfoFalse2,
	EdgeApiInfoFalse2,
	EdgeApiInfoFalse2,
	EdgeApiInfoFalse2,
	EdgeApiInfoFalseContract,
	EdgeApiInfoFalseContractMinor
};

static EDGE_APIINFO EdgeApiInformation = { &EdgeApiInfoVtable };

// Only provide compatibility activation factories for runtime classes implemented by KxCom.
// Returning the generic IActivationFactory for an unrelated class can make callers treat
// its seven-slot vtable as a class-specific interface and call beyond the vtable.
static BOOL IsKxComFallbackActivationFactoryClass(PCWSTR ClassName)
{
	return ClassName != NULL && (
		lstrcmpW(ClassName, L"Windows.System.UserProfile.GlobalizationPreferences") == 0 ||
		lstrcmpW(ClassName, L"Windows.UI.ViewManagement.UIViewSettings") == 0 ||
		lstrcmpW(ClassName, L"Windows.UI.ViewManagement.UISettings") == 0 ||
		lstrcmpW(ClassName, L"Windows.Foundation.Uri") == 0 ||
		lstrcmpW(ClassName, L"Windows.System.Launcher") == 0);
}
#include "buildcfg.h"
#include "kxcomp.h"

KXCOMAPI HRESULT WINAPI RoGetActivationFactory(
	IN	HSTRING	ActivatableClassId,
	IN	REFIID	RefIID,
	OUT	PPVOID	Factory)
{
	LPOLESTR RefIIDAsString;
	PCWSTR ClassName = WindowsGetStringRawBuffer(ActivatableClassId, NULL);
	HRESULT NativeResult = E_NOTIMPL;

	if (Factory == NULL || RefIID == NULL) {
		return E_POINTER;
	}
	*Factory = NULL;

	StringFromIID(RefIID, &RefIIDAsString);

	KexLogDetailEvent(
		L"RoGetActivationFactory called\r\n\r\n"
		L"ActivatableClassId: %s\r\n"
		L"IID:                %s",
		ClassName,
		RefIIDAsString);

	CoTaskMemFree(RefIIDAsString);

	if (ClassName != NULL && lstrcmpW(ClassName,
		L"Windows.Foundation.Metadata.ApiInformation") == 0) {
		return EdgeApiInfoQueryInterface(&EdgeApiInformation, RefIID, Factory);
	}

	// Prefer native WinRT factories for APIs implemented by Windows 8.1.
	// Only use VxKex fallback factories for runtime classes implemented by KxCom.
	{
		typedef HRESULT (WINAPI *RO_GET_ACTIVATION_FACTORY)(HSTRING, REFIID, PPVOID);
		HMODULE CombaseModule = GetModuleHandleW(L"combase.dll");

		if (CombaseModule != NULL) {
			RO_GET_ACTIVATION_FACTORY NativeFactory =
				(RO_GET_ACTIVATION_FACTORY) GetProcAddress(CombaseModule, "RoGetActivationFactory");

			if (NativeFactory != NULL) {
				NativeResult = NativeFactory(ActivatableClassId, RefIID, Factory);

				if (SUCCEEDED(NativeResult)) {
					return NativeResult;
				}
				*Factory = NULL;
			}
		}
	}

	if (!IsKxComFallbackActivationFactoryClass(ClassName)) {
		return NativeResult;
	}

	return CActivationFactory_QueryInterface(&CActivationFactory, RefIID, Factory);
}
// Added for Cavalry
KXCOMAPI HRESULT WINAPI RoRegisterActivationFactories(
	IN	HSTRING					*ActivatableClassIds,
	IN	PPVOID					ActivationFactoryCallbacks,
	IN	ULONG					Count,
	OUT	PRO_REGISTRATION_COOKIE	Cookie)
{
	KexLogWarningEvent(L"Unimplemented function RoRegisterActivationFactories called.");
	return E_NOTIMPL;
}

// Added for Cavalry
KXCOMAPI VOID WINAPI RoRevokeActivationFactories(
	IN	RO_REGISTRATION_COOKIE	Cookie)
{
	KexLogWarningEvent(L"Unimplemented function RoRevokeActivationFactories called.");
	return;
}

KXCOMAPI HRESULT WINAPI RoActivateInstance(
	IN	HSTRING			ActivatableClassId,
	OUT	IInspectable	**Instance)
{
	PCWSTR ClassName = WindowsGetStringRawBuffer(ActivatableClassId, NULL);

	ASSERT (Instance != NULL);
	if (Instance == NULL) {
		return E_POINTER;
	}
	*Instance = NULL;

	KexLogDetailEvent(
		L"RoActivateInstance called: %s",
		ClassName);

	// Preserve the explicit compatibility blocks for these classes.
	if (ClassName != NULL && (
		lstrcmpW(ClassName, L"Windows.Management.Deployment.PackageManager") == 0 ||
		lstrcmpW(ClassName, L"Windows.Internal.Security.WebAuthentication.AuthenticationManager") == 0)) {
		return REGDB_E_CLASSNOTREG;
	}

	// A generic IActivationFactory is not a valid instance of an arbitrary runtime class.
	// Delegate to the original OS implementation so supported WinRT classes receive their
	// real implementation and unsupported classes return the native activation failure.
	{
		typedef HRESULT (WINAPI *RO_ACTIVATE_INSTANCE)(HSTRING, IInspectable **);
		HMODULE CombaseModule = GetModuleHandleW(L"combase.dll");

		if (CombaseModule != NULL) {
			RO_ACTIVATE_INSTANCE NativeActivateInstance =
				(RO_ACTIVATE_INSTANCE) GetProcAddress(CombaseModule, "RoActivateInstance");

			if (NativeActivateInstance != NULL) {
				return NativeActivateInstance(ActivatableClassId, Instance);
			}
		}
	}

	return E_NOTIMPL;
}