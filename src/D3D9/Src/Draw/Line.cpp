/*=============================================================================
	Line.cpp: lines and points, mostly an editor path.
=============================================================================*/

#include "../D3D9Drv.h"
#include "../D3D9.h"

//Only transparency maps across; opaque lines get the highlighted/occlude blend instead.
static inline DWORD linePolyFlags(DWORD LineFlags) {
	return (LineFlags & LINE_Transparent) ? PF_Translucent : (PF_Highlighted | PF_Occlude);
}

void UD3D9RenderDevice::Draw3DLine(FSceneNode *Frame, FPlane Color, DWORD LineFlags, FVector P1, FVector P2) {
#ifdef UTGLR_DEBUG_SHOW_CALL_COUNTS
	{
		static int si;
		dout << L"utd3d9r: Draw3DLine = " << si++ << std::endl;
	}
#endif
	guard(UD3D9RenderDevice::Draw3DLine);

	if (m_frameSkipped) {
		return;
	}

	EndDeferredGouraudPolys();

	//The engine gives no guarantee of scene setup before a node's first draw call.
	if (SceneNodeChanged(Frame)) {
		m_sceneNodeRefreshCount++;
		SetSceneNode(Frame);
	}

	P1 = P1.TransformPointBy(Frame->Coords);
	P2 = P2.TransformPointBy(Frame->Coords);
	if (Frame->Viewport->IsOrtho()) {
		//A zero zoom puts both endpoints at infinity, and there is no actor between levels.
		if (!(Frame->Zoom > 0.0f)) {
			return;
		}

		FLOAT rcpZoom = 1.0f / Frame->Zoom;
		P1.X = (P1.X * rcpZoom) + Frame->FX2;
		P1.Y = (P1.Y * rcpZoom) + Frame->FY2;
		P2.X = (P2.X * rcpZoom) + Frame->FX2;
		P2.Y = (P2.Y * rcpZoom) + Frame->FY2;
		P1.Z = P2.Z = 1;

		// See if points form a line parallel to our line of sight (i.e. line appears as a dot).
		if (Abs(P2.X - P1.X) + Abs(P2.Y - P1.Y) >= 0.2f) {
			Draw2DLine(Frame, Color, LineFlags, P1, P2);
		} else if (Frame->Viewport->Actor && (Frame->Viewport->Actor->OrthoZoom < ORTHO_LOW_DETAIL)) {
			Draw2DPoint(Frame, Color, LINE_None, P1.X - 1.0f, P1.Y - 1.0f, P1.X + 1.0f, P1.Y + 1.0f, P1.Z);
		}
	} else {
		EndBufferingExcept(BV_TYPE_LINES);

		if (m_HitData) {
			CGClip::vec3_t lnPts[2];

			lnPts[0].x = P1.X;
			lnPts[0].y = P1.Y;
			lnPts[0].z = P1.Z;

			lnPts[1].x = P2.X;
			lnPts[1].y = P2.Y;
			lnPts[1].z = P2.Z;

			m_gclip.SelectDrawLine(lnPts);

			return;
		}

		const DWORD PolyFlags = linePolyFlags(LineFlags);
		const DWORD PolyFlags2 = 0;

		if (DeferredActive()) {
			EndBuffering();

			FQuadCmd quad;
			quad.X1 = P1.X;
			quad.Y1 = P1.Y;
			quad.X2 = P2.X;
			quad.Y2 = P2.Y;
			quad.U1 = 0.0f;
			quad.V1 = 0.0f;
			quad.U2 = 1.0f;
			quad.V2 = 0.0f;
			quad.Z1 = P1.Z;
			quad.Z2 = P2.Z;
			quad.Color = FPlaneTo_BGRClamped_A255(&Color);

			if (RecordQuad(DCMD_LINE, NULL, PolyFlags, PolyFlags, PolyFlags, PolyFlags2, quad)) {
				return;
			}
			FlushDeferred();
		}

		if ((m_curPolyFlags != PolyFlags) ||
			(m_curPolyFlags2 != PolyFlags2) ||
			(TexInfo[0].CurrentCacheID != TEX_CACHE_ID_NO_TEX) ||
			((m_curVertexBufferPos + m_bufferedVerts) >= (VERTEX_RING_SIZE - 2)) ||
			(m_bufferedVerts == 0)) {
			EndBuffering();

			if ((m_curVertexBufferPos + m_bufferedVerts) >= (VERTEX_RING_SIZE - 2)) {
				FlushVertexBuffers();
			}

			StartBuffering(BV_TYPE_LINES);

			SetDefaultAAState();
			SetDefaultProjectionState();
			SetDefaultStreamState();
			SetDefaultTextureState();

			m_curPolyFlags = PolyFlags;
			m_curPolyFlags2 = PolyFlags2;

			SetBlend(PolyFlags);
			SetNoTexture(0);

			LockVertexColorBuffer();
			LockTexCoordBuffer(0);
		}

		DWORD lineColor = FPlaneTo_BGRClamped_A255(&Color);

		FGLVertexColor *pVertexColorArray = &m_pVertexColorArray[m_bufferedVerts];
		FGLTexCoord *pTexCoordArray = &m_pTexCoordArray[0][m_bufferedVerts];

		pVertexColorArray[0].x = P1.X;
		pVertexColorArray[0].y = P1.Y;
		pVertexColorArray[0].z = P1.Z;
		pVertexColorArray[0].color = lineColor;

		pVertexColorArray[1].x = P2.X;
		pVertexColorArray[1].y = P2.Y;
		pVertexColorArray[1].z = P2.Z;
		pVertexColorArray[1].color = lineColor;

		pTexCoordArray[0].u = 0.0f;
		pTexCoordArray[0].v = 0.0f;

		pTexCoordArray[1].u = 1.0f;
		pTexCoordArray[1].v = 0.0f;

		m_bufferedVerts += 2;
	}
	unguard;
}

