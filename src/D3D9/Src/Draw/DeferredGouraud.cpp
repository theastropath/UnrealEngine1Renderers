/*=============================================================================
	DeferredGouraud.cpp: blended models held back and replayed back to front.

	Blended geometry writes no depth, so an opaque model drawn after it paints over it.
=============================================================================*/

#include "../D3D9Drv.h"
#include "../D3D9.h"

bool UD3D9RenderDevice::DeferGouraudPolygon(FSceneNode *Frame, FTextureInfo &Info, FTransTexture **Pts, INT NumPts, DWORD PolyFlags) {
	guard(UD3D9RenderDevice::DeferGouraudPolygon);

	//Both this store's own limit and the ordinary fan-width limit, matching the immediate recorder.
	if ((NumPts > MAX_DEFERRED_GP_PTS) || (NumPts > VERTEX_ARRAY_SIZE)) {
		return false;
	}

	//One mesh sends a run of polygons under one texture and flag set.
	INT texIndex = -1;
	for (INT i = m_numDeferredGouraudTextures - 1; i >= 0; i--) {
		if ((m_deferredGouraudTextures[i].Info.CacheID == Info.CacheID) && (m_deferredGouraudTextures[i].PolyFlags == PolyFlags)) {
			texIndex = i;
			break;
		}
	}

	//Replaying early costs the ordering of what is already held. Still better than dropping it.
	if ((m_numDeferredGouraudPolys >= MAX_DEFERRED_GP_POLYS) ||
		((m_numDeferredGouraudPts + NumPts) > MAX_DEFERRED_GP_PTS) ||
		((texIndex < 0) && (m_numDeferredGouraudTextures >= MAX_DEFERRED_GP_TEXTURES))) {
		EndDeferredGouraudPolysNoCheck();
		texIndex = -1;
	}

	if (m_numDeferredGouraudPolys == 0) {
		m_deferredGouraudFrame = Frame;
	}

	if (texIndex < 0) {
		//Last moment the engine's texture is guaranteed to be there, and binding it here is what uploads it.
		EndBuffering();
		SetDefaultTextureState();
		SetTextureNoPanBias(0, Info, PolyFlags);

		texIndex = m_numDeferredGouraudTextures++;
		FDeferredGouraudTexture &texture = m_deferredGouraudTextures[texIndex];
		texture.Info = Info;
		texture.PolyFlags = PolyFlags;
		//The last moment the palette pointer is valid. Computed with the same call as the bind above.
		ComposeTexCacheID(Info, PolyFlags, &texture.Is16Bit);
		//Cleared now the upload has happened.
		TexInfoClearRealtimeChanged(texture.Info);
	}

	FDeferredGouraudPoly &poly = m_deferredGouraudPolys[m_numDeferredGouraudPolys++];
	poly.PolyFlags = PolyFlags;
	poly.PolyFlags2 = NearZRangeHackFlag(PolyFlags);
	poly.TexIndex = texIndex;
	poly.FirstPt = m_numDeferredGouraudPts;
	poly.NumPts = NumPts;
#ifdef UTGLR_RUNE_BUILD
	poly.Alpha = (PolyFlags & PF_AlphaBlend) ? appRound(Info.Texture->Alpha * 255.0f) : 255;
#endif

	FLOAT depth = 0.0f;
	for (INT i = 0; i < NumPts; i++) {
		m_deferredGouraudPts[m_numDeferredGouraudPts] = *Pts[i];
		m_deferredGouraudPtPtrs[m_numDeferredGouraudPts] = &m_deferredGouraudPts[m_numDeferredGouraudPts];
		m_numDeferredGouraudPts++;
		depth += Pts[i]->Point.Z; //View space already.
	}
	poly.Depth = depth / NumPts;

	return true;
	unguard;
}

//Stable bottom-up merge sort by depth, greatest first, so that equal depths keep the engine's own
//order, and a merge sort because the input is only piecewise ordered, triangles arriving in mesh
//order with no relation to depth, which is the input an insertion sort handles quadratically.
void UD3D9RenderDevice::SortDeferredGouraudPolys(INT numPolys) {
	for (INT i = 0; i < numPolys; i++) {
		m_deferredGouraudOrder[i] = i;
	}

	INT *from = m_deferredGouraudOrder;
	INT *to = m_deferredGouraudOrderScratch;
	for (INT width = 1; width < numPolys; width *= 2) {
		for (INT lo = 0; lo < numPolys; lo += 2 * width) {
			INT mid = lo + width;
			INT hi = lo + 2 * width;
			if (mid > numPolys) mid = numPolys;
			if (hi > numPolys) hi = numPolys;

			INT a = lo, b = mid, o = lo;
			while ((a < mid) && (b < hi)) {
				//Strictly greater. An equal depth takes the left run and the sort stays stable.
				to[o++] = (m_deferredGouraudPolys[from[b]].Depth > m_deferredGouraudPolys[from[a]].Depth) ? from[b++] : from[a++];
			}
			while (a < mid) {
				to[o++] = from[a++];
			}
			while (b < hi) {
				to[o++] = from[b++];
			}
		}
		INT *swap = from;
		from = to;
		to = swap;
	}

	//Odd count lands in scratch.
	if (from != m_deferredGouraudOrder) {
		appMemcpy(m_deferredGouraudOrder, from, numPolys * sizeof(INT));
	}
}

//Furthest first.
void UD3D9RenderDevice::EndDeferredGouraudPolysNoCheck(void) {
	guard(UD3D9RenderDevice::EndDeferredGouraudPolysNoCheck);

	SortDeferredGouraudPolys(m_numDeferredGouraudPolys);

	//Counts reset first, so recursion finds nothing.
	const INT numPolys = m_numDeferredGouraudPolys;
	m_numDeferredGouraudPolys = 0;
	m_numDeferredGouraudPts = 0;
	m_numDeferredGouraudTextures = 0;

	FSceneNode *Frame = m_deferredGouraudFrame;
	{
		//A scoped guard. An error thrown mid-replay would leave the flag set.
		FScopedBoolFlag replayGuard(m_replayingDeferredGouraudPolys);

		for (INT i = 0; i < numPolys; i++) {
			const FDeferredGouraudPoly &poly = m_deferredGouraudPolys[m_deferredGouraudOrder[i]];
			m_replayPolyFlags2 = poly.PolyFlags2;
			m_replayIs16Bit = m_deferredGouraudTextures[poly.TexIndex].Is16Bit;
#ifdef UTGLR_RUNE_BUILD
			m_replayAlpha = poly.Alpha;
#endif
			DrawGouraudPolygon(Frame, m_deferredGouraudTextures[poly.TexIndex].Info,
				&m_deferredGouraudPtPtrs[poly.FirstPt], poly.NumPts, poly.PolyFlags, NULL);
		}
	}

	//A batch stays open.
	EndBuffering();

	unguard;
}
