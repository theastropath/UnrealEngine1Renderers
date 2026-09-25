/*=============================================================================
	LightmapAtlas.cpp: packing lightmaps into shared pages.

	Shelf packing, page upload, and the map from lightmap cache id to tile.
=============================================================================*/

#include "../D3D9Drv.h"
#include "../D3D9.h"


/*-----------------------------------------------------------------------------
	Lightmap atlas.
-----------------------------------------------------------------------------*/

//All zeroes is the valid empty state.
FLightmapAtlas::FLightmapAtlas() {
	m_numPages = 0;
	m_numEntries = 0;
	m_generation = 0;
	m_numFreeTiles = 0;
	NewFrame();
	for (DWORD p = 0; p < MAX_PAGES; p++) {
		m_pages[p].pTexObj = NULL;
		m_pages[p].numShelves = 0;
		m_pages[p].nextY = 0;
	}
	for (DWORD i = 0; i < MAP_SIZE; i++) {
		m_map[i].Placement.pTexObj = NULL;
	}
}

FLightmapAtlas::~FLightmapAtlas() {
	Reset();
}

void FLightmapAtlas::Reset(void) {
	for (DWORD p = 0; p < m_numPages; p++) {
		if (m_pages[p].pTexObj) {
			m_pages[p].pTexObj->Release();
			m_pages[p].pTexObj = NULL;
		}
		m_pages[p].numShelves = 0;
		m_pages[p].nextY = 0;
	}
	m_numPages = 0;

	for (DWORD i = 0; i < MAP_SIZE; i++) {
		m_map[i].Placement.pTexObj = NULL;
	}
	m_numEntries = 0;
	m_numFreeTiles = 0;

	m_generation++;
}

void FASTCALL FLightmapAtlas::Recycle(DWORD pageIndex, DWORD x, DWORD y, DWORD paddedW, DWORD paddedH) {
	if (m_numFreeTiles >= MAX_FREE_TILES) {
		return;
	}

	FFreeTile &tile = m_freeTiles[m_numFreeTiles++];
	tile.PageIndex = pageIndex;
	tile.X = x;
	tile.Y = y;
	tile.PaddedW = paddedW;
	tile.PaddedH = paddedH;
}

bool FASTCALL FLightmapAtlas::Reuse(DWORD w, DWORD h, DWORD &outPage, DWORD &outX, DWORD &outY) {
	const DWORD paddedW = w + 2 * GUTTER;
	const DWORD paddedH = h + 2 * GUTTER;

	for (DWORD i = 0; i < m_numFreeTiles; i++) {
		if ((m_freeTiles[i].PaddedW != paddedW) || (m_freeTiles[i].PaddedH != paddedH)) {
			continue;
		}

		outPage = m_freeTiles[i].PageIndex;
		outX = m_freeTiles[i].X;
		outY = m_freeTiles[i].Y;

		m_freeTiles[i] = m_freeTiles[m_numFreeTiles - 1];
		m_numFreeTiles--;
		return true;
	}

	return false;
}

void FASTCALL FLightmapAtlas::NoteChanged(QWORD CacheID) {
	DWORD slot = HashSlot(CacheID);
	for (DWORD probe = 0; probe < MAP_SIZE; probe++) {
		FEntry &entry = m_map[slot];
		if (entry.Placement.pTexObj == NULL) {
			return;
		}
		if (entry.CacheID == CacheID) {
			entry.bDirty = true;
			return;
		}
		slot = (slot + 1) & (MAP_SIZE - 1);
	}
}

bool FLightmapAtlas::AddPage(IDirect3DDevice9 *pDevice) {
	if (m_numPages >= MAX_PAGES) {
		return false;
	}

	//Managed pool, so writes go through the system memory copy.
	IDirect3DTexture9 *pTexObj = NULL;
	if (FAILED(pDevice->CreateTexture(PAGE_SIZE, PAGE_SIZE, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &pTexObj, NULL))) {
		return false;
	}

	m_pages[m_numPages].pTexObj = pTexObj;
	m_pages[m_numPages].numShelves = 0;
	m_pages[m_numPages].nextY = 0;
	m_numPages++;
	return true;
}

