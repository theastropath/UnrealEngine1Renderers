/*=============================================================================
	Gamma.cpp: brightness, as a ramp and as a post-present pass.

	The display adapter's gamma ramp is system wide and outlives the process that sets
	it, so where the hardware allows, a pixel shader corrects the finished frame.
=============================================================================*/

#include "../D3D9Drv.h"
#include "../D3D9.h"
#include "../Shaders/ShaderPrograms.h"


//Zero is a valid brightness (the slider's bottom),
//so an out of range -1 marks "unset"; 0.4 is the curve's neutral point, leaving the image unchanged.
static const FLOAT UTGLR_DEFAULT_BRIGHTNESS = 0.4f;

static FLOAT BrightnessOrDefault(FLOAT brightness) {
	return (brightness < 0.0f) ? UTGLR_DEFAULT_BRIGHTNESS : brightness;
}
void UD3D9RenderDevice::BuildGammaRamp(float redGamma, float greenGamma, float blueGamma, int brightness, D3DGAMMARAMP &ramp) {
	unsigned int u;

	if (brightness < -50) brightness = -50;
	if (brightness > 50) brightness = 50;

	//Gamma inputs are unvalidated config offsets; zero or negative makes the reciprocal non finite.
	if (!(redGamma >= 0.1f)) redGamma = 0.1f;
	if (!(greenGamma >= 0.1f)) greenGamma = 0.1f;
	if (!(blueGamma >= 0.1f)) blueGamma = 0.1f;

	float rcpRedGamma = 1.0f / (2.5f * redGamma);
	float rcpGreenGamma = 1.0f / (2.5f * greenGamma);
	float rcpBlueGamma = 1.0f / (2.5f * blueGamma);
	for (u = 0; u < 256; u++) {
		int iVal;
		int iValRed, iValGreen, iValBlue;

		iVal = u;

		iVal += brightness;
		if (iVal < 0) iVal = 0;
		if (iVal > 255) iVal = 255;

		iValRed = (int)appRound((float)appPow(iVal / 255.0f, rcpRedGamma) * 65535.0f);
		iValGreen = (int)appRound((float)appPow(iVal / 255.0f, rcpGreenGamma) * 65535.0f);
		iValBlue = (int)appRound((float)appPow(iVal / 255.0f, rcpBlueGamma) * 65535.0f);

		ramp.red[u] = (_WORD)iValRed;
		ramp.green[u] = (_WORD)iValGreen;
		ramp.blue[u] = (_WORD)iValBlue;
	}

	return;
}

void UD3D9RenderDevice::SetGamma(FLOAT GammaCorrection) {
	D3DGAMMARAMP gammaRamp;

	GammaCorrection = BrightnessOrDefault(GammaCorrection) + GammaOffset;

	if (!m_d3dDevice) {
		return;
	}

	if (m_gammaPassAvailable) {
		//Would otherwise stack.
		ResetGamma();
		return;
	}

	BuildGammaRamp(GammaCorrection + GammaOffsetRed, GammaCorrection + GammaOffsetGreen, GammaCorrection + GammaOffsetBlue, Brightness, gammaRamp);

	//Process wide, the ramp belonging to the display.
	if (!g_haveOriginalGammaRamp) {
		m_d3dDevice->GetGammaRamp(0, &g_originalGammaRamp);
		g_haveOriginalGammaRamp = true;
	}

	m_d3dDevice->SetGammaRamp(0, D3DSGR_NO_CALIBRATION, &gammaRamp);

	return;
}

void UD3D9RenderDevice::ResetGamma(void) {
	if (g_haveOriginalGammaRamp && m_d3dDevice) {
		m_d3dDevice->SetGammaRamp(0, D3DSGR_NO_CALIBRATION, &g_originalGammaRamp);
		g_haveOriginalGammaRamp = false;
	}

	return;
}


