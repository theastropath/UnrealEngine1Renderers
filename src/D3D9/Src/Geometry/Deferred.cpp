/*=============================================================================
	Deferred.cpp: the deferred recording path.

	Recording takes a draw call's geometry into the frame arena without building any
	of it, and the flush that follows groups consecutive commands of matching state
	into runs, cuts the log into as many passes as the vertex, index and instance
	rings will hold, hands each pass to the job system split by output bytes so that
	several threads fill one set of mapped buffers at once, then submits a single
	draw per run in the order the engine gave them, blended models having already
	been reordered furthest-first before recording so that a run only ever replays.
=============================================================================*/

#include "../D3D9Drv.h"
#include "../D3D9.h"


/*
Regenerated from quadinstance.vs.hlsl with fxc and then stripped of its constant table by
blobify.ps1, which is why it comes to 70 dwords where the raw blob is 102, and it uses only
c0..c3 with a literal constant landing in c4, the register the complex surface shaders need
for the facet's map axes, so the shader is loaded straight out of this array at device creation
and the source it came from is kept beside it for the next time the two have to be compared.
*/
static const DWORD g_vpQuadInstance[] = {
	0xFFFE0300, 0x0200001F, 0x80000000, 0x900F0000, 0x0200001F, 0x80010000, 0x900F0001, 0x0200001F,
	0x80010005, 0x900F0002, 0x0200001F, 0x80020005, 0x900F0003, 0x0200001F, 0x8000000A, 0x900F0004,
	0x0200001F, 0x80000000, 0xE00F0000, 0x0200001F, 0x8000000A, 0xE00F0001, 0x0200001F, 0x80000005,
	0xE0030002, 0x02000001, 0x80030000, 0x90E40000, 0x04000012, 0x80030001, 0x80E40000, 0x90EE0001,
	0x90E40001, 0x04000012, 0x80040001, 0x80000000, 0x90550003, 0x90000003, 0x02000001, 0x80080001,
	0x90FF0000, 0x03000009, 0xE0010000, 0xA0E40000, 0x80E40001, 0x03000009, 0xE0020000, 0xA0E40001,
	0x80E40001, 0x03000009, 0xE0040000, 0xA0E40002, 0x80E40001, 0x03000009, 0xE0080000, 0xA0E40003,
	0x80E40001, 0x03000002, 0x800C0000, 0x91440002, 0x90E40002, 0x04000004, 0xE0030002, 0x80E40000,
	0x80EE0000, 0x90E40002, 0x02000001, 0xE00F0001, 0x90E40004, 0x0000FFFF
};


/*-----------------------------------------------------------------------------
	Setup and teardown.
-----------------------------------------------------------------------------*/

void UD3D9RenderDevice::InitDeferred(void) {
	m_deferredReady = false;
	m_deferredBuildFailCount = 0;
	m_pJobSystem = NULL;
	m_instancingAvailable = false;
	m_instancingGaveUp = false;
	m_d3dQuadCornerBuffer = NULL;
	m_d3dInstanceBuffer = NULL;
	m_quadInstanceVertexDecl = NULL;
	m_vpQuadInstance = NULL;
	m_curInstanceBufferPos = 0;
	m_instanceBufferNeedsDiscard = true;
	m_flushingDeferred = false;
	m_csSelectedLayers = 1;
	ClearRecordedTextures();

	//2MB per device against a 16MB default, because the arena is per device and the editor runs
	//four viewports at once inside a 2GB address space, which a block that size still covers for
	//an ordinary frame's recorded geometry, chaining an extra block only on the rare frame that
	//asks for more, and everything a recorder copies out of the engine lives here, the points,
	//the per-polygon offsets and the command records the workers read, all of it handed back in
	//one step once the flush is done with it.
	m_frameArena.Init(2 * 1024 * 1024);

	m_drawCmds.reserve(MAX_RECORDED_CMDS);
	m_drawRuns.reserve(MAX_RECORDED_CMDS);
	m_passRuns.reserve(MAX_RECORDED_CMDS);
	m_buildJobs.reserve(MAX_RECORDED_CMDS);
	m_recordedTextures.reserve(MAX_RECORDED_TEXTURES);
}

void UD3D9RenderDevice::StartJobSystem(void) {
	if (!DeferredRecording || !UseFragmentProgram) {
		debugf(NAME_Init, TEXT("Deferred recording: off, no build threads started"));
		return;
	}

	if (m_pJobSystem == NULL) {
		m_pJobSystem = new (std::nothrow) FJobSystem;
		if (m_pJobSystem == NULL) {
			m_deferredGaveUp = true;
			return;
		}
	}

	//Never from the DLL entry point.
	//That runs under the loader lock and cannot create threads.
	m_pJobSystem->Start(RenderThreads);

	debugf(NAME_Init, TEXT("Deferred recording: on, %i build thread(s)"),
		m_pJobSystem->NumParticipants());

	m_deferredReady = true;
}

void UD3D9RenderDevice::ShutdownDeferred(void) {
	m_drawCmds.clear();
	m_drawRuns.clear();
	m_buildJobs.clear();
	ClearRecordedTextures();

	if (m_pJobSystem != NULL) {
		m_pJobSystem->Stop();
		delete m_pJobSystem;
		m_pJobSystem = NULL;
	}

	m_frameArena.Free();
	m_deferredReady = false;
}

void UD3D9RenderDevice::DeferredBuildFailed(void) {
	guard(UD3D9RenderDevice::DeferredBuildFailed);

	enum { DEFERRED_BUILD_FAIL_LIMIT = 8 };
	if (++m_deferredBuildFailCount < DEFERRED_BUILD_FAIL_LIMIT) {
		return;
	}

	debugf(NAME_Init, TEXT("Deferred build faulted %i flushes in a row, falling back to the immediate path"),
		(INT)DEFERRED_BUILD_FAIL_LIMIT);

	m_deferredGaveUp = true;

	unguard;
}