bool FLightmapAtlas::Reserve(FPage &page, DWORD w, DWORD h, DWORD &outX, DWORD &outY, DWORD &outShelf) {
	const DWORD needW = w + 2 * GUTTER;
	const DWORD needH = h + 2 * GUTTER;

	if ((needW > PAGE_SIZE) || (needH > PAGE_SIZE)) {
		return false;
	}

	/*
	A shelf's height is set by whichever tile opened it, so first fit wastes that difference across
	the whole width of every tile placed on the shelf afterwards, which matters here because the
	page count is hard capped and the waste comes off the space real lightmaps would have used, and
	the shelf list is short enough - one entry per distinct tile height - that scanning all of it
	every time costs nothing worth saving.
	*/
	FShelf *pBest = NULL;
	DWORD bestIndex = 0;
	for (DWORD s = 0; s < page.numShelves; s++) {
		FShelf &shelf = page.shelves[s];
		if ((needH <= shelf.height) && ((shelf.nextX + needW) <= PAGE_SIZE)) {
			if (!pBest || (shelf.height < pBest->height)) {
				pBest = &shelf;
				bestIndex = s;
			}
		}
	}
	if (pBest) {
		outX = pBest->nextX;
		outY = pBest->y;
		outShelf = bestIndex;
		pBest->nextX += needW;
		return true;
	}

	if (((page.nextY + needH) > PAGE_SIZE) || (page.numShelves >= MAX_SHELVES)) {
		return false;
	}

	FShelf &shelf = page.shelves[page.numShelves];
	shelf.y = page.nextY;
	shelf.height = needH;
	shelf.nextX = needW;
	outShelf = page.numShelves;
	page.numShelves++;
	page.nextY += needH;

	outX = 0;
	outY = shelf.y;
	return true;
}

void FLightmapAtlas::Unreserve(FPage &page, DWORD shelfIndex, DWORD w) {
	const DWORD needW = w + 2 * GUTTER;
	if ((shelfIndex < page.numShelves) && (page.shelves[shelfIndex].nextX >= needW)) {
		page.shelves[shelfIndex].nextX -= needW;
	}
}

bool FLightmapAtlas::Write(FPage &page, DWORD x, DWORD y, const FTextureInfo &Info, DWORD w, DWORD h) {
	const DWORD paddedW = w + 2 * GUTTER;
	const DWORD paddedH = h + 2 * GUTTER;

	//Locking only the padded rect.
	RECT lockRect;
	lockRect.left = (LONG)x;
	lockRect.top = (LONG)y;
	lockRect.right = (LONG)(x + paddedW);
	lockRect.bottom = (LONG)(y + paddedH);

	//Explicit dirty rect: the runtime marks the whole surface dirty once its dirty list overflows.
	D3DLOCKED_RECT locked;
	if (FAILED(page.pTexObj->LockRect(0, &locked, &lockRect, D3DLOCK_NOSYSLOCK | D3DLOCK_NO_DIRTY_UPDATE))) {
		return false;
	}

	const FColor *pSrcBase = (const FColor *)Info.Mips[0]->DataPtr;
	const DWORD srcStride = (DWORD)Info.Mips[0]->USize;
	BYTE *pDstBase = (BYTE *)locked.pBits;

	for (DWORD py = 0; py < paddedH; py++) {
		DWORD sy = (py < GUTTER) ? 0 : (py - GUTTER);
		if (sy >= h) {
			sy = h - 1;
		}
		const FColor *pSrcRow = pSrcBase + (sy * srcStride);
		DWORD *pDstRow = (DWORD *)(pDstBase + (py * locked.Pitch));

		const DWORD firstTexel = GET_COLOR_DWORD(pSrcRow[0]) * UTGLR_RGBA7_UPLOAD_MUL;
		const DWORD lastTexel = GET_COLOR_DWORD(pSrcRow[w - 1]) * UTGLR_RGBA7_UPLOAD_MUL;

		DWORD px;
		for (px = 0; px < GUTTER; px++) {
			pDstRow[px] = firstTexel;
		}
		for (px = 0; px < w; px++) {
			pDstRow[GUTTER + px] = GET_COLOR_DWORD(pSrcRow[px]) * UTGLR_RGBA7_UPLOAD_MUL;
		}
		for (px = GUTTER + w; px < paddedW; px++) {
			pDstRow[px] = lastTexel;
		}
	}

	page.pTexObj->UnlockRect(0);
	page.pTexObj->AddDirtyRect(&lockRect);
	return true;
}

//Only formats this can copy straight in: a block compressed mip holds a fraction of the bytes,
//so reading it as plain texels runs past the end of the buffer.
bool FLightmapAtlas::Accepts(const FTextureInfo &Info, DWORD &outW, DWORD &outH) {
	if (Info.Format != TEXF_RGBA7) {
		return false;
	}
	if (Info.Palette != NULL) {
		return false;
	}
	if ((Info.NumMips < 1) || (Info.Mips[0] == NULL) || (Info.Mips[0]->DataPtr == NULL)) {
		return false;
	}
	if ((Info.USize != Info.Mips[0]->USize) || (Info.VSize != Info.Mips[0]->VSize)) {
		return false;
	}

	//The clamp is the part of the mip the engine actually uses.
	const INT w = Info.UClamp;
	const INT h = Info.VClamp;
	if ((w < 1) || (h < 1) || (w > Info.USize) || (h > Info.VSize)) {
		return false;
	}
	if ((w > (INT)MAX_TILE) || (h > (INT)MAX_TILE)) {
		return false;
	}

	outW = (DWORD)w;
	outH = (DWORD)h;
	return true;
}

