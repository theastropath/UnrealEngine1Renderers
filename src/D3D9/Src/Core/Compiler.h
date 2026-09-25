/*=============================================================================
	Compiler.h: what the compiler and platform allow, and not what the game is.

	These say what was compiled. Runtime detection still guards every path.
=============================================================================*/

#pragma once

#ifdef WIN32

#define UTGLR_USE_ASM_CODE

#define UTGLR_INCLUDE_SSE_CODE

#endif

#ifdef UTGLR_INCLUDE_SSE_CODE
//Inside the clock guard.
#pragma push_macro("clock")
#undef clock
#include <intrin.h>
#include <xmmintrin.h>
#include <emmintrin.h>
//SSSE3, for palette conversion. The build already requires SSE2, MSVC's x86 default since VS2012.
#include <tmmintrin.h>
#pragma pop_macro("clock")
#endif


#define UTGLR_USE_FASTCALL

#ifdef UTGLR_USE_FASTCALL
#ifdef WIN32
#define FASTCALL __fastcall
#else
#define FASTCALL
#endif
#else
#define FASTCALL
#endif
