/*=============================================================================
	Limits.h: the fixed sizes this renderer works within.

	The arrays these bound are members of the render device, so none can grow at runtime.
=============================================================================*/

#pragma once


/*-----------------------------------------------------------------------------
	Globals.
-----------------------------------------------------------------------------*/

//If exceeds 7, various things will break
#define MAX_TMUNITS 4 // vogel: maximum number of texture mapping units supported

#define VERTEX_ARRAY_SIZE 8000 // vogel: better safe than sorry

#define INDEX_ARRAY_SIZE (3 * VERTEX_ARRAY_SIZE)

//Larger than the staging arrays so wraps stay rare. Capped at 65536: the deferred path's indices are WORDs.
#define VERTEX_RING_SIZE 64000
#define INDEX_RING_SIZE (3 * VERTEX_RING_SIZE)

static_assert(VERTEX_RING_SIZE <= 65536,
	"Deferred pass relative indices are WORDs, and one pass can span the whole ring");

static_assert(VERTEX_ARRAY_SIZE <= 65536, "Chunk relative indices are WORDs; VERTEX_ARRAY_SIZE must fit");

//Its indices are WORDs relative to the batch's own base.
#define MAX_CS_BATCH_SPAN 56000

static_assert(MAX_CS_BATCH_SPAN <= 65536,
	"Batch relative indices are WORDs; MAX_CS_BATCH_SPAN must fit");

//Largest polygon size buffered directly.
#define MAX_BUFFERED_GP_PTS 10

#define BUFFERED_GP_HEADROOM 14

static_assert((3 * (MAX_BUFFERED_GP_PTS - 2)) <= (MAX_BUFFERED_GP_PTS + BUFFERED_GP_HEADROOM),
	"BUFFERED_GP_HEADROOM is too small for MAX_BUFFERED_GP_PTS");

//These bound a pathological frame. Harry Potter needs more polygon slots.
#ifdef UTGLR_HP_BUILD
#define MAX_DEFERRED_GP_POLYS 4096
#define MAX_DEFERRED_GP_PTS (3 * MAX_DEFERRED_GP_POLYS)
#else
#define MAX_DEFERRED_GP_POLYS 1024
#define MAX_DEFERRED_GP_PTS 8192
#endif
#define MAX_DEFERRED_GP_TEXTURES 64
