/*=============================================================================
	Lock.cpp: what happens when a frame begins.

	Clears, hit testing setup, and settings changed since the last frame.
=============================================================================*/

#include "../OpenGLDrv.h"
#include "../OpenGL.h"
#include "../Geometry/VertexBuffers.h"

void UOpenGLRenderDevice::Lock(FPlane InFlashScale, FPlane InFlashFog, FPlane ScreenClear, DWORD RenderLockFlags, BYTE *InHitData, INT *InHitSize) {
	UTGLR_DEBUG_CALL_COUNT(Lock);
	guard(UOpenGLRenderDevice::Lock);
	check(LockCount == 0);
	++LockCount;

	//Nothing held may cross into this frame.
	DiscardDeferredGouraudPolys();

	BindCycles = ImageCycles = ComplexCycles = GouraudCycles = TileCycles = 0;

	m_vpEnableCount = 0;
	m_vpSwitchCount = 0;
	m_fpEnableCount = 0;
	m_fpSwitchCount = 0;
	m_AASwitchCount = 0;
	m_sceneNodeCount = 0;
	m_sceneNodeRefreshCount = 0;


	MakeCurrent();

	if (!UseZTrick || GIsEditor || (RenderLockFlags & LOCKR_ClearScreen)) {
		glClearColor(ScreenClear.X, ScreenClear.Y, ScreenClear.Z, ScreenClear.W);
		glClearDepth(1.0);
		glDepthRange(0.0, 1.0);
		ZTrickFunc = GL_LEQUAL;
		SetPolygonOffsetsForDepthDirection(false);
		SetBlend(PF_Occlude);
		glClear(GL_DEPTH_BUFFER_BIT | ((RenderLockFlags & LOCKR_ClearScreen) ? GL_COLOR_BUFFER_BIT : 0));
	} else if (ZTrickToggle) {
		ZTrickToggle = 0;
		glClearDepth(0.5);
		glDepthRange(0.0, 0.5);
		ZTrickFunc = GL_LEQUAL;
		SetPolygonOffsetsForDepthDirection(false);
	} else {
		ZTrickToggle = 1;
		glClearDepth(0.5);
		glDepthRange(1.0, 0.5);
		ZTrickFunc = GL_GEQUAL;
		SetPolygonOffsetsForDepthDirection(true);
	}
	glDepthFunc((GLenum)ZTrickFunc);


	//Scan for textures deleted by another context.
	ScanForDeletedTextures();


	bool flushTextures = false;


	ConfigValidate_RefreshDCV();

	ConfigValidate_RequiredExtensions();


	ConfigValidate_Main();


	if (OneXBlending != PL_OneXBlending) {
		PL_OneXBlending = OneXBlending;
		InitOrInvalidateTexEnvState();
	}

	//Locked in at startup.
	MaxLogUOverV = PL_MaxLogUOverV;
	MaxLogVOverU = PL_MaxLogVOverU;
	MinLogTextureSize = PL_MinLogTextureSize;
	MaxLogTextureSize = PL_MaxLogTextureSize;

	if (NoFiltering != PL_NoFiltering) {
		PL_NoFiltering = NoFiltering;
		flushTextures = true;
	}
	if (AlwaysMipmap != PL_AlwaysMipmap) {
		PL_AlwaysMipmap = AlwaysMipmap;
		flushTextures = true;
	}
	if (UseTrilinear != PL_UseTrilinear) {
		PL_UseTrilinear = UseTrilinear;
		flushTextures = true;
	}
	if (Use16BitTextures != PL_Use16BitTextures) {
		PL_Use16BitTextures = Use16BitTextures;
		flushTextures = true;
	}
	if (TexDXT1ToDXT3 != PL_TexDXT1ToDXT3) {
		PL_TexDXT1ToDXT3 = TexDXT1ToDXT3;
		flushTextures = true;
	}
	if (MaxAnisotropy < 0) {
		MaxAnisotropy = 0;
	}
	if (MaxAnisotropy != PL_MaxAnisotropy) {
		if (MaxAnisotropy) {
			GLint iMaxAnisotropyLimit = 1;
			glGetIntegerv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &iMaxAnisotropyLimit);
			if (MaxAnisotropy > iMaxAnisotropyLimit) {
				MaxAnisotropy = iMaxAnisotropyLimit;
			}
		}

		PL_MaxAnisotropy = MaxAnisotropy;
		flushTextures = true;
	}

	if (SmoothMaskedTextures != PL_SmoothMaskedTextures) {
		PL_SmoothMaskedTextures = SmoothMaskedTextures;

		SetBlend(PF_Occlude);

		if (m_alphaToCoverageEnabled) {
			m_alphaToCoverageEnabled = false;
			glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);
		}
	}

	//Smooth masked textures controls alpha blend for masked textures.
	m_smoothMaskedTexturesBit = 0;
	m_useAlphaToCoverageForMasked = false;
	if (SmoothMaskedTextures) {
		if (m_usingAA && (m_initNumAASamples >= 4)) {
			m_useAlphaToCoverageForMasked = true;
		} else {
			m_smoothMaskedTexturesBit = PF_Masked;
		}
	}

	if (LODBias != PL_LODBias) {
		PL_LODBias = LODBias;
		SetTexLODBiasState(TMUnits);
	}

	if (UsePalette != PL_UsePalette) {
		PL_UsePalette = UsePalette;
		flushTextures = true;
	}
	if (UseAlphaPalette != PL_UseAlphaPalette) {
		PL_UseAlphaPalette = UseAlphaPalette;
		flushTextures = true;
	}

	if (DetailTextures != PL_DetailTextures) {
		PL_DetailTextures = DetailTextures;
		flushTextures = true;
	}

	//Tracked only.
	//The flag can only go from on to off.
	if (UseDetailAlpha != PL_UseDetailAlpha) {
		PL_UseDetailAlpha = UseDetailAlpha;
	}

	if (SinglePassDetail != PL_SinglePassDetail) {
		PL_SinglePassDetail = SinglePassDetail;
	}


	if (UseFragmentProgram != PL_UseFragmentProgram) {
		PL_UseFragmentProgram = UseFragmentProgram;
		if (UseFragmentProgram) {
			TryInitializeFragmentProgramMode();
		} else {
			ShutdownFragmentProgramMode();
		}
	}


	//The one place this is followed.
	{
		FLOAT ClientBrightness = Viewport->GetOuterUClient()->Brightness;

		//A live edit rebuilds it.
		const bool gammaSettingsChanged =
			(GammaOffset != PL_GammaOffset) ||
			(GammaOffsetRed != PL_GammaOffsetRed) ||
			(GammaOffsetGreen != PL_GammaOffsetGreen) ||
			(GammaOffsetBlue != PL_GammaOffsetBlue) ||
			(Brightness != PL_Brightness);

		//Read only when the gamma pass initializes.
		const bool reduceBandingChanged = (ReduceBanding != PL_ReduceBanding);
		PL_ReduceBanding = ReduceBanding;

		if (UseHardwareGamma != PL_UseHardwareGamma) {
			PL_UseHardwareGamma = UseHardwareGamma;
			if (!UseHardwareGamma) {
				//Both settings, one place.
				TryInitializeGammaPostProcess();
			} else {
				ShutdownGammaPostProcess();
			}

			PL_ClientBrightness = ClientBrightness;
			SetGamma(ClientBrightness);
		} else if (reduceBandingChanged && !UseHardwareGamma) {
			//Torn down first.
			ShutdownGammaPostProcess();
			TryInitializeGammaPostProcess();

			PL_ClientBrightness = ClientBrightness;
			SetGamma(ClientBrightness);
		} else if ((ClientBrightness != PL_ClientBrightness) || gammaSettingsChanged) {
			PL_ClientBrightness = ClientBrightness;
			SetGamma(ClientBrightness);
		}

		if (gammaSettingsChanged) {
			PL_GammaOffset = GammaOffset;
			PL_GammaOffsetRed = GammaOffsetRed;
			PL_GammaOffsetGreen = GammaOffsetGreen;
			PL_GammaOffsetBlue = GammaOffsetBlue;
			PL_Brightness = Brightness;
		}
	}


	if (SwapInterval != PL_SwapInterval) {
		PL_SwapInterval = SwapInterval;
		SetSwapIntervalSafe();
	}


	if (UseSSE != PL_UseSSE) {
#ifdef UTGLR_INCLUDE_SSE_CODE
		if (UseSSE) {
			if (!CPU_DetectSSE()) {
				UseSSE = 0;
			}
		}
#else
		UseSSE = 0;
#endif
		PL_UseSSE = UseSSE;
	}
	if (UseSSE2 != PL_UseSSE2) {
#ifdef UTGLR_INCLUDE_SSE_CODE
		if (UseSSE2) {
			if (!CPU_DetectSSE2()) {
				UseSSE2 = 0;
			}
		}
#else
		UseSSE2 = 0;
#endif
		PL_UseSSE2 = UseSSE2;
	}


	m_complexSurfaceColor3f_1f[0] = 1.0f;
	m_complexSurfaceColor3f_1f[1] = 1.0f;
	m_complexSurfaceColor3f_1f[2] = 1.0f;
	//Alpha for fragment program lightmap blend scaling.
	m_complexSurfaceColor3f_1f[3] = (OneXBlending) ? 0.0f : 1.0f;

	/*
	Whether the lightmap and fog map get reconstructed this frame, derived per frame so that
	every setting behind it takes effect on the next one, and the fixed function path has
	nowhere to put a reconstruction so it never gets one at all whatever the settings behind
	it happen to say.
	*/
	m_sevenBitMapRecon = m_sevenBitMapReconLoaded && UseFragmentProgram && ReduceBanding && !NoFiltering;


	m_pBuffer3BasicVertsProc = Buffer3BasicVerts;
	m_pBuffer3ColoredVertsProc = Buffer3ColoredVerts;
	m_pBuffer3FoggedVertsProc = Buffer3FoggedVerts;