//Shader mode only.
void UD3D9RenderDevice::InitInstanceResources(void) {
	ReleaseInstanceResources();

	m_instancingAvailable = false;

	if (m_d3dDevice == NULL) {
		return;
	}

	//Default pool memory.
	if (!DeferredRecording || !UseFragmentProgram) {
		return;
	}
	if (m_d3dCaps.MaxStreams <= INSTANCE_DATA_STREAM) {
		debugf(NAME_Init, TEXT("Instancing unavailable: %u vertex streams, %u needed"),
			(DWORD)m_d3dCaps.MaxStreams, (DWORD)(INSTANCE_DATA_STREAM + 1));
		m_instancingGaveUp = true;
		return;
	}

	HRESULT hResult;

	//The w component rides in the stream with the corners because as a shader literal it would
	//claim the register the complex surface shaders use for the facet's map axes, and since a
	//line's two ends land on the same y and x pattern as the quad's diagonal, one buffer and one
	//shader serve both primitives, the four quad corners sitting at the front of the array with
	//the two line ends behind them, which is also why the buffer is created write-only and never
	//written again after this one upload.
	static const FLOAT corners[INSTANCE_CORNER_COUNT * 4] = {
		0.0f, 0.0f, 0.0f, 1.0f, //quad: X1 Y1
		1.0f, 0.0f, 0.0f, 1.0f, //quad: X2 Y1
		1.0f, 1.0f, 0.0f, 1.0f, //quad: X2 Y2
		0.0f, 1.0f, 0.0f, 1.0f, //quad: X1 Y2
		0.0f, 0.0f, 0.0f, 1.0f, //line: P1
		1.0f, 1.0f, 0.0f, 1.0f //line: P2
	};

	hResult = m_d3dDevice->CreateVertexBuffer(sizeof(corners), D3DUSAGE_WRITEONLY,
		0, D3DPOOL_MANAGED, &m_d3dQuadCornerBuffer, NULL);
	if (FAILED(hResult)) {
		m_instancingGaveUp = true;
		return;
	}
	{
		void *pData = NULL;
		if (FAILED(m_d3dQuadCornerBuffer->Lock(0, 0, &pData, 0))) {
			ReleaseInstanceResources();
			m_instancingGaveUp = true;
			return;
		}
		appMemcpy(pData, corners, sizeof(corners));
		m_d3dQuadCornerBuffer->Unlock();
	}

	hResult = m_d3dDevice->CreateVertexBuffer(sizeof(FQuadCmd) * INSTANCE_RING_SIZE,
		D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, 0, D3DPOOL_DEFAULT, &m_d3dInstanceBuffer, NULL);
	if (FAILED(hResult)) {
		ReleaseInstanceResources();
		m_instancingGaveUp = true;
		return;
	}

	static const D3DVERTEXELEMENT9 instanceStreamDef[] = {
		{ INSTANCE_CORNER_STREAM, 0, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
		{ INSTANCE_DATA_STREAM, 0, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 1 },
		{ INSTANCE_DATA_STREAM, 16, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 1 },
		{ INSTANCE_DATA_STREAM, 32, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 2 },
		{ INSTANCE_DATA_STREAM, 40, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0 },
		D3DDECL_END()
	};
	static_assert(sizeof(FQuadCmd) == 44, "FQuadCmd no longer matches instanceStreamDef");
	hResult = m_d3dDevice->CreateVertexDeclaration(instanceStreamDef, &m_quadInstanceVertexDecl);
	if (FAILED(hResult)) {
		ReleaseInstanceResources();
		m_instancingGaveUp = true;
		return;
	}

	if (!LoadVertexProgram(&m_vpQuadInstance, g_vpQuadInstance, TEXT("quad instance"))) {
		ReleaseInstanceResources();
		m_instancingGaveUp = true;
		return;
	}

	m_curInstanceBufferPos = 0;
	m_instanceBufferNeedsDiscard = true;
	m_instancingAvailable = true;

	debugf(NAME_Init, TEXT("Instancing available for tiles, lines and points"));
}

void UD3D9RenderDevice::ReleaseInstanceResources(void) {
	m_instancingAvailable = false;

	if (m_d3dDevice != NULL) {
		m_d3dDevice->SetStreamSource(INSTANCE_DATA_STREAM, NULL, 0, 0);
	}

	if (m_vpQuadInstance != NULL) {
		m_vpQuadInstance->Release();
		m_vpQuadInstance = NULL;
	}
	if (m_quadInstanceVertexDecl != NULL) {
		m_quadInstanceVertexDecl->Release();
		m_quadInstanceVertexDecl = NULL;
	}
	if (m_d3dInstanceBuffer != NULL) {
		m_d3dInstanceBuffer->Release();
		m_d3dInstanceBuffer = NULL;
	}
	if (m_d3dQuadCornerBuffer != NULL) {
		m_d3dQuadCornerBuffer->Release();
		m_d3dQuadCornerBuffer = NULL;
	}
}


/*-----------------------------------------------------------------------------
	Recording.
-----------------------------------------------------------------------------*/

//The list and its indices must be cleared together.
//A refilled slot would otherwise read as a stale binding.
void UD3D9RenderDevice::ClearRecordedTextures(void) {
	m_recordedTextures.clear();
	for (DWORD u = 0; u < MAX_TMUNITS; u++) {
		m_lastRecordedTexIndex[u] = -1;
	}
}

INT FASTCALL UD3D9RenderDevice::RecordTexture(DWORD texUnit, FTextureInfo &Info, DWORD PolyFlags,
	FLOAT PanBias, const FLightmapAtlas::FPlacement *pPlacement) {
	FRecordedTexture rec;

	if (pPlacement != NULL) {
		rec.UPan = Info.Pan.X + (PanBias * Info.UScale);
		rec.VPan = Info.Pan.Y + (PanBias * Info.VScale);
		rec.UMult = pPlacement->MultScale / Info.UScale;
		rec.VMult = pPlacement->MultScale / Info.VScale;
		rec.UOffset = pPlacement->UOffset;
		rec.VOffset = pPlacement->VOffset;
		rec.TexKey = TEX_CACHE_ID_ATLAS_PAGE + (QWORD)pPlacement->PageKey;
		rec.pAtlasTexObj = pPlacement->pTexObj;
	} else {
		SetTexture(texUnit, Info, PolyFlags, PanBias);
		rec.UPan = TexInfo[texUnit].UPan;
		rec.VPan = TexInfo[texUnit].VPan;
		rec.UMult = TexInfo[texUnit].UMult;
		rec.VMult = TexInfo[texUnit].VMult;
		rec.UOffset = 0.0f;
		rec.VOffset = 0.0f;
		rec.TexKey = TexInfo[texUnit].CurrentCacheID;
		rec.pAtlasTexObj = NULL;
	}

	rec.Info = Info;
	rec.PolyFlags = PolyFlags;
	rec.PanBias = PanBias;

	//Compared per texture unit; across units the fold breaks entirely for lightmapped surfaces.
	//Pan belongs in the comparison because surfaces sharing a page can still pan apart.
	const INT prevIndex = (texUnit < MAX_TMUNITS) ? m_lastRecordedTexIndex[texUnit] : -1;
	if ((prevIndex >= 0) && (prevIndex < (INT)m_recordedTextures.size())) {
		const FRecordedTexture &prev = m_recordedTextures[prevIndex];
		if ((prev.TexKey == rec.TexKey) &&
			(prev.PolyFlags == rec.PolyFlags) &&
			(prev.pAtlasTexObj == rec.pAtlasTexObj) &&
			(prev.UPan == rec.UPan) && (prev.VPan == rec.VPan) &&
			(prev.UMult == rec.UMult) && (prev.VMult == rec.VMult) &&
			(prev.UOffset == rec.UOffset) && (prev.VOffset == rec.VOffset)) {
			return prevIndex;
		}
	}

	if (m_recordedTextures.size() >= MAX_RECORDED_TEXTURES) {
		return -1;
	}

	m_recordedTextures.push_back(rec);
	const INT newIndex = (INT)(m_recordedTextures.size() - 1);
	if (texUnit < MAX_TMUNITS) {
		m_lastRecordedTexIndex[texUnit] = newIndex;
	}
	return newIndex;
}


//Called once every reason to decline is ruled out and the lightmap placed.
//This inherits exactly the immediate path's rejections.
bool FASTCALL UD3D9RenderDevice::RecordComplexSurface(FSurfaceInfo &Surface, const FSurfaceFacet &Facet, bool biasDepth) {
	if (m_drawCmds.size() >= MAX_RECORDED_CMDS) {
		return false;
	}

	const DWORD numLayers = m_csSelectedLayers;

	DWORD numPolys = 0;
	DWORD numPoints = 0;
	DWORD numIndices = 0;
	for (const FSavedPoly *pPoly = Facet.Polys; pPoly != NULL; pPoly = pPoly->Next) {
		const INT NumPts = pPoly->NumPts;
		if (NumPts < 3) {
			continue;
		}
		numPolys++;
		numPoints += (DWORD)NumPts;
		numIndices += 3 * (DWORD)(NumPts - 2);
	}
	if (numPolys == 0) {
		return true; //Handled, nothing to draw.
	}
	//Must fit a single pass on its own, or a flush could never make progress.
	//Both bounds allow for the reserved index range at the head of a pass.
	if ((numPoints > VERTEX_RING_SIZE) || (numIndices > (INDEX_RING_SIZE - INSTANCE_INDEX_RESERVE))) {
		return false;
	}

	FComplexSurfaceCmd *pCmd = (FComplexSurfaceCmd *)m_frameArena.Alloc(sizeof(FComplexSurfaceCmd));
	FVector *pPoints = m_frameArena.AllocArray<FVector>(numPoints);
	DWORD *pPolyFirst = m_frameArena.AllocArray<DWORD>(numPolys + 1);
	if ((pCmd == NULL) || (pPoints == NULL) || (pPolyFirst == NULL)) {
		return false;
	}

	DWORD polyIndex = 0;
	DWORD pointIndex = 0;
	for (const FSavedPoly *pPoly = Facet.Polys; pPoly != NULL; pPoly = pPoly->Next) {
		INT NumPts = pPoly->NumPts;
		if (NumPts < 3) {
			continue;
		}
		pPolyFirst[polyIndex++] = pointIndex;
		FTransform *const *pPts = &pPoly->Pts[0];
		do {
			pPoints[pointIndex++] = (*pPts++)->Point;
		} while (--NumPts != 0);
	}
	pPolyFirst[numPolys] = pointIndex;

	pCmd->pPoints = pPoints;
	pCmd->pPolyFirst = pPolyFirst;
	pCmd->NumPolys = numPolys;
	pCmd->NumPoints = numPoints;
	pCmd->XAxis = Facet.MapCoords.XAxis;
	pCmd->YAxis = Facet.MapCoords.YAxis;
	pCmd->UDot = m_csUDot;
	pCmd->VDot = m_csVDot;
	pCmd->Color = 0xFFFFFFFF;
	pCmd->NumLayers = numLayers;

	FDrawCmd cmd;
	cmd.Key.Reset();
	cmd.Key.Type = DCMD_COMPLEX_SURFACE;
	cmd.Key.PolyFlags = MultiPass.TMU[0].PolyFlags;
	cmd.Key.PolyFlags2 = 0;
	cmd.Key.NumLayers = numLayers;
	cmd.Key.BiasDepth = biasDepth ? 1 : 0;
	cmd.Key.SevenBitMask = ComposeSevenBitMask(numLayers);
	cmd.FirstTexture = -1;

	for (DWORD i = 0; i < numLayers; i++) {
		const INT texIndex = RecordTexture(i, *MultiPass.TMU[i].Info, MultiPass.TMU[i].PolyFlags,
			MultiPass.TMU[i].PanBias, MultiPass.TMU[i].pPlacement);
		if (texIndex < 0) {
			return false;
		}
		pCmd->TexIndex[i] = texIndex;
		if (i == 0) {
			cmd.FirstTexture = texIndex;
		}

		const FRecordedTexture &rec = m_recordedTextures[texIndex];
		pCmd->UPan[i] = rec.UPan;
		pCmd->VPan[i] = rec.VPan;
		pCmd->UMult[i] = rec.UMult;
		pCmd->VMult[i] = rec.VMult;
		pCmd->UOffset[i] = rec.UOffset;
		pCmd->VOffset[i] = rec.VOffset;

		cmd.Key.TexKey[i] = rec.TexKey;
		cmd.Key.LayerFlags[i] = MultiPass.TMU[i].PolyFlags;
	}

	cmd.Key.ComputeHash();
	cmd.pComplex = pCmd;
	cmd.VertexCount = numPoints;
	cmd.IndexCount = numIndices;
	cmd.Instanced = 0;
	cmd.VertexBase = 0;
	cmd.IndexBase = 0;
	cmd.OutputBytes = (numPoints * (DWORD)(sizeof(FGLVertexColor) + numLayers * sizeof(FGLTexCoord))) + (numIndices * (DWORD)sizeof(WORD));

	m_drawCmds.push_back(cmd);
	m_recordedCmdCount++;
	m_csBatchSurfaceCount++;
	return true;
}


bool FASTCALL UD3D9RenderDevice::RecordGouraudPolygon(FTextureInfo &Info, FTransTexture **Pts, INT NumPts,
	DWORD PolyFlags, DWORD PolyFlags2) {
	if (NumPts < 3) {
		return true;
	}

	//Same bound the immediate path applies.
	//Stricter than the vertex ring holds, so an accepted polygon always fits.
	if (NumPts > VERTEX_ARRAY_SIZE) {
		return false;
	}

	if (m_drawCmds.size() >= MAX_RECORDED_CMDS) {
		return false;
	}

	FGouraudPolyCmd *pCmd = (FGouraudPolyCmd *)m_frameArena.Alloc(sizeof(FGouraudPolyCmd));
	FTransTexture *pPoints = m_frameArena.AllocArray<FTransTexture>((DWORD)NumPts);
	if ((pCmd == NULL) || (pPoints == NULL)) {
		return false;
	}

	for (INT i = 0; i < NumPts; i++) {
		pPoints[i] = *Pts[i];
	}

	FDrawCmd cmd;
	cmd.Key.Reset();
	cmd.Key.Type = DCMD_GOURAUD_POLY;
	cmd.Key.PolyFlags = PolyFlags;
	cmd.Key.PolyFlags2 = PolyFlags2;
	cmd.Key.NumLayers = 1;
	cmd.Key.BiasDepth = 0;

	DWORD requestedColorFlags;
	if (PolyFlags & PF_Modulated) {
		requestedColorFlags = 0;
	} else {
		requestedColorFlags = CF_COLOR_ARRAY;
#ifdef UTGLR_RUNE_BUILD
		if (((PolyFlags & (PF_RenderFog | PF_Translucent | PF_Modulated | PF_AlphaBlend)) == PF_RenderFog) && UseVertexSpecular) {
#else
		if (((PolyFlags & (PF_RenderFog | PF_Translucent | PF_Modulated)) == PF_RenderFog) && UseVertexSpecular) {
#endif
			requestedColorFlags = CF_COLOR_ARRAY | CF_FOG_MODE;
		}
	}
	cmd.Key.LayerFlags[1] = requestedColorFlags;

	DWORD effectiveFlags = PolyFlags;
	if (!(requestedColorFlags & CF_FOG_MODE)) {
		effectiveFlags &= ~PF_RenderFog;
	}
	cmd.Key.LayerFlags[0] = effectiveFlags;

	const INT texIndex = RecordTexture(0, Info, effectiveFlags, 0.0f, NULL);
	if (texIndex < 0) {
		return false;
	}
	cmd.FirstTexture = texIndex;
	cmd.Key.TexKey[0] = m_recordedTextures[texIndex].TexKey;

	pCmd->pPoints = pPoints;
	pCmd->NumPts = (DWORD)NumPts;
	pCmd->UMult = m_recordedTextures[texIndex].UMult;
	pCmd->VMult = m_recordedTextures[texIndex].VMult;
	pCmd->RequestedColorFlags = requestedColorFlags;
#ifdef UTGLR_RUNE_BUILD
	pCmd->Alpha = 255;
	if (PolyFlags & PF_AlphaBlend) {
		pCmd->Alpha = m_replayingDeferredGouraudPolys
			? m_replayAlpha
			: (BYTE)appRound(Info.Texture->Alpha * 255.0f);
	}
	cmd.Key.LayerFlags[2] = (DWORD)pCmd->Alpha;
#endif

	cmd.Key.ComputeHash();
	cmd.pGouraud = pCmd;
	cmd.VertexCount = 3 * (DWORD)(NumPts - 2);
	cmd.IndexCount = 0;
	cmd.Instanced = 0;
	cmd.VertexBase = 0;
	cmd.IndexBase = 0;
	cmd.OutputBytes = cmd.VertexCount * (DWORD)(sizeof(FGLVertexColor) + sizeof(FGLTexCoord));
	if (requestedColorFlags & CF_FOG_MODE) {
		cmd.OutputBytes += cmd.VertexCount * (DWORD)sizeof(FGLSecondaryColor);
	}

	m_drawCmds.push_back(cmd);
	m_recordedCmdCount++;
	return true;
}


bool FASTCALL UD3D9RenderDevice::RecordQuad(EDrawCmdType type, FTextureInfo *pInfo, DWORD keyPolyFlags,
	DWORD bindPolyFlags, DWORD blendPolyFlags, DWORD PolyFlags2, const FQuadCmd &quad) {
	if (m_drawCmds.size() >= MAX_RECORDED_CMDS) {
		return false;
	}

	FQuadCmd *pCmd = (FQuadCmd *)m_frameArena.Alloc(sizeof(FQuadCmd));
	if (pCmd == NULL) {
		return false;
	}
	*pCmd = quad;

	FDrawCmd cmd;
	cmd.Key.Reset();
	cmd.Key.Type = (DWORD)type;
	cmd.Key.PolyFlags = keyPolyFlags;
	cmd.Key.PolyFlags2 = PolyFlags2;
	cmd.Key.NumLayers = 1;
	cmd.Key.BiasDepth = 0;
	cmd.FirstTexture = -1;

	if (pInfo != NULL) {
		const INT texIndex = RecordTexture(0, *pInfo, bindPolyFlags, 0.0f, NULL);
		if (texIndex < 0) {
			return false;
		}
		cmd.FirstTexture = texIndex;
		cmd.Key.TexKey[0] = m_recordedTextures[texIndex].TexKey;

		const FLOAT uMult = m_recordedTextures[texIndex].UMult;
		const FLOAT vMult = m_recordedTextures[texIndex].VMult;
		pCmd->U1 *= uMult;
		pCmd->V1 *= vMult;
		pCmd->U2 *= uMult;
		pCmd->V2 *= vMult;
	} else {
		cmd.Key.TexKey[0] = TEX_CACHE_ID_NO_TEX;
	}
	cmd.Key.LayerFlags[0] = bindPolyFlags;
	cmd.Key.LayerFlags[1] = blendPolyFlags;

	cmd.Key.ComputeHash();
	cmd.pQuad = pCmd;

	{
		const DWORD numVerts = (type == DCMD_LINE) ? 2 : 6;
		cmd.VertexCount = numVerts;
		cmd.IndexCount = 0;
		cmd.OutputBytes = numVerts * (DWORD)(sizeof(FGLVertexColor) + sizeof(FGLTexCoord));
	}
	cmd.Instanced = 0;
	cmd.VertexBase = 0;
	cmd.IndexBase = 0;

	m_drawCmds.push_back(cmd);
	m_recordedCmdCount++;
	return true;
}


/*-----------------------------------------------------------------------------
	Build. Run segmentation, offsets and job partitioning.
-----------------------------------------------------------------------------*/

//Runs before any job starts, which is what lets a job write without coordinating with any
//other: every command's output offset into the vertex, index and instance rings is fixed here
//by a prefix sum over the runs in the pass, out of counts that were already known when the
//command was recorded, and never moves again afterwards, while the rings need separate cursors
//because an instanced command consumes a single instance slot where an ordinary one consumes its
//whole vertex count, and the run bases this leaves behind are what submission hands the draw
//call once the jobs have finished.
void FASTCALL UD3D9RenderDevice::BuildRunOffsets(DWORD firstRun, DWORD numRuns, DWORD passVertexBase, DWORD passIndexBase) {
	DWORD vertexCursor = 0;
	DWORD indexCursor = 0;
	DWORD instanceCursor = 0;

	for (DWORD r = firstRun; r < (firstRun + numRuns); r++) {
		FDrawRun &run = m_passRuns[r];
		const bool bInstanced = (run.InstanceCount != 0);

		run.VertexBase = bInstanced
			? ((DWORD)m_curInstanceBufferPos + instanceCursor)
			: (passVertexBase + vertexCursor);
		run.IndexBase = passIndexBase + indexCursor;
		run.VertexCount = 0;
		run.IndexCount = 0;

		for (DWORD c = run.FirstCmd; c < (run.FirstCmd + run.NumCmds); c++) {
			FDrawCmd &cmd = m_drawCmds[c];
			cmd.IndexBase = indexCursor;
			if (bInstanced) {
				cmd.VertexBase = instanceCursor;
				instanceCursor += cmd.VertexCount; //One slot per primitive
			} else {
				cmd.VertexBase = vertexCursor;
				vertexCursor += cmd.VertexCount;
				run.VertexCount += cmd.VertexCount;
			}
			indexCursor += cmd.IndexCount;
			run.IndexCount += cmd.IndexCount;
		}
	}
}


//Split by output bytes, so that many small commands and one huge command both divide into jobs
//of similar size, with a floor on that size to keep per-job overhead and shared cache-line
//writes off work too small to be worth waking a thread for, and a job otherwise covers whole
//commands, only a complex surface carrying polygon structure fine enough to be cut part way
//through and only when it is large enough to be worth cutting.
DWORD FASTCALL UD3D9RenderDevice::PartitionBuildJobs(DWORD firstCmd, DWORD numCmds) {
	m_buildJobs.clear();

	const int participants = (m_pJobSystem != NULL) ? m_pJobSystem->NumParticipants() : 1;
	if (participants <= 1) {
		FBuildJob job;
		job.FirstCmd = firstCmd;
		job.NumCmds = numCmds;
		job.FirstPoly = 0;
		job.NumPolys = 0;
		m_buildJobs.push_back(job);
		return 1;
	}

	DWORD pending = 0;
	DWORD pendingFirst = firstCmd;
	DWORD pendingBytes = 0;

	for (DWORD c = firstCmd; c < (firstCmd + numCmds); c++) {
		const FDrawCmd &cmd = m_drawCmds[c];

		//Only complex surfaces split.
		if ((cmd.Key.Type == DCMD_COMPLEX_SURFACE) && (cmd.OutputBytes >= (2 * JS_MIN_JOB_BYTES))) {
			if (pending > 0) {
				FBuildJob job;
				job.FirstCmd = pendingFirst;
				job.NumCmds = pending;
				job.FirstPoly = 0;
				job.NumPolys = 0;
				m_buildJobs.push_back(job);
				pending = 0;
				pendingBytes = 0;
			}

			const FComplexSurfaceCmd *pComplex = cmd.pComplex;
			DWORD slices = cmd.OutputBytes / JS_MIN_JOB_BYTES;
			if (slices > pComplex->NumPolys) {
				slices = pComplex->NumPolys;
			}
			if (slices == 0) {
				slices = 1;
			}
			const DWORD polysPerSlice = (pComplex->NumPolys + slices - 1) / slices;

			for (DWORD p = 0; p < pComplex->NumPolys; p += polysPerSlice) {
				DWORD count = polysPerSlice;
				if ((p + count) > pComplex->NumPolys) {
					count = pComplex->NumPolys - p;
				}
				FBuildJob job;
				job.FirstCmd = c;
				job.NumCmds = 1;
				job.FirstPoly = p;
				job.NumPolys = count;
				m_buildJobs.push_back(job);
			}

			pendingFirst = c + 1;
			continue;
		}

		if (pending == 0) {
			pendingFirst = c;
			pendingBytes = 0;
		}
		pending++;
		pendingBytes += cmd.OutputBytes;

		if (pendingBytes >= JS_MIN_JOB_BYTES) {
			FBuildJob job;
			job.FirstCmd = pendingFirst;
			job.NumCmds = pending;
			job.FirstPoly = 0;
			job.NumPolys = 0;
			m_buildJobs.push_back(job);
			pending = 0;
			pendingBytes = 0;
			pendingFirst = c + 1;
		}
	}

	if (pending > 0) {
		FBuildJob job;
		job.FirstCmd = pendingFirst;
		job.NumCmds = pending;
		job.FirstPoly = 0;
		job.NumPolys = 0;
		m_buildJobs.push_back(job);
	}

	return (DWORD)m_buildJobs.size();
}


/*-----------------------------------------------------------------------------
	Flush: passes, locking, dispatch and submission.
-----------------------------------------------------------------------------*/

static inline bool IsQuadType(DWORD type) {
	return (type == DCMD_TILE) || (type == DCMD_LINE) || (type == DCMD_POINT);
}


void UD3D9RenderDevice::FlushDeferred(void) {
	//Submission itself can trigger state changes. A nested flush from either would draw the log twice.
	if (m_flushingDeferred) {
		return;
	}
	if (m_drawCmds.empty()) {
		ClearRecordedTextures();
		return;
	}
	if (m_frameSkipped) {
		m_drawCmds.clear();
		ClearRecordedTextures();
		m_frameArena.Reset();
		return;
	}

	//An unwind from the locking calls below is the only way this could stay set, and the damage
	//would be permanent, every later flush seeing the guard, returning early and never discarding
	//the log, so the flag belongs to a scoped object that clears it however the scope is left,
	//which is not theoretical: a failed buffer lock raises a fatal error and that error unwinds
	//straight out through here.
	FScopedBoolFlag flushGuard(m_flushingDeferred);

	const DWORD numCmds = (DWORD)m_drawCmds.size();

	//Consecutive only.
	m_drawRuns.clear();
	for (DWORD c = 0; c < numCmds; c++) {
		if (!m_drawRuns.empty()) {
			FDrawRun &open = m_drawRuns.back();
			if (m_drawCmds[open.FirstCmd].Key.Matches(m_drawCmds[c].Key)) {
				open.NumCmds++;
				continue;
			}
		}
		FDrawRun run;
		run.FirstCmd = c;
		run.NumCmds = 1;
		run.VertexBase = 0;
		run.IndexBase = 0;
		run.VertexCount = 0;
		run.IndexCount = 0;
		run.InstanceCount = 0;
		m_drawRuns.push_back(run);
	}
	m_runCount += (DWORD)m_drawRuns.size();

	if (m_instancingAvailable) {
		for (DWORD r = 0; r < (DWORD)m_drawRuns.size(); r++) {
			FDrawRun &run = m_drawRuns[r];
			if (!IsQuadType(m_drawCmds[run.FirstCmd].Key.Type) || (run.NumCmds < INSTANCE_MIN_RUN)) {
				continue;
			}
			run.InstanceCount = run.NumCmds;
			for (DWORD c = run.FirstCmd; c < (run.FirstCmd + run.NumCmds); c++) {
				FDrawCmd &cmd = m_drawCmds[c];
				//Draws the unit topology.
				cmd.Instanced = 1;
				cmd.VertexCount = 1;
				cmd.IndexCount = 0;
				cmd.OutputBytes = sizeof(FQuadCmd);
			}
		}
	}

	const DWORD numRuns = (DWORD)m_drawRuns.size();
	DWORD runIdx = 0;
	DWORD cmdCursorInRun = 0;
	INT stalledPasses = 0;
	//Tested once at the end of the flush.
	//Per pass, a recurring fault would keep being reset by an unrelated successful pass.
	bool anyPassFaulted = false;

	//--- One pass per ringful ------------------------------------------------
	while (runIdx < numRuns) {
		//Unit quad/line topology sits at the head of each pass's index range.
		//The permanently bound index buffer then never swaps.
		if ((DWORD)(INDEX_RING_SIZE - m_curIndexBufferPos) < INSTANCE_INDEX_RESERVE) {
			FlushIndexBuffer();
		}

		const DWORD unitIndexBase = (DWORD)m_curIndexBufferPos;
		const DWORD passIndexBase = unitIndexBase + INSTANCE_INDEX_RESERVE;
		const DWORD passVertexBase = (DWORD)m_curVertexBufferPos;
		const DWORD passInstanceBase = (DWORD)m_curInstanceBufferPos;

		m_passRuns.clear();
		DWORD vertUsed = 0;
		DWORD indexUsed = 0;
		DWORD instUsed = 0;
		DWORD passFirstCmd = 0;
		DWORD passNumCmds = 0;
		DWORD maxLayers = 1;
		bool wantSecondaryColor = false;
		bool anyInstanced = false;
		//Which ring ran out.
		bool vertexRingFull = false;
		bool indexRingFull = false;
		bool instanceRingFull = false;

		while (runIdx < numRuns) {
			const FDrawRun &src = m_drawRuns[runIdx];
			const bool bInstanced = (src.InstanceCount != 0);
			const DWORD firstCmd = src.FirstCmd + cmdCursorInRun;
			const DWORD remaining = src.NumCmds - cmdCursorInRun;

			//How much still fits.
			DWORD take = 0;
			DWORD v = 0, ix = 0, inst = 0;
			for (; take < remaining; take++) {
				const FDrawCmd &c = m_drawCmds[firstCmd + take];
				const DWORD nv = bInstanced ? 0 : c.VertexCount;
				const DWORD ninst = bInstanced ? c.VertexCount : 0;
				if ((passVertexBase + vertUsed + v + nv) > VERTEX_RING_SIZE) {
					vertexRingFull = true;
					break;
				}
				if ((passIndexBase + indexUsed + ix + c.IndexCount) > INDEX_RING_SIZE) {
					indexRingFull = true;
					break;
				}
				if ((passInstanceBase + instUsed + inst + ninst) > INSTANCE_RING_SIZE) {
					instanceRingFull = true;
					break;
				}
				v += nv;
				ix += c.IndexCount;
				inst += ninst;
			}

			if (take == 0) {
				break; //Pass is full
			}

			FDrawRun run = src;
			run.FirstCmd = firstCmd;
			run.NumCmds = take;
			run.InstanceCount = bInstanced ? take : 0;
			m_passRuns.push_back(run);

			if (passNumCmds == 0) {
				passFirstCmd = firstCmd;
			}
			passNumCmds += take;
			vertUsed += v;
			indexUsed += ix;
			instUsed += inst;
			anyInstanced = anyInstanced || bInstanced;

			for (DWORD c = firstCmd; c < (firstCmd + take); c++) {
				const FDrawCmd &cmd = m_drawCmds[c];
				if (cmd.Key.NumLayers > maxLayers) {
					maxLayers = cmd.Key.NumLayers;
				}
				if ((cmd.Key.Type == DCMD_GOURAUD_POLY) && (cmd.Key.LayerFlags[1] & CF_FOG_MODE)) {
					wantSecondaryColor = true;
				}
			}

			cmdCursorInRun += take;
			if (cmdCursorInRun == src.NumCmds) {
				runIdx++;
				cmdCursorInRun = 0;
			} else {
				break; //Out of room mid-run.
			}
		}

		if (passNumCmds == 0) {
			if (++stalledPasses > 2) {
				//Skips the one oversized command;
				//breaking out of the pass loop would lose everything behind it.
				//Unreachable today, every recorder bounding what it accepts.
				const FDrawRun &stalledRun = m_drawRuns[runIdx];
				debugf(NAME_Warning, TEXT("Deferred flush skipped command %u of %u: larger than the vertex rings"),
					stalledRun.FirstCmd + cmdCursorInRun, numCmds);

				cmdCursorInRun++;
				if (cmdCursorInRun == stalledRun.NumCmds) {
					runIdx++;
					cmdCursorInRun = 0;
				}
				continue;
			}
			/*
			Rewinds only the ring that actually ran out, because the instance ring is the expensive
			one to wrap - the next instanced draw has to discard-map its buffer before it can write
			a thing - and an ordinary vertex ring wrap has no business paying that cost, while the
			vertex and index rings do rewind together, an index addressing a vertex and one rewound
			without the other leaving the indices pointing into the previous pass's geometry.
			*/
			if (vertexRingFull || indexRingFull) {
				FlushVertexBuffers();
				FlushIndexBuffer();
			}
			if (instanceRingFull) {
				m_curInstanceBufferPos = 0;
				m_instanceBufferNeedsDiscard = true;
			}
			continue;
		}
		stalledPasses = 0;

		//Asserted.
		//Truncating an over-wide layer count turns it into an unchecked null write on a worker thread.
		check(maxLayers <= (DWORD)TMUnits);

		//--- Offsets, then one lock per stream for the whole pass ------------
		BuildRunOffsets(0, (DWORD)m_passRuns.size(), passVertexBase, passIndexBase);

		FBuildContext ctx;
		appMemzero(&ctx, sizeof(ctx));
		ctx.pDevice = this;
		ctx.pCmds = &m_drawCmds[0];

		LockVertexColorBuffer();
		ctx.pVertexColor = m_pVertexColorArray;
		for (DWORD t = 0; t < maxLayers; t++) {
			LockTexCoordBuffer(t);
			ctx.pTexCoord[t] = m_pTexCoordArray[t];
		}
		if (wantSecondaryColor) {
			LockSecondaryColorBuffer();
			ctx.pSecondaryColor = m_pSecondaryColorArray;
		}

		BYTE *pIndexData = NULL;
		{
			DWORD lockFlags = D3DLOCK_NOSYSLOCK;
			if (m_indexBufferNeedsDiscard) {
				m_indexBufferNeedsDiscard = false;
				lockFlags |= D3DLOCK_DISCARD;
			} else {
				lockFlags |= D3DLOCK_NOOVERWRITE;
			}
			//Whole buffer lock only.
			if (FAILED(m_d3dIndexBuffer->Lock(0, 0, (VOID **)&pIndexData, lockFlags))) {
				appErrorf(TEXT("Index buffer lock failed"));
			}
			ctx.pIndex = (WORD *)(pIndexData + (passIndexBase * sizeof(WORD)));

			if (anyInstanced) {
				WORD *pUnit = (WORD *)(pIndexData + (unitIndexBase * sizeof(WORD)));
				pUnit[0] = INSTANCE_QUAD_FIRST_CORNER + 0;
				pUnit[1] = INSTANCE_QUAD_FIRST_CORNER + 1;
				pUnit[2] = INSTANCE_QUAD_FIRST_CORNER + 2;
				pUnit[3] = INSTANCE_QUAD_FIRST_CORNER + 0;
				pUnit[4] = INSTANCE_QUAD_FIRST_CORNER + 2;
				pUnit[5] = INSTANCE_QUAD_FIRST_CORNER + 3;
				pUnit[6] = INSTANCE_LINE_FIRST_CORNER + 0;
				pUnit[7] = INSTANCE_LINE_FIRST_CORNER + 1;
			}
		}

		BYTE *pInstanceData = NULL;
		if (anyInstanced && (instUsed > 0)) {
			DWORD lockFlags = D3DLOCK_NOSYSLOCK;
			if (m_instanceBufferNeedsDiscard) {
				m_instanceBufferNeedsDiscard = false;
				lockFlags |= D3DLOCK_DISCARD;
			} else {
				lockFlags |= D3DLOCK_NOOVERWRITE;
			}
			if (FAILED(m_d3dInstanceBuffer->Lock(0, 0, (VOID **)&pInstanceData, lockFlags))) {
				appErrorf(TEXT("Instance buffer lock failed"));
			}
			ctx.pInstance = ((FQuadCmd *)pInstanceData) + passInstanceBase;
		}

		//--- Fill, in parallel ----------------------------------------------
		const DWORD numJobs = PartitionBuildJobs(passFirstCmd, passNumCmds);
		ctx.pJobs = m_buildJobs.empty() ? NULL : &m_buildJobs[0];

		bool buildOk = true;
		if (numJobs > 0) {
			const DWORD startCycles = appCycles();
			if (m_pJobSystem != NULL) {
				buildOk = m_pJobSystem->Dispatch((int)numJobs, BuildJobEntry, &ctx);
			} else {
				for (DWORD j = 0; j < numJobs; j++) {
					RunBuildJob(ctx, m_buildJobs[j]);
				}
			}
			m_buildMicroseconds += (DWORD)((appCycles() - startCycles) * GSecondsPerCycle * 1000000.0);
			m_buildJobCount += numJobs;
		}

		//--- Unlock, then submit -------------------------------------------
		if (pInstanceData != NULL) {
			if (FAILED(m_d3dInstanceBuffer->Unlock())) {
				appErrorf(TEXT("Instance buffer unlock failed"));
			}
		}
		if (FAILED(m_d3dIndexBuffer->Unlock())) {
			appErrorf(TEXT("Index buffer unlock failed"));
		}
		if (wantSecondaryColor) {
			UnlockSecondaryColorBuffer();
		}
		for (DWORD t = 0; t < maxLayers; t++) {
			UnlockTexCoordBuffer(t);
		}
		UnlockVertexColorBuffer();

		if (buildOk) {
			for (DWORD r = 0; r < (DWORD)m_passRuns.size(); r++) {
				SubmitRun(m_passRuns[r], passVertexBase, unitIndexBase);
			}
		} else {
			debugf(NAME_Warning, TEXT("Deferred build faulted; pass dropped"));
			anyPassFaulted = true;
		}

		m_curVertexBufferPos = (INT)(passVertexBase + vertUsed);
		m_curIndexBufferPos = (INT)(passIndexBase + indexUsed);
		m_curInstanceBufferPos = (INT)(passInstanceBase + instUsed);
	}

	if (anyPassFaulted) {
		DeferredBuildFailed();
	} else {
		m_deferredBuildFailCount = 0;
	}

	m_drawCmds.clear();
	m_drawRuns.clear();
	m_passRuns.clear();
	ClearRecordedTextures();

	//Rewound per flush: a frame can hard sync many times,
	//so waiting for the frame boundary would grow the arena to a whole frame's geometry.
	m_frameArena.Reset();
}


void FASTCALL UD3D9RenderDevice::SubmitRun(const FDrawRun &run, DWORD passVertexBase, DWORD unitIndexBase) {
	const FDrawCmd &first = m_drawCmds[run.FirstCmd];
	const DWORD type = first.Key.Type;

	//Per run.
	//A pass mixes types, and a leftover near-Z projection would apply to unrelated surfaces.
	if ((type == DCMD_TILE) && NoAATiles) {
		SetDisabledAAState();
	} else {
		SetDefaultAAState();
	}
	SetProjectionState((first.Key.PolyFlags2 & PF2_NEAR_Z_RANGE_HACK) != 0);

	if (IsQuadType(type)) {
		SetBlend(first.Key.LayerFlags[1]);
		if (first.FirstTexture >= 0) {
			//Non-const because binding writes to it, which is safe here: the texture was already
			//bound once when it was recorded, and nothing evicts a cache entry before this run's
			//flush completes.
			FRecordedTexture &rec = m_recordedTextures[first.FirstTexture];
			SetTextureNoPanBias(0, rec.Info, rec.PolyFlags);
		} else {
			SetNoTexture(0);
		}

		if (run.InstanceCount != 0) {
			SetStreamState(m_quadInstanceVertexDecl, m_vpQuadInstance, m_fpDefaultRenderingState);
			SetDefaultTextureState();

			//Stream 0, for D3D9.
			m_d3dDevice->SetStreamSource(INSTANCE_CORNER_STREAM, m_d3dQuadCornerBuffer, 0, sizeof(FLOAT) * 4);
			m_d3dDevice->SetStreamSource(INSTANCE_DATA_STREAM, m_d3dInstanceBuffer,
				run.VertexBase * sizeof(FQuadCmd), sizeof(FQuadCmd));
			m_d3dDevice->SetStreamSourceFreq(INSTANCE_CORNER_STREAM, D3DSTREAMSOURCE_INDEXEDDATA | run.InstanceCount);
			m_d3dDevice->SetStreamSourceFreq(INSTANCE_DATA_STREAM, D3DSTREAMSOURCE_INSTANCEDATA | 1);

			if (type == DCMD_LINE) {
				m_d3dDevice->DrawIndexedPrimitive(D3DPT_LINELIST, 0, 0, INSTANCE_CORNER_COUNT,
					unitIndexBase + 6, 1);
			} else {
				m_d3dDevice->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, INSTANCE_CORNER_COUNT,
					unitIndexBase, 2);
			}
			m_drawCallCount++;

			m_d3dDevice->SetStreamSourceFreq(INSTANCE_CORNER_STREAM, 1);
			m_d3dDevice->SetStreamSourceFreq(INSTANCE_DATA_STREAM, 1);
			m_d3dDevice->SetStreamSource(INSTANCE_CORNER_STREAM, m_d3dVertexColorBuffer, 0, sizeof(FGLVertexColor));
			return;
		}

		SetDefaultStreamState();
		SetDefaultTextureState();
		if (type == DCMD_LINE) {
			m_d3dDevice->DrawPrimitive(D3DPT_LINELIST, run.VertexBase, run.VertexCount / 2);
		} else {
			m_d3dDevice->DrawPrimitive(D3DPT_TRIANGLELIST, run.VertexBase, run.VertexCount / 3);
		}
		m_drawCallCount++;
		return;
	}

	if (type == DCMD_GOURAUD_POLY) {
		const DWORD effectiveFlags = first.Key.LayerFlags[0];
		const DWORD colorFlags = first.Key.LayerFlags[1];

		SetDefaultTextureState();
		SetBlend(effectiveFlags);
		FRecordedTexture &rec = m_recordedTextures[first.FirstTexture];
		SetTexture(0, rec.Info, rec.PolyFlags, 0.0f);

		{
			IDirect3DVertexDeclaration9 *vertexDecl = (colorFlags & CF_FOG_MODE)
				? m_twoColorSingleTextureVertexDecl
				: m_standardNTextureVertexDecl[0];
			IDirect3DVertexShader9 *vertexShader = NULL;
			IDirect3DPixelShader9 *pixelShader = NULL;

			if (UseFragmentProgram) {
				vertexShader = m_vpDefaultRenderingState;
				pixelShader = m_fpDefaultRenderingState;
				if (colorFlags & CF_FOG_MODE) {
					vertexShader = m_vpDefaultRenderingStateWithFog;
					pixelShader = m_fpDefaultRenderingStateWithFog;
				}
#ifdef UTGLR_RUNE_BUILD
				//Constant across a flush.
				if (m_gpFogEnabled) {
					vertexShader = m_vpDefaultRenderingStateWithLinearFog;
					pixelShader = m_fpDefaultRenderingStateWithLinearFog;
				}
#endif
			}
			SetStreamState(vertexDecl, vertexShader, pixelShader);
		}
		DisableSubsequentTextures(1);

		m_d3dDevice->DrawPrimitive(D3DPT_TRIANGLELIST, run.VertexBase, run.VertexCount / 3);
		m_drawCallCount++;
		return;
	}

	//--- Complex surface ---------------------------------------------------
	const DWORD numLayers = first.Key.NumLayers;

	IDirect3DPixelShader9 *pixelShader = SelectComplexSurfacePixelShader(numLayers, first.Key.LayerFlags, first.Key.SevenBitMask);
	if ((pixelShader == NULL) || (m_vpComplexSurfaceCT[numLayers - 1] == NULL) ||
		(m_standardNTextureVertexDecl[numLayers - 1] == NULL)) {
		return;
	}

	SetBlend(first.Key.PolyFlags);

	if (first.Key.BiasDepth) {
		SetDepthBias(m_moverDepthBias);
		SetSlopeScaleDepthBias(m_moverSlopeScaleDepthBias);
	}

	for (DWORD i = 0; i < numLayers; i++) {
		FRecordedTexture &rec = m_recordedTextures[first.pComplex->TexIndex[i]];
		if (rec.pAtlasTexObj != NULL) {
			//All load bearing.
			BindAtlasPage(i, rec.pAtlasTexObj, rec.TexKey);
		} else {
			SetTexture(i, (FTextureInfo &)rec.Info, rec.PolyFlags, rec.PanBias);
		}
	}
	SetSevenBitMapSizes(numLayers);

	SetStreamState(m_standardNTextureVertexDecl[numLayers - 1], m_vpComplexSurfaceCT[numLayers - 1], pixelShader);
	DisableSubsequentTextures(numLayers);

	if (run.IndexCount != 0) {
		//Indices a job writes are pass-relative, that being the only offset it can know without
		//reading another job's progress, and it stays inside 16 bits.
		m_d3dDevice->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, passVertexBase,
			run.VertexBase - passVertexBase, run.VertexCount, run.IndexBase, run.IndexCount / 3);
		m_drawCallCount++;
		m_csBatchDrawCount++;
	}

	if (first.Key.BiasDepth) {
		SetDepthBias(0.0f);
		SetSlopeScaleDepthBias(0.0f);
	}
}


