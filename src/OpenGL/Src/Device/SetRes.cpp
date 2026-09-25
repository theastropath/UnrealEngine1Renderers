/*=============================================================================
	SetRes.cpp: choosing a display mode and what depends on it.

	SetRes is called again on every resolution change.
	Everything it creates it must also destroy.
=============================================================================*/

#include "../OpenGLDrv.h"
#include "../OpenGL.h"
#include "../Core/Globals.h"
#include "../State/PolygonOffset.h"

#ifndef __UNIX__
bool UOpenGLRenderDevice::TrySetDisplayMode(INT NewX, INT NewY, INT NewColorBytes) {
	DEVMODEA dma;
	DEVMODEW dmw;

	ZeroMemory(&dma, sizeof(dma));
	ZeroMemory(&dmw, sizeof(dmw));
	dma.dmSize = sizeof(dma);
	dmw.dmSize = sizeof(dmw);
	dma.dmPelsWidth = dmw.dmPelsWidth = NewX;
	dma.dmPelsHeight = dmw.dmPelsHeight = NewY;
	dma.dmBitsPerPel = dmw.dmBitsPerPel = NewColorBytes * 8;
	dma.dmFields = dmw.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT; // | DM_BITSPERPEL;

	//Prefer the configured refresh rate.
	if (RefreshRate) {
		dma.dmDisplayFrequency = dmw.dmDisplayFrequency = RefreshRate;
		dma.dmFields |= DM_DISPLAYFREQUENCY;
		dmw.dmFields |= DM_DISPLAYFREQUENCY;

		if (TCHAR_CALL_OS(ChangeDisplaySettingsW(&dmw, CDS_FULLSCREEN), ChangeDisplaySettingsA(&dma, CDS_FULLSCREEN)) == DISP_CHANGE_SUCCESSFUL) {
			return true;
		}

		debugf(TEXT("ChangeDisplaySettings failed: %ix%i, %i Hz"), NewX, NewY, RefreshRate);
		dma.dmFields &= ~DM_DISPLAYFREQUENCY;
		dmw.dmFields &= ~DM_DISPLAYFREQUENCY;
	}

	if (TCHAR_CALL_OS(ChangeDisplaySettingsW(&dmw, CDS_FULLSCREEN), ChangeDisplaySettingsA(&dma, CDS_FULLSCREEN)) == DISP_CHANGE_SUCCESSFUL) {
		return true;
	}

	debugf(TEXT("ChangeDisplaySettings failed: %ix%i"), NewX, NewY);

	return false;
}
#endif

UBOOL UOpenGLRenderDevice::SetRes(INT NewX, INT NewY, INT NewColorBytes, UBOOL Fullscreen) {
	unsigned int u;

	guard(UOpenGLRenderDevice::SetRes);

	{
		INT i = 0;
		if (!GConfig->GetInt(g_pSection, TEXT("DebugBits"), i)) i = 0;
		m_debugBits = i;
		//Reset per mode set.
		m_numLoggedTiles = 0;
	}
	if (DebugBit(DEBUG_BIT_ANY)) dbgPrintf("utglr: DebugBits = %u\n", m_debugBits);

#ifdef __UNIX__
	UnsetRes();

	INT MinDepthBits;

	m_usingAA = false;
	m_curAAEnable = true;
	m_defAAEnable = true;

	if (!GConfig->GetInt(g_pSection, TEXT("MinDepthBits"), MinDepthBits)) MinDepthBits = 16;
	//debugf( TEXT("MinDepthBits = %i"), MinDepthBits );
	//16 is the bare minimum.
	if (MinDepthBits < 16) MinDepthBits = 16;

	INT RequestDoubleBuffer;
	if (!GConfig->GetInt(g_pSection, TEXT("RequestDoubleBuffer"), RequestDoubleBuffer)) RequestDoubleBuffer = 1;

	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, (NewColorBytes <= 2) ? 5 : 8);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, (NewColorBytes <= 2) ? 5 : 8);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, (NewColorBytes <= 2) ? 5 : 8);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, MinDepthBits);
	if (RequestDoubleBuffer) {
		SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	} else {
		SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 0);
	}

	//TODO: Set this correctly
	m_numDepthBits = MinDepthBits;

	Viewport->ResizeViewport(Fullscreen ? (BLIT_Fullscreen | UTGLR_BLIT_OPENGL) : (BLIT_HardwarePaint | UTGLR_BLIT_OPENGL), NewX, NewY, NewColorBytes);
