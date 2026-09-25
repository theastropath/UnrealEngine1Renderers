/*=============================================================================
	ComplexSurfaceBatch.cpp: drawing consecutive world surfaces as one call.

	Surfaces that agree on their texture layers, or whose lightmaps have been packed onto
	the same atlas page, accumulate into one draw and not one each, and this is where
	that is tried, where the vertex and index ranges it needs are reserved and where it
	is given up on again, every reason to decline having to be ruled out before the
	lightmap is committed to a page it cannot be taken back off.
=============================================================================*/

#include "../D3D9Drv.h"
#include "../D3D9.h"

/*-----------------------------------------------------------------------------
	Batched world surfaces.
-----------------------------------------------------------------------------*/

/*
Layer flags are passed in so the deferred path can ask the same question long after submission,
and the seven bit mask with them, because a macro texture and a lightmap both arrive as
PF_Modulated and with one modulated layer only the mask says which of the two it is, where only
the lightmap wants reconstructing at all, the macro texture carrying mips and being minified in
exactly the case a cubic filter over mip zero would alias on, so the answer has to be the same
one the immediate path would have reached from state that has since gone out of scope.
*/
IDirect3DPixelShader9 *FASTCALL UD3D9RenderDevice::SelectComplexSurfacePixelShader(DWORD numLayers, const DWORD *pLayerFlags, DWORD sevenBitMask) const {
	IDirect3DPixelShader9 *pixelShader = NULL;

	const bool recon = m_sevenBitMapRecon;

	if (numLayers == 1) {
		pixelShader = m_fpComplexSurfaceSingleTexture;
	} else if (numLayers == 2) {
		if (pLayerFlags[1] == PF_Modulated) {
			pixelShader = (recon && (sevenBitMask & 2))
				? m_fpComplexSurfaceDualTextureLightRecon
				: m_fpComplexSurfaceDualTextureModulated;
		} else if (pLayerFlags[1] == PF_Highlighted) {
			pixelShader = recon
				? m_fpComplexSurfaceSingleTextureFogRecon
				: m_fpComplexSurfaceSingleTextureWithFog;
		}
	} else if (numLayers == 3) {
		if (pLayerFlags[2] == PF_Modulated) {
			//Three modulated layers can only be diffuse, macro and lightmap in that order.
			pixelShader = (recon && (sevenBitMask & 4))
				? m_fpComplexSurfaceTripleTextureLightRecon
				: m_fpComplexSurfaceTripleTextureModulated;
		} else if (pLayerFlags[2] == PF_Highlighted) {
			if (!recon) {
				pixelShader = m_fpComplexSurfaceDualTextureModulatedWithFog;
			} else if (sevenBitMask & 2) {
				pixelShader = m_fpComplexSurfaceDualTextureLightFogRecon;
			} else {
				pixelShader = m_fpComplexSurfaceDualTextureMacroFogRecon;
			}
		}
	} else if (numLayers == 4) {
		if (pLayerFlags[3] == PF_Highlighted) {
			//Diffuse, macro, lightmap, fog.
			pixelShader = (recon && (sevenBitMask & 4))
				? m_fpComplexSurfaceTripleTextureLightFogRecon
				: m_fpComplexSurfaceTripleTextureModulatedWithFog;
		}
	}

	return pixelShader;
}

