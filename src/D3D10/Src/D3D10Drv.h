/**
\file D3D10Drv.h

Build configuration and the SDK shims.
*/

#pragma once

/*
Engines older than Unreal Tournament's, oldest last:
- Unreal Gold, Nerf, Unreal 224: no Flush argument, pure virtual Exec, no DetailTextures.
- Klingon Honor Guard: minimal URenderDevice; no FTextureInfo::Texture. An ANSI build.
*/
#if (UNREALGOLD || NERF || UNREAL_224 || KLINGON)
#define OLD_URENDERDEVICE 1
#else
#define OLD_URENDERDEVICE 0
#endif

/*
Unreal Tournament's generation: UT, X-COM Enforcer and Harry Potter, the last with a submission
interface of its own. They share the four-argument DECLARE_CLASS and the FTime overload of
UTexture::Lock, as does Rune, kept apart here for its fog.
*/
#if (UNREALTOURNAMENT || HARRYPOTTER)
#define UT_URENDERDEVICE 1
#else
#define UT_URENDERDEVICE 0
#endif

/*
Highest ETextureFormat the build's SDK declares, bounding the array it may index, and Nerf's enum runs
past RGBA8 with a TEXF_DXT5 undeclared everywhere else, which is why the bound is a macro and never
a plain constant of the enum's own type.
*/
#if NERF
#define HIGHEST_SUPPORTED_TEXF TEXF_DXT5
#else
#define HIGHEST_SUPPORTED_TEXF TEXF_RGBA8
#endif

//Klingon's TCHAR is char.
#include <tchar.h>

#include "Engine.h"
#include "UnRender.h"

#if (HARRYPOTTER || UNREAL_224)
/*
Harry Potter and Unreal 224's IMPLEMENT_CLASS omits the '&' a conforming compiler requires (C3867),
so it is redefined here.
*/
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

#if KLINGON
/* Klingon's Core (219) predates some of this. */
struct FKlingonConfigCache {
	UBOOL GetBool(const TCHAR *Section, const TCHAR *Key, UBOOL &Value) const { return GetConfigBool(Section, Key, Value); }
	UBOOL GetInt(const TCHAR *Section, const TCHAR *Key, INT &Value) const { return GetConfigInt(Section, Key, Value); }
	UBOOL GetFloat(const TCHAR *Section, const TCHAR *Key, FLOAT &Value) const { return GetConfigFloat(Section, Key, Value); }
	void SetBool(const TCHAR *Section, const TCHAR *Key, UBOOL Value) const { SetConfigBool(Section, Key, Value); }
	void SetInt(const TCHAR *Section, const TCHAR *Key, INT Value) const { SetConfigInt(Section, Key, Value); }
	void SetFloat(const TCHAR *Section, const TCHAR *Key, FLOAT Value) const { SetConfigFloat(Section, Key, Value); }
	const FKlingonConfigCache *operator->() const { return this; }
};
static const FKlingonConfigCache GConfig;

#endif

/*
The log and error output devices, pointers on later SDKs and differently named objects on Klingon's
(219), written as functions because that SDK's guard macros already expand to the error one, which a
plain reference could not satisfy.
*/
static inline FOutputDevice &logOut() {
#if KLINGON
	return GOut;
#else
	return *GLog;
#endif
}

static inline FOutputDevice &errorOut() {
#if KLINGON
	return GError;
#else
	return *GError;
#endif
}

/* A DWORD on Klingon's. */
static inline UBOOL TexInfoRealtimeChanged(const FTextureInfo &Info) {
#if KLINGON
	return (Info.TextureFlags & TF_RealtimeChanged) != 0;
#else
	return Info.bRealtimeChanged;
#endif
}

static inline void TexInfoSetRealtimeChanged(FTextureInfo &Info, UBOOL Value) {
#if KLINGON
	if (Value)
		Info.TextureFlags |= (DWORD)TF_RealtimeChanged;
	else
		Info.TextureFlags &= ~(DWORD)TF_RealtimeChanged;
#else
	Info.bRealtimeChanged = Value;
#endif
}

/** Dynamic against immutable. */
static inline UBOOL TexInfoVolatile(const FTextureInfo &Info) {
#if KLINGON
	return (Info.TextureFlags & (TF_RealtimeChanged | TF_Realtime | TF_Parametric)) != 0;
#else
	return (Info.bRealtimeChanged || Info.bRealtime || Info.bParametric) != 0;
#endif
}

/** Null where the SDK records none. */
static inline UTexture *TexInfoTexture(const FTextureInfo &Info) {
#if KLINGON
	(void)Info;
	return nullptr;
#else
	return Info.Texture;
#endif
}
