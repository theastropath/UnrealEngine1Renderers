/*=============================================================================
	RenderState.cpp: the tracked GL state and its no-check setters.
=============================================================================*/

#include "../OpenGLDrv.h"
#include "../OpenGL.h"
#include "PolygonOffset.h"

/*
The renderer alternates the depth test and depth range between frames so which sign moves
a surface away from the camera flips with it, which is why the offsets are signed here
once and not at each of the call sites that use them, and getting it wrong shows up as a
surface that flickers on alternate frames, which is a good deal harder to recognise for
what it is.
*/
void UOpenGLRenderDevice::SetPolygonOffsetsForDepthDirection(bool reversed) {
	//Towards the camera for the detail layer, away for a mover.
	const FLOAT towardsSign = reversed ? 1.0f : -1.0f;

	m_detailPolyOffsetFactor = towardsSign * UTGLR_DETAIL_POLY_OFFSET_FACTOR;
	m_detailPolyOffsetUnits = towardsSign * UTGLR_DETAIL_POLY_OFFSET_UNITS;
	m_moverPolyOffsetFactor = -towardsSign * UTGLR_MOVER_POLY_OFFSET_FACTOR;
	m_moverPolyOffsetUnits = -towardsSign * UTGLR_MOVER_POLY_OFFSET_UNITS;

	//Left installed for a pass that enables offsets without naming one.
	SetPolygonOffset(m_detailPolyOffsetFactor, m_detailPolyOffsetUnits);
}

void UOpenGLRenderDevice::SetBlendNoCheck(DWORD blendFlags) {
	guardSlow(UOpenGLRenderDevice::SetBlend);

	//All-ones marks everything changed.
	DWORD Xor = m_blendStateInvalid ? 0xFFFFFFFFu : (m_curBlendFlags ^ blendFlags);

	//Zero while invalid, so the enable below is issued.
	DWORD curBlendFlags = m_blendStateInvalid ? 0 : m_curBlendFlags;

	m_blendStateInvalid = false;

	m_curBlendFlags = blendFlags;

#ifdef UTGLR_RUNE_BUILD
	const DWORD GL_BLEND_FLAG_BITS = PF_Translucent | PF_Modulated | PF_Highlighted | PF_AlphaBlend;
#elif defined(UTGLR_UNREAL_227_BUILD)
	const DWORD GL_BLEND_FLAG_BITS = PF_Translucent | PF_Modulated | PF_Highlighted | PF_AlphaBlend;
#else
	const DWORD GL_BLEND_FLAG_BITS = PF_Translucent | PF_Modulated | PF_Highlighted;
#endif
	DWORD relevantBlendFlagBits = GL_BLEND_FLAG_BITS | m_smoothMaskedTexturesBit;
	if (Xor & (relevantBlendFlagBits)) {
		if (!(blendFlags & (relevantBlendFlagBits))) {
			glDisable(GL_BLEND);
		} else {
			if (!(curBlendFlags & (relevantBlendFlagBits))) {
				glEnable(GL_BLEND);
			}
			if (blendFlags & PF_Translucent) {
				glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_COLOR);
			} else if (blendFlags & PF_Modulated) {
				glBlendFunc(GL_DST_COLOR, GL_SRC_COLOR);
			} else if (blendFlags & PF_Highlighted) {
				glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
			}
#ifdef UTGLR_RUNE_BUILD
			else if (blendFlags & PF_AlphaBlend) {
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			}
#elif defined(UTGLR_UNREAL_227_BUILD)
			else if (blendFlags & PF_AlphaBlend) {
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			}
#endif
			else if (blendFlags & PF_Masked) {
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			}
		}
	}
#ifdef UTGLR_UNREAL_227_BUILD
	if (Xor & (PF_Masked | PF_AlphaBlend)) {
#else
	if (Xor & PF_Masked) {
#endif
		if (blendFlags & PF_Masked) {
#ifdef UTGLR_UNREAL_227_BUILD
			glAlphaFunc(GL_GREATER, 0.5f);
			glEnable(GL_ALPHA_TEST);
			if (m_useAlphaToCoverageForMasked) {
				m_alphaToCoverageEnabled = true;
				glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE);
			}
		} else if (blendFlags & PF_AlphaBlend) {
			glAlphaFunc(GL_GREATER, 0.01f);
#endif
			glEnable(GL_ALPHA_TEST);
			if (m_useAlphaToCoverageForMasked) {
				m_alphaToCoverageEnabled = true;
				glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE);
			}
		} else {
			glDisable(GL_ALPHA_TEST);
			if (m_useAlphaToCoverageForMasked) {
				m_alphaToCoverageEnabled = false;
				glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);
			}
		}
	}
	if (Xor & PF_Invisible) {
		UBOOL flag = ((blendFlags & PF_Invisible) == 0) ? GL_TRUE : GL_FALSE;
		glColorMask(flag, flag, flag, flag);
	}
	if (Xor & PF_Occlude) {
		UBOOL flag = ((blendFlags & PF_Occlude) == 0) ? GL_FALSE : GL_TRUE;
		glDepthMask(flag);
	}

	unguardSlow;
}