/*-----------------------------------------------------------------------------
	Build: the job bodies.

	Several threads run these at once, so a job body touches only the command records and its
	own slice of the mapped buffers, with no graphics API calls, no allocation and nothing that
	can unwind, which is also why every offset a job writes to was settled before the first one
	started.
-----------------------------------------------------------------------------*/

void UD3D9RenderDevice::BuildJobEntry(void *pContext, int jobIndex) {
	const FBuildContext &ctx = *(const FBuildContext *)pContext;
	ctx.pDevice->RunBuildJob(ctx, ctx.pJobs[jobIndex]);
}

void FASTCALL UD3D9RenderDevice::RunBuildJob(const FBuildContext &ctx, const FBuildJob &job) {
	if (job.NumPolys != 0) {
		FillComplexSurfaceCmd(ctx, ctx.pCmds[job.FirstCmd], job.FirstPoly, job.NumPolys);
		return;
	}

	for (DWORD c = job.FirstCmd; c < (job.FirstCmd + job.NumCmds); c++) {
		const FDrawCmd &cmd = ctx.pCmds[c];
		switch (cmd.Key.Type) {
			case DCMD_COMPLEX_SURFACE:
				FillComplexSurfaceCmd(ctx, cmd, 0, cmd.pComplex->NumPolys);
				break;
			case DCMD_GOURAUD_POLY:
				FillGouraudPolyCmd(ctx, cmd);
				break;
			default:
				FillQuadCmd(ctx, cmd);
				break;
		}
	}
}


