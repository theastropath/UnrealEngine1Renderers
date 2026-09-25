/*=============================================================================
	RenderPasses.cpp: how many passes a surface's texture layers take.
=============================================================================*/

#include "../D3D9Drv.h"
#include "../D3D9.h"

void UD3D9RenderDevice::RenderPassesExec(void) {
	guard(UD3D9RenderDevice::RenderPassesExec);

	//Some paths use a fragment program.

	if (m_rpMasked && m_rpForceSingle && !m_rpSetDepthEqual) {
		m_d3dDevice->SetRenderState(D3DRS_ZFUNC, D3DCMP_EQUAL);
		m_rpSetDepthEqual = true;
	}

	(this->*m_pRenderPassesNoCheckSetupProc)();

	m_rpTMUnits = 1;
	m_rpForceSingle = true;


	DrawComplexSurfacePolys();

#ifdef UTGLR_DEBUG_WORLD_WIREFRAME
	m_d3dDevice->SetRenderState(D3DRS_FILLMODE, D3DFILL_WIREFRAME);

	SetBlend(PF_Modulated);

	DrawComplexSurfacePolys();

	m_d3dDevice->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
#endif

	m_curVertexBufferPos += m_csPtCount;

#if 0
{
	dout << L"utd3d9r: PassCount = " << m_rpPassCount << std::endl;
}
#endif
	m_rpPassCount = 0;


	unguard;
}

void UD3D9RenderDevice::RenderPassesExec_SingleOrDualTextureAndDetailTexture(FTextureInfo &DetailTextureInfo) {
	guard(UD3D9RenderDevice::RenderPassesExec_SingleOrDualTextureAndDetailTexture);

	//Never forced here.
	(this->*m_pRenderPassesNoCheckSetup_SingleOrDualTextureAndDetailTextureProc)(DetailTextureInfo);

	//Detail texture is always last.


	DrawComplexSurfacePolys();

#ifdef UTGLR_DEBUG_WORLD_WIREFRAME
	m_d3dDevice->SetRenderState(D3DRS_FILLMODE, D3DFILL_WIREFRAME);

	SetBlend(PF_Modulated);

	DrawComplexSurfacePolys();

	m_d3dDevice->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
#endif

	m_curVertexBufferPos += m_csPtCount;

#if 0
{
	dout << L"utd3d9r: PassCount = " << m_rpPassCount << std::endl;
}
#endif
	m_rpPassCount = 0;


	unguard;
}


//A modulated layer pushed into a pass of its own already gets a 2X blend out of the frame buffer,
//so OneXBlending drops the dest blend to 1X, and only ever for a split-off pass, the engine's own
//2X modulate on the first one having to stand whatever this is set to.
void UD3D9RenderDevice::SetRenderPassBlend(void) {
	SetBlend(MultiPass.TMU[0].PolyFlags);

	if (OneXBlending && m_rpForceSingle && (MultiPass.TMU[0].PolyFlags & PF_Modulated)) {
		m_d3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ZERO);
		m_blendStateInvalid = true;
	}
}

