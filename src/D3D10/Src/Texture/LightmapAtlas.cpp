
#include "LightmapAtlas.h"
#include "../D3D10.h"

#include <new>

LightmapAtlas::LightmapAtlas(ID3D10Device *device) :
	device(device),
	numFreeTiles(0) {
	newFrame();
}

LightmapAtlas::~LightmapAtlas() {
	reset();
}

//Whole pages come back on a level change.
void LightmapAtlas::reset() {
	for (size_t i = 0; i < pages.size(); i++) {
		SAFE_RELEASE(pages[i].view);
		SAFE_RELEASE(pages[i].texture);
	}
	pages.clear();
	numFreeTiles = 0;
}

void LightmapAtlas::newFrame() {
	frameStats.placed = 0;
	frameStats.rewritten = 0;
	frameStats.failed = 0;
}

DWORD LightmapAtlas::bytes() const {
	return (DWORD)pages.size() * PAGE_SIZE * PAGE_SIZE * 4;
}

//Past MAX_PAGES a lightmap gets its own texture.
bool LightmapAtlas::addPage() {
	if (pages.size() >= MAX_PAGES)
		return false;

	//USAGE_DEFAULT. WRITE_DISCARD would drop every other tile.
	D3D10_TEXTURE2D_DESC desc;
	desc.Width = PAGE_SIZE;
	desc.Height = PAGE_SIZE;
	desc.MipLevels = 1; //Fixed mip level.
	desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Usage = D3D10_USAGE_DEFAULT;
	desc.BindFlags = D3D10_BIND_SHADER_RESOURCE;
	desc.CPUAccessFlags = 0;
	desc.MiscFlags = 0;

	Page page;
	page.texture = nullptr;
	page.view = nullptr;
	page.nextY = 0;

	if (FAILED(device->CreateTexture2D(&desc, nullptr, &page.texture))) {
		UD3D10RenderDevice::debugs("Could not create a lightmap atlas page; lightmaps will get their own textures.");
		return false;
	}

	D3D10_SHADER_RESOURCE_VIEW_DESC srDesc;
	srDesc.Format = desc.Format;
	srDesc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2D;
	srDesc.Texture2D.MostDetailedMip = 0;
	srDesc.Texture2D.MipLevels = 1;
	if (FAILED(device->CreateShaderResourceView(page.texture, &srDesc, &page.view))) {
		UD3D10RenderDevice::debugs("Could not create a lightmap atlas page view.");
		SAFE_RELEASE(page.texture);
		return false;
	}

	pages.push_back(page);
	return true;
}

bool LightmapAtlas::reserve(Page &page, UINT w, UINT h, UINT &outX, UINT &outY, size_t &outShelf) {
	const UINT needW = w + 2 * GUTTER;
	const UINT needH = h + 2 * GUTTER;

	if (needW > PAGE_SIZE || needH > PAGE_SIZE)
		return false;

	//Best fit. Whatever opened the shelf set its height.
	Shelf *best = nullptr;
	size_t bestIndex = 0;
	for (size_t s = 0; s < page.shelves.size(); s++) {
		Shelf &shelf = page.shelves[s];
		if (needH <= shelf.height && (shelf.nextX + needW) <= PAGE_SIZE) {
			if (!best || shelf.height < best->height) {
				best = &shelf;
				bestIndex = s;
			}
		}
	}
	if (best) {
		outX = best->nextX;
		outY = best->y;
		outShelf = bestIndex;
		best->nextX += needW;
		return true;
	}

	if ((page.nextY + needH) > PAGE_SIZE)
		return false;

	Shelf shelf;
	shelf.y = page.nextY;
	shelf.height = needH;
	shelf.nextX = needW;
	page.shelves.push_back(shelf);
	page.nextY += needH;

	outX = 0;
	outY = shelf.y;
	outShelf = page.shelves.size() - 1;
	return true;
}

void LightmapAtlas::unreserve(Page &page, size_t shelfIndex, UINT w) {
	const UINT needW = w + 2 * GUTTER;
	if (shelfIndex < page.shelves.size() && page.shelves[shelfIndex].nextX >= needW)
		page.shelves[shelfIndex].nextX -= needW;
}

//Otherwise pages only fill.
void LightmapAtlas::recycle(UINT pageIndex, UINT x, UINT y, UINT paddedW, UINT paddedH) {
	//Lost past the cap.
	if (numFreeTiles >= MAX_FREE_TILES)
		return;

	FreeTile &tile = freeTiles[numFreeTiles++];
	tile.pageIndex = pageIndex;
	tile.x = x;
	tile.y = y;
	tile.paddedW = paddedW;
	tile.paddedH = paddedH;
}

void LightmapAtlas::release(const Placement &placement) {
	if (placement.pageIndex >= pages.size())
		return;

	recycle(placement.pageIndex, placement.x - GUTTER, placement.y - GUTTER,
		placement.w + 2 * GUTTER, placement.h + 2 * GUTTER);
}

