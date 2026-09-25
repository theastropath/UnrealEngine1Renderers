/*=============================================================================
	Gamma.cpp: brightness, as a ramp and as a pass.

	The display adapter's gamma ramp is system wide.
	Where the hardware allows, the same correction runs over the finished frame.
=============================================================================*/

#include "../OpenGLDrv.h"
#include "../OpenGL.h"
#include "../Shaders/ShaderPrograms.h"


//Zero is a valid brightness, so -1 marks "unset".
//0.4 is the curve's neutral point.
static const FLOAT UTGLR_DEFAULT_BRIGHTNESS = 0.4f;

static FLOAT BrightnessOrDefault(FLOAT brightness) {
	return (brightness < 0.0f) ? UTGLR_DEFAULT_BRIGHTNESS : brightness;
}

void UOpenGLRenderDevice::BuildGammaRamp(float redGamma, float greenGamma, float blueGamma, int brightness, FGammaRamp &ramp) {
	unsigned int u;

	if (brightness < -50) brightness = -50;
	if (brightness > 50) brightness = 50;

	//Gamma inputs are unvalidated config offsets.
	//Zero or negative makes the reciprocal non finite.
	//Negated test, so NaN is caught.
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

		const float fVal = iVal / 255.0f;
		iValRed = (int)appRound((float)appPow(fVal, rcpRedGamma) * 65535.0f);
		iValGreen = (int)appRound((float)appPow(fVal, rcpGreenGamma) * 65535.0f);
		iValBlue = (int)appRound((float)appPow(fVal, rcpBlueGamma) * 65535.0f);

		ramp.red[u] = (_WORD)Clamp(iValRed, 0, 65535);
		ramp.green[u] = (_WORD)Clamp(iValGreen, 0, 65535);
		ramp.blue[u] = (_WORD)Clamp(iValBlue, 0, 65535);
	}

	return;
}

void UOpenGLRenderDevice::BuildGammaRamp(float redGamma, float greenGamma, float blueGamma, int brightness, FByteGammaRamp &ramp) {
	unsigned int u;

	if (brightness < -50) brightness = -50;
	if (brightness > 50) brightness = 50;

	//Same clamp as above.
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

		const float fVal = iVal / 255.0f;
		iValRed = (int)appRound((float)appPow(fVal, rcpRedGamma) * 255.0f);
		iValGreen = (int)appRound((float)appPow(fVal, rcpGreenGamma) * 255.0f);
		iValBlue = (int)appRound((float)appPow(fVal, rcpBlueGamma) * 255.0f);

		ramp.red[u] = (BYTE)Clamp(iValRed, 0, 255);
		ramp.green[u] = (BYTE)Clamp(iValGreen, 0, 255);
		ramp.blue[u] = (BYTE)Clamp(iValBlue, 0, 255);
	}

	return;
}

void UOpenGLRenderDevice::SetGamma(FLOAT GammaCorrection) {
	FGammaRamp gammaRamp;

	GammaCorrection = BrightnessOrDefault(GammaCorrection) + GammaOffset;

	//The display gamma ramp API silently does nothing in a window.
	if (UsingPostProcessGamma()) {
		//An earlier call may have set a hardware ramp.
		ResetGamma();

		//The pass uploads it.
		m_gammaRampTexOutOfDate = true;
		m_gammaRampInEffect = true;
		SavedGammaCorrection = GammaCorrection;

		return;
	}

	BuildGammaRamp(GammaCorrection + GammaOffsetRed, GammaCorrection + GammaOffsetGreen, GammaCorrection + GammaOffsetBlue, Brightness, gammaRamp);

#ifdef __UNIX__
	SDL_SetGammaRamp(gammaRamp.red, gammaRamp.green, gammaRamp.blue);
#else
	if (g_gammaFirstTime) {
		if (GetDeviceGammaRamp(m_hDC, &g_originalGammaRamp)) {
			g_haveOriginalGammaRamp = true;
		}
		g_gammaFirstTime = false;
	}

	m_gammaRampInEffect = false;
	if (SetDeviceGammaRamp(m_hDC, &gammaRamp)) {
		m_gammaRampInEffect = true;
		SavedGammaCorrection = GammaCorrection;
	}
#endif

	return;
}

void UOpenGLRenderDevice::ResetGamma(void) {
#ifdef __UNIX__

#else
	if (g_haveOriginalGammaRamp) {
		HWND hDesktopWnd;
		HDC hDC;

		hDesktopWnd = GetDesktopWindow();
		hDC = GetDC(hDesktopWnd);

		//The desktop's device context can already be invalid.
		SetDeviceGammaRamp(hDC, &g_originalGammaRamp);

		ReleaseDC(hDesktopWnd, hDC);
	}
#endif

	return;
}