#else
	debugf(TEXT("Enter SetRes()"));

	if (m_hRC && !Fullscreen && !WasFullscreen && NewColorBytes == Viewport->ColorBytes) {
		if (!Viewport->ResizeViewport(BLIT_HardwarePaint | UTGLR_BLIT_OPENGL, NewX, NewY, NewColorBytes)) {
			return 0;
		}
		SetViewport(0, 0, NewX, NewY);

		return 1;
	}

	//The context goes first.
	//Resizing can recreate the window and invalidate its DC.
	if (m_hRC) {
		debugf(TEXT("UnSetRes() -> hRC != NULL"));
		UnsetRes();
	}

	if (Fullscreen) {
		//Configured size first.
		if (!TrySetDisplayMode(NewX, NewY, NewColorBytes)) {
			INT FallbackX = NewX, FallbackY = NewY, BestError = MAXINT;
			for (INT i = 0; i < Modes.Num(); i++) {
				if (Modes(i).Z == NewColorBytes * 8) {
					INT Error = (Modes(i).X - NewX) * (Modes(i).X - NewX) + (Modes(i).Y - NewY) * (Modes(i).Y - NewY);
					if (Error < BestError) {
						FallbackX = Modes(i).X;
						FallbackY = Modes(i).Y;
						BestError = Error;
					}
				}
			}

			//The context is already gone.
			if ((BestError == MAXINT) || ((FallbackX == NewX) && (FallbackY == NewY))) {
				return FailedInitf(TEXT("No display mode matching %ix%ix%i is available"), NewX, NewY, NewColorBytes * 8);
			}

			debugf(TEXT("Falling back to nearest available mode: %ix%i"), FallbackX, FallbackY);
			if (!TrySetDisplayMode(FallbackX, FallbackY, NewColorBytes)) {
				return FailedInitf(TEXT("Failed to set display mode %ix%ix%i"), FallbackX, FallbackY, NewColorBytes * 8);
			}

			NewX = FallbackX;
			NewY = FallbackY;
		}
	}

	UBOOL Result = Viewport->ResizeViewport(Fullscreen ? (BLIT_Fullscreen | UTGLR_BLIT_OPENGL) : (BLIT_HardwarePaint | UTGLR_BLIT_OPENGL), NewX, NewY, NewColorBytes);
	if (!Result) {
		if (Fullscreen) {
			TCHAR_CALL_OS(ChangeDisplaySettingsW(NULL, 0), ChangeDisplaySettingsA(NULL, 0));
		}

		//The desktop must not be left in the game's resolution.
		return FailedInitf(TEXT("Failed to resize viewport to %ix%ix%i"), NewX, NewY, NewColorBytes * 8);
	}


	//Set default numDepthBits in case a failure prevents it later.
	m_numDepthBits = 16;

	//The sample count is the switch now.
	//Taken before the clamp below.
	UseAA = (NumAASamples != 0) ? 1 : 0;

	if (NumAASamples < 1) NumAASamples = 1;

	m_usingAA = false;
	m_curAAEnable = true;
	m_defAAEnable = true;
	m_initNumAASamples = NumAASamples;

	bool doBasicInit = true;
	//Same as the D3D9 path.
	if (UseAA && (NumAASamples >= 2)) {
		//The setup reports back the sample count it settled on.
		if (SetAAPixelFormat(NewColorBytes)) {
			doBasicInit = false;
			m_usingAA = true;
		}
	}
	if (doBasicInit) {
		//The basic pixel format path cannot refuse a shallow depth buffer.
		if (SetARBPixelFormat(NewColorBytes)) {
			doBasicInit = false;
		}
	}
	if (doBasicInit) {
		SetBasicPixelFormat(NewColorBytes);
	}

	if (ShareLists && AllContexts.Num()) {
		verify(wglShareLists(AllContexts(0), m_hRC) == 1);
	}
	AllContexts.AddItem(m_hRC);
