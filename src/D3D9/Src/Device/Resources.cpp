/*=============================================================================
	Resources.cpp: the device objects that outlive a frame.
=============================================================================*/

#include "../D3D9Drv.h"
#include "../D3D9.h"
#include "../Shaders/ShaderPrograms.h"


void UD3D9RenderDevice::InitPermanentResourcesAndRenderingState(void) {
	guard(InitPermanentResourcesAndRenderingState);

	unsigned int u;
	HRESULT hResult;

	m_deferredDeviceReset = false;

	D3DMATRIX d3dView = { +1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, -1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, -1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, +1.0f };
	m_d3dDevice->SetTransform(D3DTS_VIEW, &d3dView);

	InitNoTextureSafe();

	m_fsBlendInfo[0] = 0.0f;
	m_fsBlendInfo[1] = 0.0f;
	m_fsBlendInfo[2] = 0.0f;
	m_fsBlendInfo[3] = 0.0f;

	m_fsBlendInfo[0] = 0.0f;

	//This render state returns to its default on every device reset,
	//so the shadow is re-seeded here as well.
	m_curAAEnable = true;

	m_d3dDevice->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
	m_d3dDevice->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
	m_d3dDevice->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);

	{
		FLOAT zeroDepthBias = 0.0f;
		m_d3dDevice->SetRenderState(D3DRS_DEPTHBIAS, *(DWORD *)&zeroDepthBias);
		m_d3dDevice->SetRenderState(D3DRS_SLOPESCALEDEPTHBIAS, *(DWORD *)&zeroDepthBias);
	}

	/*
	Bias applied to a model other than the level's own, in practice a mover.
	The constant term counts in depth buffer last-bit steps while the slope scaled term covers
	surfaces seen at an angle, where the depth gradient across a single pixel dominates whatever
	the constant term contributes, and positive is away from the camera here, zero being the near
	plane.
	*/
	const FLOAT MOVER_DEPTH_BIAS_STEPS = 4.0f;
	m_moverDepthBias = 0.0f;
	m_moverSlopeScaleDepthBias = 0.0f;
	if (m_d3dCaps.RasterCaps & D3DPRASTERCAPS_DEPTHBIAS) {
		m_moverDepthBias = MOVER_DEPTH_BIAS_STEPS / (FLOAT)((QWORD)1 << m_numDepthBits);
	}
	if (m_d3dCaps.RasterCaps & D3DPRASTERCAPS_SLOPESCALEDEPTHBIAS) {
		m_moverSlopeScaleDepthBias = 1.0f;
	}

	m_d3dDevice->SetRenderState(D3DRS_ALPHAFUNC, D3DCMP_GREATER);
	m_d3dDevice->SetRenderState(D3DRS_ALPHAREF, 127);

	m_d3dDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
	m_d3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ZERO);

	m_d3dDevice->SetRenderState(D3DRS_SHADEMODE, D3DSHADE_GOURAUD);
	m_d3dDevice->SetRenderState(D3DRS_DITHERENABLE, TRUE);

#ifdef UTGLR_RUNE_BUILD
	m_d3dDevice->SetRenderState(D3DRS_FOGTABLEMODE, D3DFOG_LINEAR);
	FLOAT fFogStart = 0.0f;
	m_d3dDevice->SetRenderState(D3DRS_FOGSTART, *(DWORD *)&fFogStart);
	m_gpFogEnabled = false;
