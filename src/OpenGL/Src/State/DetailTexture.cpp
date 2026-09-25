/*=============================================================================
	DetailTexture.cpp: the detail layer, drawn only up close.

	Only near surfaces show it, so the geometry is clipped.
=============================================================================*/

#include "../OpenGLDrv.h"
#include "../OpenGL.h"

//Only call this with at least one detail polygon.
void UOpenGLRenderDevice::DrawDetailTexture(FTextureInfo &DetailTextureInfo, INT BaseClipIndex, bool clipDetailTexture) {
	bool vboWasActive = m_vboActive;
	if (vboWasActive) {
		m_vboActive = false;
		SetVertexStreamPointers();
	}

	//Clip branch only.
	FLOAT savedPolyOffsetFactor = m_curPolyOffsetFactor;
	FLOAT savedPolyOffsetUnits = m_curPolyOffsetUnits;
	bool savedPolyOffsetEnabled = m_polyOffsetEnabled;

	SetBlend(PF_Modulated);

	SetDefaultShaderState();

	bool detailAlphaMode = ((clipDetailTexture == false) && UseDetailAlpha) ? true : false;

	if ((m_clientTexEnableBits & 0x1) == 0) {
		m_clientTexEnableBits |= 0x1;

		SetClientActiveTexUnit(0);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	}

	if (detailAlphaMode) {
		SetColor3fv(m_detailTextureColor3f_1f);

		SetActiveTexUnit(0);
		SetAlphaTexture(0);
		//TexEnv 0 is PF_Modulated by default.

		SetActiveTexUnit(1);
		EnableTexUnit(1);
		if ((m_clientTexEnableBits & 0x2) == 0) {
			m_clientTexEnableBits |= 0x2;

			SetClientActiveTexUnit(1);
			glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		}
		SetTexEnv(1, PF_Memorized);
		SetTextureNoPanBias(1, DetailTextureInfo, PF_Modulated);

		DisableSubsequentTextures(2);
		DisableSubsequentClientTextures(2);
	} else {
		SetActiveTexUnit(0);
		SetTexEnv(0, PF_Memorized);
		SetTextureNoPanBias(0, DetailTextureInfo, PF_Modulated);

		glEnableClientState(GL_COLOR_ARRAY);
		if (clipDetailTexture == true) {
			/*
			Set explicitly, because this can run inside a pass already carrying a mover's opposite
			bias which would push the detail layer behind its surface, and it is saved and restored
			since the surface's later passes need their own bias back when this one returns.
			*/
			savedPolyOffsetFactor = m_curPolyOffsetFactor;
			savedPolyOffsetUnits = m_curPolyOffsetUnits;
			savedPolyOffsetEnabled = m_polyOffsetEnabled;
			SetPolygonOffset(m_detailPolyOffsetFactor, m_detailPolyOffsetUnits);
			SetPolygonOffsetEnabled(true);
		}

		DisableSubsequentTextures(1);
		DisableSubsequentClientTextures(1);
	}


	DWORD detailColor = m_detailTextureColor4ub;

	INT detailPassNum = 0;
	FLOAT NearZ = 380.0f;
	FLOAT RNearZ = 1.0f / NearZ;
	FLOAT DetailScale = 1.0f;
	do {
		if (detailPassNum > 0) {
			NearZ /= 4.223f;
			RNearZ *= 4.223f;
			DetailScale *= 4.223f;

			(this->*m_pBufferDetailTextureDataProc)(NearZ);
		}

		//Scaled UMult and VMult for the detail texture.
		FLOAT DetailUMult;
		FLOAT DetailVMult;
		if (detailAlphaMode) {
			DetailUMult = TexInfo[1].UMult * DetailScale;
			DetailVMult = TexInfo[1].VMult * DetailScale;
		} else {
			DetailUMult = TexInfo[0].UMult * DetailScale;
			DetailVMult = TexInfo[0].VMult * DetailScale;
		}

		INT Index = 0;
		INT NextClipIndex = BaseClipIndex;

		INT *pNumPts = &MultiDrawCountArray[0];
		DWORD *pDetailTextureIsNear = DetailTextureIsNearArray;
		for (DWORD PolyNum = 0; PolyNum < m_csPolyCount; PolyNum++, pNumPts++, pDetailTextureIsNear++) {
			DWORD NumPts = *pNumPts;
			DWORD isNearBits = *pDetailTextureIsNear;

			//Only skip when the mask can represent the polygon.
			//Past 32 points, drawing anyway is safe.
			if ((isNearBits == 0) && (NumPts <= 32)) {
				Index += NumPts;
				continue;
			}
			INT StartIndex = Index;

			//Shifted down because NumPts may be 32.
			DWORD allPtsBits = ~0U >> (DETAIL_TEXTURE_MAX_POLY_PTS - NumPts);
			if (detailAlphaMode) {
				for (DWORD i = 0; i < NumPts; i++) {
					const FGLVertex &Point = VertexArray[Index];
					FLOAT U = MapDotArray[Index].u;
					FLOAT V = MapDotArray[Index].v;

					FGLTexCoord *pTexCoord0 = TexCoordArray[0];
					FGLTexCoord *pTexCoord1 = TexCoordArray[1];
					FLOAT PointZ_Times_RNearZ = Point.z * RNearZ;
					pTexCoord0[Index].u = PointZ_Times_RNearZ;
					pTexCoord0[Index].v = 0.5f;
					pTexCoord1[Index].u = (U - TexInfo[1].UPan) * DetailUMult;
					pTexCoord1[Index].v = (V - TexInfo[1].VPan) * DetailVMult;

					Index++;
				}

				glDrawArrays(GL_TRIANGLE_FAN, StartIndex, NumPts);
			}
			//Otherwise no clipping is required, or it is disabled.
			else if ((clipDetailTexture == false) || (isNearBits == allPtsBits)) {
				for (DWORD i = 0; i < NumPts; i++) {
					const FGLVertex &Point = VertexArray[Index];
					FLOAT U = MapDotArray[Index].u;
					FLOAT V = MapDotArray[Index].v;

					FGLTexCoord *pTexCoord = TexCoordArray[0];
					pTexCoord[Index].u = (U - TexInfo[0].UPan) * DetailUMult;
					pTexCoord[Index].v = (V - TexInfo[0].VPan) * DetailVMult;
					DWORD alpha = appRound((1.0f - (Clamp(Point.z, 0.0f, NearZ) * RNearZ)) * 255.0f);
					SingleColorArray[Index].color = detailColor | (alpha << 24);

					Index++;
				}

				glDrawArrays(GL_TRIANGLE_FAN, StartIndex, NumPts);
			}
			//Otherwise clipping is required and enabled.
			else {
				DWORD NextIndex = 0;
				GLuint IndexList[DETAIL_TEXTURE_MAX_POLY_PTS * 2];
				DWORD isNear_i_bit = 1U << (NumPts - 1);
				DWORD isNear_j_bit = 1U;
				/*
				DWORD to match NumPts, which cannot be zero here, so NumPts - 1 cannot wrap, and the
				bound is 1 where the gouraud paths use 3 because a fan of 1 or 2 just draws nothing
				and rejecting it here would cost a test per polygon, the caller having already
				refused anything wider than the mask can hold.
				*/
				for (DWORD i = 0, j = NumPts - 1; i < NumPts; j = i++, isNear_j_bit = isNear_i_bit, isNear_i_bit >>= 1) {
					const FGLVertex &Point = VertexArray[Index];
					FLOAT U = MapDotArray[Index].u;
					FLOAT V = MapDotArray[Index].v;

					if (((isNear_i_bit & isNearBits) != 0) && ((isNear_j_bit & isNearBits) == 0)) {
						const FGLVertex &PrevPoint = VertexArray[StartIndex + j];
						FLOAT PrevU = MapDotArray[StartIndex + j].u;
						FLOAT PrevV = MapDotArray[StartIndex + j].v;

						FLOAT dist = PrevPoint.z - Point.z;
						FLOAT m = 1.0f;
						if (dist > 0.001f) {
							m = (NearZ - Point.z) / dist;
						}
						FGLVertex *pVertex = &VertexArray[NextClipIndex];
						FGLTexCoord *pTexCoord = &TexCoordArray[0][NextClipIndex];
						pVertex->x = (m * (PrevPoint.x - Point.x)) + Point.x;
						pVertex->y = (m * (PrevPoint.y - Point.y)) + Point.y;
						pVertex->z = NearZ;
						pTexCoord->u = ((m * (PrevU - U)) + U - TexInfo[0].UPan) * DetailUMult;
						pTexCoord->v = ((m * (PrevV - V)) + V - TexInfo[0].VPan) * DetailVMult;
						DWORD alpha = 0;
						SingleColorArray[NextClipIndex].color = detailColor | (alpha << 24);
						IndexList[NextIndex++] = NextClipIndex++;
					}

					if ((isNear_i_bit & isNearBits) != 0) {
						FGLTexCoord *pTexCoord = &TexCoordArray[0][Index];
						pTexCoord->u = (U - TexInfo[0].UPan) * DetailUMult;
						pTexCoord->v = (V - TexInfo[0].VPan) * DetailVMult;
						DWORD alpha = appRound((1.0f - (Clamp(Point.z, 0.0f, NearZ) * RNearZ)) * 255.0f);
						SingleColorArray[Index].color = detailColor | (alpha << 24);
						IndexList[NextIndex++] = Index;
					}

					if (((isNear_i_bit & isNearBits) == 0) && ((isNear_j_bit & isNearBits) != 0)) {
						const FGLVertex &PrevPoint = VertexArray[StartIndex + j];
						FLOAT PrevU = MapDotArray[StartIndex + j].u;
						FLOAT PrevV = MapDotArray[StartIndex + j].v;

						FLOAT dist = Point.z - PrevPoint.z;
						FLOAT m = 1.0f;
						if (dist > 0.001f) {
							m = (NearZ - PrevPoint.z) / dist;
						}
						FGLVertex *pVertex = &VertexArray[NextClipIndex];
						FGLTexCoord *pTexCoord = &TexCoordArray[0][NextClipIndex];
						pVertex->x = (m * (Point.x - PrevPoint.x)) + PrevPoint.x;
						pVertex->y = (m * (Point.y - PrevPoint.y)) + PrevPoint.y;
						pVertex->z = NearZ;
						pTexCoord->u = ((m * (U - PrevU)) + PrevU - TexInfo[0].UPan) * DetailUMult;
						pTexCoord->v = ((m * (V - PrevV)) + PrevV - TexInfo[0].VPan) * DetailVMult;
						DWORD alpha = 0;
						SingleColorArray[NextClipIndex].color = detailColor | (alpha << 24);
						IndexList[NextIndex++] = NextClipIndex++;
					}
					Index++;
				}

				glDrawElements(GL_TRIANGLE_FAN, NextIndex, GL_UNSIGNED_INT, IndexList);
			}
		}
	} while (++detailPassNum < DetailMax);


	//Clear detail texture state.
	SetActiveTexUnit(0);
	//Bypassed the tracked state.
	InvalidateColorShadow();
	//The two branches leave different units enabled.
	InvalidateTexAttribShadows();
	if (detailAlphaMode) {
		//TexEnv 0 was left at the default PF_Modulated.
	} else {
		SetTexEnv(0, PF_Modulated);

		glDisableClientState(GL_COLOR_ARRAY);
		if (clipDetailTexture == true) {
			SetPolygonOffsetEnabled(savedPolyOffsetEnabled);
			SetPolygonOffset(savedPolyOffsetFactor, savedPolyOffsetUnits);
		}
	}

	if (vboWasActive) {
		//Back to the ring.
		m_vboActive = true;
		SetVertexStreamPointers();
	}

	return;
}