#endif

	/*
	State shadows are seeded to what the spec guarantees a fresh context holds, because
	seeding a value GL does not hold means the first matching request gets skipped and the
	shadow stays wrong for the rest of the run, and getting one of these wrong shows up as a
	slow drift where some later frame draws with a blend mode nobody set, on one driver and
	not another, and the report comes back as a screenshot of something faintly wrong.
	*/
	m_curActiveTexUnit = 0;
	m_curClientActiveTexUnit = 0;
	m_curColor[0] = 1.0f;
	m_curColor[1] = 1.0f;
	m_curColor[2] = 1.0f;
	m_curColor[3] = 1.0f;
	m_curColorValid = true;
	for (INT texUnit = 0; texUnit < MAX_TMUNITS; texUnit++) {
		m_curTexAttrib[texUnit][0] = 0.0f;
		m_curTexAttrib[texUnit][1] = 0.0f;
		m_curTexAttrib[texUnit][2] = 0.0f;
		m_curTexAttrib[texUnit][3] = 1.0f;
	}
	m_curTexAttribValidBits = (1U << MAX_TMUNITS) - 1;
	//This shadow is the only record the viewport is restored from.
	SetViewport(0, 0, Viewport->SizeX, Viewport->SizeY);

	m_prevSwapBuffersStatus = true;

	//PrintFormat( hDC, nPixelFormat );
	debugf(NAME_Init, TEXT("GL_VENDOR     : %s"), appFromAnsi(GLStringOrEmpty(glGetString(GL_VENDOR))));
	debugf(NAME_Init, TEXT("GL_RENDERER   : %s"), appFromAnsi(GLStringOrEmpty(glGetString(GL_RENDERER))));
	debugf(NAME_Init, TEXT("GL_VERSION    : %s"), appFromAnsi(GLStringOrEmpty(glGetString(GL_VERSION))));

	//Logging more than 1024 characters at once is unsafe.
	const char *pGLExtensions = GLStringOrEmpty(glGetString(GL_EXTENSIONS));
	if (strlen(pGLExtensions) < 1024) {
		debugf(NAME_Init, TEXT("GL_EXTENSIONS : %s"), appFromAnsi(pGLExtensions));
	} else {
#ifdef __UNIX__
		printf("GL_EXTENSIONS : %s\n", pGLExtensions);
#endif
	}

	GetGLExtProcs();

	debugf(NAME_Init, TEXT("Depth bits: %u"), m_numDepthBits);
	if (DebugBit(DEBUG_BIT_BASIC)) dbgPrintf("utglr: Depth bits: %u\n", m_numDepthBits);
	if (m_usingAA) {
		debugf(NAME_Init, TEXT("AA samples: %u"), m_initNumAASamples);
		if (DebugBit(DEBUG_BIT_BASIC)) dbgPrintf("utglr: AA samples: %u\n", m_initNumAASamples);
	}

	//Depth diagnostics only.
	if (m_numDepthBits < 24) {
		debugf(NAME_Init, TEXT("Warning: only %u depth bits were available; coplanar surfaces will z-fight. 24 or more is strongly recommended."), m_numDepthBits);
	}
	if (UseZTrick) {
		debugf(NAME_Init, TEXT("Warning: UseZTrick gives each frame half the depth range and flips the depth function every frame, which shows up as z-fighting that strobes at the frame rate. Set UseZTrick=False to rule it out."));
	}
	//Both branches log.
#ifdef UTGLR_OLD_URENDERDEVICE
	debugf(NAME_Init, TEXT("Note: this engine has no GUglyHackFlags, so ZRangeHack cannot tell the player's weapon from the world and is not offered. The near plane is 0.5 rather than 4.0, which costs 8x depth precision at every distance."));
#else
	if (!ZRangeHack) {
		debugf(NAME_Init, TEXT("Note: ZRangeHack is off, so the near plane is 0.5 rather than 4.0, which costs 8x depth precision at every distance."));
	}
