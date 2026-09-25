/*=============================================================================
	Limits.h: fixed array bounds.
=============================================================================*/

#pragma once

//If exceeds 7, various things will break
#define MAX_TMUNITS 4 // vogel: maximum number of texture mapping units supported
static_assert(MAX_TMUNITS <= 7, "the BYTE texBit walk in DisableSubsequentTextures would not terminate");

//At least 2000.
#define VERTEX_ARRAY_SIZE 4000 // vogel: better safe than sorry

//Largest polygon buffered directly; bigger ones take the unbuffered path.
#define MAX_BUFFERED_GP_PTS 10

//A buffered polygon costs 3 * (points - 2) entries, so hold back extra room.
#define BUFFERED_GP_HEADROOM 14

static_assert((3 * (MAX_BUFFERED_GP_PTS - 2)) <= (MAX_BUFFERED_GP_PTS + BUFFERED_GP_HEADROOM),
	"BUFFERED_GP_HEADROOM is too small for MAX_BUFFERED_GP_PTS");

//Held back and replayed.
//Harry Potter holds back single triangles, so it needs far more polygon slots.
#ifdef UTGLR_HP_BUILD
#define MAX_DEFERRED_GP_POLYS 4096
#define MAX_DEFERRED_GP_PTS (3 * MAX_DEFERRED_GP_POLYS)
#else
#define MAX_DEFERRED_GP_POLYS 1024
#define MAX_DEFERRED_GP_PTS 8192
#endif
#define MAX_DEFERRED_GP_TEXTURES 64

//Retried next frame.
#define MAX_TEX_ID_RECYCLES_PER_FRAME 64

#ifdef _WIN32
#define GL_DLL ("OpenGL32.dll")
#endif