/*
The filter works in texel space and a normalized coordinate cannot say how big a texel is,
which ps_3_0 gives no way of asking the sampler either, so the sizes are handed over as
constants starting at register one, layer zero being the diffuse texture and never a seven
bit map. Every layer gets a size, the ones the mask leaves out included:
the shader divides by the width, so a register left at zero puts the taps
at infinity and everything downstream turns to NaN.
*/
void FASTCALL UD3D9RenderDevice::SetSevenBitMapSizes(DWORD numLayers) {
	if (!m_sevenBitMapRecon || (numLayers < 2)) {
		return;
	}

	FLOAT sizes[SEVEN_BIT_MAP_SIZE_REG_COUNT * 4];
	memset(sizes, 0, sizeof(sizes));

	for (DWORD i = 1; i < numLayers; i++) {
		const FCachedTexture *pBind = TexInfo[i].pBind;
		const FLOAT width = (pBind != NULL) ? (FLOAT)(1 << pBind->UBits) : (FLOAT)FLightmapAtlas::PAGE_SIZE;
		const FLOAT height = (pBind != NULL) ? (FLOAT)(1 << pBind->VBits) : (FLOAT)FLightmapAtlas::PAGE_SIZE;

		FLOAT *pReg = &sizes[(i - 1) * 4];
		pReg[0] = width;
		pReg[1] = height;
		pReg[2] = 1.0f / width;
		pReg[3] = 1.0f / height;
	}

	if (m_sevenBitMapSizesValid && (memcmp(m_sevenBitMapSizes, sizes, sizeof(sizes)) == 0)) {
		return;
	}

	memcpy(m_sevenBitMapSizes, sizes, sizeof(sizes));
	m_sevenBitMapSizesValid = true;
	m_d3dDevice->SetPixelShaderConstantF(SEVEN_BIT_MAP_SIZE_REG, sizes, SEVEN_BIT_MAP_SIZE_REG_COUNT);
}

void FASTCALL UD3D9RenderDevice::BindComplexSurfaceLayer(DWORD texUnit) {
	const FGLRenderPass::FGLSinglePass &pass = MultiPass.TMU[texUnit];
	const FLightmapAtlas::FPlacement *pPlacement = pass.pPlacement;

	if (pPlacement == NULL) {
		SetTexture(texUnit, *pass.Info, pass.PolyFlags, pass.PanBias);
		TexInfo[texUnit].UOffset = 0.0f;
		TexInfo[texUnit].VOffset = 0.0f;
		return;
	}

	FTexInfo &Tex = TexInfo[texUnit];

	Tex.UPan = pass.Info->Pan.X + (pass.PanBias * pass.Info->UScale);
	Tex.VPan = pass.Info->Pan.Y + (pass.PanBias * pass.Info->VScale);

	//Normalized against the page.
	Tex.UMult = pPlacement->MultScale / pass.Info->UScale;
	Tex.VMult = pPlacement->MultScale / pass.Info->VScale;
	Tex.UOffset = pPlacement->UOffset;
	Tex.VOffset = pPlacement->VOffset;

	BindAtlasPage(texUnit, pPlacement->pTexObj, TEX_CACHE_ID_ATLAS_PAGE + (QWORD)pPlacement->PageKey);
}

//The bound fields must always describe a live cache entry.
//A stale one is dereferenced elsewhere.
void FASTCALL UD3D9RenderDevice::BindAtlasPage(DWORD texUnit, IDirect3DTexture9 *pTexObj, QWORD pageID) {
	FTexInfo &Tex = TexInfo[texUnit];

	if (Tex.CurrentCacheID == pageID) {
		return;
	}

	clock(BindCycles);
	m_d3dDevice->SetTexture(texUnit, pTexObj);
	unclock(BindCycles);

	SetTexFilter(texUnit, CT_MIN_FILTER_LINEAR | CT_MIP_FILTER_NONE | CT_MAG_FILTER_LINEAR_NOT_POINT_BIT);

	Tex.CurrentCacheID = pageID;
	Tex.CurrentDynamicPolyFlags = 0;
	Tex.pBind = NULL;
}

INT FASTCALL UD3D9RenderDevice::BuildBatchedComplexSurfaceIndices(WORD *pIndex, DWORD vertexOffset) {
	const WORD *const pFirstIndex = pIndex;

	for (DWORD PolyNum = 0; PolyNum < m_csPolyCount; PolyNum++) {
		const WORD first = (WORD)((DWORD)MultiDrawFirstArray[PolyNum] + vertexOffset);
		const INT numPts = MultiDrawCountArray[PolyNum];

		for (INT i = 2; i < numPts; i++) {
			*pIndex++ = first;
			*pIndex++ = (WORD)(first + (i - 1));
			*pIndex++ = (WORD)(first + i);
		}
	}

	return (INT)(pIndex - pFirstIndex);
}