//Idempotent.
void UOpenGLRenderDevice::TryInitializeGammaPostProcess(void) {
	unsigned int u;
	BYTE identityRamp[256 * 4];

	m_gammaPostProcessSupported = false;

	//The editor has no brightness slider.
	if (GIsEditor) {
		return;
	}

	//Needs a dependent texture read for the lookup.
	if (!SUPPORTS_GL_ARB_vertex_program || !SUPPORTS_GL_ARB_fragment_program) {
		return;
	}
	if (!SUPPORTS_GL_ARB_multitexture || (TMUnits < 2)) {
		return;
	}

	if (m_fpGammaRamp == 0) {
		glGenProgramsARB(1, &m_fpGammaRamp);
	}
	const char *gammaProgram = ReduceBanding ? g_fpGammaRampDithered : g_fpGammaRamp;
	const bool loadOk = LoadFragmentProgram(m_fpGammaRamp, gammaProgram, "Gamma ramp post process");

	//Resynced first, on purpose.
	//Deleting a program still tracked as current reverts the binding to zero.
	//Nothing rebinds until a draw asks for another program.
	glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, 0);
	glDisable(GL_FRAGMENT_PROGRAM_ARB);
	m_fpCurrent = 0;
	//Left by hand.
	ResyncTextureEnables();

	if (!loadOk) {
		ShutdownGammaPostProcess();

		if (DebugBit(DEBUG_BIT_BASIC)) dbgPrintf("utglr: Gamma post process initialization failed\n");

		return;
	}

	SetActiveTexUnit(0);

	for (u = 0; u < 256; u++) {
		identityRamp[(u * 4) + 0] = (BYTE)u;
		identityRamp[(u * 4) + 1] = (BYTE)u;
		identityRamp[(u * 4) + 2] = (BYTE)u;
		identityRamp[(u * 4) + 3] = 255;
	}
	if (m_gammaRampTexId == 0) {
		glGenTextures(1, &m_gammaRampTexId);
	}
	//Linear when dithering, so a jittered lookup lands between entries.
	//Nearest otherwise.
	const GLint rampFilter = ReduceBanding ? GL_LINEAR : GL_NEAREST;
	glBindTexture(GL_TEXTURE_2D, m_gammaRampTexId);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, rampFilter);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, rampFilter);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 256, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, identityRamp);

	//Sized on first use.
	if (m_gammaSceneTexId == 0) {
		glGenTextures(1, &m_gammaSceneTexId);
	}
	glBindTexture(GL_TEXTURE_2D, m_gammaSceneTexId);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	m_gammaSceneTexWidth = 0;
	m_gammaSceneTexHeight = 0;

	//Went around the texture cache.
	TexInfo[0].CurrentCacheID = TEX_CACHE_ID_UNUSED;
	TexInfo[0].pBind = NULL;

	m_gammaRampTexOutOfDate = true;
	m_gammaRampIsIdentity = false;
	m_gammaPostProcessSupported = true;

	return;
}

void UOpenGLRenderDevice::ShutdownGammaPostProcess(void) {
	if (m_fpGammaRamp != 0) {
		glDeleteProgramsARB(1, &m_fpGammaRamp);
		m_fpGammaRamp = 0;
	}
	if (m_gammaSceneTexId != 0) {
		glDeleteTextures(1, &m_gammaSceneTexId);
		m_gammaSceneTexId = 0;
	}
	if (m_gammaRampTexId != 0) {
		glDeleteTextures(1, &m_gammaRampTexId);
		m_gammaRampTexId = 0;
	}

	m_gammaSceneTexWidth = 0;
	m_gammaSceneTexHeight = 0;
	m_gammaPostProcessSupported = false;

	return;
}

//The scene texture must already be bound.
bool FASTCALL UOpenGLRenderDevice::ResizeGammaSceneTextureSafe(INT SizeX, INT SizeY) {
	INT texWidth = SizeX;
	INT texHeight = SizeY;
	GLint allocatedWidth = 0;
	GLint allocatedHeight = 0;

	if (!SUPPORTS_GL_ARB_texture_non_power_of_two) {
		texWidth = 1;
		while (texWidth < SizeX) {
			texWidth <<= 1;
		}
		texHeight = 1;
		while (texHeight < SizeY) {
			texHeight <<= 1;
		}
	}

	if ((texWidth <= m_gammaSceneTexWidth) && (texHeight <= m_gammaSceneTexHeight)) {
		return true;
	}

	//Grow only, never shrink.
	if (texWidth < m_gammaSceneTexWidth) texWidth = m_gammaSceneTexWidth;
	if (texHeight < m_gammaSceneTexHeight) texHeight = m_gammaSceneTexHeight;

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, texWidth, texHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);

	/*
	A refused size shows up only in the resulting mip level, which keeps the previous one, so
	both dimensions have to be checked because a height-only refusal still reads the old
	width as accepted and the capture would then sample outside itself, and proxy textures
	are the only way to ask a driver whether it will take a size without committing to it.
	*/
	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &allocatedWidth);
	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &allocatedHeight);
	if ((allocatedWidth != texWidth) || (allocatedHeight != texHeight)) {
		m_gammaSceneTexWidth = 0;
		m_gammaSceneTexHeight = 0;
		return false;
	}

	m_gammaSceneTexWidth = texWidth;
	m_gammaSceneTexHeight = texHeight;

	return true;
}

