/*=============================================================================
	DetailTexture.cpp: the close-up detail layer.

	Only on surfaces near enough to see it, so the geometry is clipped against a distance.
=============================================================================*/

#include "../D3D9Drv.h"
#include "../D3D9.h"

//At least one polygon.
void UD3D9RenderDevice::DrawDetailTexture(FTextureInfo &DetailTextureInfo, bool clipDetailTexture) {
	//Restored below.
	FLOAT savedSlopeScaleDepthBias = m_curSlopeScaleDepthBias;

	SetBlend(PF_Modulated);

	bool detailAlphaMode = ((clipDetailTexture == false) && UseDetailAlpha) ? true : false;

	if (detailAlphaMode) {
		SetAlphaTexture(0);

		SetTextureNoPanBias(1, DetailTextureInfo, PF_Modulated);
		SetTexEnv(1, PF_Memorized);

		SetStreamState(m_standardNTextureVertexDecl[1], NULL, NULL);

		DisableSubsequentTextures(2);
	} else {
		SetTextureNoPanBias(0, DetailTextureInfo, PF_Modulated);
		SetTexEnv(0, PF_Memorized);

		if (clipDetailTexture == true) {
			SetSlopeScaleDepthBias(-1.0f);
		}

		SetStreamState(m_standardNTextureVertexDecl[0], NULL, NULL);

		DisableSubsequentTextures(1);
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

		INT *pNumPts = &MultiDrawCountArray[0];
		DWORD *pDetailTextureIsNear = DetailTextureIsNearArray;
		for (DWORD PolyNum = 0; PolyNum < m_csPolyCount; PolyNum++, pNumPts++, pDetailTextureIsNear++) {
			DWORD NumPts = *pNumPts;
			DWORD isNearBits = *pDetailTextureIsNear;

			//Skips the polygon only when the mask can represent it.
			//Past 32 points bits have shifted out, so a zero mask may hide near vertices.
			if ((isNearBits == 0) && (NumPts <= 32)) {
				Index += NumPts;
				continue;
			}
			INT StartIndex = Index;

			//isNearBits is one bit per vertex. Shifting by 32 or more is undefined.
			DWORD allPtsBits = (NumPts >= 32) ? ~0U : ~(~0U << NumPts);
			bool canClipDetailTexture = clipDetailTexture && (NumPts <= 32);
			if (detailAlphaMode) {
				if ((m_curVertexBufferPos + NumPts) >= VERTEX_RING_SIZE) {
					FlushVertexBuffers();
				}

				LockVertexColorBuffer();
				LockTexCoordBuffer(0);
				LockTexCoordBuffer(1);

				FGLTexCoord *pTexCoord0 = m_pTexCoordArray[0];
				FGLTexCoord *pTexCoord1 = m_pTexCoordArray[1];
				FGLVertexColor *pVertexColorArray = m_pVertexColorArray;
				const FGLVertex *pSrcVertexArray = &m_csVertexArray[StartIndex];
				for (DWORD i = 0; i < NumPts; i++) {
					FLOAT U = MapDotArray[Index].u;
					FLOAT V = MapDotArray[Index].v;

					FLOAT PointZ_Times_RNearZ = pSrcVertexArray[i].z * RNearZ;
					pTexCoord0[i].u = PointZ_Times_RNearZ;
					pTexCoord0[i].v = 0.5f;
					pTexCoord1[i].u = (U - TexInfo[1].UPan) * DetailUMult;
					pTexCoord1[i].v = (V - TexInfo[1].VPan) * DetailVMult;

					pVertexColorArray[i].x = pSrcVertexArray[i].x;
					pVertexColorArray[i].y = pSrcVertexArray[i].y;
					pVertexColorArray[i].z = pSrcVertexArray[i].z;
					pVertexColorArray[i].color = detailColor | 0xFF000000;

					Index++;
				}

				UnlockVertexColorBuffer();
				UnlockTexCoordBuffer(0);
				UnlockTexCoordBuffer(1);

				m_d3dDevice->DrawPrimitive(D3DPT_TRIANGLEFAN, m_curVertexBufferPos, NumPts - 2);

				m_curVertexBufferPos += NumPts;
			}
			else if ((canClipDetailTexture == false) || (isNearBits == allPtsBits)) {
				if ((m_curVertexBufferPos + NumPts) >= VERTEX_RING_SIZE) {
					FlushVertexBuffers();
				}

				LockVertexColorBuffer();
				LockTexCoordBuffer(0);

				FGLTexCoord *pTexCoord = m_pTexCoordArray[0];
				FGLVertexColor *pVertexColorArray = m_pVertexColorArray;
				const FGLVertex *pSrcVertexArray = &m_csVertexArray[StartIndex];
				for (DWORD i = 0; i < NumPts; i++) {
					FLOAT U = MapDotArray[Index].u;
					FLOAT V = MapDotArray[Index].v;

					pTexCoord[i].u = (U - TexInfo[0].UPan) * DetailUMult;
					pTexCoord[i].v = (V - TexInfo[0].VPan) * DetailVMult;

					pVertexColorArray[i].x = pSrcVertexArray[i].x;
					pVertexColorArray[i].y = pSrcVertexArray[i].y;
					pVertexColorArray[i].z = pSrcVertexArray[i].z;
					DWORD alpha = appRound((1.0f - (Clamp(pSrcVertexArray[i].z, 0.0f, NearZ) * RNearZ)) * 255.0f);
					pVertexColorArray[i].color = detailColor | (alpha << 24);

					Index++;
				}

				UnlockVertexColorBuffer();
				UnlockTexCoordBuffer(0);

				m_d3dDevice->DrawPrimitive(D3DPT_TRIANGLEFAN, m_curVertexBufferPos, NumPts - 2);

				m_curVertexBufferPos += NumPts;
			}
			else {
				if ((m_curVertexBufferPos + (NumPts * 2)) >= VERTEX_RING_SIZE) {
					FlushVertexBuffers();
				}

				LockVertexColorBuffer();
				LockTexCoordBuffer(0);

				DWORD NextIndex = 0;
				DWORD isNear_i_bit = 1U << (NumPts - 1);
				DWORD isNear_j_bit = 1U;
				FGLTexCoord *pTexCoord = m_pTexCoordArray[0];
				//DWORD to match NumPts. NumPts >= 3 here, so NumPts - 1 cannot wrap.
				for (DWORD i = 0, j = NumPts - 1; i < NumPts; j = i++, isNear_j_bit = isNear_i_bit, isNear_i_bit >>= 1) {
					const FGLVertex &Point = m_csVertexArray[Index];
					FLOAT U = MapDotArray[Index].u;
					FLOAT V = MapDotArray[Index].v;

					if (((isNear_i_bit & isNearBits) != 0) && ((isNear_j_bit & isNearBits) == 0)) {
						const FGLVertex &PrevPoint = m_csVertexArray[StartIndex + j];
						FLOAT PrevU = MapDotArray[StartIndex + j].u;
						FLOAT PrevV = MapDotArray[StartIndex + j].v;

						FLOAT dist = PrevPoint.z - Point.z;
						FLOAT m = 1.0f;
						if (dist > 0.001f) {
							m = (NearZ - Point.z) / dist;
						}
						FGLVertexColor *pVertexColor = &m_pVertexColorArray[NextIndex];
						pVertexColor->x = (m * (PrevPoint.x - Point.x)) + Point.x;
						pVertexColor->y = (m * (PrevPoint.y - Point.y)) + Point.y;
						pVertexColor->z = NearZ;
						DWORD alpha = 0;
						pVertexColor->color = detailColor | (alpha << 24);

						pTexCoord[NextIndex].u = ((m * (PrevU - U)) + U - TexInfo[0].UPan) * DetailUMult;
						pTexCoord[NextIndex].v = ((m * (PrevV - V)) + V - TexInfo[0].VPan) * DetailVMult;

						NextIndex++;
					}

					if ((isNear_i_bit & isNearBits) != 0) {
						pTexCoord[NextIndex].u = (U - TexInfo[0].UPan) * DetailUMult;
						pTexCoord[NextIndex].v = (V - TexInfo[0].VPan) * DetailVMult;

						FGLVertexColor *pVertexColor = &m_pVertexColorArray[NextIndex];
						pVertexColor->x = Point.x;
						pVertexColor->y = Point.y;
						pVertexColor->z = Point.z;
						DWORD alpha = appRound((1.0f - (Clamp(Point.z, 0.0f, NearZ) * RNearZ)) * 255.0f);
						pVertexColor->color = detailColor | (alpha << 24);

						NextIndex++;
					}

					if (((isNear_i_bit & isNearBits) == 0) && ((isNear_j_bit & isNearBits) != 0)) {
						const FGLVertex &PrevPoint = m_csVertexArray[StartIndex + j];
						FLOAT PrevU = MapDotArray[StartIndex + j].u;
						FLOAT PrevV = MapDotArray[StartIndex + j].v;

						FLOAT dist = Point.z - PrevPoint.z;
						FLOAT m = 1.0f;
						if (dist > 0.001f) {
							m = (NearZ - PrevPoint.z) / dist;
						}
						FGLVertexColor *pVertexColor = &m_pVertexColorArray[NextIndex];
						pVertexColor->x = (m * (Point.x - PrevPoint.x)) + PrevPoint.x;
						pVertexColor->y = (m * (Point.y - PrevPoint.y)) + PrevPoint.y;
						pVertexColor->z = NearZ;
						DWORD alpha = 0;
						pVertexColor->color = detailColor | (alpha << 24);

						pTexCoord[NextIndex].u = ((m * (U - PrevU)) + PrevU - TexInfo[0].UPan) * DetailUMult;
						pTexCoord[NextIndex].v = ((m * (V - PrevV)) + PrevV - TexInfo[0].VPan) * DetailVMult;

						NextIndex++;
					}

					Index++;
				}

				UnlockVertexColorBuffer();
				UnlockTexCoordBuffer(0);

				m_d3dDevice->DrawPrimitive(D3DPT_TRIANGLEFAN, m_curVertexBufferPos, NextIndex - 2);

				m_curVertexBufferPos += NextIndex;
			}
		}
	} while (++detailPassNum < DetailMax);


	if (detailAlphaMode) {
	} else {
		SetTexEnv(0, PF_Modulated);

		if (clipDetailTexture == true) {
			SetSlopeScaleDepthBias(savedSlopeScaleDepthBias);
		}
	}

	return;
}

