/**
\file lightmapatlas.h

Lightmaps onto shared pages, so neighbours batch.
*/

#pragma once

#include <d3d10.h>
#include <vector>

class LightmapAtlas {
public:
	/** The view is borrowed. */
	struct Placement {
		ID3D10ShaderResourceView *view;
		UINT pageIndex;
		/** Past the gutter. */
		UINT x;
		UINT y;
		UINT w;
		UINT h;
		UINT pageSize;
	};

	static const UINT PAGE_SIZE = 2048;

	/** Edge replication. */
	static const UINT GUTTER = 4;

	/** How far past a tile's own texels the cubic reconstruction in common.fxh reaches, and the reason
	the gutter exists at all, since a tap that lands outside it samples whatever lightmap the packer
	happened to shelve next door and tints the offending surface along that one border. */
	static const UINT BICUBIC_TEXEL_REACH = 2;

	static_assert(GUTTER >= BICUBIC_TEXEL_REACH,
		"The gutter has to hold every texel the shader's reconstruction can reach for");

	/** Otherwise its own texture. */
	static const UINT MAX_TILE = 256;

	static const UINT MAX_PAGES = 4;

	explicit LightmapAtlas(ID3D10Device *device);
	~LightmapAtlas();

	/**
	Reserve room for a tile and write it, gutter included.
	\param w,h In texels.
	\param rows R8G8B8A8, top row first.
	\param pitchBytes Row stride.
	\return false when it fits no page.
	*/
	bool add(UINT w, UINT h, const void *rows, UINT pitchBytes, Placement &out);

	/** For a lightmap regenerated in place. */
	bool write(const Placement &placement, const void *rows, UINT pitchBytes);

	/** Shelf packing can't notice a tile going unused. */
	void release(const Placement &placement);

	void reset();

	DWORD bytes() const;
	UINT pageCount() const { return (UINT)pages.size(); }
	UINT freeTileCount() const { return numFreeTiles; }

	/** The atlas trades uploads for batching, and counting only the batching half of that is how a rewrite
	of every changed lightmap on every subsequent frame managed to go unnoticed for as long as it did, so
	both halves are counted here. */
	struct Stats {
		DWORD placed; /**< First-time packings. */
		DWORD rewritten;
		DWORD failed; /**< A surface out of its batch. */
	};

	const Stats &stats() const { return frameStats; }
	/** Zeroes the counters. The tiles remain. */
	void newFrame();

private:
	/** Filled left to right. */
	struct Shelf {
		UINT y;
		UINT height;
		UINT nextX;
	};

	struct Page {
		ID3D10Texture2D *texture;
		ID3D10ShaderResourceView *view;
		std::vector<Shelf> shelves;
		UINT nextY;
	};

	/** Exact sizes only. */
	struct FreeTile {
		UINT pageIndex;
		/** Gutter not yet added. */
		UINT x;
		UINT y;
		UINT paddedW;
		UINT paddedH;
	};

	static const UINT MAX_FREE_TILES = 1024;

	ID3D10Device *device;
	std::vector<Page> pages;

	std::vector<DWORD> paddedScratch;

	/** Overflow drops it. */
	FreeTile freeTiles[MAX_FREE_TILES];
	UINT numFreeTiles;

	Stats frameStats;

	bool addPage();
	static bool reserve(Page &page, UINT w, UINT h, UINT &outX, UINT &outY, size_t &outShelf);
	/** Must follow its reserve. */
	static void unreserve(Page &page, size_t shelfIndex, UINT w);
	bool reuse(UINT w, UINT h, UINT &outPage, UINT &outX, UINT &outY);
	void recycle(UINT pageIndex, UINT x, UINT y, UINT paddedW, UINT paddedH);
	bool writeTile(const Placement &placement, const void *rows, UINT pitchBytes);

	LightmapAtlas(const LightmapAtlas &);
	LightmapAtlas &operator=(const LightmapAtlas &);
};