#endif

	//The limiter only engages at 20 and above.
	if ((FrameRateLimit > 0) && (FrameRateLimit < 20)) {
		debugf(NAME_Init, TEXT("Warning: FrameRateLimit %i is below the minimum of 20 and is being ignored. Use 0 to turn the limiter off."), FrameRateLimit);
	}

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glScalef(1.0f, -1.0f, -1.0f);

	//Debug parameter listing.
	if (DebugBit(DEBUG_BIT_BASIC)) {
#define UTGLR_DEBUG_SHOW_PARAM_REG(name) DbgPrintInitParam(#name, name)
#define UTGLR_DEBUG_SHOW_PARAM_DCV(name) DbgPrintInitParam(#name, DCV.name)

		UTGLR_DEBUG_SHOW_PARAM_REG(LODBias);
		UTGLR_DEBUG_SHOW_PARAM_REG(GammaOffset);
		UTGLR_DEBUG_SHOW_PARAM_REG(GammaOffsetRed);
		UTGLR_DEBUG_SHOW_PARAM_REG(GammaOffsetGreen);
		UTGLR_DEBUG_SHOW_PARAM_REG(GammaOffsetBlue);
		UTGLR_DEBUG_SHOW_PARAM_REG(Brightness);
		UTGLR_DEBUG_SHOW_PARAM_REG(UseHardwareGamma);
		UTGLR_DEBUG_SHOW_PARAM_REG(ReduceBanding);
		UTGLR_DEBUG_SHOW_PARAM_REG(OneXBlending);
		UTGLR_DEBUG_SHOW_PARAM_REG(MinLogTextureSize);
		UTGLR_DEBUG_SHOW_PARAM_REG(MaxLogTextureSize);
		UTGLR_DEBUG_SHOW_PARAM_REG(MaxAnisotropy);
		UTGLR_DEBUG_SHOW_PARAM_REG(TMUnits);
		UTGLR_DEBUG_SHOW_PARAM_REG(MaxTMUnits);
		UTGLR_DEBUG_SHOW_PARAM_REG(RefreshRate);
		UTGLR_DEBUG_SHOW_PARAM_REG(UseZTrick);
		UTGLR_DEBUG_SHOW_PARAM_REG(UseBGRATextures);
		UTGLR_DEBUG_SHOW_PARAM_REG(UseMultiTexture);
		UTGLR_DEBUG_SHOW_PARAM_REG(UsePalette);
		UTGLR_DEBUG_SHOW_PARAM_REG(ShareLists);
//		UTGLR_DEBUG_SHOW_PARAM_REG(AlwaysMipmap);
#ifndef UTGLR_OLD_URENDERDEVICE
		UTGLR_DEBUG_SHOW_PARAM_REG(UsePrecache);
#endif
		UTGLR_DEBUG_SHOW_PARAM_REG(UseTrilinear);
		//		UTGLR_DEBUG_SHOW_PARAM_REG(UseVertexSpecular);
		UTGLR_DEBUG_SHOW_PARAM_REG(UseAlphaPalette);
#ifndef UTGLR_KLINGON_BUILD
		UTGLR_DEBUG_SHOW_PARAM_REG(UseS3TC);
#endif
		UTGLR_DEBUG_SHOW_PARAM_REG(Use16BitTextures);
		UTGLR_DEBUG_SHOW_PARAM_REG(NoFiltering);
		UTGLR_DEBUG_SHOW_PARAM_DCV(DetailMax);
		//		UTGLR_DEBUG_SHOW_PARAM_REG(UseDetailAlpha);
		UTGLR_DEBUG_SHOW_PARAM_REG(DetailClipping);
		UTGLR_DEBUG_SHOW_PARAM_REG(ColorizeDetailTextures);
		UTGLR_DEBUG_SHOW_PARAM_DCV(SinglePassFog);
		UTGLR_DEBUG_SHOW_PARAM_DCV(SinglePassDetail);
		//		UTGLR_DEBUG_SHOW_PARAM_REG(BufferActorTris);
		//		UTGLR_DEBUG_SHOW_PARAM_REG(BufferClippedActorTris);
		UTGLR_DEBUG_SHOW_PARAM_REG(BufferTileQuads);
		UTGLR_DEBUG_SHOW_PARAM_REG(UseSSE);
		UTGLR_DEBUG_SHOW_PARAM_REG(UseSSE2);
		UTGLR_DEBUG_SHOW_PARAM_REG(UseTexIdPool);
		UTGLR_DEBUG_SHOW_PARAM_REG(UseTexPool);
		UTGLR_DEBUG_SHOW_PARAM_REG(CacheStaticMaps);
		UTGLR_DEBUG_SHOW_PARAM_REG(TexCacheBudgetMegs);
		UTGLR_DEBUG_SHOW_PARAM_REG(DynamicTexIdRecycleLevel);
		UTGLR_DEBUG_SHOW_PARAM_REG(TexDXT1ToDXT3);
		UTGLR_DEBUG_SHOW_PARAM_REG(UseMultiDrawArrays);
		UTGLR_DEBUG_SHOW_PARAM_REG(UseVBO);
		UTGLR_DEBUG_SHOW_PARAM_DCV(UseFragmentProgram);
		UTGLR_DEBUG_SHOW_PARAM_REG(SwapInterval);
		UTGLR_DEBUG_SHOW_PARAM_REG(FrameRateLimit);
		UTGLR_DEBUG_SHOW_PARAM_REG(SmoothMaskedTextures);
		UTGLR_DEBUG_SHOW_PARAM_REG(NumAASamples);
		UTGLR_DEBUG_SHOW_PARAM_REG(NoAATiles);
#ifdef UTGLR_OLD_URENDERDEVICE
		UTGLR_DEBUG_SHOW_PARAM_REG(DetailTextures);
#else
		UTGLR_DEBUG_SHOW_PARAM_REG(ZRangeHack);
#endif

#undef UTGLR_DEBUG_SHOW_PARAM_REG
#undef UTGLR_DEBUG_SHOW_PARAM_DCV
	}

	//Refreshed each frame.
	SetSwapIntervalSafe();
	PL_SwapInterval = SwapInterval;


