/*=============================================================================
	buildconfig.h: which game this DLL is built for, and what follows.

	build.bat sets one UTGLR_*_BUILD macro; everything else here derives from it.
=============================================================================*/

#pragma once


#undef UTGLR_VALID_BUILD_CONFIG

#ifdef UTGLR_UT_BUILD
#define UTGLR_VALID_BUILD_CONFIG 1
#endif
#ifdef UTGLR_DX_BUILD
#define UTGLR_VALID_BUILD_CONFIG 1
#endif
#ifdef UTGLR_RUNE_BUILD
#define UTGLR_VALID_BUILD_CONFIG 1
#endif
//XXX: untested. No SDK here builds Unreal 227; kept for a fork with one.
#ifdef UTGLR_UNREAL_227_BUILD
#define UTGLR_VALID_BUILD_CONFIG 1
#endif
#ifdef UTGLR_UNREAL_226_BUILD
#define UTGLR_VALID_BUILD_CONFIG 1
#endif
#ifdef UTGLR_UNREAL_224_BUILD
#define UTGLR_VALID_BUILD_CONFIG 1
#endif
#ifdef UTGLR_NERF_BUILD
#define UTGLR_VALID_BUILD_CONFIG 1
#endif
#ifdef UTGLR_HP_BUILD
#define UTGLR_VALID_BUILD_CONFIG 1
#endif
#ifdef UTGLR_KLINGON_BUILD
#define UTGLR_VALID_BUILD_CONFIG 1
#endif

#if !UTGLR_VALID_BUILD_CONFIG
#error Valid build config not selected.
#endif
#undef UTGLR_VALID_BUILD_CONFIG

//Engines predating DetailTextures: no Exec chaining, no GUglyHackFlags, so no weapon
//pass and no ZRangeHack.
#if defined UTGLR_UNREAL_226_BUILD || defined UTGLR_NERF_BUILD || defined UTGLR_UNREAL_224_BUILD || defined UTGLR_KLINGON_BUILD
#define UTGLR_OLD_URENDERDEVICE 1
#endif

//Format enums carrying TEXF_DXT3/TEXF_DXT5. The source switch still dispatches on the
//texture's own format; an unrecognised one reads as 32 bit colour and overruns the mip.
#if defined UTGLR_UNREAL_227_BUILD || defined UTGLR_NERF_BUILD
#define UTGLR_HAS_DXT3_DXT5 1
#endif

/*
Format 0x01 is TEXF_RGBA7 in every SDK here: BGRA, seven bits a channel, doubled to eight.
Seven bits cannot carry across, so a whole texel doubles at once.

Klingon (219) calls the same slot TEXF_RGB32, and its content really is full range.
The shipped Glide driver reads it as 32-bit RGB. Doubling would saturate every channel
and drive a partial alpha opaque, so the multiplier is 1.
*/
#ifdef UTGLR_KLINGON_BUILD
static const unsigned int UTGLR_RGBA7_UPLOAD_MUL = 1;
#else
static const unsigned int UTGLR_RGBA7_UPLOAD_MUL = 2;
#endif

//Harry Potter's engine (433) submits actor geometry as indexed triangles.
