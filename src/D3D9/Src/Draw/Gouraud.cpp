/*=============================================================================
	Gouraud.cpp: models and other per-vertex lit geometry.

	As triangle fans, and on Harry Potter's engine as indexed triangles.
=============================================================================*/

#include "../D3D9Drv.h"
#include "../D3D9.h"
#include "../Geometry/VertexBuffers.h"

void UD3D9RenderDevice::DrawGouraudPolygonOld(FSceneNode *Frame, FTextureInfo &Info, FTransTexture **Pts, INT NumPts, DWORD PolyFlags, FSpanBuffer *Span) {
#ifdef UTGLR_DEBUG_SHOW_CALL_COUNTS
	{
		static int si;
		dout << L"utd3d9r: DrawGouraudPolygonOld = " << si++ << std::endl;
	}
#endif
	guard(UD3D9RenderDevice::DrawGouraudPolygonOld);

	if (m_frameSkipped || (NumPts < 3) || (NumPts > VERTEX_ARRAY_SIZE)) {
		return;
	}

	clock(GouraudCycles);

	//Replay keeps the decision.
	const DWORD nearZRangeHackFlag = m_replayingDeferredGouraudPolys
		? (m_replayPolyFlags2 & PF2_NEAR_Z_RANGE_HACK)
		: NearZRangeHackFlag(PolyFlags);

	SetProjectionState(nearZRangeHackFlag != 0);

#ifdef UTGLR_RUNE_BUILD
	bool drawFog = (((PolyFlags & (PF_RenderFog | PF_Translucent | PF_Modulated | PF_AlphaBlend)) == PF_RenderFog) && UseVertexSpecular) ? true : false;
#else
	bool drawFog = (((PolyFlags & (PF_RenderFog | PF_Translucent | PF_Modulated)) == PF_RenderFog) && UseVertexSpecular) ? true : false;
#endif

	if (!drawFog) {
		PolyFlags &= ~PF_RenderFog;
	}

	SetBlend(PolyFlags);
	SetTextureNoPanBias(0, Info, PolyFlags);

#ifdef UTGLR_RUNE_BUILD
	BYTE alpha = 255;
	if (PolyFlags & PF_AlphaBlend) {
		//Replay keeps the alpha.
		alpha = m_replayingDeferredGouraudPolys ? m_replayAlpha : appRound(Info.Texture->Alpha * 255.0f);
	}
#endif

	{
		IDirect3DVertexDeclaration9 *vertexDecl = (drawFog) ? m_twoColorSingleTextureVertexDecl : m_standardNTextureVertexDecl[0];
		IDirect3DVertexShader9 *vertexShader = NULL;
		IDirect3DPixelShader9 *pixelShader = NULL;

		if (UseFragmentProgram) {
			vertexShader = m_vpDefaultRenderingState;
			pixelShader = m_fpDefaultRenderingState;
			if (drawFog) {
				vertexShader = m_vpDefaultRenderingStateWithFog;
				pixelShader = m_fpDefaultRenderingStateWithFog;
			}
#ifdef UTGLR_RUNE_BUILD
			if (m_gpFogEnabled) {
				vertexShader = m_vpDefaultRenderingStateWithLinearFog;
				pixelShader = m_fpDefaultRenderingStateWithLinearFog;
			}
#endif
		}

		SetStreamState(vertexDecl, vertexShader, pixelShader);
	}

	if ((m_curVertexBufferPos + NumPts) >= VERTEX_RING_SIZE) {
		FlushVertexBuffers();
	}

	LockVertexColorBuffer();
	if (drawFog) {
		LockSecondaryColorBuffer();
	}
	LockTexCoordBuffer(0);

	INT Index = 0;
	for (INT i = 0; i < NumPts; i++) {
		FTransTexture *P = Pts[i];

		FGLTexCoord &destTexCoord = m_pTexCoordArray[0][Index];
		destTexCoord.u = P->U * TexInfo[0].UMult;
		destTexCoord.v = P->V * TexInfo[0].VMult;

		FGLVertexColor &destVertexColor = m_pVertexColorArray[Index];
		destVertexColor.x = P->Point.X;
		destVertexColor.y = P->Point.Y;
		destVertexColor.z = P->Point.Z;

		if (PolyFlags & PF_Modulated) {
			destVertexColor.color = 0xFFFFFFFF;
		} else if (drawFog) {
			FLOAT f255_Times_One_Minus_FogW = 255.0f * (1.0f - P->Fog.W);
			destVertexColor.color = FPlaneTo_BGRScaledClamped_A255(&P->Light, f255_Times_One_Minus_FogW);
			m_pSecondaryColorArray[Index].specular = FPlaneTo_BGRClamped_A0(&P->Fog);
		} else {
#ifdef UTGLR_RUNE_BUILD
			destVertexColor.color = FPlaneTo_BGRClamped_Aub(&P->Light, alpha);
#else
			destVertexColor.color = FPlaneTo_BGRClamped_A255(&P->Light);
#endif
		}

		Index++;
	}

	UnlockVertexColorBuffer();
	if (drawFog) {
		UnlockSecondaryColorBuffer();
	}
	UnlockTexCoordBuffer(0);

#ifdef UTGLR_DEBUG_ACTOR_WIREFRAME
	m_d3dDevice->SetRenderState(D3DRS_FILLMODE, D3DFILL_WIREFRAME);
#endif

	m_d3dDevice->DrawPrimitive(D3DPT_TRIANGLEFAN, m_curVertexBufferPos, NumPts - 2);

	m_curVertexBufferPos += NumPts;

#ifdef UTGLR_DEBUG_ACTOR_WIREFRAME
	m_d3dDevice->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
#endif

	unclock(GouraudCycles);
	unguard;
}