//Only call this with at least one detail polygon.
void UOpenGLRenderDevice::DrawDetailTexture_FP(FTextureInfo &DetailTextureInfo) {
	//Same positions the pass draws used.
	INT Index = m_csBaseVertex;
	GLuint fpId;

	SetBlend(PF_Modulated);

	fpId = m_fpDetailTexture;
	if (DetailMax >= 2) fpId = m_fpDetailTextureTwoLayer;
	SetShaderState(m_vpComplexSurfaceSingleTextureWithPos, fpId);

	SetColor3fv(m_detailTextureColor3f_1f);

	SetActiveTexUnit(0);
	SetTextureNoPanBias(0, DetailTextureInfo, PF_Modulated);
	SetTexAttrib(0);

	DisableSubsequentTextures(1);
	DisableSubsequentClientTextures(0);


	INT *pNumPts = &MultiDrawCountArray[0];
	DWORD *pDetailTextureIsNear = DetailTextureIsNearArray;
	DWORD csPolyCount = m_csPolyCount;
	for (DWORD PolyNum = 0; PolyNum < csPolyCount; PolyNum++, pNumPts++, pDetailTextureIsNear++) {
		DWORD NumPts = *pNumPts;
		DWORD isNearBits = *pDetailTextureIsNear;

		//As above.
		if ((isNearBits == 0) && (NumPts <= 32)) {
			Index += NumPts;
			continue;
		}

		glDrawArrays(GL_TRIANGLE_FAN, Index, NumPts);
		Index += NumPts;
	}


	//Clear detail texture state.
	SetActiveTexUnit(0);
	//TexEnv 0 was left at the default PF_Modulated.

	return;
}
