/*=============================================================================
	RenderPasses.cpp: splitting a surface's texture layers across passes.

	With fragment programs one pass carries every layer.
	Without them the layers split across the texture units.
=============================================================================*/

#include "../OpenGLDrv.h"
#include "../OpenGL.h"

void UOpenGLRenderDevice::RenderPassesExec(void) {
	guard(UOpenGLRenderDevice::RenderPassesExec);

	//Some paths use a fragment program.

	if (m_rpMasked && m_rpForceSingle && !m_rpSetDepthEqual) {
		glDepthFunc(GL_EQUAL);
		m_rpSetDepthEqual = true;
	}

	(this->*m_pRenderPassesNoCheckSetupProc)();

	//Only what changed.
	UploadComplexSurfaceStreams(UseFragmentProgram ? 0 : (DWORD)m_rpPassCount);

	m_rpTMUnits = 1;
	m_rpForceSingle = true;


	if (UseMultiDrawArrays && (m_csPolyCount > 1)) {
		glMultiDrawArraysEXT(GL_TRIANGLE_FAN, MultiDrawFirstArray, MultiDrawCountArray, m_csPolyCount);
	} else {
		for (DWORD PolyNum = 0; PolyNum < m_csPolyCount; PolyNum++) {
			glDrawArrays(GL_TRIANGLE_FAN, MultiDrawFirstArray[PolyNum], MultiDrawCountArray[PolyNum]);
		}
	}

#ifdef UTGLR_DEBUG_WORLD_WIREFRAME
	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

	SetBlend(PF_Modulated);

	if (UseMultiDrawArrays && (m_csPolyCount > 1)) {
		glMultiDrawArraysEXT(GL_TRIANGLE_FAN, MultiDrawFirstArray, MultiDrawCountArray, m_csPolyCount);
	} else {
		for (DWORD PolyNum = 0; PolyNum < m_csPolyCount; PolyNum++) {
			glDrawArrays(GL_TRIANGLE_FAN, MultiDrawFirstArray[PolyNum], MultiDrawCountArray[PolyNum]);
		}
	}

	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
#endif

	SetActiveTexUnit(0);
	//Only the fixed function path needs this.
	//Done unconditionally, because the mode can change per frame.
	InvalidateTexAttribShadows();

#if 0
{
	dbgPrintf("utglr: PassCount = %d\n", m_rpPassCount);
}
#endif
	m_rpPassCount = 0;


	unguard;
}

void UOpenGLRenderDevice::RenderPassesExec_SingleOrDualTextureAndDetailTexture(FTextureInfo &DetailTextureInfo) {
	guard(UOpenGLRenderDevice::RenderPassesExec_SingleOrDualTextureAndDetailTexture);

	//Never forced here.
	(this->*m_pRenderPassesNoCheckSetup_SingleOrDualTextureAndDetailTextureProc)(DetailTextureInfo);

	//Detail texture is always last.
	UploadComplexSurfaceStreams(UseFragmentProgram ? 0 : (DWORD)m_rpPassCount);

	if (UseMultiDrawArrays && (m_csPolyCount > 1)) {
		glMultiDrawArraysEXT(GL_TRIANGLE_FAN, MultiDrawFirstArray, MultiDrawCountArray, m_csPolyCount);
	} else {
		for (DWORD PolyNum = 0; PolyNum < m_csPolyCount; PolyNum++) {
			glDrawArrays(GL_TRIANGLE_FAN, MultiDrawFirstArray[PolyNum], MultiDrawCountArray[PolyNum]);
		}
	}

#ifdef UTGLR_DEBUG_WORLD_WIREFRAME
	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

	SetBlend(PF_Modulated);

	if (UseMultiDrawArrays && (m_csPolyCount > 1)) {
		glMultiDrawArraysEXT(GL_TRIANGLE_FAN, MultiDrawFirstArray, MultiDrawCountArray, m_csPolyCount);
	} else {
		for (DWORD PolyNum = 0; PolyNum < m_csPolyCount; PolyNum++) {
			glDrawArrays(GL_TRIANGLE_FAN, MultiDrawFirstArray[PolyNum], MultiDrawCountArray[PolyNum]);
		}
	}

	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
#endif

	SetActiveTexUnit(0);
	//Same as the single texture path.
	InvalidateTexAttribShadows();

#if 0
{
	dbgPrintf("utglr: PassCount = %d\n", m_rpPassCount);
}
#endif
	m_rpPassCount = 0;


	unguard;
}

