/*=============================================================================
	Debug.h: the switches that are compiled out of a normal build.
=============================================================================*/

#pragma once


//Debug defines
//#define UTGLR_DEBUG_SHOW_TEX_CONVERT_COUNTS
//#define UTGLR_DEBUG_SHOW_CALL_COUNTS
//#define UTGLR_DEBUG_WORLD_WIREFRAME
//#define UTGLR_DEBUG_ACTOR_WIREFRAME
//#define UTGLR_DEBUG_Z_RANGE_HACK_WIREFRAME


#ifdef UTGLR_DEBUG_SHOW_CALL_COUNTS
#define UTGLR_DEBUG_CALL_COUNT(name) \
	{ \
		static unsigned int s_c; \
		dbgPrintf("utglr: " #name " = %u\n", s_c); \
		s_c++; \
	}
#else
#define UTGLR_DEBUG_CALL_COUNT(name)
#endif

#ifdef UTGLR_DEBUG_SHOW_TEX_CONVERT_COUNTS
#define UTGLR_DEBUG_TEX_CONVERT_COUNT(name) \
	{ \
		static unsigned int s_c; \
		dbgPrintf("utglr: " #name " = %u\n", s_c); \
		s_c++; \
	}
#else
#define UTGLR_DEBUG_TEX_CONVERT_COUNT(name)
#endif
