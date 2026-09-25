/*=============================================================================
	RenderState.cpp: the device state this renderer tracks so it need not re-set it.

	Every setter here is the no-check half of a pair whose other half is in D3D9.h.
=============================================================================*/

#include "../D3D9Drv.h"
#include "../D3D9.h"

void UD3D9RenderDevice::EnableAlphaToCoverageNoCheck(void) {
	if (m_isATI) {
		m_d3dDevice->SetRenderState(D3DRS_POINTSIZE, MAKEFOURCC('A', '2', 'M', '1'));
	} else if (m_isNVIDIA) {
		m_d3dDevice->SetRenderState(D3DRS_ADAPTIVETESS_Y, (D3DFORMAT)MAKEFOURCC('A', 'T', 'O', 'C'));
	}

	return;
}

void UD3D9RenderDevice::DisableAlphaToCoverageNoCheck(void) {
	if (m_isATI) {
		m_d3dDevice->SetRenderState(D3DRS_POINTSIZE, MAKEFOURCC('A', '2', 'M', '0'));
	} else if (m_isNVIDIA) {
		m_d3dDevice->SetRenderState(D3DRS_ADAPTIVETESS_Y, D3DFMT_UNKNOWN);
	}

	return;
}

void UD3D9RenderDevice::SetBlendNoCheck(DWORD blendFlags) {
	guardSlow(UD3D9RenderDevice::SetBlend);

	//Every group below is gated on this difference. All-ones marks every group as changed.
	DWORD Xor = m_blendStateInvalid ? 0xFFFFFFFFu : (m_curBlendFlags ^ blendFlags);
	m_blendStateInvalid = false;

	m_curBlendFlags = blendFlags;

#ifdef UTGLR_RUNE_BUILD
	const DWORD GL_BLEND_FLAG_BITS = PF_Translucent | PF_Modulated | PF_Highlighted | PF_AlphaBlend;
#else
	const DWORD GL_BLEND_FLAG_BITS = PF_Translucent | PF_Modulated | PF_Highlighted;
#endif
	DWORD relevantBlendFlagBits = GL_BLEND_FLAG_BITS | m_smoothMaskedTexturesBit;
	if (Xor & (relevantBlendFlagBits)) {
		if (!(blendFlags & (relevantBlendFlagBits))) {
			m_d3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
			m_d3dDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
			m_d3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ZERO);
		} else {
			if (blendFlags & PF_Translucent) {
				m_d3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
				m_d3dDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
				m_d3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCCOLOR);
			} else if (blendFlags & PF_Modulated) {
				m_d3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
				m_d3dDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_DESTCOLOR);
				m_d3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_SRCCOLOR);
			} else if (blendFlags & PF_Highlighted) {
				m_d3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
				m_d3dDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
				m_d3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
			}
#ifdef UTGLR_RUNE_BUILD
			else if (blendFlags & PF_AlphaBlend) {
				m_d3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
				m_d3dDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
				m_d3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
			}
#endif
			else if (blendFlags & PF_Masked) {
				m_d3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
				m_d3dDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
				m_d3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
			}
		}
	}
	if (Xor & PF_Masked) {
		if (blendFlags & PF_Masked) {
			//Enable alpha test with alpha ref of D3D9 version of 0.5
			m_fsBlendInfo[0] = (127.0f / 255.0f) + 1e-6f;

			if (UseFragmentProgram) {
				m_d3dDevice->SetPixelShaderConstantF(0, m_fsBlendInfo, 1);
			} else {
				m_alphaTestEnabled = true;
				m_d3dDevice->SetRenderState(D3DRS_ALPHATESTENABLE, TRUE);
			}

			if (m_useAlphaToCoverageForMasked) {
				m_alphaToCoverageEnabled = true;
				EnableAlphaToCoverageNoCheck();
			}
		} else {
			m_fsBlendInfo[0] = 0.0f;

			if (UseFragmentProgram) {
				m_d3dDevice->SetPixelShaderConstantF(0, m_fsBlendInfo, 1);
			} else {
				m_alphaTestEnabled = false;
				m_d3dDevice->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
			}

			if (m_useAlphaToCoverageForMasked) {
				m_alphaToCoverageEnabled = false;
				DisableAlphaToCoverageNoCheck();
			}
		}
	}
	if (Xor & PF_Invisible) {
		DWORD colorEnableBits = ((blendFlags & PF_Invisible) == 0) ? D3DCOLORWRITEENABLE_ALPHA | D3DCOLORWRITEENABLE_BLUE | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_RED : 0;
		m_d3dDevice->SetRenderState(D3DRS_COLORWRITEENABLE, colorEnableBits);
	}
	if (Xor & PF_Occlude) {
		DWORD flag = ((blendFlags & PF_Occlude) == 0) ? FALSE : TRUE;
		m_d3dDevice->SetRenderState(D3DRS_ZWRITEENABLE, flag);
	}
	if (Xor & PF_RenderFog) {
		DWORD flag = ((blendFlags & PF_RenderFog) == 0) ? FALSE : TRUE;
		m_d3dDevice->SetRenderState(D3DRS_SPECULARENABLE, flag);
	}

	unguardSlow;
}