#ifdef UTGLR_INCLUDE_SSE_CODE
	if (UseSSE) {
		if (!CPU_DetectSSE()) {
			UseSSE = 0;
		}
	}
	if (UseSSE2) {
		if (!CPU_DetectSSE2()) {
			UseSSE2 = 0;
		}
	}
#else
	UseSSE = 0;
	UseSSE2 = 0;
#endif
	if (DebugBit(DEBUG_BIT_BASIC)) dbgPrintf("utglr: UseSSE = %u\n", UseSSE);
	if (DebugBit(DEBUG_BIT_BASIC)) dbgPrintf("utglr: UseSSE2 = %u\n", UseSSE2);

	if (DynamicTexIdRecycleLevel < 10) DynamicTexIdRecycleLevel = 10;

	//Inherited members that read like config.
	//Never registered anywhere.
	AlwaysMipmap = 0;
	UseVertexSpecular = 1;

	//Klingon has no engine-side field to negotiate through.
#ifdef UTGLR_KLINGON_BUILD
	SupportsTC = 0;
#else
	SupportsTC = UseS3TC;
#endif

	BufferClippedActorTris = 1;

	UseDetailAlpha = 1;


	SUPPORTS_GL_EXT_texture_env_combine |= SUPPORTS_GL_ARB_texture_env_combine;

	//Fragment program mode uses vertex program defines.
	if (!SUPPORTS_GL_ARB_vertex_program) SUPPORTS_GL_ARB_fragment_program = 0;


	if (!SUPPORTS_GL_ARB_multitexture) UseMultiTexture = 0;
	if (!SUPPORTS_GL_EXT_secondary_color) UseVertexSpecular = 0;
	if (!SUPPORTS_GL_EXT_texture_compression_s3tc || !SUPPORTS_GL_ARB_texture_compression) SupportsTC = 0;

	ConfigValidate_RefreshDCV();

	ConfigValidate_RequiredExtensions();


	//<= 0 because the ini can set this to anything.
	//A negative reaches an unsigned loop counter downstream.
	//Every config value used as a count gets this treatment.
	if ((MaxTMUnits <= 0) || (MaxTMUnits > MAX_TMUNITS)) {
		MaxTMUnits = MAX_TMUNITS;
	}

	if (UseMultiTexture) {
		glGetIntegerv(GL_MAX_TEXTURE_UNITS_ARB, &TMUnits);
		debugf(TEXT("%i Texture Mapping Units found"), TMUnits);
		if (TMUnits > MaxTMUnits) {
			TMUnits = MaxTMUnits;
		}
	} else {
		TMUnits = 1;
	}

	//Driver-supplied, so clamp it.
	if (TMUnits < 1) {
		TMUnits = 1;
	} else if (TMUnits > MAX_TMUNITS) {
		TMUnits = MAX_TMUNITS;
	}


	ConfigValidate_Main();


	m_vpCurrent = 0;
	m_fpCurrent = 0;

	m_allocatedShaderNames = false;

	if (UseFragmentProgram) {
		TryInitializeFragmentProgramMode();
	}

	if (!UseHardwareGamma) {
		TryInitializeGammaPostProcess();
	}
	PL_UseHardwareGamma = UseHardwareGamma;
	PL_ReduceBanding = ReduceBanding;
	PL_ClientBrightness = Viewport->GetOuterUClient()->Brightness;
	//Seeds the per-frame comparison.
	PL_GammaOffset = GammaOffset;
	PL_GammaOffsetRed = GammaOffsetRed;
	PL_GammaOffsetGreen = GammaOffsetGreen;
	PL_GammaOffsetBlue = GammaOffsetBlue;
	PL_Brightness = Brightness;
	SetGamma(PL_ClientBrightness);


	if (MaxAnisotropy < 0) {
		MaxAnisotropy = 0;
	}
	if (MaxAnisotropy) {
		GLint iMaxAnisotropyLimit = 1;
		glGetIntegerv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &iMaxAnisotropyLimit);
		debugf(TEXT("MaxAnisotropy: %i"), iMaxAnisotropyLimit);
		if (MaxAnisotropy > iMaxAnisotropyLimit) {
			MaxAnisotropy = iMaxAnisotropyLimit;
		}
	}

	//Only the secondary-colour vertex mode needs the extension.
	//It checks for itself.
	BufferActorTris = 1;

	if (SupportsTC) {
		debugf(TEXT("Trying to use S3TC extension."));
	}

	if (MaxLogTextureSize <= 0) MaxLogTextureSize = 12;

	INT MaxTextureSize;
	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &MaxTextureSize);
	INT Dummy = -1;
	while (MaxTextureSize > 0) {
		MaxTextureSize >>= 1;
		Dummy++;
	}

	if ((MaxLogTextureSize > Dummy) || (SupportsTC)) MaxLogTextureSize = Dummy;
	if ((MinLogTextureSize < 2) || (SupportsTC)) MinLogTextureSize = 2;

	MaxLogUOverV = MaxLogTextureSize;
	MaxLogVOverU = MaxLogTextureSize;

	debugf(TEXT("MinLogTextureSize: %i"), MinLogTextureSize);
	debugf(TEXT("MaxLogTextureSize: %i"), MaxLogTextureSize);

	debugf(TEXT("BufferActorTris: %i"), BufferActorTris);

	debugf(TEXT("UseDetailAlpha: %i"), UseDetailAlpha);

	if (UseFragmentProgram) {
		debugf(TEXT("World rendering: ARB vertex/fragment program (texture coordinates generated on GPU)"));
	} else {
		debugf(TEXT("World rendering: fixed function multipass (texture coordinates regenerated per pass on CPU)"));
		if (!SUPPORTS_GL_ARB_vertex_program) {
			debugf(TEXT("   GL_ARB_vertex_program is not supported"));
		}
		if (!SUPPORTS_GL_ARB_fragment_program) {
			debugf(TEXT("   GL_ARB_fragment_program is not supported"));
		}
	}


	VertexArray = (FGLVertex *)AlignMemPtr(m_VertexArrayMem, VERTEX_ARRAY_ALIGN);
	for (u = 0; u < MAX_TMUNITS; u++) {
		TexCoordArray[u] = (FGLTexCoord *)AlignMemPtr(m_TexCoordArrayMem[u], VERTEX_ARRAY_ALIGN);
	}
	MapDotArray = (FGLMapDot *)AlignMemPtr(m_MapDotArrayMem, VERTEX_ARRAY_ALIGN);
	SingleColorArray = (FGLSingleColor *)AlignMemPtr(m_ColorArrayMem, VERTEX_ARRAY_ALIGN);
	DoubleColorArray = (FGLDoubleColor *)AlignMemPtr(m_ColorArrayMem, VERTEX_ARRAY_ALIGN);


	check(MinLogTextureSize >= 0);
	check(MaxLogTextureSize >= 0);
	check(MinLogTextureSize <= MaxLogTextureSize);

	InternalFlush();

	m_noTextureId = 0;
	m_alphaTextureId = 0;

	InitNoTextureSafe();

	// Set permanent state.
	glEnable(GL_DEPTH_TEST);
	glShadeModel(GL_SMOOTH);
	glEnable(GL_TEXTURE_2D);
	glAlphaFunc(GL_GREATER, 0.5f);
	glDepthMask(GL_TRUE);
	glDepthFunc(GL_LEQUAL);
	glPolygonOffset(-1.0f, -1.0f);
	glBlendFunc(GL_ONE, GL_ZERO);
	//Seeded to match the two calls above.
	m_curPolyOffsetFactor = -1.0f;
	m_curPolyOffsetUnits = -1.0f;
	m_polyOffsetEnabled = false;
	//Real values arrive per frame.
	m_detailPolyOffsetFactor = -1.0f;
	m_detailPolyOffsetUnits = -1.0f;
	m_moverPolyOffsetFactor = UTGLR_MOVER_POLY_OFFSET_FACTOR;
	m_moverPolyOffsetUnits = UTGLR_MOVER_POLY_OFFSET_UNITS;
	glDisable(GL_BLEND);
	glEnable(GL_DITHER);