//Requires a render pass count above zero.
/*
A modulated layer in a pass of its own already gets a 2X blend from the frame buffer so
OneXBlending drops the destination blend to 1X, and that applies only to a split off pass
because the engine's own 2X modulate on the first one has to stand, and the two cases look
identical from the poly flags alone, which is why the decision is made here where the pass
the layer landed in is still known.
*/
void UOpenGLRenderDevice::SetRenderPassBlend(void) {
	SetBlend(MultiPass.TMU[0].PolyFlags);

	if (OneXBlending && m_rpForceSingle && (MultiPass.TMU[0].PolyFlags & PF_Modulated)) {
		glBlendFunc(GL_DST_COLOR, GL_ZERO);
		m_blendStateInvalid = true;
	}
}

//Requires at least one active pass.
void UOpenGLRenderDevice::RenderPassesNoCheckSetup(void) {
	INT i;
	INT t;

	SetDefaultShaderState();

	SetColor3fv(m_complexSurfaceColor3f_1f);

	SetRenderPassBlend();

	i = 0;
	do {
		if (i != 0) {
			DWORD texBit;

			SetActiveTexUnit(i);

			EnableTexUnit(i);

			texBit = 1 << i;
			if ((m_clientTexEnableBits & texBit) == 0) {
				m_clientTexEnableBits |= texBit;

				SetClientActiveTexUnit(i);
				glEnableClientState(GL_TEXTURE_COORD_ARRAY);
			}

			SetTexEnv(i, MultiPass.TMU[i].PolyFlags);
		}

		SetTexture(i, *MultiPass.TMU[i].Info, MultiPass.TMU[i].PolyFlags, MultiPass.TMU[i].PanBias);
	} while (++i < m_rpPassCount);

	DisableSubsequentTextures(m_rpPassCount);
	DisableSubsequentClientTextures(m_rpPassCount);

	//Guards the test as well as the body.
#ifdef UTGLR_INCLUDE_SSE_CODE
	if (UseSSE) {
		t = 0;
		do {
			__m128 uvPan;
			__m128 uvMult;
			const FGLMapDot *pMapDot = &MapDotArray[0];
			FGLTexCoord *pTexCoord = &TexCoordArray[t][0];

			uvPan = _mm_setzero_ps();
			uvMult = _mm_setzero_ps();
			uvPan = _mm_loadl_pi(uvPan, (const __m64 *)&TexInfo[t].UPan);
			uvMult = _mm_loadl_pi(uvMult, (const __m64 *)&TexInfo[t].UMult);
			uvPan = _mm_movelh_ps(uvPan, uvPan);
			uvMult = _mm_movelh_ps(uvMult, uvMult);

			INT ptCounter = m_csPtCount;
			do {
				__m128 data;

				data = _mm_load_ps((const float *)pMapDot);
				data = _mm_sub_ps(data, uvPan);
				data = _mm_mul_ps(data, uvMult);
				_mm_store_ps((float *)pTexCoord, data);

				pMapDot += 2;
				pTexCoord += 2;
			} while ((ptCounter -= 2) > 0);
		} while (++t < m_rpPassCount);
	} else
#endif //UTGLR_INCLUDE_SSE_CODE
	{
		t = 0;
		do {
			FLOAT UPan = TexInfo[t].UPan;
			FLOAT VPan = TexInfo[t].VPan;
			FLOAT UMult = TexInfo[t].UMult;
			FLOAT VMult = TexInfo[t].VMult;
			const FGLMapDot *pMapDot = &MapDotArray[0];
			FGLTexCoord *pTexCoord = &TexCoordArray[t][0];

			INT ptCounter = m_csPtCount;
			do {
				pTexCoord->u = (pMapDot->u - UPan) * UMult;
				pTexCoord->v = (pMapDot->v - VPan) * VMult;

				pMapDot++;
				pTexCoord++;
			} while (--ptCounter != 0);
		} while (++t < m_rpPassCount);
	}

	return;
}

