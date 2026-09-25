/*=============================================================================
	D3D9Drv.h: Unreal D3D9 support header.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
	* Created by Chris Dohnal

=============================================================================*/

/*-----------------------------------------------------------------------------
	Includes.
-----------------------------------------------------------------------------*/

#include <windows.h>

#define DIRECT3D_VERSION 0x0900
#include <d3d9.h>

//Unused here and in the game SDKs.
#define UTGLR_NO_APP_MALLOC
#include <stdlib.h>

#include "Engine.h"
#include "UnRender.h"

#if defined UTGLR_HP_BUILD || defined UTGLR_UNREAL_224_BUILD
//The Harry Potter and Unreal 224 SDKs take the address of a non-static member without an '&' in their
//class-registration macro, which VC6 accepted and a conforming compiler rejects.
//Patched here so the vendor SDK stays untouched.
#undef IMPLEMENT_CLASS
#define IMPLEMENT_CLASS(TClass) \
	UClass TClass::PrivateStaticClass( \
		EC_NativeConstructor, \
		sizeof(TClass), \
		TClass::StaticClassFlags, \
		TClass::Super::StaticClass(), \
		TClass::WithinClass::StaticClass(), \
		FGuid(TClass::GUID1, TClass::GUID2, TClass::GUID3, TClass::GUID4), \
		TEXT(#TClass) + 1, \
		GPackage, \
		StaticConfigName(), \
		RF_Public | RF_Standalone | RF_Transient | RF_Native, \
		(void (*)(void *))TClass::InternalConstructor, \
		(void(UObject::*)())(&TClass::StaticConstructor)); \
	extern "C" DLL_EXPORT UClass *autoclass##TClass; \
	DLL_EXPORT UClass *autoclass##TClass = TClass::StaticClass();
#endif

//Unreal 224 predates the colour-to-DWORD macro later SDKs provide.
#ifndef GET_COLOR_DWORD
#define GET_COLOR_DWORD(color) (*(DWORD *)&(color))
#endif

#ifdef UTGLR_KLINGON_BUILD
//Unreal engine build 219, the Klingon SDK, predates the config-cache facility.
struct FKlingonConfigCache {
	UBOOL GetString(const TCHAR *Section, const TCHAR *Key, TCHAR *Value, INT Size) const {
		return GetConfigString(Section, Key, Value, Size);
	}
	UBOOL GetInt(const TCHAR *Section, const TCHAR *Key, INT &Value) const {
		return GetConfigInt(Section, Key, Value);
	}
	void SetBool(const TCHAR *Section, const TCHAR *Key, UBOOL Value) const {
		SetConfigBool(Section, Key, Value);
	}
	const FKlingonConfigCache *operator->() const { return this; }
};
static const FKlingonConfigCache GConfig;

//Unreal engine build 219 has no timed wait. Sub-millisecond requests round down to a yield.
static inline void appSleep(FLOAT Seconds) {
	::Sleep((DWORD)(Seconds * 1000.0f));
}

//Unreal engine build 219's formatted-string helper takes no buffer length.
#undef GET_VARARGS
#define GET_VARARGS(msg, len, fmt) appGetVarArgs(msg, fmt)
#endif

//Every SDK here stores FTextureInfo's realtime flags as individual bitfields;
//the Klingon SDK packs them into one flags field.
static inline UBOOL TexInfoRealtimeChanged(const FTextureInfo &Info) {
#ifdef UTGLR_KLINGON_BUILD
	return (Info.TextureFlags & TF_RealtimeChanged) != 0;
#else
	return Info.bRealtimeChanged;
#endif
}

static inline void TexInfoClearRealtimeChanged(FTextureInfo &Info) {
#ifdef UTGLR_KLINGON_BUILD
	Info.TextureFlags &= ~(DWORD)TF_RealtimeChanged;
#else
	Info.bRealtimeChanged = 0;
#endif
}

static inline UBOOL TexInfoRealtime(const FTextureInfo &Info) {
#ifdef UTGLR_KLINGON_BUILD
	return (Info.TextureFlags & TF_Realtime) != 0;
#else
	return Info.bRealtime;
#endif
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