//Requires at least one active pass.
void UD3D9RenderDevice::RenderPassesNoCheckSetup(void) {
	INT i;
	INT t;

	SetRenderPassBlend();

	i = 0;
	do {
		if (i != 0) {
			SetTexEnv(i, MultiPass.TMU[i].PolyFlags);
		}

		SetTexture(i, *MultiPass.TMU[i].Info, MultiPass.TMU[i].PolyFlags, MultiPass.TMU[i].PanBias);
	} while (++i < m_rpPassCount);

	SetStreamState(m_standardNTextureVertexDecl[m_rpPassCount - 1], NULL, NULL);

	DisableSubsequentTextures(m_rpPassCount);

	if ((m_curVertexBufferPos + m_csPtCount) >= VERTEX_RING_SIZE) {
		FlushVertexBuffers();
	}

	LockVertexColorBuffer();
	t = 0;
	do {
		LockTexCoordBuffer(t);
	} while (++t < m_rpPassCount);

	const FGLVertex *pSrcVertexArray = m_csVertexArray;
	FGLVertexColor *pVertexColorArray = m_pVertexColorArray;
	DWORD rpColor = m_rpColor;
	i = m_csPtCount;
	do {
		pVertexColorArray->x = pSrcVertexArray->x;
		pVertexColorArray->y = pSrcVertexArray->y;
		pVertexColorArray->z = pSrcVertexArray->z;
		pVertexColorArray->color = rpColor;
		pSrcVertexArray++;
		pVertexColorArray++;
	} while (--i != 0);

	t = 0;
	do {
		FLOAT UPan = TexInfo[t].UPan;
		FLOAT VPan = TexInfo[t].VPan;
		FLOAT UMult = TexInfo[t].UMult;
		FLOAT VMult = TexInfo[t].VMult;
		const FGLMapDot *pMapDot = MapDotArray;
		FGLTexCoord *pTexCoord = m_pTexCoordArray[t];

		INT ptCounter = m_csPtCount;
		do {
			pTexCoord->u = (pMapDot->u - UPan) * UMult;
			pTexCoord->v = (pMapDot->v - VPan) * VMult;

			pMapDot++;
			pTexCoord++;
		} while (--ptCounter != 0);
	} while (++t < m_rpPassCount);

	UnlockVertexColorBuffer();
	t = 0;
	do {
		UnlockTexCoordBuffer(t);
	} while (++t < m_rpPassCount);

	return;
}

//Requires at least one active pass.
void UD3D9RenderDevice::RenderPassesNoCheckSetup_FP(void) {
	INT i;
	FLOAT vsParams[MAX_TMUNITS * 4];
	IDirect3DPixelShader9 *pixelShader = NULL;

	SetRenderPassBlend();

	//The shared lookup.
	const DWORD sevenBitMask = ComposeSevenBitMask((DWORD)m_rpPassCount);
	if (UseFragmentProgram) {
		DWORD layerFlags[MAX_TMUNITS];
		for (i = 0; i < m_rpPassCount; i++) {
			layerFlags[i] = MultiPass.TMU[i].PolyFlags;
		}
		pixelShader = SelectComplexSurfacePixelShader((DWORD)m_rpPassCount, layerFlags, sevenBitMask);
	}

	//A shader must always be bound here.
	if (pixelShader == NULL) {
		pixelShader = m_fpComplexSurfaceSingleTexture;
	}

	i = 0;
	do {

		SetTexture(i, *MultiPass.TMU[i].Info, MultiPass.TMU[i].PolyFlags, MultiPass.TMU[i].PanBias);

		vsParams[(i * 4) + 0] = TexInfo[i].UPan;
		vsParams[(i * 4) + 1] = TexInfo[i].VPan;
		vsParams[(i * 4) + 2] = TexInfo[i].UMult;
		vsParams[(i * 4) + 3] = TexInfo[i].VMult;
	} while (++i < m_rpPassCount);
	m_d3dDevice->SetVertexShaderConstantF(6, vsParams, m_rpPassCount);

	SetSevenBitMapSizes((DWORD)m_rpPassCount);

	SetStreamState(m_oneColorVertexDecl, m_vpComplexSurface[m_rpPassCount - 1], pixelShader);

	DisableSubsequentTextures(m_rpPassCount);

	if ((m_curVertexBufferPos + m_csPtCount) >= VERTEX_RING_SIZE) {
		FlushVertexBuffers();
	}

	LockVertexColorBuffer();

	const FGLVertex *pSrcVertexArray = m_csVertexArray;
	FGLVertexColor *pVertexColorArray = m_pVertexColorArray;
	DWORD rpColor = m_rpColor;
	i = m_csPtCount;
	do {
		pVertexColorArray->x = pSrcVertexArray->x;
		pVertexColorArray->y = pSrcVertexArray->y;
		pVertexColorArray->z = pSrcVertexArray->z;
		pVertexColorArray->color = rpColor;
		pSrcVertexArray++;
		pVertexColorArray++;
	} while (--i != 0);

	UnlockVertexColorBuffer();

	return;
}