#ifdef UTGLR_RUNE_BUILD
	glFogi(GL_FOG_MODE, GL_LINEAR);
	glFogf(GL_FOG_START, 0.0f);
#endif
	m_gpFogEnabled = false;

	if (LODBias) {
		SetTexLODBiasState(TMUnits);
	}

	if (UseDetailAlpha) {
		InitAlphaTextureSafe();
	}

	//Falls back to plain staging arrays where no buffer was made.
	InitVertexStreams();

	glEnableClientState(GL_VERTEX_ARRAY);
	if (UseMultiTexture) {
		//Driver bug workaround, bypassing the state setter.
		glClientActiveTextureARB(GL_TEXTURE0_ARB);
		m_curClientActiveTexUnit = 0;
	}
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);

	SetVertexStreamPointers();

	m_texEnableBits = 0x1;
	m_clientTexEnableBits = 0x1;

	//No chunk buffered yet.
	m_csBaseVertex = 0;

	BufferedVerts = 0;
	BufferedTileVerts = 0;
	BufferedLineVerts = 0;
	BufferedPointVerts = 0;

	DiscardDeferredGouraudPolys();

	//A fresh context's blend state already matches this.
	//Clearing the dirty flag matters too.
	m_curBlendFlags = PF_Occlude;
	m_blendStateInvalid = false;
	m_smoothMaskedTexturesBit = 0;
	m_useAlphaToCoverageForMasked = false;
	m_alphaToCoverageEnabled = false;
	m_curPolyFlags = 0;
	m_curPolyFlags2 = 0;

	InitOrInvalidateTexEnvState();
	SetPermanentTexEnvState(TMUnits);

	m_currentColorFlags = 0;
	m_requestedColorFlags = 0;

	m_useZRangeHack = false;
	m_nearZRangeHackProjectionActive = false;
	m_requestNearZRangeHackProjection = false;

	//Zeroed so the next frame's first draw refreshes.
	//A real scene node has a non zero width.
	appMemzero(&m_sceneNodeKey, sizeof(m_sceneNodeKey));

	//A float literal, because an int literal is ambiguous here.
	m_prevFrameTimestamp = 0.0f;


	//Initialize previous lock variables.
	PL_DetailTextures = DetailTextures;
	PL_OneXBlending = OneXBlending;
	PL_MaxLogUOverV = MaxLogUOverV;
	PL_MaxLogVOverU = MaxLogVOverU;
	PL_MinLogTextureSize = MinLogTextureSize;
	PL_MaxLogTextureSize = MaxLogTextureSize;
	PL_NoFiltering = NoFiltering;
	PL_AlwaysMipmap = AlwaysMipmap;
	PL_UseTrilinear = UseTrilinear;
	PL_Use16BitTextures = Use16BitTextures;
	PL_TexDXT1ToDXT3 = TexDXT1ToDXT3;
	PL_MaxAnisotropy = MaxAnisotropy;
	PL_SmoothMaskedTextures = SmoothMaskedTextures;
	PL_LODBias = LODBias;
	PL_UsePalette = UsePalette;
	PL_UseAlphaPalette = UseAlphaPalette;
	PL_UseDetailAlpha = UseDetailAlpha;
	PL_SinglePassDetail = SinglePassDetail;
	PL_UseFragmentProgram = UseFragmentProgram;
	PL_UseSSE = UseSSE;
	PL_UseSSE2 = UseSSE2;


	m_currentFrameCount = 0;

	WasFullscreen = Fullscreen;

	return 1;

	unguard;
}

