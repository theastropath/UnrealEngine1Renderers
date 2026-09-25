/*=============================================================================
	Line.cpp: lines and points, mostly an editor path.
=============================================================================*/

#include "../OpenGLDrv.h"
#include "../OpenGL.h"

//Only transparency maps across.
static inline DWORD linePolyFlags(DWORD LineFlags) {
	return (LineFlags & LINE_Transparent) ? PF_Translucent : (PF_Highlighted | PF_Occlude);
}

//Breaks the batch.
void UOpenGLRenderDevice::BufferLine(DWORD LineFlags, FPlane Color, FLOAT X1, FLOAT Y1, FLOAT Z1, FLOAT X2, FLOAT Y2, FLOAT Z2) {
	const DWORD PolyFlags = linePolyFlags(LineFlags);
	const DWORD PolyFlags2 = 0;

	if ((m_curPolyFlags != PolyFlags) ||
		(m_curPolyFlags2 != PolyFlags2) ||
		(TexInfo[0].CurrentCacheID != TEX_CACHE_ID_NO_TEX) ||
		(BufferedLineVerts >= (VERTEX_ARRAY_SIZE - 2)) ||
		(BufferedLineVerts == 0)) {
		EndLineBuffering();

		SetDefaultAAState();
		SetDefaultProjectionState();
		SetDefaultShaderState();
		SetDefaultTextureState();

		m_curPolyFlags = PolyFlags;
		m_curPolyFlags2 = PolyFlags2;

		SetBlend(PolyFlags);
		SetNoTexture(0);

		//Colors come from the array, so lines of different colors share a draw.
		m_requestedColorFlags = CF_COLOR_ARRAY;
	}

	FGLVertex *pVertexArray = &VertexArray[BufferedLineVerts];
	FGLTexCoord *pTexCoordArray = &TexCoordArray[0][BufferedLineVerts];
	DWORD lineColor = FPlaneTo_RGBClamped_A255(&Color);

	pVertexArray[0].x = X1;
	pVertexArray[0].y = Y1;
	pVertexArray[0].z = Z1;
	pVertexArray[1].x = X2;
	pVertexArray[1].y = Y2;
	pVertexArray[1].z = Z2;

	//Only the 1x1 white texture is bound.
	//Written anyway, so nothing samples a stale slot.
	pTexCoordArray[0].u = 0.0f;
	pTexCoordArray[0].v = 0.0f;
	pTexCoordArray[1].u = 0.0f;
	pTexCoordArray[1].v = 0.0f;

	SingleColorArray[BufferedLineVerts + 0].color = lineColor;
	SingleColorArray[BufferedLineVerts + 1].color = lineColor;

	BufferedLineVerts += 2;
}

void UOpenGLRenderDevice::Draw3DLine(FSceneNode *Frame, FPlane Color, DWORD LineFlags, FVector P1, FVector P2) {
	UTGLR_DEBUG_CALL_COUNT(Draw3DLine);
	guard(UOpenGLRenderDevice::Draw3DLine);

	EndDeferredGouraudPolys();

	//Consecutive lines accumulate.
	EndBufferingExceptLines();

	//No guarantee scene setup has run first.
	if (SceneNodeChanged(Frame)) {
		m_sceneNodeRefreshCount++;
		SetSceneNode(Frame);
	}

	P1 = P1.TransformPointBy(Frame->Coords);
	P2 = P2.TransformPointBy(Frame->Coords);
	if (Frame->Viewport->IsOrtho()) {
		FLOAT rcpZoom = 1.0f / Frame->Zoom;
		P1.X = (P1.X * rcpZoom) + Frame->FX2;
		P1.Y = (P1.Y * rcpZoom) + Frame->FY2;
		P2.X = (P2.X * rcpZoom) + Frame->FX2;
		P2.Y = (P2.Y * rcpZoom) + Frame->FY2;
		P1.Z = P2.Z = 1;

		// See if points form a line parallel to our line of sight (i.e. line appears as a dot).
		if (Abs(P2.X - P1.X) + Abs(P2.Y - P1.Y) >= 0.2f) {
			Draw2DLine(Frame, Color, LineFlags, P1, P2);
		} else if (Frame->Viewport->Actor->OrthoZoom < ORTHO_LOW_DETAIL) {
			Draw2DPoint(Frame, Color, LINE_None, P1.X - 1.0f, P1.Y - 1.0f, P1.X + 1.0f, P1.Y + 1.0f, P1.Z);
		}
	} else {
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

		BufferLine(LineFlags, Color, P1.X, P1.Y, P1.Z, P2.X, P2.Y, P2.Z);
	}
	unguard;
}