void FASTCALL UD3D9RenderDevice::LockComplexSurfaceBatchBuffers(DWORD numLayers) {
	if (m_csBatchLocked) {
		return;
	}

	//The skipped frame scratch is much smaller than the ring.
	check(!m_frameSkipped);
	//Locked implies open.
	check(m_csBatchOpen);

	//Chunks measure from here.
	m_csBatchLockVertexPos = m_curVertexBufferPos;
	m_csBatchLockIndexPos = m_curIndexBufferPos;

	LockVertexColorBuffer();
	for (DWORD t = 0; t < numLayers; t++) {
		LockTexCoordBuffer(t);
	}
	LockIndexBuffer();

	m_csBatchLockedLayers = numLayers;
	m_csBatchLocked = true;
}

void UD3D9RenderDevice::UnlockComplexSurfaceBatchBuffers(void) {
	if (!m_csBatchLocked) {
		return;
	}
	//Cleared first.
	m_csBatchLocked = false;

	UnlockVertexColorBuffer();
	for (DWORD t = 0; t < m_csBatchLockedLayers; t++) {
		UnlockTexCoordBuffer(t);
	}
	UnlockIndexBuffer();
}

void UD3D9RenderDevice::DrawBatchedComplexSurface(void) {
	UnlockComplexSurfaceBatchBuffers();

	if (m_csBatchIndexCount == 0) {
		return;
	}

	m_d3dDevice->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, m_csBatchVertexBase, 0,
		m_csBatchVertexCount, m_csBatchIndexBase, m_csBatchIndexCount / 3);
	m_csBatchDrawCount++;

	m_csBatchVertexCount = 0;
	m_csBatchIndexCount = 0;
}

void UD3D9RenderDevice::EndComplexSurfaceBufferingNoCheck(void) {
	DrawBatchedComplexSurface();

	if (m_csBatchDepthBias) {
		SetDepthBias(0.0f);
		SetSlopeScaleDepthBias(0.0f);
		m_csBatchDepthBias = false;
	}

	m_csBatchOpen = false;
	m_bufferedVertsType = BV_TYPE_NONE;
}

void FASTCALL UD3D9RenderDevice::AppendBatchedComplexSurfaceChunk(DWORD numLayers) {
	const INT maxIndices = 3 * m_csPtCount;

	//Indices are WORDs relative to the batch's vertex base.
	const bool spanExhausted =
		((m_curVertexBufferPos - m_csBatchVertexBase) + m_csPtCount) > MAX_CS_BATCH_SPAN;

	if (((m_curVertexBufferPos + m_csPtCount) >= VERTEX_RING_SIZE) ||
		((m_curIndexBufferPos + maxIndices) > INDEX_RING_SIZE) ||
		spanExhausted) {
		//A ring wrap discards data the GPU hasn't read, so the batch draws first.
		DrawBatchedComplexSurface();

		if ((m_curVertexBufferPos + m_csPtCount) >= VERTEX_RING_SIZE) {
			FlushVertexBuffers();
		}
		if ((m_curIndexBufferPos + maxIndices) > INDEX_RING_SIZE) {
			FlushIndexBuffer();
		}

		m_csBatchVertexBase = m_curVertexBufferPos;
		m_csBatchIndexBase = m_curIndexBufferPos;
	}

	LockComplexSurfaceBatchBuffers(numLayers);

	const INT vertexRunOffset = m_curVertexBufferPos - m_csBatchLockVertexPos;

	const INT indexCount = BuildBatchedComplexSurfaceIndices(
		m_pIndexArray + (m_curIndexBufferPos - m_csBatchLockIndexPos),
		(DWORD)(m_curVertexBufferPos - m_csBatchVertexBase));
	if (indexCount == 0) {
		return;
	}

	{
		const FGLVertex *pSrcVertexArray = m_csVertexArray;
		FGLVertexColor *pVertexColorArray = m_pVertexColorArray + vertexRunOffset;
		const DWORD rpColor = m_rpColor;
		INT i = m_csPtCount;
		do {
			pVertexColorArray->x = pSrcVertexArray->x;
			pVertexColorArray->y = pSrcVertexArray->y;
			pVertexColorArray->z = pSrcVertexArray->z;
			pVertexColorArray->color = rpColor;
			pSrcVertexArray++;
			pVertexColorArray++;
		} while (--i != 0);
	}

	//Texture coordinates, one stream per layer. The vertex shader can't do it.
	for (DWORD t = 0; t < numLayers; t++) {
		const FLOAT UMult = TexInfo[t].UMult;
		const FLOAT VMult = TexInfo[t].VMult;
		//Pan folded into the offset, so the inner loop is one multiply-add per component.
		const FLOAT UAdd = TexInfo[t].UOffset - (TexInfo[t].UPan * UMult);
		const FLOAT VAdd = TexInfo[t].VOffset - (TexInfo[t].VPan * VMult);

		const FGLMapDot *pMapDot = MapDotArray;
		FGLTexCoord *pTexCoord = m_pTexCoordArray[t] + vertexRunOffset;

		INT ptCounter = m_csPtCount;

#ifdef UTGLR_INCLUDE_SSE_CODE
		if (UseSSE2) {
			//One register holds two vertices.
			const __m128 mult = _mm_setr_ps(UMult, VMult, UMult, VMult);
			const __m128 add = _mm_setr_ps(UAdd, VAdd, UAdd, VAdd);

			//Source is 16 byte aligned and stepped in 8 byte pairs, so loads are aligned; stores are not.
			for (INT pairs = ptCounter >> 1; pairs != 0; pairs--) {
				_mm_storeu_ps((FLOAT *)pTexCoord,
					_mm_add_ps(_mm_mul_ps(_mm_load_ps((const FLOAT *)pMapDot), mult), add));
				pMapDot += 2;
				pTexCoord += 2;
			}
			ptCounter &= 1;
			if (ptCounter == 0) {
				continue;
			}
		}
#endif //UTGLR_INCLUDE_SSE_CODE

		do {
			pTexCoord->u = pMapDot->u * UMult + UAdd;
			pTexCoord->v = pMapDot->v * VMult + VAdd;

			pMapDot++;
			pTexCoord++;
		} while (--ptCounter != 0);
	}

	m_curVertexBufferPos += m_csPtCount;
	m_curIndexBufferPos += indexCount;
	m_csBatchVertexCount += m_csPtCount;
	m_csBatchIndexCount += indexCount;
}