void UD3D9RenderDevice::Draw2DLine(FSceneNode *Frame, FPlane Color, DWORD LineFlags, FVector P1, FVector P2) {
#ifdef UTGLR_DEBUG_SHOW_CALL_COUNTS
	{
		static int si;
		dout << L"utd3d9r: Draw2DLine = " << si++ << std::endl;
	}
#endif
	guard(UD3D9RenderDevice::Draw2DLine);

	if (m_frameSkipped) {
		return;
	}

	EndDeferredGouraudPolys();
	EndBufferingExcept(BV_TYPE_LINES);

	if (SceneNodeChanged(Frame)) {
		m_sceneNodeRefreshCount++;
		SetSceneNode(Frame);
	}

	FLOAT X1Pos = m_RFX2 * (P1.X - Frame->FX2);
	FLOAT Y1Pos = m_RFY2 * (P1.Y - Frame->FY2);
	FLOAT X2Pos = m_RFX2 * (P2.X - Frame->FX2);
	FLOAT Y2Pos = m_RFY2 * (P2.Y - Frame->FY2);
	if (!Frame->Viewport->IsOrtho()) {
		X1Pos *= P1.Z;
		Y1Pos *= P1.Z;
		X2Pos *= P2.Z;
		Y2Pos *= P2.Z;
	}

	if (m_HitData) {
		CGClip::vec3_t lnPts[2];

		lnPts[0].x = X1Pos;
		lnPts[0].y = Y1Pos;
		lnPts[0].z = P1.Z;

		lnPts[1].x = X2Pos;
		lnPts[1].y = Y2Pos;
		lnPts[1].z = P2.Z;

		m_gclip.SelectDrawLine(lnPts);

		return;
	}

	const DWORD PolyFlags = linePolyFlags(LineFlags);
	const DWORD PolyFlags2 = 0;

	if (DeferredActive()) {
		EndBuffering();

		FQuadCmd quad;
		quad.X1 = X1Pos;
		quad.Y1 = Y1Pos;
		quad.X2 = X2Pos;
		quad.Y2 = Y2Pos;
		quad.U1 = 0.0f;
		quad.V1 = 0.0f;
		quad.U2 = 1.0f;
		quad.V2 = 0.0f;
		quad.Z1 = P1.Z;
		quad.Z2 = P2.Z;
		quad.Color = FPlaneTo_BGRClamped_A255(&Color);

		if (RecordQuad(DCMD_LINE, NULL, PolyFlags, PolyFlags, PolyFlags, PolyFlags2, quad)) {
			return;
		}
		FlushDeferred();
	}

	if ((m_curPolyFlags != PolyFlags) ||
		(m_curPolyFlags2 != PolyFlags2) ||
		(TexInfo[0].CurrentCacheID != TEX_CACHE_ID_NO_TEX) ||
		((m_curVertexBufferPos + m_bufferedVerts) >= (VERTEX_RING_SIZE - 2)) ||
		(m_bufferedVerts == 0)) {
		EndBuffering();

		if ((m_curVertexBufferPos + m_bufferedVerts) >= (VERTEX_RING_SIZE - 2)) {
			FlushVertexBuffers();
		}

		StartBuffering(BV_TYPE_LINES);

		SetDefaultAAState();
		SetDefaultProjectionState();
		SetDefaultStreamState();
		SetDefaultTextureState();

		m_curPolyFlags = PolyFlags;
		m_curPolyFlags2 = PolyFlags2;

		SetBlend(PolyFlags);
		SetNoTexture(0);

		LockVertexColorBuffer();
		LockTexCoordBuffer(0);
	}

	DWORD lineColor = FPlaneTo_BGRClamped_A255(&Color);

	FGLVertexColor *pVertexColorArray = &m_pVertexColorArray[m_bufferedVerts];
	FGLTexCoord *pTexCoordArray = &m_pTexCoordArray[0][m_bufferedVerts];

	pVertexColorArray[0].x = X1Pos;
	pVertexColorArray[0].y = Y1Pos;
	pVertexColorArray[0].z = P1.Z;
	pVertexColorArray[0].color = lineColor;

	pVertexColorArray[1].x = X2Pos;
	pVertexColorArray[1].y = Y2Pos;
	pVertexColorArray[1].z = P2.Z;
	pVertexColorArray[1].color = lineColor;

	pTexCoordArray[0].u = 0.0f;
	pTexCoordArray[0].v = 0.0f;

	pTexCoordArray[1].u = 1.0f;
	pTexCoordArray[1].v = 0.0f;

	m_bufferedVerts += 2;

	unguard;
}

