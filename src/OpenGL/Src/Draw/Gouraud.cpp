/*=============================================================================
	Gouraud.cpp: actors and anything else lit per vertex.

	Triangle fans, plus indexed triangles on Harry Potter's engine.
=============================================================================*/

#include "../OpenGLDrv.h"
#include "../OpenGL.h"
#include "../Geometry/VertexBuffers.h"

void UOpenGLRenderDevice::DrawGouraudPolygonOld(FSceneNode *Frame, FTextureInfo &Info, FTransTexture **Pts, INT NumPts, DWORD PolyFlags, FSpanBuffer *Span) {
	UTGLR_DEBUG_CALL_COUNT(DrawGouraudPolygonOld);
	guard(UOpenGLRenderDevice::DrawGouraudPolygonOld);

	//No flush part way.
	if ((NumPts < 3) || (NumPts > VERTEX_ARRAY_SIZE)) {
		return;
	}

	clock(GouraudCycles);

	//As originally submitted.
	const DWORD nearZRangeHackFlag = m_replayingDeferredGouraudPolys
		? (m_replayPolyFlags2 & PF2_NEAR_Z_RANGE_HACK)
		: NearZRangeHackFlag(PolyFlags);

	SetProjectionState(nearZRangeHackFlag != 0);

	SetBlend(PolyFlags);
	SetTextureNoPanBias(0, Info, PolyFlags);

#ifdef UTGLR_RUNE_BUILD
	BYTE alpha = 255;
	if (PolyFlags & PF_AlphaBlend) {
		//The engine sets alpha per object.
		//Taken here, while the submission is still live.
		alpha = m_replayingDeferredGouraudPolys ? m_replayAlpha : appRound(Info.Texture->Alpha * 255.0f);
	}
#endif
	if (PolyFlags & PF_Modulated) {
		SetColor3f(1.0f, 1.0f, 1.0f);
		m_requestedColorFlags = 0;
	} else {
		m_requestedColorFlags = CF_COLOR_ARRAY;
	}

	SetColorState();

	INT Index = 0;
	for (INT i = 0; i < NumPts; i++) {
		FTransTexture *P = Pts[i];
		FGLTexCoord &destTexCoord = TexCoordArray[0][Index];
		destTexCoord.u = P->U * TexInfo[0].UMult;
		destTexCoord.v = P->V * TexInfo[0].VMult;
		if (m_requestedColorFlags & CF_COLOR_ARRAY) {
#ifdef UTGLR_RUNE_BUILD
			SingleColorArray[Index].color = FPlaneTo_RGBClamped_Aub(&P->Light, alpha);
#else
			SingleColorArray[Index].color = FPlaneTo_RGBClamped_A255(&P->Light);
#endif
		}
		FGLVertex &destVertex = VertexArray[Index];
		destVertex.x = P->Point.X;
		destVertex.y = P->Point.Y;
		destVertex.z = P->Point.Z;
		Index++;
	}

	UploadGouraudStreams(Index);

#ifdef UTGLR_DEBUG_ACTOR_WIREFRAME
	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
#endif

	glDrawArrays(GL_TRIANGLE_FAN, 0, Index);

#ifdef UTGLR_RUNE_BUILD
	if ((PolyFlags & (PF_RenderFog | PF_Translucent | PF_Modulated | PF_AlphaBlend)) == PF_RenderFog) {
#elif defined(UTGLR_UNREAL_227_BUILD)
	if ((PolyFlags & (PF_RenderFog | PF_Translucent | PF_Modulated | PF_AlphaBlend)) == PF_RenderFog) {
#else
	if ((PolyFlags & (PF_RenderFog | PF_Translucent | PF_Modulated)) == PF_RenderFog) {
#endif
		//In the color array.
		m_requestedColorFlags = CF_COLOR_ARRAY;

		SetColorState();

		SetNoTexture(0);
		SetBlend(PF_Highlighted);
		Index = 0;
		for (INT i = 0; i < NumPts; i++) {
			const FTransTexture *P = Pts[i];
			SingleColorArray[Index].color = FPlaneTo_RGBAClamped(&P->Fog);
			Index++;
		}

		//Only the colors changed.
		if (m_vboActive) {
			StageVertexStream(VERTEX_STREAM_COLOR, SingleColorArray, sizeof(FGLSingleColor), Index);
			UploadVertexStream(VERTEX_STREAM_COLOR);
		}

		glDrawArrays(GL_TRIANGLE_FAN, 0, Index);
	}

	//The shadow goes stale.
	InvalidateColorShadow();
	//Both read the unit 0 coordinate array.
	InvalidateTexAttribShadows();

#ifdef UTGLR_DEBUG_ACTOR_WIREFRAME
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
#endif

	unclock(GouraudCycles);
	unguard;
}

void UOpenGLRenderDevice::DrawGouraudPolygon(FSceneNode *Frame, FTextureInfo &Info, FTransTexture **Pts, INT NumPts, DWORD PolyFlags, FSpanBuffer *Span) {
	UTGLR_DEBUG_CALL_COUNT(DrawGouraudPolygon);
	guard(UOpenGLRenderDevice::DrawGouraudPolygon);

	DWORD PolyFlags2 = 0;

	//Possibly another node's.
	EndDeferredGouraudPolysForFrame(Frame);

	EndBufferingExceptGouraud();

	//Skipped while replaying.
	/*
	The replay hands back the node pointer the run was opened under and a child node such as
	a mirror or a security camera may have been popped and its address reused by then, so
	reading through it could reprogram the frustum from unrelated memory, and the pointer is
	still worth keeping because it is the only thing that tells one node's models from
	another's and comparing it costs nothing as long as nobody follows it.
	*/
	if (!m_replayingDeferredGouraudPolys && SceneNodeChanged(Frame)) {
		m_sceneNodeRefreshCount++;
		SetSceneNode(Frame);
	}

	//Reject invalid polygons early.
	if (NumPts < 3) {
		return;
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

	/*
	Blended geometry writes no depth, so it is held back and replayed after this run of
	models because an opaque model drawn afterwards would otherwise paint over it, and a
	polygon too wide to hold falls through to the ordinary path and is drawn in place.
	*/
	if (IsBlendedGeometry(PolyFlags) && !m_replayingDeferredGouraudPolys) {
		if (DeferGouraudPolygon(Frame, Info, Pts, NumPts, PolyFlags)) {
			return;
		}
	}

	if (NumPts > m_bufferActorTrisCutoff) {
		EndBuffering();

		SetDefaultAAState();
		//The fallback path below sets its own.
#ifdef UTGLR_RUNE_BUILD
		if (UseFragmentProgram && m_gpFogEnabled) {
			SetShaderState(m_vpDefaultRenderingStateWithLinearFog, m_fpDefaultRenderingStateWithLinearFog);
		} else {
			SetDefaultShaderState();
		}
#else
		SetDefaultShaderState();
#endif
		SetDefaultTextureState();

		DrawGouraudPolygonOld(Frame, Info, Pts, NumPts, PolyFlags, Span);

		return;
	}

	PolyFlags2 |= m_replayingDeferredGouraudPolys
		? (m_replayPolyFlags2 & PF2_NEAR_Z_RANGE_HACK)
		: NearZRangeHackFlag(PolyFlags);

	OpenGouraudPolygonBatch(Info, PolyFlags, PolyFlags2, NumPts);

	(m_pBuffer3VertsProc)(this, Pts);

	if (NumPts > 3) {
		BufferAdditionalClippedVerts(Pts, NumPts);
	}

	unguard;
}

/*
Shared by the fan and indexed triangle paths so the batch-break test, state setup and
vertex writer choice stay identical, because if they diverged a frame mixing both kinds of
geometry would draw part of it wrong and only on the game that sends both, which is the
one this codebase can least easily be tested against.
*/
void UOpenGLRenderDevice::OpenGouraudPolygonBatch(FTextureInfo &Info, DWORD PolyFlags, DWORD PolyFlags2, INT arrayCost) {
	//Same as the upload path.
	const QWORD CacheID = ComposeTexCacheID(Info, PolyFlags);

	if ((m_curPolyFlags != PolyFlags) ||
		(m_curPolyFlags2 != PolyFlags2) ||
		(TexInfo[0].CurrentCacheID != CacheID) ||
		((BufferedVerts + arrayCost) >= (VERTEX_ARRAY_SIZE - BUFFERED_GP_HEADROOM)) ||
		(BufferedVerts == 0)) {
		EndGouraudPolygonBuffering();

		m_curPolyFlags = PolyFlags;
		m_curPolyFlags2 = PolyFlags2;

		m_requestNearZRangeHackProjection = (PolyFlags2 & PF2_NEAR_Z_RANGE_HACK) ? true : false;

		SetDefaultTextureState();

		SetBlend(PolyFlags);
		SetTextureNoPanBias(0, Info, PolyFlags);

		if (PolyFlags & PF_Modulated) {
			SetColor3f(1.0f, 1.0f, 1.0f);
			m_requestedColorFlags = 0;
		} else {
			m_requestedColorFlags = CF_COLOR_ARRAY;

#ifdef UTGLR_RUNE_BUILD
			if (((PolyFlags & (PF_RenderFog | PF_Translucent | PF_Modulated | PF_AlphaBlend)) == PF_RenderFog) && UseVertexSpecular) {
#elif defined(UTGLR_UNREAL_227_BUILD)
			if (((PolyFlags & (PF_RenderFog | PF_Translucent | PF_Modulated | PF_AlphaBlend)) == PF_RenderFog) && UseVertexSpecular) {
#else
			if (((PolyFlags & (PF_RenderFog | PF_Translucent | PF_Modulated)) == PF_RenderFog) && UseVertexSpecular) {
#endif
				m_requestedColorFlags = CF_COLOR_ARRAY | CF_DUAL_COLOR_ARRAY | CF_COLOR_SUM;
			}
		}

#ifdef UTGLR_RUNE_BUILD
		if (UseFragmentProgram && m_gpFogEnabled) {
			SetShaderState(m_vpDefaultRenderingStateWithLinearFog, m_fpDefaultRenderingStateWithLinearFog);
		} else {
			SetDefaultShaderState();
		}
#else
		if (UseFragmentProgram && (m_requestedColorFlags & CF_DUAL_COLOR_ARRAY)) {
			m_requestedColorFlags &= ~CF_COLOR_SUM;
			SetShaderState(m_vpDefaultRenderingStateWithFog, m_fpDefaultRenderingStateWithFog);
		} else {
			SetDefaultShaderState();
		}
#endif

		if (m_requestedColorFlags & CF_DUAL_COLOR_ARRAY) {
			m_pBuffer3VertsProc = m_pBuffer3FoggedVertsProc;
		} else if (m_requestedColorFlags & CF_COLOR_ARRAY) {
			m_pBuffer3VertsProc = m_pBuffer3ColoredVertsProc;
		} else {
			m_pBuffer3VertsProc = m_pBuffer3BasicVertsProc;
		}
#ifdef UTGLR_RUNE_BUILD
		m_gpAlpha = 255;
		if (PolyFlags & PF_AlphaBlend) {
			//As submitted, unscaled.
			m_gpAlpha = m_replayingDeferredGouraudPolys ? m_replayAlpha : appRound(Info.Texture->Alpha * 255.0f);
			m_pBuffer3VertsProc = Buffer3Verts;
		}
#endif
	}
}

#ifdef UTGLR_HP_BUILD

//The per-call staging capacity KnowWonder's driver reports.
//The draw path batches one triangle at a time.
INT UOpenGLRenderDevice::MaxVertices() {
	guard(UOpenGLRenderDevice::MaxVertices);

	return VERTEX_ARRAY_SIZE;

	unguard;
}

//Every mesh in Harry Potter arrives here.
//A plain fan goes straight to the fan path.
//Anything else takes the slower route below.
void UOpenGLRenderDevice::DrawTriangles(FSceneNode *Frame, FTextureInfo &Info, FTransTexture **Pts, INT NumPts, _WORD *Indices, INT NumIndices, DWORD PolyFlags, FSpanBuffer *Span) {
	UTGLR_DEBUG_CALL_COUNT(DrawTriangles);
	guard(UOpenGLRenderDevice::DrawTriangles);

	//Harry's glasses are marked highlighted and translucent on a masked texture.
	//Taken literally that draws them opaque white.
	if (((PolyFlags & (PF_Highlighted | PF_Translucent)) == (PF_Highlighted | PF_Translucent)) && (PolyFlags & PF_Masked)) {
		PolyFlags &= ~(PF_Highlighted | PF_Translucent);
	}

	if (!Indices && (NumIndices == NumPts)) {
		DrawGouraudPolygon(Frame, Info, Pts, NumPts, PolyFlags, Span);
		return;
	}

	DWORD PolyFlags2 = 0;

	//May be another node's.
	EndDeferredGouraudPolysForFrame(Frame);

	EndBufferingExceptGouraud();

	if (!m_replayingDeferredGouraudPolys && SceneNodeChanged(Frame)) {
		m_sceneNodeRefreshCount++;
		SetSceneNode(Frame);
	}

	//An unindexed list takes Pts in threes.
	const INT numIndices = Indices ? NumIndices : NumPts;

	//The range check below uses NumPts.
	if ((numIndices < 3) || (NumPts < 3)) {
		return;
	}

	//Indices, in threes.
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

	//One triangle at a time from here.
	//A translucent mesh sorts against itself more finely.
	//Overflow only loses ordering.
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

		//One draw per triangle is costly.
		if (3 > m_bufferActorTrisCutoff) {
			EndBuffering();

			SetDefaultAAState();
			SetDefaultShaderState();
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