bool FASTCALL UD3D9RenderDevice::TryBatchComplexSurface(FSurfaceInfo &Surface, const FSurfaceFacet &Facet, bool biasDepth) {
	//Not with fixed function.
	if (!UseSurfaceBatching || !UseFragmentProgram) {
		return false;
	}

	const DWORD PolyFlags = Surface.PolyFlags;

	if (GIsEditor && (PolyFlags & (PF_Selected | PF_FlatShaded))) {
		return false;
	}

	//The batch has moved past.
	if ((DetailTextures != 0) && Surface.DetailTexture && !Surface.FogMap) {
		return false;
	}

	//Fog wanting its own pass means multipass.
	if (Surface.FogMap && !SinglePassFog) {
		return false;
	}

	DWORD numLayers = 0;

	StageComplexSurfaceLayer(numLayers, Surface.Texture, PolyFlags & ~PF_FlatShaded, 0.0f, false);
	numLayers++;

	if (Surface.MacroTexture) {
		StageComplexSurfaceLayer(numLayers, Surface.MacroTexture, PF_Modulated, -0.5f, false);
		numLayers++;
	}

	//Packing waits until every other reason to decline is ruled out.
	INT lightMapLayer = -1;
	if (Surface.LightMap) {
		if (numLayers >= (DWORD)TMUnits) {
			return false;
		}
		lightMapLayer = (INT)numLayers;
		//Seven bit, hence the last argument here and on the fog map below; see FGLSinglePass::bSevenBitMap.
		StageComplexSurfaceLayer(numLayers, Surface.LightMap, PF_Modulated, -0.5f, true);
		numLayers++;
	}

	if (Surface.FogMap) {
		StageComplexSurfaceLayer(numLayers, Surface.FogMap, PF_Highlighted, -0.5f, true);
		numLayers++;
	}

	//Needs a second pass.
	if (numLayers > (DWORD)TMUnits) {
		return false;
	}

	if ((m_vpComplexSurfaceCT[numLayers - 1] == NULL) || (m_standardNTextureVertexDecl[numLayers - 1] == NULL)) {
		return false;
	}

	//Layer flags in their own array, so the shader choice never reaches into per-surface state.
	DWORD layerFlags[MAX_TMUNITS];
	for (DWORD i = 0; i < numLayers; i++) {
		layerFlags[i] = MultiPass.TMU[i].PolyFlags;
	}
	const DWORD sevenBitMask = ComposeSevenBitMask(numLayers);

	IDirect3DPixelShader9 *pixelShader = SelectComplexSurfacePixelShader(numLayers, layerFlags, sevenBitMask);
	if (pixelShader == NULL) {
		//No combiner for this layer set. The per-surface path substitutes one.
		return false;
	}

	//Last decision.
	if (lightMapLayer >= 0) {
		if (!LightmapAtlas) {
			return false;
		}
		const FLightmapAtlas::FPlacement *pPlacement =
			m_lightmapAtlas.Place(m_d3dDevice, *MultiPass.TMU[lightMapLayer].Info);
		if (pPlacement == NULL) {
			return false;
		}
		MultiPass.TMU[lightMapLayer].pPlacement = pPlacement;
	}

	if (DeferredActive()) {
		m_csSelectedLayers = numLayers;
		//An immediate batch and a recorded log cannot both be open.
		EndBuffering();
		if (RecordComplexSurface(Surface, Facet, biasDepth)) {
			return true;
		}
		//Recording failed.
		FlushDeferred();
	}

	QWORD texKey[MAX_TMUNITS];
	for (DWORD i = 0; i < numLayers; i++) {
		texKey[i] = (MultiPass.TMU[i].pPlacement != NULL)
			? (TEX_CACHE_ID_ATLAS_PAGE + (QWORD)MultiPass.TMU[i].pPlacement->PageKey)
			: MultiPass.TMU[i].Info->CacheID;
	}

	bool sameState = m_csBatchOpen && (m_csBatchPassCount == numLayers) && (m_csBatchDepthBias == biasDepth) &&
		(m_csBatchSevenBitMask == sevenBitMask);
	if (sameState) {
		for (DWORD i = 0; i < numLayers; i++) {
			if ((m_csBatchTexKey[i] != texKey[i]) || (m_csBatchLayerFlags[i] != MultiPass.TMU[i].PolyFlags)) {
				sameState = false;
				break;
			}
		}
	}

	if (m_csBatchOpen && !sameState) {
		EndComplexSurfaceBufferingNoCheck();
	}

	if (!m_csBatchOpen) {
		SetBlend(MultiPass.TMU[0].PolyFlags);

		if (biasDepth) {
			SetDepthBias(m_moverDepthBias);
			SetSlopeScaleDepthBias(m_moverSlopeScaleDepthBias);
		}

		for (DWORD i = 0; i < numLayers; i++) {
			BindComplexSurfaceLayer(i);
		}
		SetSevenBitMapSizes(numLayers);

		SetStreamState(m_standardNTextureVertexDecl[numLayers - 1], m_vpComplexSurfaceCT[numLayers - 1], pixelShader);
		DisableSubsequentTextures(numLayers);

		m_csBatchOpen = true;
		m_bufferedVertsType = BV_TYPE_COMPLEX_SURFACE;
		m_csBatchVertexBase = m_curVertexBufferPos;
		m_csBatchIndexBase = m_curIndexBufferPos;
		m_csBatchVertexCount = 0;
		m_csBatchIndexCount = 0;
		m_csBatchPassCount = numLayers;
		m_csBatchDepthBias = biasDepth;
		m_csBatchSevenBitMask = sevenBitMask;
		for (DWORD i = 0; i < numLayers; i++) {
			m_csBatchTexKey[i] = texKey[i];
			m_csBatchLayerFlags[i] = MultiPass.TMU[i].PolyFlags;
		}
	} else {
		for (DWORD i = 0; i < numLayers; i++) {
			BindComplexSurfaceLayer(i);
		}
	}

	m_rpColor = 0xFFFFFFFF;
	m_csBatchSurfaceCount++;

	FSavedPoly *pNextPoly = Facet.Polys;
	do {
		const INT numVerts = BufferStaticComplexSurfaceGeometry(Facet, pNextPoly);
		if (numVerts == 0) {
			break;
		}
		m_csPtCount = numVerts;
		AppendBatchedComplexSurfaceChunk(numLayers);
	} while (pNextPoly != NULL);

	return true;
}
