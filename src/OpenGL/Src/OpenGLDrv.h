/*=============================================================================
	OpenGLDrv.h: Unreal OpenGL support header.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
	* Created by Tim Sweeney
	* Multitexture and context support - Andy Hanson (hanson@3dfx.com) and
	  Jack Mathews (jack@3dfx.com)
	* Unified by Daniel Vogel

=============================================================================*/

#pragma once

/*-----------------------------------------------------------------------------
	Includes.
-----------------------------------------------------------------------------*/

#ifdef WIN32
#include <windows.h>
#else
#include <SDL/SDL.h>
#endif
#include <GL/gl.h>
#ifdef WIN32
#include "ThirdParty/glext.h"
#include "ThirdParty/wglext.h"
#else
#include <GL/glext.h>
#endif

#include <stdlib.h>

#include "Engine.h"
#include "UnRender.h"

#ifdef UTGLR_UNREAL_227_BUILD
//Hack to avoid moving the render device's interface into a header. --ryan.
//#define AUTO_INITIALIZE_REGISTRANTS_OPENGLDRV UOpenGLRenderDevice::StaticClass();
extern "C" {
void autoInitializeRegistrantsOpenGL1xDrv(void);
}
// #define AUTO_INITIALIZE_REGISTRANTS_OPENGL1XDRV autoInitializeRegistrantsOpenGL1xDrv();
#endif

#if defined UTGLR_HP_BUILD || defined UTGLR_UNREAL_224_BUILD
//Harry Potter and Unreal 224 take a member address with no '&'.
//Patched here so the vendor SDKs stay untouched.
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

//Tells the engine a viewport resize is for OpenGL.
//Unreal 224 reuses the mouse workaround's bit.
//Klingon 219 shares it.
//Neither build ever sets the other meaning.
#if defined UTGLR_UNREAL_224_BUILD || defined UTGLR_KLINGON_BUILD
#define UTGLR_BLIT_OPENGL BLIT_Race
#else
#define UTGLR_BLIT_OPENGL BLIT_OpenGL
#endif

#ifdef UTGLR_KLINGON_BUILD
//Build 219 predates the config cache.
//Forwards to what it does have.
struct FKlingonConfigCache {
	UBOOL GetString(const TCHAR *Section, const TCHAR *Key, TCHAR *Value, INT Size) const {
		return GetConfigString(Section, Key, Value, Size);
	}
	UBOOL GetString(const TCHAR *Section, const TCHAR *Key, FString &Str) const {
		return GetConfigString(Section, Key, Str);
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

//Build 219 has no timed wait.
//The caller's spin loop enforces the target.
static inline void appSleep(FLOAT Seconds) {
	::Sleep((DWORD)(Seconds * 1000.0f));
}

//Build 219's helper takes no buffer length.
//Every call site here assumes 4096 characters.
#undef GET_VARARGS
#define GET_VARARGS(msg, len, fmt) appGetVarArgs(msg, fmt)
#endif

//Klingon packs the realtime flags into one field.
//Every other SDK gives them separate bitfields.
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

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
