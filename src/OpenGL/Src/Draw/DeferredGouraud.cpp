/*=============================================================================
	DeferredGouraud.cpp: holding blended models back for a sorted replay.
=============================================================================*/

#include "../OpenGLDrv.h"
#include "../OpenGL.h"

bool UOpenGLRenderDevice::DeferGouraudPolygon(FSceneNode *Frame, FTextureInfo &Info, FTransTexture **Pts, INT NumPts, DWORD PolyFlags) {
	guard(UOpenGLRenderDevice::DeferGouraudPolygon);

	//Both limits, because refusing on replay would drop the polygon.
	if ((NumPts > MAX_DEFERRED_GP_PTS) || (NumPts > VERTEX_ARRAY_SIZE)) {
		return false;
	}

	/*
	Searching newest first pays off because one mesh sends its whole run under a single
	texture and flag set, and keying on all of PolyFlags is coarse enough that two batches
	occasionally split where one would have served, which costs a state change now and again
	and saves a narrower comparison on every polygon that arrives.
	*/
	INT texIndex = -1;
	for (INT i = m_numDeferredGouraudTextures - 1; i >= 0; i--) {
		if ((m_deferredGouraudTextures[i].Info.CacheID == Info.CacheID) && (m_deferredGouraudTextures[i].PolyFlags == PolyFlags)) {
			texIndex = i;
			break;
		}
	}

	//Better than dropping it.
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
		//Last moment this texture is guaranteed live.
		//Binding uploads, so drain the open batch.
		EndBuffering();
		SetDefaultTextureState();
		SetTextureNoPanBias(0, Info, PolyFlags);

		texIndex = m_numDeferredGouraudTextures++;
		FDeferredGouraudTexture &texture = m_deferredGouraudTextures[texIndex];
		texture.Info = Info;
		texture.PolyFlags = PolyFlags;
		//Same call as the bind.
		ComposeTexCacheID(Info, PolyFlags, &texture.Is16Bit);
		//The upload already happened.
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

//Stable merge sort, greatest depth first.
//The input is only piecewise ordered.
void UOpenGLRenderDevice::SortDeferredGouraudPolys(INT numPolys) {
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
				//Strictly greater keeps it stable.
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

	//Odd pass count.
	if (from != m_deferredGouraudOrder) {
		appMemcpy(m_deferredGouraudOrder, from, numPolys * sizeof(INT));
	}
}

//Furthest first, after every opaque model.
void UOpenGLRenderDevice::EndDeferredGouraudPolysNoCheck(void) {
	guard(UOpenGLRenderDevice::EndDeferredGouraudPolysNoCheck);

	SortDeferredGouraudPolys(m_numDeferredGouraudPolys);

	//Counts reset first, so recursion finds nothing.
	const INT numPolys = m_numDeferredGouraudPolys;
	m_numDeferredGouraudPolys = 0;
	m_numDeferredGouraudPts = 0;
	m_numDeferredGouraudTextures = 0;

	//The replayed copy has dead mip and palette pointers.
	//The cached-bind branch never reads them.
	//Eviction cannot run while a hold-back is pending.
	FSceneNode *Frame = m_deferredGouraudFrame;
	m_replayingDeferredGouraudPolys = true;
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
	m_replayingDeferredGouraudPolys = false;

	//A batch stays open.
	EndBuffering();

	unguard;
}