/**
Exactly the size wanted.
\note Exact, because a larger tile handed to a smaller lightmap could never be split back up again,
	and reusing one that way would quietly lose the difference on every placement until the level ends
	and the page is left full of holes nothing can use.
*/
bool LightmapAtlas::reuse(UINT w, UINT h, UINT &outPage, UINT &outX, UINT &outY) {
	const UINT paddedW = w + 2 * GUTTER;
	const UINT paddedH = h + 2 * GUTTER;

	for (UINT i = 0; i < numFreeTiles; i++) {
		if (freeTiles[i].paddedW != paddedW || freeTiles[i].paddedH != paddedH)
			continue;

		outPage = freeTiles[i].pageIndex;
		outX = freeTiles[i].x;
		outY = freeTiles[i].y;

		freeTiles[i] = freeTiles[numFreeTiles - 1];
		numFreeTiles--;
		return true;
	}

	return false;
}

//Free list, open shelf, new page.
bool LightmapAtlas::add(UINT w, UINT h, const void *rows, UINT pitchBytes, Placement &out) {
	if (w == 0 || h == 0 || rows == nullptr)
		return false;

	//Big tiles waste shelf height for their neighbours.
	if (w > MAX_TILE || h > MAX_TILE) {
		frameStats.failed++;
		return false;
	}

	UINT pageIndex = 0;
	UINT x = 0, y = 0;
	size_t shelfIndex = 0;
	bool fromFreeList = reuse(w, h, pageIndex, x, y);
	bool placed = fromFreeList;
	for (; !placed && pageIndex < pages.size(); pageIndex++) {
		if (reserve(pages[pageIndex], w, h, x, y, shelfIndex)) {
			placed = true;
			break;
		}
	}
	if (!placed) {
		if (!addPage()) {
			frameStats.failed++;
			return false;
		}
		pageIndex = (UINT)pages.size() - 1;
		if (!reserve(pages[pageIndex], w, h, x, y, shelfIndex)) {
			frameStats.failed++;
			return false;
		}
	}

	out.view = pages[pageIndex].view;
	out.pageIndex = pageIndex;
	out.w = w;
	out.h = h;
	out.x = x + GUTTER;
	out.y = y + GUTTER;
	out.pageSize = PAGE_SIZE;

	//Or a failing lightmap eats every page.
	if (writeTile(out, rows, pitchBytes)) {
		frameStats.placed++;
		return true;
	}

	//Returned the way it was taken, because a tile off the free list was never reserved and rolling back
	//a reservation it does not own would hand a neighbour's space out to two different lightmaps at once.
	if (fromFreeList)
		recycle(pageIndex, x, y, w + 2 * GUTTER, h + 2 * GUTTER);
	else
		unreserve(pages[pageIndex], shelfIndex, w);

	frameStats.failed++;
	return false;
}

bool LightmapAtlas::write(const Placement &placement, const void *rows, UINT pitchBytes) {
	if (!writeTile(placement, rows, pitchBytes))
		return false;

	//Packing a tile is not also a rewrite.
	frameStats.rewritten++;
	return true;
}

/** Through the update path a default-usage resource needs. */
bool LightmapAtlas::writeTile(const Placement &placement, const void *rows, UINT pitchBytes) {
	if (placement.pageIndex >= pages.size() || rows == nullptr)
		return false;

	const UINT w = placement.w;
	const UINT h = placement.h;
	const UINT paddedW = w + 2 * GUTTER;
	const UINT paddedH = h + 2 * GUTTER;

	//One upload.
	const size_t needed = (size_t)paddedW * paddedH;
	if (paddedScratch.size() < needed) {
		try {
			paddedScratch.resize(needed);
		} catch (const std::bad_alloc &) {
			return false;
		}
	}
	DWORD *padded = &paddedScratch[0];

	const BYTE *srcBase = static_cast<const BYTE *>(rows);
	for (UINT py = 0; py < paddedH; py++) {
		//Clamping replicates the edge.
		UINT sy = (py < GUTTER) ? 0 : (py - GUTTER);
		if (sy >= h)
			sy = h - 1;
		const DWORD *srcRow = reinterpret_cast<const DWORD *>(srcBase + (size_t)sy * pitchBytes);
		DWORD *dstRow = padded + (size_t)py * paddedW;

		for (UINT px = 0; px < GUTTER; px++)
			dstRow[px] = srcRow[0];
		for (UINT px = 0; px < w; px++)
			dstRow[GUTTER + px] = srcRow[px];
		for (UINT px = GUTTER + w; px < paddedW; px++)
			dstRow[px] = srcRow[w - 1];
	}

	D3D10_BOX box;
	box.left = placement.x - GUTTER;
	box.top = placement.y - GUTTER;
	box.front = 0;
	box.right = box.left + paddedW;
	box.bottom = box.top + paddedH;
	box.back = 1;
	device->UpdateSubresource(pages[placement.pageIndex].texture, 0, &box, padded, paddedW * sizeof(DWORD), 0);

	return true;
}