//Requires at least one active pass.
void UD3D9RenderDevice::RenderPassesNoCheckSetup_SingleOrDualTextureAndDetailTexture(FTextureInfo &DetailTextureInfo) {
	INT i;
	INT t;
	FLOAT NearZ = 380.0f;
	FLOAT RNearZ = 1.0f / NearZ;

	//Two extra units.
	m_rpPassCount += 2;

	SetBlend(MultiPass.TMU[0].PolyFlags);

	//Forced modulated here too.
	MultiPass.TMU[0].PolyFlags |= (PF_Modulated | PF_FlatShaded);

	//Detail texture occupies the first two texture units. The surface's own textures use the last two.
	i = 2;
	do {
		//No zero-index guard: the counter starts at 2 and only rises.
		SetTexEnv(i, MultiPass.TMU[i - 2].PolyFlags);

		SetTexture(i, *MultiPass.TMU[i - 2].Info, MultiPass.TMU[i - 2].PolyFlags, MultiPass.TMU[i - 2].PanBias);
	} while (++i < m_rpPassCount);

	SetAlphaTexture(0);

	SetTexEnv(1, PF_Memorized);
	SetTextureNoPanBias(1, DetailTextureInfo, PF_Modulated);

	SetStreamState(m_standardNTextureVertexDecl[m_rpPassCount - 1], NULL, NULL);

	DisableSubsequentTextures(m_rpPassCount);

	if ((m_curVertexBufferPos + m_csPtCount) >= VERTEX_RING_SIZE) {
		FlushVertexBuffers();
	}

	LockVertexColorBuffer();
	t = 0;
	do {
		LockTexCoordBuffer(t);
	} while (++t < m_rpPassCount);

	const FGLVertex *pSrcVertexArray = m_csVertexArray;
	FGLVertexColor *pVertexColorArray = m_pVertexColorArray;
	DWORD detailColor = m_detailTextureColor4ub | 0xFF000000;
	i = m_csPtCount;
	do {
		pVertexColorArray->x = pSrcVertexArray->x;
		pVertexColorArray->y = pSrcVertexArray->y;
		pVertexColorArray->z = pSrcVertexArray->z;
		pVertexColorArray->color = detailColor;
		pSrcVertexArray++;
		pVertexColorArray++;
	} while (--i != 0);

	//Unit 0.
	{
		INT t = 0;
		const FGLVertex *pVertex = &m_csVertexArray[0];
		FGLTexCoord *pTexCoord = m_pTexCoordArray[t];

		INT ptCounter = m_csPtCount;
		do {
			pTexCoord->u = pVertex->z * RNearZ;
			pTexCoord->v = 0.5f;

			pVertex++;
			pTexCoord++;
		} while (--ptCounter != 0);
	}
	//Detail texture uses texture unit 1. The remaining one or two textures use units 2 and 3.
	t = 1;
	do {
		FLOAT UPan = TexInfo[t].UPan;
		FLOAT VPan = TexInfo[t].VPan;
		FLOAT UMult = TexInfo[t].UMult;
		FLOAT VMult = TexInfo[t].VMult;
		const FGLMapDot *pMapDot = &MapDotArray[0];
		FGLTexCoord *pTexCoord = m_pTexCoordArray[t];

		INT ptCounter = m_csPtCount;
		do {
			pTexCoord->u = (pMapDot->u - UPan) * UMult;
			pTexCoord->v = (pMapDot->v - VPan) * VMult;

			pMapDot++;
			pTexCoord++;
		} while (--ptCounter != 0);
	} while (++t < m_rpPassCount);

	UnlockVertexColorBuffer();
	t = 0;
	do {
		UnlockTexCoordBuffer(t);
	} while (++t < m_rpPassCount);

	return;
}