//Requires at least one active pass.
void UOpenGLRenderDevice::RenderPassesNoCheckSetup_FP(void) {
	INT i;
	GLuint fpId = 0;

	SetColor4fv(m_complexSurfaceColor3f_1f);

	SetRenderPassBlend();

	//A macro texture and a lightmap both arrive as PF_Modulated.
	//Only the lightmap wants reconstructing.
	const bool recon = m_sevenBitMapRecon;

	if (UseFragmentProgram) {
		if (m_rpPassCount == 1) {
			fpId = m_fpComplexSurfaceSingleTexture;
		} else if (m_rpPassCount == 2) {
			if (MultiPass.TMU[1].PolyFlags == PF_Modulated) {
				fpId = (recon && MultiPass.TMU[1].bSevenBitMap)
					? m_fpComplexSurfaceDualTextureLightRecon
					: m_fpComplexSurfaceDualTextureModulated;
			} else if (MultiPass.TMU[1].PolyFlags == PF_Highlighted) {
				fpId = recon
					? m_fpComplexSurfaceSingleTextureFogRecon
					: m_fpComplexSurfaceSingleTextureWithFog;
			}
		} else if (m_rpPassCount == 3) {
			if (MultiPass.TMU[2].PolyFlags == PF_Modulated) {
				//Three modulated layers can only be diffuse, macro and lightmap.
				//Asked, so reordering cannot change it.
				//The flag travels with the layer.
				fpId = (recon && MultiPass.TMU[2].bSevenBitMap)
					? m_fpComplexSurfaceTripleTextureLightRecon
					: m_fpComplexSurfaceTripleTextureModulated;
			} else if (MultiPass.TMU[2].PolyFlags == PF_Highlighted) {
				if (!recon) {
					fpId = m_fpComplexSurfaceDualTextureModulatedWithFog;
				} else if (MultiPass.TMU[1].bSevenBitMap) {
					fpId = m_fpComplexSurfaceDualTextureLightFogRecon;
				} else {
					fpId = m_fpComplexSurfaceDualTextureMacroFogRecon;
				}
			}
		} else if (m_rpPassCount == 4) {
			if (MultiPass.TMU[3].PolyFlags == PF_Highlighted) {
				//Diffuse, macro, lightmap, fog.
				fpId = (recon && MultiPass.TMU[2].bSevenBitMap)
					? m_fpComplexSurfaceTripleTextureLightFogRecon
					: m_fpComplexSurfaceTripleTextureModulatedWithFog;
			}
		}
	}

	//The fallback path.
	if (fpId == 0) {
		fpId = m_fpComplexSurfaceSingleTexture;
	}
	SetShaderState(m_vpComplexSurface[m_rpPassCount - 1], fpId);

	i = 0;
	do {
		if (i != 0) {
			SetActiveTexUnit(i);

			EnableTexUnit(i);
		}

		SetTexture(i, *MultiPass.TMU[i].Info, MultiPass.TMU[i].PolyFlags, MultiPass.TMU[i].PanBias);

		SetTexAttrib(i);
		/*
		After the bind, which is what settles the size, and for every layer, because the program
		divides by the width to take its taps back to normalized coordinates and an unwritten
		parameter puts those taps at infinity with everything downstream at NaN, so they all get
		filled against a surface that would otherwise render as a black hole on some path nobody
		thought to check.
		*/
		if (recon && (i != 0)) {
			SetSevenBitMapSize(i);
		}
	} while (++i < m_rpPassCount);

	DisableSubsequentTextures(m_rpPassCount);
	DisableSubsequentClientTextures(0);

	return;
}