#ifdef UTGLR_INCLUDE_SSE_CODE
	if (UseSSE) {
		m_pBuffer3ColoredVertsProc = Buffer3ColoredVerts_SSE;
		m_pBuffer3FoggedVertsProc = Buffer3FoggedVerts_SSE;
	}
	if (UseSSE2) {
		m_pBuffer3ColoredVertsProc = Buffer3ColoredVerts_SSE2;
		m_pBuffer3FoggedVertsProc = Buffer3FoggedVerts_SSE2;
	}
#endif //UTGLR_INCLUDE_SSE_CODE

	m_pBuffer3VertsProc = NULL;


	if (UseFragmentProgram) {
		m_pRenderPassesNoCheckSetupProc = &UOpenGLRenderDevice::RenderPassesNoCheckSetup_FP;
		m_pRenderPassesNoCheckSetup_SingleOrDualTextureAndDetailTextureProc = &UOpenGLRenderDevice::RenderPassesNoCheckSetup_SingleOrDualTextureAndDetailTexture_FP;
	} else {
		m_pRenderPassesNoCheckSetupProc = &UOpenGLRenderDevice::RenderPassesNoCheckSetup;
		m_pRenderPassesNoCheckSetup_SingleOrDualTextureAndDetailTextureProc = &UOpenGLRenderDevice::RenderPassesNoCheckSetup_SingleOrDualTextureAndDetailTexture;
	}

	m_pBufferDetailTextureDataProc = &UOpenGLRenderDevice::BufferDetailTextureData;