//At least one polygon.
void UD3D9RenderDevice::DrawDetailTexture_FP(FTextureInfo &DetailTextureInfo) {
	INT Index = 0;

	SetBlend(PF_Modulated);

	SetTextureNoPanBias(0, DetailTextureInfo, PF_Modulated);

	FLOAT vsParams[4] = { TexInfo[0].UPan, TexInfo[0].VPan, TexInfo[0].UMult, TexInfo[0].VMult };
	m_d3dDevice->SetVertexShaderConstantF(6, vsParams, 1);

	IDirect3DPixelShader9 *pixelShader = m_fpDetailTexture;
	if (DetailMax >= 2) pixelShader = m_fpDetailTextureTwoLayer;
	SetStreamState(m_oneColorVertexDecl, m_vpDetailTexture, pixelShader);

	DisableSubsequentTextures(1);

	DWORD detailColor = m_detailTextureColor4ub | 0xFF000000;
	INT *pNumPts = &MultiDrawCountArray[0];
	DWORD *pDetailTextureIsNear = DetailTextureIsNearArray;
	DWORD csPolyCount = m_csPolyCount;
	for (DWORD PolyNum = 0; PolyNum < csPolyCount; PolyNum++, pNumPts++, pDetailTextureIsNear++) {
		INT NumPts = *pNumPts;
		DWORD isNearBits = *pDetailTextureIsNear;
		INT i;

		if ((isNearBits == 0) && (NumPts <= 32)) {
			Index += NumPts;
			continue;
		}

		if ((m_curVertexBufferPos + NumPts) >= VERTEX_RING_SIZE) {
			FlushVertexBuffers();
		}

		LockVertexColorBuffer();

		const FGLVertex *pSrcVertexArray = &m_csVertexArray[Index];
		FGLVertexColor *pVertexColorArray = m_pVertexColorArray;
		for (i = 0; i < NumPts; i++) {
			pVertexColorArray[i].x = pSrcVertexArray[i].x;
			pVertexColorArray[i].y = pSrcVertexArray[i].y;
			pVertexColorArray[i].z = pSrcVertexArray[i].z;
			pVertexColorArray[i].color = detailColor;
		}

		UnlockVertexColorBuffer();

		m_d3dDevice->DrawPrimitive(D3DPT_TRIANGLEFAN, m_curVertexBufferPos, NumPts - 2);

		m_curVertexBufferPos += NumPts;

		Index += NumPts;
	}


	return;
}