void UOpenGLRenderDevice::Draw2DLine(FSceneNode *Frame, FPlane Color, DWORD LineFlags, FVector P1, FVector P2) {
	UTGLR_DEBUG_CALL_COUNT(Draw2DLine);
	guard(UOpenGLRenderDevice::Draw2DLine);

	EndDeferredGouraudPolys();

	EndBufferingExceptLines();

	//Without this the conversion could use the previous node's values.
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

	BufferLine(LineFlags, Color, X1Pos, Y1Pos, P1.Z, X2Pos, Y2Pos, P2.Z);

	unguard;
}

void UOpenGLRenderDevice::Draw2DPoint(FSceneNode *Frame, FPlane Color, DWORD LineFlags, FLOAT X1, FLOAT Y1, FLOAT X2, FLOAT Y2, FLOAT Z) {
	UTGLR_DEBUG_CALL_COUNT(Draw2DPoint);
	guard(UOpenGLRenderDevice::Draw2DPoint);

	EndDeferredGouraudPolys();

	//As above.
	EndBufferingExceptPoints();

	//A point can be the first primitive of a node.
	if (SceneNodeChanged(Frame)) {
		m_sceneNodeRefreshCount++;
		SetSceneNode(Frame);
	}

	// Hack to fix UED selection problem with selection brush
	if (GIsEditor) {
		Z = 1.0f;

		/*
		With the Z range hack on the near plane sits at 4.0, so this manufactured Z has to be
		brought into range or it clips away in a perspective viewport, and ortho viewports never
		enable the hack, which is why the test is on the hack itself, the constant being the same
		one the projection uses so that moving one means moving the other with nothing to warn
		about it.
		*/
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

	//A quad, like tiles.
	{
		const DWORD PolyFlags = linePolyFlags(LineFlags);
		const DWORD PolyFlags2 = 0;

		if ((m_curPolyFlags != PolyFlags) ||
			(m_curPolyFlags2 != PolyFlags2) ||
			(TexInfo[0].CurrentCacheID != TEX_CACHE_ID_NO_TEX) ||
			(BufferedPointVerts >= (VERTEX_ARRAY_SIZE - 4)) ||
			(BufferedPointVerts == 0)) {
			EndPointBuffering();

			SetDefaultAAState();
			SetDefaultProjectionState();
			SetDefaultShaderState();
			SetDefaultTextureState();

			m_curPolyFlags = PolyFlags;
			m_curPolyFlags2 = PolyFlags2;

			SetBlend(PolyFlags);
			SetNoTexture(0);

			m_requestedColorFlags = CF_COLOR_ARRAY;
		}

		FGLVertex *pVertexArray = &VertexArray[BufferedPointVerts];
		FGLTexCoord *pTexCoordArray = &TexCoordArray[0][BufferedPointVerts];
		DWORD pointColor = FPlaneTo_RGBClamped_A255(&Color);

		pVertexArray[0].x = X1Pos;
		pVertexArray[0].y = Y1Pos;
		pVertexArray[0].z = Z;
		pVertexArray[1].x = X2Pos;
		pVertexArray[1].y = Y1Pos;
		pVertexArray[1].z = Z;
		pVertexArray[2].x = X2Pos;
		pVertexArray[2].y = Y2Pos;
		pVertexArray[2].z = Z;
		pVertexArray[3].x = X1Pos;
		pVertexArray[3].y = Y2Pos;
		pVertexArray[3].z = Z;

		for (INT i = 0; i < 4; i++) {
			pTexCoordArray[i].u = 0.0f;
			pTexCoordArray[i].v = 0.0f;
			SingleColorArray[BufferedPointVerts + i].color = pointColor;
		}

		BufferedPointVerts += 4;
	}

	unguard;
}