#ifdef UTGLR_INCLUDE_SSE_CODE
	if (UseSSE2) {
		m_pBufferDetailTextureDataProc = &UOpenGLRenderDevice::BufferDetailTextureData_SSE2;
	}
#endif //UTGLR_INCLUDE_SSE_CODE


	if (!BufferActorTris) {
		m_bufferActorTrisCutoff = 0;
	} else if (!BufferClippedActorTris) {
		m_bufferActorTrisCutoff = 3;
	} else {
		m_bufferActorTrisCutoff = MAX_BUFFERED_GP_PTS;
	}

	if (ColorizeDetailTextures) {
		m_detailTextureColor3f_1f[0] = 0.25f;
		m_detailTextureColor3f_1f[1] = 0.5f;
		m_detailTextureColor3f_1f[2] = 0.25f;
		m_detailTextureColor4ub = 0x00408040;
	} else {
		m_detailTextureColor3f_1f[0] = 0.5f;
		m_detailTextureColor3f_1f[1] = 0.5f;
		m_detailTextureColor3f_1f[2] = 0.5f;
		m_detailTextureColor4ub = 0x00808080;
	}
	//Alpha for fragment program lightmap blend scaling.
	m_detailTextureColor3f_1f[3] = (OneXBlending) ? 0.0f : 1.0f;

	FlashScale = InFlashScale;
	FlashFog = InFlashFog;

	m_HitData = InHitData;
	m_HitSize = InHitSize;
	m_HitCount = 0;
	if (m_HitData) {
		m_HitBufSize = *m_HitSize;
		*m_HitSize = 0;

		m_gclip.SelectModeStart();

		/*
		Clip planes are programmed once per frame and cleared at the end of every hit-test frame
		so the cached node key has to be reset too, because a hit pass reusing last frame's node
		would clip against nothing and pick whatever is nearest the camera, which in the editor
		reads as a click selecting the wrong brush.
		*/
		appMemzero(&m_sceneNodeKey, sizeof(m_sceneNodeKey));
	}

	if (flushTextures) {
		InternalFlush();
	}

	unguard;
}
