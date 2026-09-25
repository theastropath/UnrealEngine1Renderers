/*=============================================================================
	Compiler.h: platform and toolchain switches.

	These only decide what gets compiled. The SSE paths are gated at runtime too.
=============================================================================*/

#pragma once


#ifdef _WIN32

#define UTGLR_USE_ASM_CODE

#define UTGLR_INCLUDE_SSE_CODE

#endif

#ifdef UTGLR_INCLUDE_SSE_CODE
#include <xmmintrin.h>
#include <emmintrin.h>
#endif


//Optional opcode patch code
//#define UTGLR_INCLUDE_OPCODE_PATCH_CODE

#ifdef UTGLR_INCLUDE_OPCODE_PATCH_CODE
#include "OpcodePatch.h"
#endif


#define UTGLR_USE_FASTCALL

#ifdef UTGLR_USE_FASTCALL
#ifdef _WIN32
#define FASTCALL __fastcall
#else
#define FASTCALL
#endif
#else
#define FASTCALL
#endif