//The ramp texture must already be bound.
void UOpenGLRenderDevice::UpdateGammaRampTexture(void) {
	FByteGammaRamp gammaRamp;
	BYTE rampTexels[256 * 4];
	unsigned int u;
	bool isIdentity;

	BuildGammaRamp(SavedGammaCorrection + GammaOffsetRed, SavedGammaCorrection + GammaOffsetGreen, SavedGammaCorrection + GammaOffsetBlue, Brightness, gammaRamp);

	isIdentity = true;
	for (u = 0; u < 256; u++) {
		rampTexels[(u * 4) + 0] = gammaRamp.red[u];
		rampTexels[(u * 4) + 1] = gammaRamp.green[u];
		rampTexels[(u * 4) + 2] = gammaRamp.blue[u];
		rampTexels[(u * 4) + 3] = 255;

		if ((gammaRamp.red[u] != u) || (gammaRamp.green[u] != u) || (gammaRamp.blue[u] != u)) {
			isIdentity = false;
		}
	}

	m_gammaRampIsIdentity = isIdentity;

	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 1, GL_RGBA, GL_UNSIGNED_BYTE, rampTexels);

	return;
}

//After the HUD and menus.
void UOpenGLRenderDevice::DrawGammaPostProcess(void) {
	INT sizeX = Viewport->SizeX;
	INT sizeY = Viewport->SizeY;
	GLint savedViewport[4];
	FLOAT uMax, vMax;

	if ((sizeX <= 0) || (sizeY <= 0)) {
		return;
	}

	//Only the binds matter.
	SetActiveTexUnit(1);
	glBindTexture(GL_TEXTURE_2D, m_gammaRampTexId);
	if (m_gammaRampTexOutOfDate) {
		UpdateGammaRampTexture();
		m_gammaRampTexOutOfDate = false;
	}
	SetActiveTexUnit(0);

	//Around the cache again.
	TexInfo[1].CurrentCacheID = TEX_CACHE_ID_UNUSED;
	TexInfo[1].pBind = NULL;

	if (m_gammaRampIsIdentity) {
		return;
	}

	glBindTexture(GL_TEXTURE_2D, m_gammaSceneTexId);

	TexInfo[0].CurrentCacheID = TEX_CACHE_ID_UNUSED;
	TexInfo[0].pBind = NULL;

	if (ResizeGammaSceneTextureSafe(sizeX, sizeY) == false) {
		//Fall back to the display driver.
		m_gammaPostProcessSupported = false;
		SetGamma(Viewport->GetOuterUClient()->Brightness);

		if (DebugBit(DEBUG_BIT_BASIC)) dbgPrintf("utglr: Gamma post process frame capture allocation failed\n");

		return;
	}

	//Reads window coordinates.
	glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, sizeX, sizeY);

	//From the shadow copy.
	/*
	This runs immediately before the buffer swap and restoring the viewport matters because
	it is only reinstated when the scene node changes, so a frame whose node matches the last
	one would be drawn through this pass's viewport, and in a windowed editor with several
	viewports open that is the common case, the symptom being a view drawn at the size of
	whichever viewport last ran the gamma pass.
	*/
	savedViewport[0] = m_curViewport[0];
	savedViewport[1] = m_curViewport[1];
	savedViewport[2] = m_curViewport[2];
	savedViewport[3] = m_curViewport[3];
	SetViewport(0, 0, sizeX, sizeY);

	SetShaderState(0, m_fpGammaRamp);

	//No blending, no alpha test, full color mask.
	SetBlend(PF_Occlude);
	//Also stops the quad writing depth.
	glDisable(GL_DEPTH_TEST);

	uMax = (FLOAT)sizeX / (FLOAT)m_gammaSceneTexWidth;
	vMax = (FLOAT)sizeY / (FLOAT)m_gammaSceneTexHeight;

	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();

	glBegin(GL_TRIANGLE_FAN);
	glTexCoord2f(0.0f, 0.0f);
	glVertex3f(-1.0f, -1.0f, 0.0f);
	glTexCoord2f(uMax, 0.0f);
	glVertex3f(+1.0f, -1.0f, 0.0f);
	glTexCoord2f(uMax, vMax);
	glVertex3f(+1.0f, +1.0f, 0.0f);
	glTexCoord2f(0.0f, vMax);
	glVertex3f(-1.0f, +1.0f, 0.0f);
	glEnd();

	//The quad leaves the tracked coordinate state at its last corner.
	InvalidateTexAttribShadows();

	glPopMatrix();
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	//The pushes and pops balance, the mode selection does not.
	glMatrixMode(GL_MODELVIEW);

	glEnable(GL_DEPTH_TEST);
	SetViewport(savedViewport[0], savedViewport[1], savedViewport[2], savedViewport[3]);

	SetDefaultShaderState();

	return;
}
