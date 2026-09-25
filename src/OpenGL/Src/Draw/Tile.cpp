/*=============================================================================
	Tile.cpp: HUD, menus and text, all screen space quads.
=============================================================================*/

#include "../OpenGLDrv.h"
#include "../OpenGL.h"

void UOpenGLRenderDevice::DrawTile(FSceneNode *Frame, FTextureInfo &Info, FLOAT X, FLOAT Y, FLOAT XL, FLOAT YL, FLOAT U, FLOAT V, FLOAT UL, FLOAT VL, class FSpanBuffer *Span, FLOAT Z, FPlane Color, FPlane Fog, DWORD PolyFlags) {
	UTGLR_DEBUG_CALL_COUNT(DrawTile);
	guard(UOpenGLRenderDevice::DrawTile);

	const DWORD PolyFlags2 = 0;

	EndDeferredGouraudPolys();
	EndBufferingExceptTiles();

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

	if (BufferTileQuads) {
		//Same as the upload path.
		const QWORD CacheID = ComposeTexCacheID(Info, PolyFlags);

		if ((m_curPolyFlags != PolyFlags) ||
			(m_curPolyFlags2 != PolyFlags2) ||
			(TexInfo[0].CurrentCacheID != CacheID) ||
			(BufferedTileVerts >= (VERTEX_ARRAY_SIZE - 4)) ||
			(BufferedTileVerts == 0)) {
			EndTileBuffering();

			SetDefaultShaderState();

			//Update current poly flags first.
			m_curPolyFlags = PolyFlags;
			m_curPolyFlags2 = PolyFlags2;

			DWORD blendPolyFlags = PolyFlags;
#ifdef UTGLR_RUNE_BUILD
			if (Info.Palette && Info.Palette[128].A != 255 && !(PolyFlags & (PF_Translucent | PF_AlphaBlend))) {
#elif defined(UTGLR_UNREAL_227_BUILD)
			if (Info.Palette && Info.Palette[128].A != 255 && !(PolyFlags & (PF_Translucent | PF_AlphaBlend))) {
#else
			if (Info.Palette && Info.Palette[128].A != 255 && !(PolyFlags & PF_Translucent)) {
#endif
				PolyFlags |= PF_Highlighted | PF_Occlude;

				//Blend flags only.
				//The texture-selecting side would pick the masked variant.
				blendPolyFlags = PolyFlags | PF_Masked;
			}

			//The cached copy.
			if (DebugBit(DEBUG_BIT_TILES)) {
				LogTileOnce(Info, m_curPolyFlags, blendPolyFlags);
			}

			SetDefaultTextureState();

			SetBlend(blendPolyFlags);
			SetTextureNoPanBias(0, Info, PolyFlags);

			if (PolyFlags & PF_Modulated) {
				SetColor3f(1.0f, 1.0f, 1.0f);
				m_requestedColorFlags = 0;
			} else {
				m_requestedColorFlags = CF_COLOR_ARRAY;
			}
		}

		FGLTexCoord *pTexCoordArray = &TexCoordArray[0][BufferedTileVerts];
		FGLVertex *pVertexArray = &VertexArray[BufferedTileVerts];

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
		pTexCoordArray[3].v = SV2;

		pVertexArray[0].x = RPX1;
		pVertexArray[0].y = RPY1;
		pVertexArray[0].z = Z;

		pVertexArray[1].x = RPX2;
		pVertexArray[1].y = RPY1;
		pVertexArray[1].z = Z;

		pVertexArray[2].x = RPX2;
		pVertexArray[2].y = RPY2;
		pVertexArray[2].z = Z;

		pVertexArray[3].x = RPX1;
		pVertexArray[3].y = RPY2;
		pVertexArray[3].z = Z;

		if (!(PolyFlags & PF_Modulated)) {
			/*
			The #ifdef covers the UseSSE2 test and its body only, and the else block below compiles
			unconditionally, so extending the #ifdef over that block would leave the tile's four
			colours unwritten with SSE compiled out and the batch would draw with whatever the
			previous tile happened to set.
			*/
#ifdef UTGLR_INCLUDE_SSE_CODE
			if (UseSSE2) {
				FGLSingleColor *pSingleColorArray = &SingleColorArray[BufferedTileVerts];
				static __m128 fColorMul = { 255.0f, 255.0f, 255.0f, 0.0f };
				static const union {
					DWORD u[4];
					__m128 f;
				} fRGBLaneMask = { { 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0x00000000u } };
				__m128 fColorMulReg;
				__m128 fColor;
				__m128 fAlpha;
				__m128i iColor;

				fColorMulReg = fColorMul;
				fColor = _mm_loadu_ps(&Color.X);
				fColor = _mm_mul_ps(fColor, fColorMulReg);
				fColor = _mm_and_ps(fColor, fRGBLaneMask.f);

				fAlpha = _mm_setzero_ps();
				fAlpha = _mm_move_ss(fAlpha, fColorMulReg);
#ifdef UTGLR_RUNE_BUILD
				if (PolyFlags & PF_AlphaBlend) {
					fAlpha = _mm_mul_ss(fAlpha, _mm_load_ss(&Info.Texture->Alpha));
				}
#elif defined(UTGLR_UNREAL_227_BUILD)
				if (PolyFlags & PF_AlphaBlend) {
					fAlpha = _mm_mul_ss(fAlpha, _mm_load_ss(&Color.W));
				}
#endif
				fAlpha = _mm_shuffle_ps(fAlpha, fAlpha, _MM_SHUFFLE(0, 1, 1, 1));

				fColor = _mm_or_ps(fColor, fAlpha);

				iColor = _mm_cvtps_epi32(fColor);
				iColor = _mm_packs_epi32(iColor, iColor);
				iColor = _mm_packus_epi16(iColor, iColor);

				_mm_store_si128((__m128i *)pSingleColorArray, iColor);
			} else
#endif
			{
				DWORD dwColor;
				FGLSingleColor *pSingleColorArray = &SingleColorArray[BufferedTileVerts];

#ifdef UTGLR_RUNE_BUILD
				if (PolyFlags & PF_AlphaBlend) {
					Color.W = Info.Texture->Alpha;
					dwColor = FPlaneTo_RGBAClamped(&Color);
				} else {
					dwColor = FPlaneTo_RGBClamped_A255(&Color);
				}
#elif defined(UTGLR_UNREAL_227_BUILD)
				if (PolyFlags & PF_AlphaBlend) {
					dwColor = FPlaneTo_RGBAClamped(&Color);
				} else {
					dwColor = FPlaneTo_RGBClamped_A255(&Color);
				}
#else
				dwColor = FPlaneTo_RGBClamped_A255(&Color);
#endif

				pSingleColorArray[0].color = dwColor;
				pSingleColorArray[1].color = dwColor;
				pSingleColorArray[2].color = dwColor;
				pSingleColorArray[3].color = dwColor;
			}
		}

		BufferedTileVerts += 4;
	} else {
		EndTileBuffering();

		clock(TileCycles);

		if (NoAATiles) {
			SetDisabledAAState();
		} else {
			SetDefaultAAState();
		}
		SetDefaultProjectionState();
		SetDefaultColorState();
		SetDefaultShaderState();
		SetDefaultTextureState();

		//The block below modifies PolyFlags in place.
		const DWORD submittedPolyFlags = PolyFlags;
		DWORD blendPolyFlags = PolyFlags;
#ifdef UTGLR_RUNE_BUILD
		if (Info.Palette && Info.Palette[128].A != 255 && !(PolyFlags & (PF_Translucent | PF_AlphaBlend))) {
#elif defined(UTGLR_UNREAL_227_BUILD)
		if (Info.Palette && Info.Palette[128].A != 255 && !(PolyFlags & (PF_Translucent | PF_AlphaBlend))) {
#else
		if (Info.Palette && Info.Palette[128].A != 255 && !(PolyFlags & PF_Translucent)) {
#endif
			PolyFlags |= PF_Highlighted | PF_Occlude;

			//As above.
			blendPolyFlags = PolyFlags | PF_Masked;
		}

		if (DebugBit(DEBUG_BIT_TILES)) {
			LogTileOnce(Info, submittedPolyFlags, blendPolyFlags);
		}

		SetBlend(blendPolyFlags);
		SetTextureNoPanBias(0, Info, PolyFlags);

		if (PolyFlags & PF_Modulated) {
			SetColor3f(1.0f, 1.0f, 1.0f);
		} else {
#ifdef UTGLR_RUNE_BUILD
			Color.W = 1.0f;
			if (PolyFlags & PF_AlphaBlend) {
				Color.W = Info.Texture->Alpha;
			}
			SetColor4fv(&Color.X);
#elif defined(UTGLR_UNREAL_227_BUILD)
			if (PolyFlags & PF_AlphaBlend) {
				SetColor4fv(&Color.X);
			} else {
				SetColor3fv(&Color.X);
			}
#else
			SetColor3fv(&Color.X);
#endif
		}

		FLOAT TexInfoUMult = TexInfo[0].UMult;
		FLOAT TexInfoVMult = TexInfo[0].VMult;

		FLOAT SU1 = (U)*TexInfoUMult;
		FLOAT SU2 = (U + UL) * TexInfoUMult;
		FLOAT SV1 = (V)*TexInfoVMult;
		FLOAT SV2 = (V + VL) * TexInfoVMult;

		glBegin(GL_TRIANGLE_FAN);

		glTexCoord2f(SU1, SV1);
		glVertex3f(RPX1, RPY1, Z);

		glTexCoord2f(SU2, SV1);
		glVertex3f(RPX2, RPY1, Z);

		glTexCoord2f(SU2, SV2);
		glVertex3f(RPX2, RPY2, Z);

		glTexCoord2f(SU1, SV2);
		glVertex3f(RPX1, RPY2, Z);

		glEnd();

		//Whatever corner was last.
		InvalidateTexAttribShadows();

		unclock(TileCycles);
	}

	unguard;
}