void UD3D9RenderDevice::DrawGouraudPolygon(FSceneNode *Frame, FTextureInfo &Info, FTransTexture **Pts, INT NumPts, DWORD PolyFlags, FSpanBuffer *Span) {
#ifdef UTGLR_DEBUG_SHOW_CALL_COUNTS
	{
		static int si;
		dout << L"utd3d9r: DrawGouraudPolygon = " << si++ << std::endl;
	}
#endif
	guard(UD3D9RenderDevice::DrawGouraudPolygon);

	if (m_frameSkipped || (NumPts < 3)) {
		return;
	}

	DWORD PolyFlags2 = 0;

	EndDeferredGouraudPolysForFrame(Frame);

	EndBufferingExcept(BV_TYPE_GOURAUD_POLYS);

	//Skipped while replaying: the scene node reference can be stale by then.
	if (!m_replayingDeferredGouraudPolys && SceneNodeChanged(Frame)) {
		m_sceneNodeRefreshCount++;
		SetSceneNode(Frame);
	}

	if (m_HitData) {
		CGClip::vec3_t triPts[3];
		const FTransTexture *Pt;
		INT i;

		Pt = Pts[0];
		triPts[0].x = Pt->Point.X;
		triPts[0].y = Pt->Point.Y;
		triPts[0].z = Pt->Point.Z;

		for (i = 2; i < NumPts; i++) {
			Pt = Pts[i - 1];
			triPts[1].x = Pt->Point.X;
			triPts[1].y = Pt->Point.Y;
			triPts[1].z = Pt->Point.Z;

			Pt = Pts[i];
			triPts[2].x = Pt->Point.X;
			triPts[2].y = Pt->Point.Y;
			triPts[2].z = Pt->Point.Z;

			m_gclip.SelectDrawTri(triPts);
		}

		return;
	}

	//Blended geometry writes no depth, so it is held back and replayed after this run of models.
	//A polygon too wide to hold falls through to the ordinary path.
	if (IsBlendedGeometry(PolyFlags) && !m_replayingDeferredGouraudPolys) {
		if (DeferGouraudPolygon(Frame, Info, Pts, NumPts, PolyFlags)) {
			return;
		}
	}

	//Replay keeps the decision.
	PolyFlags2 |= m_replayingDeferredGouraudPolys
		? (m_replayPolyFlags2 & PF2_NEAR_Z_RANGE_HACK)
		: NearZRangeHackFlag(PolyFlags);

	/*
	The flush test sits outside the cutoff test and the recording test inside it because they
	answer different questions, whether this polygon may be recorded at all and whether what is
	already recorded has to be drawn first, and the one time they were the other way round an open
	batch drew ahead of a still-pending log, which is invisible for opaque geometry but not for
	blended geometry, that writing no depth and having to stay behind whatever holds it back.
	*/
	if (DeferredActive()) {
		EndBuffering();
		if ((NumPts <= m_bufferActorTrisCutoff) &&
			RecordGouraudPolygon(Info, Pts, NumPts, PolyFlags, PolyFlags2)) {
			return;
		}
		//Before this polygon changes any state below: replay re-establishes blend, texture and stream
		//state from the batch's own keys.
		FlushDeferred();
	}

	if (NumPts > m_bufferActorTrisCutoff) {
		EndBuffering();

		SetDefaultAAState();
		SetDefaultTextureState();

		DrawGouraudPolygonOld(Frame, Info, Pts, NumPts, PolyFlags, Span);

		return;
	}

	OpenGouraudPolygonBatch(Info, PolyFlags, PolyFlags2, NumPts);

	(m_pBuffer3VertsProc)(this, Pts);

	if (NumPts > 3) {
		BufferAdditionalClippedVerts(Pts, NumPts);
	}

	unguard;
}

