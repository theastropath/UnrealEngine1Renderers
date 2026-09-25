/*=============================================================================
	LightmapAtlas.h: packing lightmaps into shared pages.
=============================================================================*/

#pragma once

class FLightmapAtlas {
public:
	struct FPlacement {
		IDirect3DTexture9 *pTexObj; //Borrowed; the atlas owns it
		//The reset generation is folded in:
		//without it a page created after a Reset could reuse an index a unit still thinks it has bound.
		DWORD PageKey;
		FLOAT UOffset, VOffset; //Fraction of the page.
		FLOAT MultScale; //1 / PAGE_SIZE, the tile's share of a texel
	};

	//Larger pages batch better.
	//They cost their full size as soon as one lightmap needs them.
	enum { PAGE_SIZE = 2048 };

	//Edge replication.
	//Linear filtering near a tile edge would otherwise read its neighbour on the page.
	enum { GUTTER = 4 };

	/*
	How far past a tile's own texels the cubic reconstruction reaches.
	*/
	enum { BICUBIC_TEXEL_REACH = 2 };

	static_assert(GUTTER >= BICUBIC_TEXEL_REACH,
		"The gutter has to hold every texel the shader's reconstruction can reach for");

	enum { MAX_TILE = 256 };

	enum { MAX_PAGES = 4 };

	FLightmapAtlas();
	~FLightmapAtlas();

	void Reset(void);

	const FPlacement *FASTCALL Place(IDirect3DDevice9 *pDevice, const FTextureInfo &Info);

	//The next Place writes the tile.
	//That is where the texels are read and a failure can be retried.
	void FASTCALL NoteChanged(QWORD CacheID);

	//Separate from the main texture budget because nothing ever evicts a page, so folding the two
	//together would leave that budget permanently unreachable and the eviction loop spending every
	//frame failing to reach it, and D3DPOOL_MANAGED doubles the real cost of a page besides, the
	//runtime keeping its own system memory copy behind the one counted here, which is why the
	//page count is capped outright rather than left to a byte total nothing can bring back down.
	DWORD Bytes(void) const { return m_numPages * (DWORD)PAGE_SIZE * (DWORD)PAGE_SIZE * 4; }
	DWORD PageCount(void) const { return m_numPages; }
	DWORD FreeTileCount(void) const { return m_numFreeTiles; }

	struct FStats {
		DWORD Placed; //Packed the first time.
		DWORD Rewritten; //Rewritten in place.
		DWORD Failed; //Placements refused, each one a surface that loses its batch
	};

	const FStats &Stats(void) const { return m_stats; }
	void NewFrame(void) {
		m_stats.Placed = 0;
		m_stats.Rewritten = 0;
		m_stats.Failed = 0;
	}

private:
	struct FShelf {
		DWORD y, height, nextX;
	};

	//The shortest tile a shelf can hold is one texel plus its gutter. That bounds the count.
	enum { MAX_SHELVES = PAGE_SIZE / (1 + 2 * GUTTER) };

	struct FPage {
		IDirect3DTexture9 *pTexObj;
		FShelf shelves[MAX_SHELVES];
		DWORD numShelves;
		DWORD nextY;
	};

	struct FEntry {
		QWORD CacheID;
		FPlacement Placement;
		DWORD PageIndex;
		DWORD X, Y, W, H;
		//One rewrite per change.
		bool bDirty;
	};

	/*
	A hole cannot be merged into the shelf around it, so only a tile of exactly the same padded
	size can reuse it, which covers most of them in practice, lightmap extents coming from a small
	set of BSP node sizes and the padded size rounding away what variation is left.
	Without this a page only ever fills.
	*/
	struct FFreeTile {
		DWORD PageIndex;
		DWORD X, Y; //Padded origin, as Reserve hands it out. The gutter has not been added yet
		DWORD PaddedW, PaddedH;
	};

	//Fixed capacity, so returning a tile allocates nothing. On overflow its space is lost until Reset.
	enum { MAX_FREE_TILES = 1024 };
	FFreeTile m_freeTiles[MAX_FREE_TILES];
	DWORD m_numFreeTiles;

	FStats m_stats;

	FPage m_pages[MAX_PAGES];
	DWORD m_numPages;
	DWORD m_generation;

	//Open addressed, a power of two, half left empty so probing always terminates:
	//the real ceiling is MAP_SIZE/2. Hitting it falls back to an unbatched texture.
	enum { MAP_SIZE = 16384 };
	FEntry m_map[MAP_SIZE];
	DWORD m_numEntries;

	static inline DWORD FASTCALL HashSlot(QWORD CacheID) {
		return (DWORD)((((DWORD)(CacheID >> 32)) ^ ((DWORD)CacheID)) * 2654435761u) & (MAP_SIZE - 1);
	}

	bool AddPage(IDirect3DDevice9 *pDevice);
	//A larger tile handed to a smaller lightmap could never be split back up.
	//The difference would be wasted for good.
	bool FASTCALL Reuse(DWORD w, DWORD h, DWORD &outPage, DWORD &outX, DWORD &outY);
	void FASTCALL Recycle(DWORD pageIndex, DWORD x, DWORD y, DWORD paddedW, DWORD paddedH);
	static bool Reserve(FPage &page, DWORD w, DWORD h, DWORD &outX, DWORD &outY, DWORD &outShelf);
	static void Unreserve(FPage &page, DWORD shelfIndex, DWORD w);
	static bool Write(FPage &page, DWORD x, DWORD y, const FTextureInfo &Info, DWORD w, DWORD h);
	static bool Accepts(const FTextureInfo &Info, DWORD &outW, DWORD &outH);
};