//A couple of dot products per vertex per layer, times tens of thousands of vertices:
//the work this path exists to move off the engine's thread.
void FASTCALL UD3D9RenderDevice::FillComplexSurfaceCmd(const FBuildContext &ctx, const FDrawCmd &cmd,
	DWORD firstPoly, DWORD numPolys) {
	const FComplexSurfaceCmd *pSrc = cmd.pComplex;
	const DWORD numLayers = cmd.Key.NumLayers;

	const FLOAT xAxisX = pSrc->XAxis.X, xAxisY = pSrc->XAxis.Y, xAxisZ = pSrc->XAxis.Z;
	const FLOAT yAxisX = pSrc->YAxis.X, yAxisY = pSrc->YAxis.Y, yAxisZ = pSrc->YAxis.Z;
	const FLOAT uDot = pSrc->UDot;
	const FLOAT vDot = pSrc->VDot;
	const DWORD color = pSrc->Color;

	const DWORD firstPoint = pSrc->pPolyFirst[firstPoly];
	const DWORD lastPoint = pSrc->pPolyFirst[firstPoly + numPolys];
	const DWORD vertexBase = cmd.VertexBase + firstPoint;

	FGLVertexColor *pOutVertex = ctx.pVertexColor + vertexBase;
	const FVector *pPoint = pSrc->pPoints + firstPoint;

	for (DWORD i = firstPoint; i < lastPoint; i++) {
		pOutVertex->x = pPoint->X;
		pOutVertex->y = pPoint->Y;
		pOutVertex->z = pPoint->Z;
		pOutVertex->color = color;
		pOutVertex++;
		pPoint++;
	}

	for (DWORD t = 0; t < numLayers; t++) {
		const FLOAT uPan = pSrc->UPan[t];
		const FLOAT vPan = pSrc->VPan[t];
		const FLOAT uMult = pSrc->UMult[t];
		const FLOAT vMult = pSrc->VMult[t];
		const FLOAT uOffset = pSrc->UOffset[t];
		const FLOAT vOffset = pSrc->VOffset[t];

		FGLTexCoord *pOutCoord = ctx.pTexCoord[t] + vertexBase;
		const FVector *pSrcPoint = pSrc->pPoints + firstPoint;

		for (DWORD i = firstPoint; i < lastPoint; i++) {
			const FLOAT dotU = (xAxisX * pSrcPoint->X + xAxisY * pSrcPoint->Y + xAxisZ * pSrcPoint->Z) - uDot;
			const FLOAT dotV = (yAxisX * pSrcPoint->X + yAxisY * pSrcPoint->Y + yAxisZ * pSrcPoint->Z) - vDot;

			pOutCoord->u = (dotU - uPan) * uMult + uOffset;
			pOutCoord->v = (dotV - vPan) * vMult + vOffset;

			pOutCoord++;
			pSrcPoint++;
		}
	}

	//Run-relative, so several commands can share one base, and this slice has to start past
	//whatever the polygons ahead of it produced, a count it walks for itself because a shared
	//cursor would mean reading another job's progress, which is cheap next to the geometry it
	//guards: the walk is one subtraction per polygon skipped, against the dot products every
	//point of them would have cost.
	DWORD indexCursor = cmd.IndexBase;
	for (DWORD p = 0; p < firstPoly; p++) {
		indexCursor += 3 * (pSrc->pPolyFirst[p + 1] - pSrc->pPolyFirst[p] - 2);
	}

	WORD *pOutIndex = ctx.pIndex + indexCursor;
	for (DWORD p = firstPoly; p < (firstPoly + numPolys); p++) {
		const DWORD polyFirst = pSrc->pPolyFirst[p];
		const DWORD polyCount = pSrc->pPolyFirst[p + 1] - polyFirst;
		const WORD first = (WORD)(cmd.VertexBase + polyFirst);

		for (DWORD i = 2; i < polyCount; i++) {
			*pOutIndex++ = first;
			*pOutIndex++ = (WORD)(first + (i - 1));
			*pOutIndex++ = (WORD)(first + i);
		}
	}
}