//Both, on one path.
void UOpenGLRenderDevice::InitOrInvalidateTexEnvState(void) {
	INT TMU;

	//Cheap enough unconditionally.
	for (TMU = 0; TMU < MAX_TMUNITS; TMU++) {
		m_curTexEnvFlags[TMU] = 0;
	}

	SetActiveTexUnit(0);
	SetTexEnv(0, PF_Modulated);

	return;
}

void UOpenGLRenderDevice::SetPermanentTexEnvState(INT TMUnits) {
	INT TMU;

	for (TMU = 0; TMU < TMUnits; TMU++) {
		SetActiveTexUnit(TMU);

		if (SUPPORTS_GL_EXT_texture_env_combine) {
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_EXT, GL_TEXTURE);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE2_RGB_EXT, GL_PREVIOUS_EXT);

			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA_EXT, GL_TEXTURE);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_ALPHA_EXT, GL_PREVIOUS_EXT);
		}
	}
	SetActiveTexUnit(0);

	return;
}

void UOpenGLRenderDevice::SetTexLODBiasState(INT TMUnits) {
	INT TMU;

	for (TMU = 0; TMU < TMUnits; TMU++) {
		SetActiveTexUnit(TMU);

		glTexEnvf(GL_TEXTURE_FILTER_CONTROL_EXT, GL_TEXTURE_LOD_BIAS_EXT, LODBias);
	}
	SetActiveTexUnit(0);

	return;
}

void UOpenGLRenderDevice::SetTexEnvNoCheck(DWORD texUnit, DWORD texEnvFlags) {
	guardSlow(UOpenGLRenderDevice::SetTexEnv);

	//Updated early, since nothing later depends on it.
	m_curTexEnvFlags[texUnit] = texEnvFlags;

	if (texEnvFlags & PF_Modulated) {
		if ((texEnvFlags & PF_FlatShaded) || ((texUnit != 0) && !OneXBlending)) {
			glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_EXT);

			glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_EXT, GL_MODULATE);
			glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA_EXT, GL_MODULATE);

			glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB_EXT, GL_SRC_COLOR);

			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_EXT, GL_TEXTURE);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_EXT, GL_PREVIOUS_EXT);

			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA_EXT, GL_TEXTURE);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_ALPHA_EXT, GL_PREVIOUS_EXT);

			glTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE_EXT, 2.0f);
		} else {
			glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		}
	} else if (texEnvFlags & PF_Memorized) {
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_EXT);

		glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_EXT, GL_INTERPOLATE_EXT);
		glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA_EXT, GL_REPLACE);

		glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB_EXT, GL_SRC_COLOR);
		glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND2_RGB_EXT, GL_SRC_ALPHA);

		glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_EXT, GL_TEXTURE);
		glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_EXT, GL_PRIMARY_COLOR_EXT);
		glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE2_RGB_EXT, GL_PREVIOUS_EXT);

		glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA_EXT, GL_TEXTURE);

		glTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE_EXT, 1.0f);
	} else if (texEnvFlags & PF_Highlighted) {
		if (SUPPORTS_GL_ATI_texture_env_combine3) {
			glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_EXT);

			glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_EXT, GL_MODULATE_ADD_ATI);
			glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA_EXT, GL_REPLACE);

			glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB_EXT, GL_SRC_COLOR);
			glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND2_RGB_EXT, GL_ONE_MINUS_SRC_ALPHA);

			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_EXT, GL_PREVIOUS_EXT);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_EXT, GL_TEXTURE);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE2_RGB_EXT, GL_TEXTURE);

			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA_EXT, GL_PREVIOUS_EXT);

			glTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE_EXT, 1.0f);
		} else {
			glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE4_NV);

			glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_EXT, GL_ADD);
			glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA_EXT, GL_ADD);

			glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB_EXT, GL_ONE_MINUS_SRC_ALPHA);
			glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND2_RGB_EXT, GL_SRC_COLOR);

			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_EXT, GL_PREVIOUS_EXT);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_EXT, GL_TEXTURE);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE2_RGB_EXT, GL_TEXTURE);

			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA_EXT, GL_ZERO);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_ALPHA_EXT, GL_PREVIOUS_EXT);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE2_ALPHA_EXT, GL_PREVIOUS_EXT);

			glTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE_EXT, 1.0f);
		}
	}

	unguardSlow;
}