const FLightmapAtlas::FPlacement *FASTCALL FLightmapAtlas::Place(IDirect3DDevice9 *pDevice, const FTextureInfo &Info) {
	DWORD w = 0, h = 0;

	const QWORD CacheID = Info.CacheID;
	DWORD slot = HashSlot(CacheID);
	bool reusingSlot = false;
	//The tile a re-placed entry gives up, held until the end: everything below can still fail
	//and leave the entry serving that tile.
	bool haveOldTile = false;
	DWORD oldPage = 0, oldX = 0, oldY = 0, oldPaddedW = 0, oldPaddedH = 0;
	for (DWORD probe = 0; probe < MAP_SIZE; probe++) {
		FEntry &entry = m_map[slot];
		if (entry.Placement.pTexObj == NULL) {
			break;
		}
		if (entry.CacheID == CacheID) {
			if (!Accepts(Info, w, h)) {
				m_stats.Failed++;
				return NULL;
			}
			/*
			A lightmap that changed shape gets a new tile, since returning null would leave the
			stale entry in place and permanently mismatch and unbatch that surface, and the slot
			itself is reused because clearing one mid-probe-chain would orphan whatever hashed past
			it, with the old tile going back on the free list only once nothing further can fail,
			a tile handed out twice being far worse than a tile lost until the next Reset.
			*/
			if ((w != entry.W) || (h != entry.H)) {
				haveOldTile = true;
				oldPage = entry.PageIndex;
				oldX = entry.X;
				oldY = entry.Y;
				oldPaddedW = entry.W + 2 * GUTTER;
				oldPaddedH = entry.H + 2 * GUTTER;
				reusingSlot = true;
				break;
			}
			//Written only for a change the engine reported, which NoteLightmapChanged marks here.
			if (entry.bDirty) {
				if (!Write(m_pages[entry.PageIndex], entry.X, entry.Y, Info, entry.W, entry.H)) {
					return NULL;
				}
				entry.bDirty = false;
				m_stats.Rewritten++;
			}
			return &entry.Placement;
		}
		slot = (slot + 1) & (MAP_SIZE - 1);
	}

	if (!reusingSlot && (m_numEntries >= (MAP_SIZE / 2))) {
		m_stats.Failed++;
		return NULL;
	}

	if (!Accepts(Info, w, h)) {
		m_stats.Failed++;
		return NULL;
	}

	DWORD pageIndex = 0, x = 0, yy = 0, shelfIndex = 0;
	const bool fromFreeList = Reuse(w, h, pageIndex, x, yy);
	bool placed = fromFreeList;
	for (; !placed && (pageIndex < m_numPages); pageIndex++) {
		if (Reserve(m_pages[pageIndex], w, h, x, yy, shelfIndex)) {
			placed = true;
			break;
		}
	}
	if (!placed) {
		if (!AddPage(pDevice)) {
			m_stats.Failed++;
			return NULL;
		}
		pageIndex = m_numPages - 1;
		if (!Reserve(m_pages[pageIndex], w, h, x, yy, shelfIndex)) {
			m_stats.Failed++;
			return NULL;
		}
	}

	if (!Write(m_pages[pageIndex], x, yy, Info, w, h)) {
		/*
		The space goes back the way it was taken.
		Without this a persistently failing lock consumes every shelf on every page.
		*/
		if (fromFreeList) {
			Recycle(pageIndex, x, yy, w + 2 * GUTTER, h + 2 * GUTTER);
		} else {
			Unreserve(m_pages[pageIndex], shelfIndex, w);
		}
		m_stats.Failed++;
		return NULL;
	}

	//Only now that nothing further can fail.
	if (haveOldTile) {
		Recycle(oldPage, oldX, oldY, oldPaddedW, oldPaddedH);
	}

	const FLOAT invPage = 1.0f / (FLOAT)PAGE_SIZE;
	FEntry &entry = m_map[slot];
	entry.CacheID = CacheID;
	entry.Placement.pTexObj = m_pages[pageIndex].pTexObj;
	entry.Placement.PageKey = (m_generation * MAX_PAGES) + pageIndex;
	entry.Placement.UOffset = (FLOAT)(x + GUTTER) * invPage;
	entry.Placement.VOffset = (FLOAT)(yy + GUTTER) * invPage;
	entry.Placement.MultScale = invPage;
	entry.PageIndex = pageIndex;
	entry.X = x;
	entry.Y = yy;
	entry.W = w;
	entry.H = h;
	entry.bDirty = false;
	if (!reusingSlot) {
		m_numEntries++;
	}
	m_stats.Placed++;

	return &entry.Placement;
}