void UD3D9RenderDevice::InitGammaResourcesSafe(void) {
	guard(UD3D9RenderDevice::InitGammaResourcesSafe);

	HRESULT hResult;

	if (m_gammaPassAvailable) {
		return;
	}

	if (UseHardwareGamma) {
		return;
	}

	if (m_gammaPassGaveUp) {
		return;
	}

	if (m_d3dCaps.PixelShaderVersion < D3DPS_VERSION(3, 0)) {
		debugf(NAME_Init, TEXT("Gamma correction pass needs pixel shader 3.0, falling back to display gamma ramp"));
		return;
	}
	if (m_d3dCaps.VertexShaderVersion < D3DVS_VERSION(3, 0)) {
		debugf(NAME_Init, TEXT("Gamma correction pass needs vertex shader 3.0, falling back to display gamma ramp"));
		return;
	}

	hResult = m_d3d9->CheckDeviceFormat(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, m_d3dpp.BackBufferFormat,
		D3DUSAGE_RENDERTARGET, D3DRTYPE_TEXTURE, m_d3dpp.BackBufferFormat);
	if (FAILED(hResult)) {
		debugf(NAME_Init, TEXT("Gamma correction pass needs a render target texture, falling back to display gamma ramp"));
		return;
	}

	if (!m_pGammaTexObj) {
		hResult = m_d3dDevice->CreateTexture(m_d3dpp.BackBufferWidth, m_d3dpp.BackBufferHeight, 1,
			D3DUSAGE_RENDERTARGET, m_d3dpp.BackBufferFormat, D3DPOOL_DEFAULT, &m_pGammaTexObj, NULL);
		if (FAILED(hResult)) {
			m_pGammaTexObj = NULL;
			debugf(NAME_Init, TEXT("CreateTexture (gamma correction) failed (0x%08X), falling back to display gamma ramp"), hResult);
			return;
		}
	}

	if (!m_pGammaTexSurface) {
		hResult = m_pGammaTexObj->GetSurfaceLevel(0, &m_pGammaTexSurface);
		if (FAILED(hResult)) {
			m_pGammaTexSurface = NULL;
			FreeGammaResources();
			return;
		}
	}

	if (!m_pBackBufferSurface) {
		hResult = m_d3dDevice->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &m_pBackBufferSurface);
		if (FAILED(hResult)) {
			m_pBackBufferSurface = NULL;
			FreeGammaResources();
			return;
		}
	}

	if (!m_vpGammaCorrection) {
		if (!LoadVertexProgram(&m_vpGammaCorrection, g_vpGammaCorrection, TEXT("Gamma correction"))) {
			FreeGammaResources();
			return;
		}
	}
	if (!m_fpGammaCorrection) {
		const DWORD *gammaProgram = ReduceBanding ? g_fpGammaCorrectionDithered : g_fpGammaCorrection;
		if (!LoadFragmentProgram(&m_fpGammaCorrection, gammaProgram, TEXT("Gamma correction"))) {
			FreeGammaResources();
			return;
		}
	}

	m_gammaPassAvailable = true;

	unguard;
}

void UD3D9RenderDevice::FreeGammaResources(void) {
	guard(UD3D9RenderDevice::FreeGammaResources);

	m_gammaPassAvailable = false;
	m_gammaPassFailCount = 0;

	//The pass leaves its render target bound to unit 0, and that holds a device reference failing the reset.
	if (m_d3dDevice) {
		m_d3dDevice->SetTexture(0, NULL);
		TexInfo[0].CurrentCacheID = TEX_CACHE_ID_UNUSED;
		TexInfo[0].pBind = NULL;
	}

	if (m_pBackBufferSurface) {
		m_pBackBufferSurface->Release();
		m_pBackBufferSurface = NULL;
	}
	if (m_pGammaTexSurface) {
		m_pGammaTexSurface->Release();
		m_pGammaTexSurface = NULL;
	}
	if (m_pGammaTexObj) {
		m_pGammaTexObj->Release();
		m_pGammaTexObj = NULL;
	}
	if (m_vpGammaCorrection) {
		m_vpGammaCorrection->Release();
		m_vpGammaCorrection = NULL;
	}
	if (m_fpGammaCorrection) {
		m_fpGammaCorrection->Release();
		m_fpGammaCorrection = NULL;
	}

	unguard;
}