//Requires at least one active pass.
void UOpenGLRenderDevice::RenderPassesNoCheckSetup_SingleOrDualTextureAndDetailTexture(FTextureInfo &DetailTextureInfo) {
	INT i;
	INT t;
	FLOAT NearZ = 380.0f;
	FLOAT RNearZ = 1.0f / NearZ;

	//Two extra texture units for detail texture.
	m_rpPassCount += 2;

	SetDefaultShaderState();

	SetColor3fv(m_detailTextureColor3f_1f);

	SetBlend(MultiPass.TMU[0].PolyFlags);

	//Surface texture must be 2X blended.
	MultiPass.TMU[0].PolyFlags |= (PF_Modulated | PF_FlatShaded);

	//Detail texture uses the first two units.
	i = 2;
	do {
		DWORD texBit;

		SetActiveTexUnit(i);

		EnableTexUnit(i);

		texBit = 1 << i;
		if ((m_clientTexEnableBits & texBit) == 0) {
			m_clientTexEnableBits |= texBit;

			SetClientActiveTexUnit(i);
			glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		}

		SetTexEnv(i, MultiPass.TMU[i - 2].PolyFlags);

		SetTexture(i, *MultiPass.TMU[i - 2].Info, MultiPass.TMU[i - 2].PolyFlags, MultiPass.TMU[i - 2].PanBias);
	} while (++i < m_rpPassCount);

	SetActiveTexUnit(0);
	SetAlphaTexture(0);
	{
		EnableTexUnit(0);

		DWORD texBit = 1 << 0;
		if ((m_clientTexEnableBits & texBit) == 0) {
			m_clientTexEnableBits |= texBit;

			SetClientActiveTexUnit(0);
			glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		}
	}

	SetActiveTexUnit(1);
	SetTexEnv(1, PF_Memorized);
	SetTextureNoPanBias(1, DetailTextureInfo, PF_Modulated);
	{
		EnableTexUnit(1);

		DWORD texBit = 1 << 1;
		if ((m_clientTexEnableBits & texBit) == 0) {
			m_clientTexEnableBits |= texBit;

			SetClientActiveTexUnit(1);
			glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		}
	}

	DisableSubsequentTextures(m_rpPassCount);
	DisableSubsequentClientTextures(m_rpPassCount);

	//Alpha texture for detail texture uses unit 0.
	{
		const FGLVertex *pVertex = &VertexArray[0];
		FGLTexCoord *pTexCoord = &TexCoordArray[0][0];

		INT ptCounter = m_csPtCount;
		do {
			pTexCoord->u = pVertex->z * RNearZ;
			pTexCoord->v = 0.5f;

			pVertex++;
			pTexCoord++;
		} while (--ptCounter != 0);
	}
	//Detail texture in unit 1.
	//Same guard as above.
#ifdef UTGLR_INCLUDE_SSE_CODE
	if (UseSSE) {
		t = 1;
		do {
			__m128 uvPan;
			__m128 uvMult;
			const FGLMapDot *pMapDot = &MapDotArray[0];
			FGLTexCoord *pTexCoord = &TexCoordArray[t][0];

			uvPan = _mm_setzero_ps();
			uvMult = _mm_setzero_ps();
			uvPan = _mm_loadl_pi(uvPan, (const __m64 *)&TexInfo[t].UPan);
			uvMult = _mm_loadl_pi(uvMult, (const __m64 *)&TexInfo[t].UMult);
			uvPan = _mm_movelh_ps(uvPan, uvPan);
			uvMult = _mm_movelh_ps(uvMult, uvMult);

			INT ptCounter = m_csPtCount;
			do {
				__m128 data;

				data = _mm_load_ps((const float *)pMapDot);
				data = _mm_sub_ps(data, uvPan);
				data = _mm_mul_ps(data, uvMult);
				_mm_store_ps((float *)pTexCoord, data);

				pMapDot += 2;
				pTexCoord += 2;
			} while ((ptCounter -= 2) > 0);
		} while (++t < m_rpPassCount);
	} else
#endif //UTGLR_INCLUDE_SSE_CODE
	{
		t = 1;
		do {
			FLOAT UPan = TexInfo[t].UPan;
			FLOAT VPan = TexInfo[t].VPan;
			FLOAT UMult = TexInfo[t].UMult;
			FLOAT VMult = TexInfo[t].VMult;
			const FGLMapDot *pMapDot = &MapDotArray[0];
			FGLTexCoord *pTexCoord = &TexCoordArray[t][0];

			INT ptCounter = m_csPtCount;
			do {
				pTexCoord->u = (pMapDot->u - UPan) * UMult;
				pTexCoord->v = (pMapDot->v - VPan) * VMult;

				pMapDot++;
				pTexCoord++;
			} while (--ptCounter != 0);
		} while (++t < m_rpPassCount);
	}

	return;
}

