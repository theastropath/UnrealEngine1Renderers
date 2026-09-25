/*=============================================================================
	Tile.cpp: HUD, menus and text, all screen space quads.
=============================================================================*/

#include "../D3D9Drv.h"
#include "../D3D9.h"

void UD3D9RenderDevice::DrawTile(FSceneNode *Frame, FTextureInfo &Info, FLOAT X, FLOAT Y, FLOAT XL, FLOAT YL, FLOAT U, FLOAT V, FLOAT UL, FLOAT VL, class FSpanBuffer *Span, FLOAT Z, FPlane Color, FPlane Fog, DWORD PolyFlags) {
#ifdef UTGLR_DEBUG_SHOW_CALL_COUNTS
	{
		static int si;
		dout << L"utd3d9r: DrawTile = " << si++ << std::endl;
	}
#endif
	guard(UD3D9RenderDevice::DrawTile);

	if (m_frameSkipped) {
		return;
	}

	const DWORD PolyFlags2 = 0;

	EndDeferredGouraudPolys();
	EndBufferingExcept(BV_TYPE_TILES);

	if (SceneNodeChanged(Frame)) {
		m_sceneNodeRefreshCount++;
		SetSceneNode(Frame);
	}

	if (m_useZRangeHack) {
		if ((Z >= 0.5f) && (Z < 8.0f)) {
			Z = (((Z - 0.5f) / 7.5f) * 4.0f) + 4.0f;
		}
	}

	FLOAT PX1 = X - Frame->FX2;
	FLOAT PX2 = PX1 + XL;
	FLOAT PY1 = Y - Frame->FY2;
	FLOAT PY2 = PY1 + YL;

	FLOAT RPX1 = m_RFX2 * PX1;
	FLOAT RPX2 = m_RFX2 * PX2;
	FLOAT RPY1 = m_RFY2 * PY1;
	FLOAT RPY2 = m_RFY2 * PY2;
	if (!Frame->Viewport->IsOrtho()) {
		RPX1 *= Z;
		RPX2 *= Z;
		RPY1 *= Z;
		RPY2 *= Z;
	}

	if (m_HitData) {
		CGClip::vec3_t triPts[3];

		triPts[0].x = RPX1;
		triPts[0].y = RPY1;
		triPts[0].z = Z;

		triPts[1].x = RPX2;
		triPts[1].y = RPY1;
		triPts[1].z = Z;

		triPts[2].x = RPX2;
		triPts[2].y = RPY2;
		triPts[2].z = Z;

		m_gclip.SelectDrawTri(triPts);

		triPts[0].x = RPX1;
		triPts[0].y = RPY1;
		triPts[0].z = Z;

		triPts[1].x = RPX2;
		triPts[1].y = RPY2;
		triPts[1].z = Z;

		triPts[2].x = RPX1;
		triPts[2].y = RPY2;
		triPts[2].z = Z;

		m_gclip.SelectDrawTri(triPts);

		return;
	}

	{
		const QWORD CacheID = ComposeTexCacheID(Info, PolyFlags);

		if (DeferredActive()) {
			const DWORD keyPolyFlags = PolyFlags;
			DWORD bindPolyFlags = PolyFlags;
			DWORD blendPolyFlags = PolyFlags;
#ifdef UTGLR_RUNE_BUILD
			if (Info.Palette && Info.Palette[128].A != 255 && !(PolyFlags & (PF_Translucent | PF_AlphaBlend))) {
#else
			if (Info.Palette && Info.Palette[128].A != 255 && !(PolyFlags & PF_Translucent)) {
#endif
				bindPolyFlags = PolyFlags | PF_Highlighted | PF_Occlude;
				blendPolyFlags = bindPolyFlags | PF_Masked;
			}

			if (DebugBit(DEBUG_BIT_TILES)) {
				LogTileOnce(Info, keyPolyFlags, blendPolyFlags);
			}

			EndBuffering();

			//Texels here.
			FQuadCmd quad;
			quad.X1 = RPX1;
			quad.Y1 = RPY1;
			quad.X2 = RPX2;
			quad.Y2 = RPY2;
			quad.U1 = U;
			quad.V1 = V;
			quad.U2 = U + UL;
			quad.V2 = V + VL;
			quad.Z1 = Z;
			quad.Z2 = Z;
			quad.Color = ComputeTileColor(Color, Info, keyPolyFlags);

			if (RecordQuad(DCMD_TILE, &Info, keyPolyFlags, bindPolyFlags, blendPolyFlags, PolyFlags2, quad)) {
				return;
			}
			FlushDeferred();
		}

		if ((m_curPolyFlags != PolyFlags) ||
			(m_curPolyFlags2 != PolyFlags2) ||
			(TexInfo[0].CurrentCacheID != CacheID) ||
			((m_curVertexBufferPos + m_bufferedVerts) >= (VERTEX_RING_SIZE - 6)) ||
			(m_bufferedVerts == 0)) {
			EndBuffering();

			if ((m_curVertexBufferPos + m_bufferedVerts) >= (VERTEX_RING_SIZE - 6)) {
				FlushVertexBuffers();
			}

			StartBuffering(BV_TYPE_TILES);

			m_curPolyFlags = PolyFlags;
			m_curPolyFlags2 = PolyFlags2;

			DWORD blendPolyFlags = PolyFlags;
#ifdef UTGLR_RUNE_BUILD
			if (Info.Palette && Info.Palette[128].A != 255 && !(PolyFlags & (PF_Translucent | PF_AlphaBlend))) {
#else
			if (Info.Palette && Info.Palette[128].A != 255 && !(PolyFlags & PF_Translucent)) {
#endif
				PolyFlags |= PF_Highlighted | PF_Occlude;

				//Blend flags only.
				blendPolyFlags = PolyFlags | PF_Masked;
			}

			if (DebugBit(DEBUG_BIT_TILES)) {
				LogTileOnce(Info, m_curPolyFlags, blendPolyFlags);
			}

			SetDefaultTextureState();

			SetBlend(blendPolyFlags);
			SetTextureNoPanBias(0, Info, PolyFlags);

			if (PolyFlags & PF_Modulated) {
				m_requestedColorFlags = 0;
			} else {
				m_requestedColorFlags = CF_COLOR_ARRAY;
			}

			LockVertexColorBuffer();
			LockTexCoordBuffer(0);

			SetDefaultStreamState();
		}

		const DWORD tileColor = ComputeTileColor(Color, Info, m_curPolyFlags);

		FGLVertexColor *pVertexColorArray = &m_pVertexColorArray[m_bufferedVerts];
		FGLTexCoord *pTexCoordArray = &m_pTexCoordArray[0][m_bufferedVerts];

		pVertexColorArray[0].x = RPX1;
		pVertexColorArray[0].y = RPY1;
		pVertexColorArray[0].z = Z;
		pVertexColorArray[0].color = tileColor;

		pVertexColorArray[1].x = RPX2;
		pVertexColorArray[1].y = RPY1;
		pVertexColorArray[1].z = Z;
		pVertexColorArray[1].color = tileColor;

		pVertexColorArray[2].x = RPX2;
		pVertexColorArray[2].y = RPY2;
		pVertexColorArray[2].z = Z;
		pVertexColorArray[2].color = tileColor;

		pVertexColorArray[3].x = RPX1;
		pVertexColorArray[3].y = RPY1;
		pVertexColorArray[3].z = Z;
		pVertexColorArray[3].color = tileColor;

		pVertexColorArray[4].x = RPX2;
		pVertexColorArray[4].y = RPY2;
		pVertexColorArray[4].z = Z;
		pVertexColorArray[4].color = tileColor;

		pVertexColorArray[5].x = RPX1;
		pVertexColorArray[5].y = RPY2;
		pVertexColorArray[5].z = Z;
		pVertexColorArray[5].color = tileColor;

		FLOAT TexInfoUMult = TexInfo[0].UMult;
		FLOAT TexInfoVMult = TexInfo[0].VMult;

		FLOAT SU1 = (U)*TexInfoUMult;
		FLOAT SU2 = (U + UL) * TexInfoUMult;
		FLOAT SV1 = (V)*TexInfoVMult;
		FLOAT SV2 = (V + VL) * TexInfoVMult;

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
	}

	unguard;
}