void UOpenGLRenderDevice::UnsetRes() {
	guard(UOpenGLRenderDevice::UnsetRes);

#ifdef _WIN32
	check(m_hRC);
#endif

	/*
	Neither caller makes the context current, so without this the deletes below would land in
	whichever context was last current and each editor viewport has its own object namespace,
	which means one closing could delete another's textures out from under a viewport still
	running and the symptom is white geometry in a window nobody touched.
	*/
	MakeCurrent();

	/*
	Released only when this device owns what the bind cache points at, because with sharing
	off the arrays name objects in the context about to be deleted and they go now, while
	with sharing on they are process wide so only the last device out releases them, any
	earlier and another viewport loses cached binds for textures still current in its own
	context.
	*/
	if (!ShareLists || (NumDevices <= 1)) {
		InternalFlush();
	} else {
		DiscardDeferredGouraudPolys();
		for (unsigned int u = 0; u < MAX_TMUNITS; u++) {
			TexInfo[u].CurrentCacheID = TEX_CACHE_ID_UNUSED;
			TexInfo[u].pBind = NULL;
		}
	}

	if (m_noTextureId != 0) {
		glDeleteTextures(1, &m_noTextureId);
		m_noTextureId = 0;
	}
	if (m_alphaTextureId != 0) {
		glDeleteTextures(1, &m_alphaTextureId);
		m_alphaTextureId = 0;
	}

	ShutdownGammaPostProcess();

	//Release the streaming buffers while the context is current.
	ShutdownVertexStreams();

	ShutdownFragmentProgramMode();

#ifdef _WIN32
	hCurrentRC = NULL;
	wglMakeCurrent(NULL, NULL);
	//	verify(wglDeleteContext(m_hRC));
	wglDeleteContext(m_hRC);
	verify(AllContexts.RemoveItem(m_hRC) == 1);
	m_hRC = NULL;
	if (WasFullscreen) {
		TCHAR_CALL_OS(ChangeDisplaySettingsW(NULL, 0), ChangeDisplaySettingsA(NULL, 0));
	}
#endif

	unguard;
}