#endif

	m_d3dDevice->SetRenderState(D3DRS_LIGHTING, FALSE);
	m_d3dDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);

	m_d3dDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
	m_d3dDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);

	for (u = 0; u < MAX_TMUNITS; u++) {
		m_curTexStageParams[u] = CT_DEFAULT_TEX_PARAMS;
	}

	if (LODBias) {
		SetTexLODBiasState(TMUnits);
	}

	if (MaxAnisotropy) {
		SetTexMaxAnisotropyState(TMUnits);
	}

	if (UseDetailAlpha) {
		InitAlphaTextureSafe();
	}

	InitOrInvalidateTexEnvState();

	for (u = 0; u < MAX_TMUNITS; u++) {
		TexInfo[u].CurrentCacheID = TEX_CACHE_ID_UNUSED;
		TexInfo[u].pBind = NULL;
	}


	D3DPOOL vertexBufferPool = D3DPOOL_DEFAULT;

	//Vertex and primary color
	hResult = m_d3dDevice->CreateVertexBuffer(sizeof(FGLVertexColor) * VERTEX_RING_SIZE, D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, 0, vertexBufferPool, &m_d3dVertexColorBuffer, NULL);
	if (FAILED(hResult)) {
		appErrorf(TEXT("CreateVertexBuffer failed"));
	}

	//Secondary color
	hResult = m_d3dDevice->CreateVertexBuffer(sizeof(FGLSecondaryColor) * VERTEX_RING_SIZE, D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, 0, vertexBufferPool, &m_d3dSecondaryColorBuffer, NULL);
	if (FAILED(hResult)) {
		appErrorf(TEXT("CreateVertexBuffer failed"));
	}

	//TexCoord
	for (u = 0; u < (DWORD)TMUnits; u++) {
		hResult = m_d3dDevice->CreateVertexBuffer(sizeof(FGLTexCoord) * VERTEX_RING_SIZE, D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, 0, vertexBufferPool, &m_d3dTexCoordBuffer[u], NULL);
		if (FAILED(hResult)) {
			appErrorf(TEXT("CreateVertexBuffer failed"));
		}
	}

	//Relative to the surface.
	hResult = m_d3dDevice->CreateIndexBuffer(sizeof(WORD) * INDEX_RING_SIZE, D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, D3DFMT_INDEX16, vertexBufferPool, &m_d3dIndexBuffer, NULL);
	if (FAILED(hResult)) {
		appErrorf(TEXT("CreateIndexBuffer failed"));
	}


	//Stream definition with vertices and color.
	hResult = m_d3dDevice->CreateVertexDeclaration(g_oneColorStreamDef, &m_oneColorVertexDecl);
	if (FAILED(hResult)) {
		appErrorf(TEXT("CreateVertexDeclaration failed"));
	}

	//Standard stream definitions with vertices, color, and a variable number of tex coords
	for (u = 0; u < (DWORD)TMUnits; u++) {
		hResult = m_d3dDevice->CreateVertexDeclaration(g_standardNTextureStreamDefs[u], &m_standardNTextureVertexDecl[u]);
		if (FAILED(hResult)) {
			appErrorf(TEXT("CreateVertexDeclaration failed"));
		}
	}

	//Stream definition with vertices, two colors, and one tex coord
	hResult = m_d3dDevice->CreateVertexDeclaration(g_twoColorSingleTextureStreamDef, &m_twoColorSingleTextureVertexDecl);
	if (FAILED(hResult)) {
		appErrorf(TEXT("CreateVertexDeclaration failed"));
	}


	m_curVertexBufferPos = 0;
	m_vertexColorBufferNeedsDiscard = false;
	m_secondaryColorBufferNeedsDiscard = false;
	for (u = 0; u < MAX_TMUNITS; u++) {
		m_texCoordBufferNeedsDiscard[u] = false;
	}

	m_curIndexBufferPos = 0;
	m_indexBufferNeedsDiscard = false;
	m_csIndexCount = 0;
	m_csIndexBufferPos = 0;


	//Vertex and primary color.
	hResult = m_d3dDevice->SetStreamSource(0, m_d3dVertexColorBuffer, 0, sizeof(FGLVertexColor));
	if (FAILED(hResult)) {
		appErrorf(TEXT("SetStreamSource failed"));
	}

	//Secondary Color
	hResult = m_d3dDevice->SetStreamSource(1, m_d3dSecondaryColorBuffer, 0, sizeof(FGLSecondaryColor));
	if (FAILED(hResult)) {
		appErrorf(TEXT("SetStreamSource failed"));
	}

	//TexCoord
	for (u = 0; u < (DWORD)TMUnits; u++) {
		hResult = m_d3dDevice->SetStreamSource(2 + u, m_d3dTexCoordBuffer[u], 0, sizeof(FGLTexCoord));
		if (FAILED(hResult)) {
			appErrorf(TEXT("SetStreamSource failed"));
		}
	}

	hResult = m_d3dDevice->SetIndices(m_d3dIndexBuffer);
	if (FAILED(hResult)) {
		appErrorf(TEXT("SetIndices failed"));
	}


	if (UseFragmentProgram) {
		TryInitializeFragmentProgramMode();
	}

	InitGammaResourcesSafe();

	InitInstanceResources();


	hResult = m_d3dDevice->SetVertexDeclaration(m_standardNTextureVertexDecl[0]);
	if (FAILED(hResult)) {
		appErrorf(TEXT("SetVertexDeclaration failed"));
	}
	m_curVertexDecl = m_standardNTextureVertexDecl[0];

	m_curVertexShader = NULL;

	m_curPixelShader = NULL;


	m_texEnableBits = 0x1;

	m_bufferedVertsType = BV_TYPE_NONE;
	m_bufferedVerts = 0;

	DiscardDeferredGouraudPolys();

	m_curDepthBias = 0.0f;
	m_curSlopeScaleDepthBias = 0.0f;

	//Seeded, the render states just programmed already matching what a default poly-flag combination describes.
	//Clearing the dirty flag matters as much.
	m_curBlendFlags = PF_Occlude;
	m_blendStateInvalid = false;
	m_smoothMaskedTexturesBit = 0;
	m_useAlphaToCoverageForMasked = false;
	m_alphaToCoverageEnabled = false;
	m_alphaTestEnabled = false;
	m_curPolyFlags = 0;
	m_curPolyFlags2 = 0;

	m_requestedColorFlags = 0;

	m_useZRangeHack = false;
	m_nearZRangeHackProjectionActive = false;
	m_requestNearZRangeHackProjection = false;

	m_sceneNodeX = 0;
	m_sceneNodeY = 0;
	appMemzero(&m_sceneNodeKey, sizeof(m_sceneNodeKey));


	unguard;
}