//Shared by the two draw paths that turn engine geometry into vertices, so they cannot diverge.
void UD3D9RenderDevice::OpenGouraudPolygonBatch(FTextureInfo &Info, DWORD PolyFlags, DWORD PolyFlags2, INT ringCost) {
	const QWORD CacheID = ComposeTexCacheID(Info, PolyFlags);

	if ((m_curPolyFlags != PolyFlags) ||
		(m_curPolyFlags2 != PolyFlags2) ||
		(TexInfo[0].CurrentCacheID != CacheID) ||
		((m_curVertexBufferPos + m_bufferedVerts + ringCost) >= (VERTEX_RING_SIZE - BUFFERED_GP_HEADROOM)) ||
		(m_bufferedVerts == 0)) {
		EndBuffering();

		if ((m_curVertexBufferPos + m_bufferedVerts + ringCost) >= (VERTEX_RING_SIZE - BUFFERED_GP_HEADROOM)) {
			FlushVertexBuffers();
		}

		StartBuffering(BV_TYPE_GOURAUD_POLYS);

		if (PolyFlags & PF_Modulated) {
			m_requestedColorFlags = 0;
		} else {
			m_requestedColorFlags = CF_COLOR_ARRAY;

#ifdef UTGLR_RUNE_BUILD
			if (((PolyFlags & (PF_RenderFog | PF_Translucent | PF_Modulated | PF_AlphaBlend)) == PF_RenderFog) && UseVertexSpecular) {
#else
			if (((PolyFlags & (PF_RenderFog | PF_Translucent | PF_Modulated)) == PF_RenderFog) && UseVertexSpecular) {
#endif
				m_requestedColorFlags = CF_COLOR_ARRAY | CF_FOG_MODE;
			}
		}

		m_curPolyFlags = PolyFlags;
		m_curPolyFlags2 = PolyFlags2;

		if (!(m_requestedColorFlags & CF_FOG_MODE)) {
			PolyFlags &= ~PF_RenderFog;
		}

		m_requestNearZRangeHackProjection = (PolyFlags2 & PF2_NEAR_Z_RANGE_HACK) ? true : false;

		SetDefaultTextureState();

		SetBlend(PolyFlags);
		SetTextureNoPanBias(0, Info, PolyFlags);

		LockVertexColorBuffer();
		if (m_requestedColorFlags & CF_FOG_MODE) {
			LockSecondaryColorBuffer();
		}
		LockTexCoordBuffer(0);

		{
			IDirect3DVertexDeclaration9 *vertexDecl = (m_requestedColorFlags & CF_FOG_MODE) ? m_twoColorSingleTextureVertexDecl : m_standardNTextureVertexDecl[0];
			IDirect3DVertexShader9 *vertexShader = NULL;
			IDirect3DPixelShader9 *pixelShader = NULL;

			if (UseFragmentProgram) {
				vertexShader = m_vpDefaultRenderingState;
				pixelShader = m_fpDefaultRenderingState;
				if (m_requestedColorFlags & CF_FOG_MODE) {
					vertexShader = m_vpDefaultRenderingStateWithFog;
					pixelShader = m_fpDefaultRenderingStateWithFog;
				}
#ifdef UTGLR_RUNE_BUILD
				if (m_gpFogEnabled) {
					vertexShader = m_vpDefaultRenderingStateWithLinearFog;
					pixelShader = m_fpDefaultRenderingStateWithLinearFog;
				}
#endif
			}

			SetStreamState(vertexDecl, vertexShader, pixelShader);
		}

		if (m_requestedColorFlags & CF_FOG_MODE) {
			m_pBuffer3VertsProc = m_pBuffer3FoggedVertsProc;
		} else if (m_requestedColorFlags & CF_COLOR_ARRAY) {
			m_pBuffer3VertsProc = m_pBuffer3ColoredVertsProc;
		} else {
			m_pBuffer3VertsProc = m_pBuffer3BasicVertsProc;
		}
#ifdef UTGLR_RUNE_BUILD
		m_gpAlpha = 255;
		if (PolyFlags & PF_AlphaBlend) {
			m_gpAlpha = m_replayingDeferredGouraudPolys ? m_replayAlpha : appRound(Info.Texture->Alpha * 255.0f);
			m_pBuffer3VertsProc = Buffer3Verts;
		}
#endif
	}
}

#ifdef UTGLR_HP_BUILD

//Reports the same per-call staging capacity KnowWonder's OpenGL driver does, for parity.
INT UD3D9RenderDevice::MaxVertices() {
	guard(UD3D9RenderDevice::MaxVertices);

	return VERTEX_ARRAY_SIZE;

	unguard;
}

/*
Every mesh in Harry Potter arrives here, with the same three destinations as the fan path and in
the same order, held back if blended, recorded if deferred and batched otherwise, while a plain
fan - indices null, count equal to point count - is handed straight to the fan path instead.
*/
void UD3D9RenderDevice::DrawTriangles(FSceneNode *Frame, FTextureInfo &Info, FTransTexture **Pts, INT NumPts, _WORD *Indices, INT NumIndices, DWORD PolyFlags, FSpanBuffer *Span) {
	guard(UD3D9RenderDevice::DrawTriangles);

	//Quirk for Harry's glasses. The game marks his lenses highlighted and translucent on a masked texture.
	if (((PolyFlags & (PF_Highlighted | PF_Translucent)) == (PF_Highlighted | PF_Translucent)) && (PolyFlags & PF_Masked)) {
		PolyFlags &= ~(PF_Highlighted | PF_Translucent);
	}

	//Exactly the fan path.
	if (!Indices && (NumIndices == NumPts)) {
		DrawGouraudPolygon(Frame, Info, Pts, NumPts, PolyFlags, Span);
		return;
	}

	const INT numIndices = Indices ? NumIndices : NumPts;

	//NumPts is tested separately. An indexed submission bounds the two independently.
	if (m_frameSkipped || (numIndices < 3) || (NumPts < 3)) {
		return;
	}

	DWORD PolyFlags2 = 0;

	EndDeferredGouraudPolysForFrame(Frame);

	EndBufferingExcept(BV_TYPE_GOURAUD_POLYS);

	if (!m_replayingDeferredGouraudPolys && SceneNodeChanged(Frame)) {
		m_sceneNodeRefreshCount++;
		SetSceneNode(Frame);
	}

	if (m_HitData) {
		CGClip::vec3_t triPts[3];

		for (INT i = 0; (i + 3) <= numIndices; i += 3) {
			if (!IndexedTriangleInRange(Indices, i, NumPts)) {
				continue;
			}

			for (INT j = 0; j < 3; j++) {
				const FTransTexture *Pt = Pts[Indices ? Indices[i + j] : (i + j)];
				triPts[j].x = Pt->Point.X;
				triPts[j].y = Pt->Point.Y;
				triPts[j].z = Pt->Point.Z;
			}

			m_gclip.SelectDrawTri(triPts);
		}

		return;
	}

	PolyFlags2 |= m_replayingDeferredGouraudPolys
		? (m_replayPolyFlags2 & PF2_NEAR_Z_RANGE_HACK)
		: NearZRangeHackFlag(PolyFlags);

	//One triangle at a time from here, treated like any three-point polygon.
	const bool holdBlended = IsBlendedGeometry(PolyFlags) && !m_replayingDeferredGouraudPolys;

	for (INT i = 0; (i + 3) <= numIndices; i += 3) {
		if (!IndexedTriangleInRange(Indices, i, NumPts)) {
			continue;
		}

		FTransTexture *tri[3];

		tri[0] = Pts[Indices ? Indices[i + 0] : (i + 0)];
		tri[1] = Pts[Indices ? Indices[i + 1] : (i + 1)];
		tri[2] = Pts[Indices ? Indices[i + 2] : (i + 2)];

		if (holdBlended && DeferGouraudPolygon(Frame, Info, tri, 3, PolyFlags)) {
			continue;
		}

		if (DeferredActive()) {
			EndBuffering();
			if (RecordGouraudPolygon(Info, tri, 3, PolyFlags, PolyFlags2)) {
				continue;
			}
			FlushDeferred();
		}

		//One draw per triangle is costly when every actor arrives this way.
		if (3 > m_bufferActorTrisCutoff) {
			EndBuffering();

			SetDefaultAAState();
			SetDefaultTextureState();

			DrawGouraudPolygonOld(Frame, Info, tri, 3, PolyFlags, Span);

			continue;
		}

		OpenGouraudPolygonBatch(Info, PolyFlags, PolyFlags2, 3);

		(m_pBuffer3VertsProc)(this, tri);
	}

	unguard;
}

#endif //UTGLR_HP_BUILD