bool UD3D9RenderDevice::CalcGammaPassParams(FLOAT GammaCorrection, FLOAT *pExponents, FLOAT *pOffsets) {
	INT brightness;
	FLOAT redGamma, greenGamma, blueGamma;
	FLOAT offset;

	GammaCorrection = BrightnessOrDefault(GammaCorrection) + GammaOffset;

	redGamma = GammaCorrection + GammaOffsetRed;
	greenGamma = GammaCorrection + GammaOffsetGreen;
	blueGamma = GammaCorrection + GammaOffsetBlue;

	//Written as a negated comparison so a NaN fails it too.
	//Must stay after the offset sums and before the reciprocals.
	if (!(redGamma >= 0.1f)) redGamma = 0.1f;
	if (!(greenGamma >= 0.1f)) greenGamma = 0.1f;
	if (!(blueGamma >= 0.1f)) blueGamma = 0.1f;

	brightness = Brightness;
	if (brightness < -50) brightness = -50;
	if (brightness > 50) brightness = 50;

	pExponents[0] = 1.0f / (2.5f * redGamma);
	pExponents[1] = 1.0f / (2.5f * greenGamma);
	pExponents[2] = 1.0f / (2.5f * blueGamma);
	pExponents[3] = 1.0f;

	offset = brightness / 255.0f;
	pOffsets[0] = offset;
	pOffsets[1] = offset;
	pOffsets[2] = offset;
	pOffsets[3] = 1.17549435e-38f;

	if ((brightness == 0) &&
		(Abs(pExponents[0] - 1.0f) < 0.0001f) &&
		(Abs(pExponents[1] - 1.0f) < 0.0001f) &&
		(Abs(pExponents[2] - 1.0f) < 0.0001f)) {
		return false;
	}

	return true;
}

/*
Dither parameters: three interleaved gradient noise constants and then one back buffer code, sized
for 8 bits since a 16 bit buffer already gets a coarser hardware dither of its own, all of it
passed as data because a def would write shared pixel shader constant registers and some of those
belong to other shaders.
*/
static const FLOAT g_gammaDitherParams[8] = {
	0.06711056f, 0.00583715f, 52.9829189f, 1.0f / 255.0f,
	-0.5f, 255.0f, 0.0f, 0.0f
};

/*
Everything the seven bit map reconstruction computes with, passed as data for the same reason as
the dither parameters above, a def here claiming c1 which belongs to the gamma pass, and the rows
are c10 to c14 of the constant map in the order the shader reads them: the cubic B-spline's
coefficients, the far tap's base offset, the noise plane, then the detail pass's own constants.
*/
extern const FLOAT g_sevenBitMapParams[5 * 4] = {
	0.5f, 1.0f, 2.0f / 3.0f, 1.0f / 6.0f,
	1.0f / 3.0f, 1.5f, 2.0f, 255.0f / (FLOAT)UTGLR_RGBA7_UPLOAD_MUL,
	0.06711056f, 0.00583715f, 52.9829189f, (FLOAT)UTGLR_RGBA7_UPLOAD_MUL / 255.0f,
	(UTGLR_RGBA7_UPLOAD_MUL == 2) ? (255.0f / 254.0f) : 1.0f, 0.0f, 0.0f, 0.0f,
	1.0f / 380.0f, 0.999f, 4.223f, 0.0f
};