void UD3D9RenderDevice::Draw2DPoint(FSceneNode *Frame, FPlane Color, DWORD LineFlags, FLOAT X1, FLOAT Y1, FLOAT X2, FLOAT Y2, FLOAT Z) {
#ifdef UTGLR_DEBUG_SHOW_CALL_COUNTS
	{
		static int si;
		dout << L"utd3d9r: Draw2DPoint = " << si++ << std::endl;
	}
#endif
	guard(UD3D9RenderDevice::Draw2DPoint);

	if (m_frameSkipped) {
		return;
	}

	EndDeferredGouraudPolys();
	EndBufferingExcept(BV_TYPE_POINTS);

	if (SceneNodeChanged(Frame)) {
		m_sceneNodeRefreshCount++;
		SetSceneNode(Frame);
	}

	// Hack to fix UED selection problem with selection brush
	if (GIsEditor) {
		Z = 1.0f;

		//The Z range hack moves the near plane to 4.0, so a manufactured Z of 1.0 would clip.
		//Remapped into the range the hack leaves usable, as tiles do.
		if (m_useZRangeHack) {
			Z = (((Z - 0.5f) / 7.5f) * 4.0f) + 4.0f;
		}
	}

	FLOAT X1Pos = m_RFX2 * (X1 - Frame->FX2);
	FLOAT Y1Pos = m_RFY2 * (Y1 - Frame->FY2);
	FLOAT X2Pos = m_RFX2 * (X2 - Frame->FX2);
	FLOAT Y2Pos = m_RFY2 * (Y2 - Frame->FY2);
	if (!Frame->Viewport->IsOrtho()) {
		X1Pos *= Z;
		Y1Pos *= Z;
		X2Pos *= Z;
		Y2Pos *= Z;
	}

	if (m_HitData) {
		CGClip::vec3_t triPts[3];

		triPts[0].x = X1Pos;
		triPts[0].y = Y1Pos;
		triPts[0].z = Z;

		triPts[1].x = X2Pos;
		triPts[1].y = Y1Pos;
		triPts[1].z = Z;

		triPts[2].x = X2Pos;
		triPts[2].y = Y2Pos;
		triPts[2].z = Z;

		m_gclip.SelectDrawTri(triPts);

		triPts[0].x = X1Pos;
		triPts[0].y = Y1Pos;
		triPts[0].z = Z;

		triPts[1].x = X2Pos;
		triPts[1].y = Y2Pos;
		triPts[1].z = Z;

		triPts[2].x = X1Pos;
		triPts[2].y = Y2Pos;
		triPts[2].z = Z;

		m_gclip.SelectDrawTri(triPts);

		return;
	}

	const DWORD PolyFlags = linePolyFlags(LineFlags);
	const DWORD PolyFlags2 = 0;

	if (DeferredActive()) {
		EndBuffering();

		FQuadCmd quad;
		quad.X1 = X1Pos;
		quad.Y1 = Y1Pos;
		quad.X2 = X2Pos;
		quad.Y2 = Y2Pos;
		quad.U1 = 0.0f;
		quad.V1 = 0.0f;
		quad.U2 = 1.0f;
		quad.V2 = 1.0f;
		quad.Z1 = Z;
		quad.Z2 = Z;
		quad.Color = FPlaneTo_BGRClamped_A255(&Color);

		if (RecordQuad(DCMD_POINT, NULL, PolyFlags, PolyFlags, PolyFlags, PolyFlags2, quad)) {
			return;
		}
		FlushDeferred();
	}

	if ((m_curPolyFlags != PolyFlags) ||
		(m_curPolyFlags2 != PolyFlags2) ||
		(TexInfo[0].CurrentCacheID != TEX_CACHE_ID_NO_TEX) ||
		((m_curVertexBufferPos + m_bufferedVerts) >= (VERTEX_RING_SIZE - 6)) ||
		(m_bufferedVerts == 0)) {
		EndBuffering();

		if ((m_curVertexBufferPos + m_bufferedVerts) >= (VERTEX_RING_SIZE - 6)) {
			FlushVertexBuffers();
		}

		StartBuffering(BV_TYPE_POINTS);

		SetDefaultAAState();
		SetDefaultProjectionState();
		SetDefaultStreamState();
		SetDefaultTextureState();

		m_curPolyFlags = PolyFlags;
		m_curPolyFlags2 = PolyFlags2;

		SetBlend(PolyFlags);
		SetNoTexture(0);

		LockVertexColorBuffer();
		LockTexCoordBuffer(0);
	}

	DWORD pointColor = FPlaneTo_BGRClamped_A255(&Color);

	FGLVertexColor *pVertexColorArray = &m_pVertexColorArray[m_bufferedVerts];
	FGLTexCoord *pTexCoordArray = &m_pTexCoordArray[0][m_bufferedVerts];

	pVertexColorArray[0].x = X1Pos;
	pVertexColorArray[0].y = Y1Pos;
	pVertexColorArray[0].z = Z;
	pVertexColorArray[0].color = pointColor;

	pVertexColorArray[1].x = X2Pos;
	pVertexColorArray[1].y = Y1Pos;
	pVertexColorArray[1].z = Z;
	pVertexColorArray[1].color = pointColor;

	pVertexColorArray[2].x = X2Pos;
	pVertexColorArray[2].y = Y2Pos;
	pVertexColorArray[2].z = Z;
	pVertexColorArray[2].color = pointColor;

	pVertexColorArray[3].x = X1Pos;
	pVertexColorArray[3].y = Y1Pos;
	pVertexColorArray[3].z = Z;
	pVertexColorArray[3].color = pointColor;

	pVertexColorArray[4].x = X2Pos;
	pVertexColorArray[4].y = Y2Pos;
	pVertexColorArray[4].z = Z;
	pVertexColorArray[4].color = pointColor;

	pVertexColorArray[5].x = X1Pos;
	pVertexColorArray[5].y = Y2Pos;
	pVertexColorArray[5].z = Z;
	pVertexColorArray[5].color = pointColor;

	FLOAT SU1 = 0.0f;
	FLOAT SU2 = 1.0f;
	FLOAT SV1 = 0.0f;
	FLOAT SV2 = 1.0f;

	pTexCoordArray[0].u = SU1;
	pTexCoordArray[0].v = SV1;

	pTexCoordArray[1].u = SU2;
	pTexCoordArray[1].v = SV1;

	pTexCoordArray[2].u = SU2;
	pTexCoordArray[2].v = SV2;

	pTexCoordArray[3].u = SU1;
	pTexCoordArray[3].v = SV1;

	pTexCoordArray[4].u = SU2;
	pTexCoordArray[4].v = SV2;

	pTexCoordArray[5].u = SU1;
	pTexCoordArray[5].v = SV2;

	m_bufferedVerts += 6;

	unguard;
}