void FASTCALL UD3D9RenderDevice::FillGouraudPolyCmd(const FBuildContext &ctx, const FDrawCmd &cmd) {
	const FGouraudPolyCmd *pSrc = cmd.pGouraud;
	const DWORD numPts = pSrc->NumPts;
	const FLOAT uMult = pSrc->UMult;
	const FLOAT vMult = pSrc->VMult;
	const DWORD colorFlags = pSrc->RequestedColorFlags;

	FGLVertexColor *pOutVertex = ctx.pVertexColor + cmd.VertexBase;
	FGLTexCoord *pOutCoord = ctx.pTexCoord[0] + cmd.VertexBase;
	FGLSecondaryColor *pOutFog = ((colorFlags & CF_FOG_MODE) && (ctx.pSecondaryColor != NULL))
		? (ctx.pSecondaryColor + cmd.VertexBase)
		: NULL;

	for (DWORD i = 2; i < numPts; i++) {
		const FTransTexture *tri[3];
		tri[0] = &pSrc->pPoints[0];
		tri[1] = &pSrc->pPoints[i - 1];
		tri[2] = &pSrc->pPoints[i];

		for (DWORD v = 0; v < 3; v++) {
			const FTransTexture *pPt = tri[v];

			pOutVertex->x = pPt->Point.X;
			pOutVertex->y = pPt->Point.Y;
			pOutVertex->z = pPt->Point.Z;

			//A saturating conversion, since values above 1.0 have to clamp to 255 here exactly as
			//they do on the immediate path, and if the two disagreed by even one channel then the
			//DeferredRecording switch would stop comparing one picture against itself and start
			//comparing two different ones, which is the whole value of having the switch at all,
			//anything still visible after toggling it being a fault in this path and nowhere else.
			if (colorFlags & CF_FOG_MODE) {
				const FLOAT f255_Times_One_Minus_FogW = 255.0f * (1.0f - pPt->Fog.W);
				pOutVertex->color = FPlaneTo_BGRScaledClamped_A255(&pPt->Light, f255_Times_One_Minus_FogW);
				if (pOutFog != NULL) {
					pOutFog->specular = FPlaneTo_BGRClamped_A0(&pPt->Fog);
					pOutFog++;
				}
			} else if (colorFlags & CF_COLOR_ARRAY) {
#ifdef UTGLR_RUNE_BUILD
				pOutVertex->color = FPlaneTo_BGRClamped_Aub(&pPt->Light, pSrc->Alpha);
#else
				pOutVertex->color = FPlaneTo_BGRClamped_A255(&pPt->Light);
#endif
			} else {
				pOutVertex->color = 0xFFFFFFFF;
			}
			pOutVertex++;

			pOutCoord->u = pPt->U * uMult;
			pOutCoord->v = pPt->V * vMult;
			pOutCoord++;
		}
	}
}