//Requires at least one active pass.
void UOpenGLRenderDevice::RenderPassesNoCheckSetup_SingleOrDualTextureAndDetailTexture_FP(FTextureInfo &DetailTextureInfo) {
	INT i;
	DWORD detailTexUnit;
	GLuint vpId = 0;
	GLuint fpId = 0;

	//One extra texture unit for detail texture.
	m_rpPassCount += 1;

	detailTexUnit = (m_rpPassCount - 1);

	if (m_rpPassCount == 2) {
		vpId = m_vpComplexSurfaceDualTextureWithPos;
	} else {
		vpId = m_vpComplexSurfaceTripleTextureWithPos;
	}
	//A surface with a fog map never gets a detail pass.
	//The flag settles which kind of layer this is.
	const bool reconLight = m_sevenBitMapRecon && (m_rpPassCount == 3) && MultiPass.TMU[1].bSevenBitMap;

	if (DetailMax >= 2) {
		if (m_rpPassCount == 2) {
			fpId = m_fpSingleTextureAndDetailTextureTwoLayer;
		} else {
			fpId = reconLight
				? m_fpDualTextureAndDetailTextureTwoLayerLightRecon
				: m_fpDualTextureAndDetailTextureTwoLayer;
		}
	} else {
		if (m_rpPassCount == 2) {
			fpId = m_fpSingleTextureAndDetailTexture;
		} else {
			fpId = reconLight
				? m_fpDualTextureAndDetailTextureLightRecon
				: m_fpDualTextureAndDetailTexture;
		}
	}
	SetShaderState(vpId, fpId);

	SetColor4fv(m_detailTextureColor3f_1f);

	SetBlend(MultiPass.TMU[0].PolyFlags);

	//First one or two textures in the first two units.
	i = 0;
	do {
		if (i != 0) {
			SetActiveTexUnit(i);

			EnableTexUnit(i);

			//Only works when the poly flags are modulated.
		}

		SetTexture(i, *MultiPass.TMU[i].Info, MultiPass.TMU[i].PolyFlags, MultiPass.TMU[i].PanBias);

		SetTexAttrib(i);
		//As in RenderPassesNoCheckSetup_FP.
		if (m_sevenBitMapRecon && (i != 0)) {
			SetSevenBitMapSize(i);
		}
	} while (++i < (INT)detailTexUnit);

	//Detail texture in the second or third unit.
	SetActiveTexUnit(detailTexUnit);
	SetTextureNoPanBias(detailTexUnit, DetailTextureInfo, PF_Modulated);
	EnableTexUnit(detailTexUnit);
	SetTexAttrib(detailTexUnit);

	DisableSubsequentTextures(m_rpPassCount);
	DisableSubsequentClientTextures(0);

	return;
}