void UOpenGLRenderDevice::MakeCurrent(void) {
	guard(UOpenGLRenderDevice::MakeCurrent);
#ifdef _WIN32
	check(m_hRC);
	check(m_hDC);
	if (hCurrentRC != m_hRC) {
		verify(wglMakeCurrent(m_hDC, m_hRC));
		hCurrentRC = m_hRC;
	}
#endif
	unguard;
}

void UOpenGLRenderDevice::CheckGLErrorFlag(const TCHAR *pTag) {
	GLenum Error = glGetError();
	if ((Error != GL_NO_ERROR) && DebugBit(DEBUG_BIT_GL_ERROR)) {
		const TCHAR *pMsg;
		switch (Error) {
			case GL_INVALID_ENUM:
				pMsg = TEXT("GL_INVALID_ENUM");
				break;
			case GL_INVALID_VALUE:
				pMsg = TEXT("GL_INVALID_VALUE");
				break;
			case GL_INVALID_OPERATION:
				pMsg = TEXT("GL_INVALID_OPERATION");
				break;
			case GL_STACK_OVERFLOW:
				pMsg = TEXT("GL_STACK_OVERFLOW");
				break;
			case GL_STACK_UNDERFLOW:
				pMsg = TEXT("GL_STACK_UNDERFLOW");
				break;
			case GL_OUT_OF_MEMORY:
				pMsg = TEXT("GL_OUT_OF_MEMORY");
				break;
			default:
				pMsg = TEXT("UNKNOWN");
		}
		//appErrorf(TEXT("OpenGL Error: %s (%s)"), pMsg, pTag);
		debugf(TEXT("OpenGL Error: %s (%s)"), pMsg, pTag);
	}
}