//Requires at least one active pass.
void UD3D9RenderDevice::RenderPassesNoCheckSetup_SingleOrDualTextureAndDetailTexture_FP(FTextureInfo &DetailTextureInfo) {
	INT i;
	DWORD detailTexUnit;
	IDirect3DVertexShader9 *vertexShader = NULL;
	IDirect3DPixelShader9 *pixelShader = NULL;
	FLOAT vsParams[3 * 4];

	//One extra unit.
	m_rpPassCount += 1;

	detailTexUnit = (m_rpPassCount - 1);

	if (m_rpPassCount == 2) {
		vertexShader = m_vpComplexSurfaceSingleTextureAndDetailTexture;
	} else {
		vertexShader = m_vpComplexSurfaceDualTextureAndDetailTexture;
	}
	//The layers before the detail one, and which of them is a seven bit map.
	const DWORD sevenBitMask = ComposeSevenBitMask(detailTexUnit);
	const bool reconLight = m_sevenBitMapRecon && ((sevenBitMask & 2) != 0);

	if (DetailMax >= 2) {
		if (m_rpPassCount == 2) {
			pixelShader = m_fpSingleTextureAndDetailTextureTwoLayer;
		} else {
			pixelShader = reconLight
				? m_fpComplexSurfaceDualTextureLightDetailTwoLayerRecon
				: m_fpDualTextureAndDetailTextureTwoLayer;
		}
	} else {
		if (m_rpPassCount == 2) {
			pixelShader = m_fpSingleTextureAndDetailTexture;
		} else {
			pixelShader = reconLight
				? m_fpComplexSurfaceDualTextureLightDetailRecon
				: m_fpDualTextureAndDetailTexture;
		}
	}
	//Falls back where DetailTextures was off when the fragment programs were built.
	if (pixelShader == NULL) {
		pixelShader = (DetailMax >= 2) ? m_fpDualTextureAndDetailTextureTwoLayer : m_fpDualTextureAndDetailTexture;
	}

	SetBlend(MultiPass.TMU[0].PolyFlags);

	//First one or two textures in the first two units.
	i = 0;
	do {
		//Modulated only.

		SetTexture(i, *MultiPass.TMU[i].Info, MultiPass.TMU[i].PolyFlags, MultiPass.TMU[i].PanBias);

		vsParams[(i * 4) + 0] = TexInfo[i].UPan;
		vsParams[(i * 4) + 1] = TexInfo[i].VPan;
		vsParams[(i * 4) + 2] = TexInfo[i].UMult;
		vsParams[(i * 4) + 3] = TexInfo[i].VMult;
	} while (++i < (INT)detailTexUnit);

	//Detail texture in the second or third unit.
	SetTextureNoPanBias(detailTexUnit, DetailTextureInfo, PF_Modulated);

	vsParams[(detailTexUnit * 4) + 0] = TexInfo[detailTexUnit].UPan;
	vsParams[(detailTexUnit * 4) + 1] = TexInfo[detailTexUnit].VPan;
	vsParams[(detailTexUnit * 4) + 2] = TexInfo[detailTexUnit].UMult;
	vsParams[(detailTexUnit * 4) + 3] = TexInfo[detailTexUnit].VMult;
	m_d3dDevice->SetVertexShaderConstantF(6, vsParams, m_rpPassCount);

	SetSevenBitMapSizes(detailTexUnit);

	SetStreamState(m_oneColorVertexDecl, vertexShader, pixelShader);

	DisableSubsequentTextures(m_rpPassCount);

	if ((m_curVertexBufferPos + m_csPtCount) >= VERTEX_RING_SIZE) {
		FlushVertexBuffers();
	}

	LockVertexColorBuffer();

	const FGLVertex *pSrcVertexArray = m_csVertexArray;
	FGLVertexColor *pVertexColorArray = m_pVertexColorArray;
	DWORD detailColor = m_detailTextureColor4ub | 0xFF000000;
	i = m_csPtCount;
	do {
		pVertexColorArray->x = pSrcVertexArray->x;
		pVertexColorArray->y = pSrcVertexArray->y;
		pVertexColorArray->z = pSrcVertexArray->z;
		pVertexColorArray->color = detailColor;
		pSrcVertexArray++;
		pVertexColorArray++;
	} while (--i != 0);

	UnlockVertexColorBuffer();

	return;
}