void FASTCALL UD3D9RenderDevice::FillQuadCmd(const FBuildContext &ctx, const FDrawCmd &cmd) {
	const FQuadCmd *pSrc = cmd.pQuad;

	if (cmd.Instanced) {
		ctx.pInstance[cmd.VertexBase] = *pSrc;
		return;
	}

	FGLVertexColor *pOutVertex = ctx.pVertexColor + cmd.VertexBase;
	FGLTexCoord *pOutCoord = ctx.pTexCoord[0] + cmd.VertexBase;

	if (cmd.Key.Type == DCMD_LINE) {
		pOutVertex[0].x = pSrc->X1;
		pOutVertex[0].y = pSrc->Y1;
		pOutVertex[0].z = pSrc->Z1;
		pOutVertex[0].color = pSrc->Color;

		pOutVertex[1].x = pSrc->X2;
		pOutVertex[1].y = pSrc->Y2;
		pOutVertex[1].z = pSrc->Z2;
		pOutVertex[1].color = pSrc->Color;

		pOutCoord[0].u = pSrc->U1;
		pOutCoord[0].v = pSrc->V1;
		pOutCoord[1].u = pSrc->U2;
		pOutCoord[1].v = pSrc->V2;
		return;
	}

	static const int cornerX[6] = { 0, 1, 1, 0, 1, 0 };
	static const int cornerY[6] = { 0, 0, 1, 0, 1, 1 };

	for (int v = 0; v < 6; v++) {
		pOutVertex[v].x = cornerX[v] ? pSrc->X2 : pSrc->X1;
		pOutVertex[v].y = cornerY[v] ? pSrc->Y2 : pSrc->Y1;
		pOutVertex[v].z = pSrc->Z1;
		pOutVertex[v].color = pSrc->Color;

		pOutCoord[v].u = cornerX[v] ? pSrc->U2 : pSrc->U1;
		pOutCoord[v].v = cornerY[v] ? pSrc->V2 : pSrc->V1;
	}
}