void UOpenGLRenderDevice::SetVertexProgramNoCheck(GLuint vpId) {
	//Zero means no program bound.
	if (vpId == 0) {
		if (m_vpCurrent != 0) {
			m_vpCurrent = 0;

			glDisable(GL_VERTEX_PROGRAM_ARB);
		}

		return;
	}

	if (m_vpCurrent == 0) {
		glEnable(GL_VERTEX_PROGRAM_ARB);

		m_vpEnableCount++;
	}

	m_vpCurrent = vpId;

	glBindProgramARB(GL_VERTEX_PROGRAM_ARB, vpId);

	m_vpSwitchCount++;

	return;
}

void UOpenGLRenderDevice::SetFragmentProgramNoCheck(GLuint fpId) {
	//Same as above.
	if (fpId == 0) {
		if (m_fpCurrent != 0) {
			m_fpCurrent = 0;

			glDisable(GL_FRAGMENT_PROGRAM_ARB);

			//Cleared first.
			ResyncTextureEnables();
		}

		return;
	}

	if (m_fpCurrent == 0) {
		glEnable(GL_FRAGMENT_PROGRAM_ARB);

		m_fpEnableCount++;
	}

	m_fpCurrent = fpId;

	glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, fpId);

	m_fpSwitchCount++;

	return;
}

//Only called when leaving fragment program mode.
//While a program is bound the enable bits are ignored.
//Bounded by the active unit count.
void UOpenGLRenderDevice::ResyncTextureEnables(void) {
	const DWORD numUnits = (TMUnits > 0) ? (DWORD)TMUnits : 1;

	for (DWORD texUnit = 0; texUnit < numUnits; texUnit++) {
		SetActiveTexUnit(texUnit);
		if (m_texEnableBits & (1U << texUnit)) {
			glEnable(GL_TEXTURE_2D);
		} else {
			glDisable(GL_TEXTURE_2D);
		}
	}

	//The resting state.
	SetActiveTexUnit(0);

	return;
}


void UOpenGLRenderDevice::SetDefaultColorStateNoCheck(void) {
	if (m_currentColorFlags & CF_COLOR_SUM) {
		glDisable(GL_COLOR_SUM_EXT);
	}

	if (m_currentColorFlags & CF_DUAL_COLOR_ARRAY) {
		glDisableClientState(GL_SECONDARY_COLOR_ARRAY_EXT);

		m_colorStreamIsDouble = false;
		SetColorArrayPointer();
	}

	if (m_currentColorFlags & CF_COLOR_ARRAY) {
		glDisableClientState(GL_COLOR_ARRAY);
	}

	m_currentColorFlags = 0;

	return;
}

void UOpenGLRenderDevice::SetColorStateNoCheck(void) {
	BYTE changedFlags;

	changedFlags = m_requestedColorFlags ^ m_currentColorFlags;

	if (changedFlags & CF_COLOR_SUM) {
		if (m_requestedColorFlags & CF_COLOR_SUM) {
			glEnable(GL_COLOR_SUM_EXT);
		} else {
			glDisable(GL_COLOR_SUM_EXT);
		}
	}

	if (changedFlags & CF_DUAL_COLOR_ARRAY) {
		if (m_requestedColorFlags & CF_DUAL_COLOR_ARRAY) {
			glEnableClientState(GL_SECONDARY_COLOR_ARRAY_EXT);

			m_colorStreamIsDouble = true;
			SetColorArrayPointer();
		} else {
			glDisableClientState(GL_SECONDARY_COLOR_ARRAY_EXT);

			m_colorStreamIsDouble = false;
			SetColorArrayPointer();
		}
	}

	if (changedFlags & CF_COLOR_ARRAY) {
		if (m_requestedColorFlags & CF_COLOR_ARRAY) {
			glEnableClientState(GL_COLOR_ARRAY);
		} else {
			glDisableClientState(GL_COLOR_ARRAY);
		}
	}

	m_currentColorFlags = m_requestedColorFlags;

	return;
}


void UOpenGLRenderDevice::SetAAStateNoCheck(bool AAEnable) {
	m_curAAEnable = AAEnable;

	m_AASwitchCount++;

	if (AAEnable) {
		glEnable(GL_MULTISAMPLE_ARB);
	} else {
		glDisable(GL_MULTISAMPLE_ARB);
	}

	return;
}