void UD3D9RenderDevice::FreePermanentResources(void) {
	guard(FreePermanentResources);

	unsigned int u;

	//Both callers should reach this with an empty log.
	if (!m_drawCmds.empty()) {
		debugf(NAME_Warning, TEXT("FreePermanentResources: dropping %u recorded command(s); the deferred log should have been flushed before this"),
			(DWORD)m_drawCmds.size());
	}

	ShutdownFragmentProgramMode();

	m_csBatchOpen = false;
	m_csBatchVertexCount = 0;
	m_csBatchIndexCount = 0;
	m_csBatchDepthBias = false;
	m_csBatchLocked = false;
	m_csBatchLockedLayers = 0;
	m_lightmapAtlas.Reset();

	m_drawCmds.clear();
	m_drawRuns.clear();
	m_passRuns.clear();
	ClearRecordedTextures();

	//Dropped before a reset.
	FreeGammaResources();

	//Default pool, likewise.
	ReleaseInstanceResources();


	if (m_d3dDevice) {
		//Vertex.
		m_d3dDevice->SetStreamSource(0, NULL, 0, 0);

		//Secondary Color
		m_d3dDevice->SetStreamSource(1, NULL, 0, 0);

		//TexCoord
		for (u = 0; u < (DWORD)TMUnits; u++) {
			m_d3dDevice->SetStreamSource(2 + u, NULL, 0, 0);
		}

		m_d3dDevice->SetIndices(NULL);
	}


	if (m_d3dVertexColorBuffer) {
		m_d3dVertexColorBuffer->Release();
		m_d3dVertexColorBuffer = NULL;
	}
	if (m_d3dSecondaryColorBuffer) {
		m_d3dSecondaryColorBuffer->Release();
		m_d3dSecondaryColorBuffer = NULL;
	}
	for (u = 0; u < (DWORD)TMUnits; u++) {
		if (m_d3dTexCoordBuffer[u]) {
			m_d3dTexCoordBuffer[u]->Release();
			m_d3dTexCoordBuffer[u] = NULL;
		}
	}

	if (m_d3dIndexBuffer) {
		m_d3dIndexBuffer->Release();
		m_d3dIndexBuffer = NULL;
	}


	if (m_d3dDevice) {
		m_d3dDevice->SetFVF(D3DFVF_XYZ | D3DFVF_DIFFUSE);
	}

	//Standard stream definition with vertices and color.
	if (m_oneColorVertexDecl) {
		m_oneColorVertexDecl->Release();
		m_oneColorVertexDecl = NULL;
	}
	//Standard stream definitions with vertices, color, and a variable number of tex coords
	for (u = 0; u < (DWORD)TMUnits; u++) {
		if (m_standardNTextureVertexDecl[u]) {
			m_standardNTextureVertexDecl[u]->Release();
			m_standardNTextureVertexDecl[u] = NULL;
		}
	}
	//Stream definition with vertices, two colors, and one tex coord
	if (m_twoColorSingleTextureVertexDecl) {
		m_twoColorSingleTextureVertexDecl->Release();
		m_twoColorSingleTextureVertexDecl = NULL;
	}

	unguard;
}