void UD3D9RenderDevice::ApplyGammaPass(void) {
	guard(UD3D9RenderDevice::ApplyGammaPass);

	FLOAT exponents[4];
	FLOAT offsets[4];

	if (!m_gammaPassAvailable) {
		return;
	}

	if (!CalcGammaPassParams(Viewport->GetOuterUClient()->Brightness, exponents, offsets)) {
		return;
	}

	if (FAILED(m_d3dDevice->StretchRect(m_pBackBufferSurface, NULL, m_pGammaTexSurface, NULL, D3DTEXF_NONE))) {
		GammaPassFailed();
		return;
	}

	if (FAILED(m_d3dDevice->BeginScene())) {
		GammaPassFailed();
		return;
	}

	D3DVIEWPORT9 savedViewport;
	const bool restoreViewport = SUCCEEDED(m_d3dDevice->GetViewport(&savedViewport)) ? true : false;

	D3DVIEWPORT9 d3dViewport;
	d3dViewport.X = 0;
	d3dViewport.Y = 0;
	d3dViewport.Width = m_d3dpp.BackBufferWidth;
	d3dViewport.Height = m_d3dpp.BackBufferHeight;
	d3dViewport.MinZ = 0.0f;
	d3dViewport.MaxZ = 1.0f;
	m_d3dDevice->SetViewport(&d3dViewport);

	SetDisabledAAState();
	SetBlend(PF_Occlude);
	m_d3dDevice->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);

	SetStreamState(m_standardNTextureVertexDecl[0], m_vpGammaCorrection, m_fpGammaCorrection);

	m_d3dDevice->SetPixelShaderConstantF(1, offsets, 1);
	m_d3dDevice->SetPixelShaderConstantF(2, exponents, 1);
	m_d3dDevice->SetPixelShaderConstantF(5, g_gammaDitherParams, 2);

	m_d3dDevice->SetTexture(0, m_pGammaTexObj);
	SetTexFilter(0, CT_MIN_FILTER_POINT | CT_MIP_FILTER_NONE | CT_ADDRESS_CLAMP_NOT_WRAP_BIT);
	TexInfo[0].CurrentCacheID = TEX_CACHE_ID_UNUSED;
	TexInfo[0].pBind = NULL;

	if ((m_curVertexBufferPos + 4) >= VERTEX_RING_SIZE) {
		FlushVertexBuffers();
	}

	LockVertexColorBuffer();
	LockTexCoordBuffer(0);

	FGLTexCoord *pTexCoordArray = m_pTexCoordArray[0];
	FGLVertexColor *pVertexColorArray = m_pVertexColorArray;

	//Shifts the quad half a pixel up and left, the correction the projection matrix carries everywhere else.
	const FLOAT halfPixelX = 1.0f / (FLOAT)m_d3dpp.BackBufferWidth;
	const FLOAT halfPixelY = 1.0f / (FLOAT)m_d3dpp.BackBufferHeight;

	pTexCoordArray[0].u = 0.0f;
	pTexCoordArray[0].v = 0.0f;

	pTexCoordArray[1].u = 1.0f;
	pTexCoordArray[1].v = 0.0f;

	pTexCoordArray[2].u = 1.0f;
	pTexCoordArray[2].v = 1.0f;

	pTexCoordArray[3].u = 0.0f;
	pTexCoordArray[3].v = 1.0f;

	pVertexColorArray[0].x = -1.0f - halfPixelX;
	pVertexColorArray[0].y = +1.0f + halfPixelY;
	pVertexColorArray[0].z = 0.5f;
	pVertexColorArray[0].color = 0xFFFFFFFF;

	pVertexColorArray[1].x = +1.0f - halfPixelX;
	pVertexColorArray[1].y = +1.0f + halfPixelY;
	pVertexColorArray[1].z = 0.5f;
	pVertexColorArray[1].color = 0xFFFFFFFF;

	pVertexColorArray[2].x = +1.0f - halfPixelX;
	pVertexColorArray[2].y = -1.0f + halfPixelY;
	pVertexColorArray[2].z = 0.5f;
	pVertexColorArray[2].color = 0xFFFFFFFF;

	pVertexColorArray[3].x = -1.0f - halfPixelX;
	pVertexColorArray[3].y = -1.0f + halfPixelY;
	pVertexColorArray[3].z = 0.5f;
	pVertexColorArray[3].color = 0xFFFFFFFF;

	UnlockVertexColorBuffer();
	UnlockTexCoordBuffer(0);

	m_d3dDevice->DrawPrimitive(D3DPT_TRIANGLEFAN, m_curVertexBufferPos, 2);

	m_curVertexBufferPos += 4;

	m_d3dDevice->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
	SetDefaultStreamState();
	SetDefaultAAState();
	//The viewport has to go back. Left widened, geometry under a sub-rectangle the engine never
	//re-announces would spill across it.
	if (restoreViewport) {
		m_d3dDevice->SetViewport(&savedViewport);
	}

	m_d3dDevice->EndScene();

	m_gammaPassFailCount = 0;

	unguard;
}

//The display gamma path leaves the ramp alone while the pass is available.
void UD3D9RenderDevice::GammaPassFailed(void) {
	guard(UD3D9RenderDevice::GammaPassFailed);

	enum { GAMMA_PASS_FAIL_LIMIT = 8 };
	if (++m_gammaPassFailCount < GAMMA_PASS_FAIL_LIMIT) {
		return;
	}

	debugf(NAME_Init, TEXT("Gamma correction pass failed %i frames in a row, falling back to display gamma ramp"), (INT)GAMMA_PASS_FAIL_LIMIT);

	m_gammaPassGaveUp = true;

	FreeGammaResources();

	SetGamma(Viewport->GetOuterUClient()->Brightness);

	unguard;
}