//Initializes or invalidates.
void UD3D9RenderDevice::InitOrInvalidateTexEnvState(void) {
	INT TMU;

	//Flags for all units are cleared either way. On initialization the first unit defaults to modulated.
	for (TMU = 0; TMU < MAX_TMUNITS; TMU++) {
		m_curTexEnvFlags[TMU] = 0;
	}

	SetTexEnv(0, PF_Modulated);

	return;
}

//Covers every stage anything can bind to.
//The single pass detail path reaches stages up to its own pass count.
void UD3D9RenderDevice::SetTexLODBiasState(INT TMUnits) {
	INT TMU;

	for (TMU = 0; TMU < TMUnits; TMU++) {
		float fParam;

		fParam = LODBias;
		m_d3dDevice->SetSamplerState(TMU, D3DSAMP_MIPMAPLODBIAS, *(DWORD *)&fParam);
	}

	return;
}

void UD3D9RenderDevice::SetTexMaxAnisotropyState(INT TMUnits) {
	INT TMU;

	for (TMU = 0; TMU < TMUnits; TMU++) {
		m_d3dDevice->SetSamplerState(TMU, D3DSAMP_MAXANISOTROPY, MaxAnisotropy);
	}

	return;
}

void UD3D9RenderDevice::SetTexEnvNoCheck(DWORD texUnit, DWORD texEnvFlags) {
	guardSlow(UD3D9RenderDevice::SetTexEnv);

	//No later dependencies.
	m_curTexEnvFlags[texUnit] = texEnvFlags;

	m_texEnableBits |= 1U << texUnit;

	if (texEnvFlags & PF_Modulated) {
		D3DTEXTUREOP texOp;

		//Parenthesised as it parses and as it is meant: 2X modulate when flat shaded.
		if ((texEnvFlags & PF_FlatShaded) || ((texUnit != 0) && !OneXBlending)) {
			texOp = D3DTOP_MODULATE2X;
		} else {
			texOp = D3DTOP_MODULATE;
		}

		m_d3dDevice->SetTextureStageState(texUnit, D3DTSS_COLOROP, texOp);
		m_d3dDevice->SetTextureStageState(texUnit, D3DTSS_ALPHAOP, D3DTOP_MODULATE);

		//		m_d3dDevice->SetTextureStageState(texUnit, D3DTSS_COLORARG1, D3DTA_TEXTURE);
		m_d3dDevice->SetTextureStageState(texUnit, D3DTSS_COLORARG2, D3DTA_CURRENT);
	} else if (texEnvFlags & PF_Memorized) {
		m_d3dDevice->SetTextureStageState(texUnit, D3DTSS_COLOROP, D3DTOP_BLENDCURRENTALPHA);
		m_d3dDevice->SetTextureStageState(texUnit, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);

		//		m_d3dDevice->SetTextureStageState(texUnit, D3DTSS_COLORARG1, D3DTA_TEXTURE);
		m_d3dDevice->SetTextureStageState(texUnit, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
	} else if (texEnvFlags & PF_Highlighted) {
		m_d3dDevice->SetTextureStageState(texUnit, D3DTSS_COLOROP, D3DTOP_MODULATEINVALPHA_ADDCOLOR);
		m_d3dDevice->SetTextureStageState(texUnit, D3DTSS_ALPHAOP, D3DTOP_SELECTARG2);

		//		m_d3dDevice->SetTextureStageState(texUnit, D3DTSS_COLORARG1, D3DTA_TEXTURE);
		m_d3dDevice->SetTextureStageState(texUnit, D3DTSS_COLORARG2, D3DTA_CURRENT);
	}

	unguardSlow;
}


void UD3D9RenderDevice::SetTexFilterNoCheck(DWORD texNum, BYTE texFilterParams) {
	guardSlow(UD3D9RenderDevice::SetTexFilter);

	BYTE texFilterParamsXor = m_curTexStageParams[texNum].filter ^ texFilterParams;

	m_curTexStageParams[texNum].filter = texFilterParams;

	if (texFilterParamsXor & CT_MIN_FILTER_MASK) {
		D3DTEXTUREFILTERTYPE texFilterType = D3DTEXF_POINT;

		switch (texFilterParams & CT_MIN_FILTER_MASK) {
			case CT_MIN_FILTER_POINT: texFilterType = D3DTEXF_POINT; break;
			case CT_MIN_FILTER_LINEAR: texFilterType = D3DTEXF_LINEAR; break;
			case CT_MIN_FILTER_ANISOTROPIC: texFilterType = D3DTEXF_ANISOTROPIC; break;
			default:;
		}

		m_d3dDevice->SetSamplerState(texNum, D3DSAMP_MINFILTER, texFilterType);
	}
	if (texFilterParamsXor & CT_MIP_FILTER_MASK) {
		D3DTEXTUREFILTERTYPE texFilterType = D3DTEXF_NONE;

		switch (texFilterParams & CT_MIP_FILTER_MASK) {
			case CT_MIP_FILTER_NONE: texFilterType = D3DTEXF_NONE; break;
			case CT_MIP_FILTER_POINT: texFilterType = D3DTEXF_POINT; break;
			case CT_MIP_FILTER_LINEAR: texFilterType = D3DTEXF_LINEAR; break;
			default:;
		}

		m_d3dDevice->SetSamplerState(texNum, D3DSAMP_MIPFILTER, texFilterType);
	}
	//The mag filter's anisotropic mode depends on the min filter.
	//A change to either reprograms both.
	if (texFilterParamsXor & (CT_MAG_FILTER_LINEAR_NOT_POINT_BIT | CT_MIN_FILTER_MASK)) {
		D3DTEXTUREFILTERTYPE texFilterType = D3DTEXF_POINT;

		if (texFilterParams & CT_MAG_FILTER_LINEAR_NOT_POINT_BIT) {
			bool wantAnisotropic = m_anisotropicMagFilterCap &&
				((texFilterParams & CT_MIN_FILTER_MASK) == CT_MIN_FILTER_ANISOTROPIC);
			texFilterType = wantAnisotropic ? D3DTEXF_ANISOTROPIC : D3DTEXF_LINEAR;
		}

		m_d3dDevice->SetSamplerState(texNum, D3DSAMP_MAGFILTER, texFilterType);
	}
	if (texFilterParamsXor & CT_ADDRESS_CLAMP_NOT_WRAP_BIT) {
		D3DTEXTUREADDRESS texAddressMode = (texFilterParams & CT_ADDRESS_CLAMP_NOT_WRAP_BIT) ? D3DTADDRESS_CLAMP : D3DTADDRESS_WRAP;
		m_d3dDevice->SetSamplerState(texNum, D3DSAMP_ADDRESSU, texAddressMode);
		m_d3dDevice->SetSamplerState(texNum, D3DSAMP_ADDRESSV, texAddressMode);
	}

	unguardSlow;
}


void UD3D9RenderDevice::SetVertexDeclNoCheck(IDirect3DVertexDeclaration9 *vertexDecl) {
	HRESULT hResult;

	hResult = m_d3dDevice->SetVertexDeclaration(vertexDecl);
	if (FAILED(hResult)) {
		appErrorf(TEXT("SetVertexDeclaration failed"));
	}

	m_curVertexDecl = vertexDecl;

	return;
}

void UD3D9RenderDevice::SetVertexShaderNoCheck(IDirect3DVertexShader9 *vertexShader) {
	HRESULT hResult;

	hResult = m_d3dDevice->SetVertexShader(vertexShader);
	if (FAILED(hResult)) {
		appErrorf(TEXT("SetVertexShader failed"));
	}

	m_vpSwitchCount++;
	if ((vertexShader != NULL) && (m_curVertexShader == NULL)) m_vpEnableCount++;

	m_curVertexShader = vertexShader;

	return;
}

void UD3D9RenderDevice::SetPixelShaderNoCheck(IDirect3DPixelShader9 *pixelShader) {
	HRESULT hResult;

	hResult = m_d3dDevice->SetPixelShader(pixelShader);
	if (FAILED(hResult)) {
		appErrorf(TEXT("SetPixelShader failed"));
	}

	m_fpSwitchCount++;
	if ((pixelShader != NULL) && (m_curPixelShader == NULL)) m_fpEnableCount++;

	m_curPixelShader = pixelShader;

	return;
}


void UD3D9RenderDevice::SetAAStateNoCheck(bool AAEnable) {
	m_curAAEnable = AAEnable;

	m_AASwitchCount++;

	m_d3dDevice->SetRenderState(D3DRS_MULTISAMPLEANTIALIAS, (AAEnable) ? TRUE : FALSE);

	return;
}
