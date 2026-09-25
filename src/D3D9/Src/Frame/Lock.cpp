/*=============================================================================
	Lock.cpp: the start of a frame.

	Clears, device loss recovery, hit testing setup, and where a settings change made
	since the last frame is noticed and acted on.
=============================================================================*/

#include "../D3D9Drv.h"
#include "../D3D9.h"
#include "../Core/Globals.h"
#include "../Geometry/VertexBuffers.h"

void UD3D9RenderDevice::Lock(FPlane InFlashScale, FPlane InFlashFog, FPlane ScreenClear, DWORD RenderLockFlags, BYTE *InHitData, INT *InHitSize) {
#ifdef UTGLR_DEBUG_SHOW_CALL_COUNTS
	{
		static int si;
		dout << L"utd3d9r: Lock = " << si++ << std::endl;
	}
#endif
	guard(UD3D9RenderDevice::Lock);
	check(LockCount == 0);
	++LockCount;

	DiscardDeferredGouraudPolys();

	BindCycles = ImageCycles = ComplexCycles = GouraudCycles = TileCycles = 0;

	m_vpEnableCount = 0;
	m_vpSwitchCount = 0;
	m_fpEnableCount = 0;
	m_fpSwitchCount = 0;
	m_AASwitchCount = 0;
	m_sceneNodeCount = 0;
	m_sceneNodeRefreshCount = 0;
	m_vbFlushCount = 0;
	m_csBatchSurfaceCount = 0;
	m_csBatchDrawCount = 0;
	m_stat0Count = 0;
	m_stat1Count = 0;

	m_drawCallCount = 0;
	m_recordedCmdCount = 0;
	m_runCount = 0;
	m_buildJobCount = 0;
	m_buildMicroseconds = 0;

	m_drawCmds.clear();
	m_drawRuns.clear();
	m_passRuns.clear();
	ClearRecordedTextures();
	m_frameArena.Reset();


	HRESULT hResult;

	m_frameSkipped = (m_d3dDevice == NULL);
	if (m_frameSkipped) {
		hResult = D3D_OK;
	} else if (FAILED(hResult = m_d3dDevice->TestCooperativeLevel())) {
#if 0
{
	dout << L"utd3d9r: Device Lost" << std::endl;
}
#endif
		for (INT waitCount = 0; (hResult == D3DERR_DEVICELOST) && (waitCount < 10); waitCount++) {
			Sleep(10);
			hResult = m_d3dDevice->TestCooperativeLevel();
		}

		if (hResult == D3DERR_DEVICENOTRESET) {
			m_SetRes_isDeviceReset = true;
			if (!SetRes(m_SetRes_NewX, m_SetRes_NewY, m_SetRes_NewColorBytes, m_SetRes_Fullscreen)) {
				appErrorf(TEXT("Failed to reset lost D3D device"));
			}
		} else if (hResult == D3DERR_DEVICELOST) {
			m_frameSkipped = true;
		}
		/*
		The wait loop exits on anything that isn't a lost device, including one that has simply
		recovered on its own, which is undocumented, a lost device being supposed to pass through
		a resettable state first, but the normal frame path is still the right answer to a device
		that reports itself as fine, and the alternative of insisting on the documented sequence
		is a renderer that waits forever on hardware which has already handed the device back.
		*/
		else if (SUCCEEDED(hResult)) {
			if (m_deferredDeviceReset) {
				m_SetRes_isDeviceReset = true;
				if (!SetRes(m_SetRes_NewX, m_SetRes_NewY, m_SetRes_NewColorBytes, m_SetRes_Fullscreen)) {
					appErrorf(TEXT("Failed to reset lost D3D device"));
				}
			}
		}
		//If not lost and not ready to be restored, error. Meant for D3DERR_DRIVERINTERNALERROR.
		else {
			appErrorf(TEXT("Error checking for lost D3D device"));
		}
	}

	if (!m_frameSkipped) {
		if (FAILED(m_d3dDevice->BeginScene())) {
			m_frameSkipped = true;
		}
	}

	if (!m_frameSkipped) {
		{
			//The viewport still holds the previous frame's last scene node.
			D3DVIEWPORT9 savedViewport;
			bool restoreViewport = SUCCEEDED(m_d3dDevice->GetViewport(&savedViewport)) ? true : false;

			D3DVIEWPORT9 fullViewport;
			fullViewport.X = 0;
			fullViewport.Y = 0;
			fullViewport.Width = m_d3dpp.BackBufferWidth;
			fullViewport.Height = m_d3dpp.BackBufferHeight;
			fullViewport.MinZ = 0.0f;
			fullViewport.MaxZ = 1.0f;
			m_d3dDevice->SetViewport(&fullViewport);

			SetBlend(PF_Occlude);
			m_d3dDevice->Clear(0, NULL, D3DCLEAR_ZBUFFER | ((RenderLockFlags & LOCKR_ClearScreen) ? D3DCLEAR_TARGET : 0), (DWORD)FColor(ScreenClear).TrueColor(), 1.0f, 0);

			if (restoreViewport) {
				m_d3dDevice->SetViewport(&savedViewport);
			}
		}
		m_d3dDevice->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
	}


	bool flushTextures = false;
	bool needShaderReload = false;


	ConfigValidate_RefreshDCV();

	ConfigValidate_RequiredExtensions();


	ConfigValidate_Main();


	if (!m_frameSkipped) {
		if (OneXBlending != PL_OneXBlending) {
			PL_OneXBlending = OneXBlending;
			InitOrInvalidateTexEnvState();
		}

		/*
		Both pick between gamma paths and are read once per device, so toggling either at runtime
		used to do nothing at all. UseHardwareGamma was worse still: both corrections could stack
		into a washed out image.
		*/
		if ((ReduceBanding != PL_ReduceBanding) || (UseHardwareGamma != PL_UseHardwareGamma)) {
			PL_ReduceBanding = ReduceBanding;
			PL_UseHardwareGamma = UseHardwareGamma;
			FreeGammaResources();
			InitGammaResourcesSafe();
			SetGamma(Viewport->GetOuterUClient()->Brightness);
			PL_ClientBrightness = Viewport->GetOuterUClient()->Brightness;
		}

		//Here and only here.
		{
			const FLOAT ClientBrightness = Viewport->GetOuterUClient()->Brightness;
			const bool gammaSettingsChanged =
				(GammaOffset != PL_GammaOffset) ||
				(GammaOffsetRed != PL_GammaOffsetRed) ||
				(GammaOffsetGreen != PL_GammaOffsetGreen) ||
				(GammaOffsetBlue != PL_GammaOffsetBlue) ||
				(Brightness != PL_Brightness);

			if ((ClientBrightness != PL_ClientBrightness) || gammaSettingsChanged) {
				PL_ClientBrightness = ClientBrightness;
				PL_GammaOffset = GammaOffset;
				PL_GammaOffsetRed = GammaOffsetRed;
				PL_GammaOffsetGreen = GammaOffsetGreen;
				PL_GammaOffsetBlue = GammaOffsetBlue;
				PL_Brightness = Brightness;
				SetGamma(ClientBrightness);
			}
		}

		MaxLogUOverV = PL_MaxLogUOverV;
		MaxLogVOverU = PL_MaxLogVOverU;
		MinLogTextureSize = PL_MinLogTextureSize;
		MaxLogTextureSize = PL_MaxLogTextureSize;

		if (NoFiltering != PL_NoFiltering) {
			PL_NoFiltering = NoFiltering;
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
		if (Use565Textures != PL_Use565Textures) {
			PL_Use565Textures = Use565Textures;
			flushTextures = true;
		}
		if (TexDXT1ToDXT3 != PL_TexDXT1ToDXT3) {
			PL_TexDXT1ToDXT3 = TexDXT1ToDXT3;
			flushTextures = true;
		}
		if (GenerateMipMaps != PL_GenerateMipMaps) {
			PL_GenerateMipMaps = GenerateMipMaps;
			flushTextures = true;
		}
		if (MaxAnisotropy < 0) {
			MaxAnisotropy = 0;
		}
		if ((DWORD)MaxAnisotropy > m_d3dCaps.MaxAnisotropy) {
			MaxAnisotropy = m_d3dCaps.MaxAnisotropy;
		}
		if (MaxAnisotropy != PL_MaxAnisotropy) {
			PL_MaxAnisotropy = MaxAnisotropy;
			flushTextures = true;

			m_anisotropicMagFilterCap = (MaxAnisotropy != 0) && ((m_d3dCaps.TextureFilterCaps & D3DPTFILTERCAPS_MAGFANISOTROPIC) != 0);

			SetTexMaxAnisotropyState(TMUnits);
		}

		if (SmoothMaskedTextures != PL_SmoothMaskedTextures) {
			PL_SmoothMaskedTextures = SmoothMaskedTextures;

			SetBlend(PF_Occlude);

		}

		if (LODBias != PL_LODBias) {
			PL_LODBias = LODBias;
			SetTexLODBiasState(TMUnits);
		}

		if (DetailTextures != PL_DetailTextures) {
			PL_DetailTextures = DetailTextures;
			flushTextures = true;
			if (DetailTextures) {
				needShaderReload = true;
			}
		}

		if (UseDetailAlpha != PL_UseDetailAlpha) {
			PL_UseDetailAlpha = UseDetailAlpha;
			if (UseDetailAlpha) {
				InitAlphaTextureSafe();
			}
		}

		if (SinglePassDetail != PL_SinglePassDetail) {
			PL_SinglePassDetail = SinglePassDetail;
			if (SinglePassDetail) {
				needShaderReload = true;
			}
		}


		if (UseFragmentProgram != PL_UseFragmentProgram) {
			PL_UseFragmentProgram = UseFragmentProgram;
			if (UseFragmentProgram) {
				TryInitializeFragmentProgramMode();
				needShaderReload = false;
			} else {
				ShutdownFragmentProgramMode();

				/*
				The shader path never touches the fixed function alpha test state, so a stale
				masked flag would have the next masked draw skip re-establishing it, and it has to
				be a dirty flag because the comparison works on differing bits and no sentinel
				stored in the old value can force one. Set ahead of the alpha test resync below.
				*/
				m_blendStateInvalid = true;
			}
		}

		if (UseFragmentProgram) {
			if (needShaderReload) {
				TryInitializeFragmentProgramMode();
			}
		}

		if (UseFragmentProgram) {
			if (m_alphaTestEnabled) {
				m_alphaTestEnabled = false;
				m_d3dDevice->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);

				SetBlend(PF_Occlude);
			}
		}

		m_smoothMaskedTexturesBit = 0;
		m_useAlphaToCoverageForMasked = false;
		if (SmoothMaskedTextures) {
			if (UseFragmentProgram && m_usingAA && (m_initNumAASamples >= 4) && m_supportsAlphaToCoverage) {
				m_useAlphaToCoverageForMasked = true;
			} else {
				m_smoothMaskedTexturesBit = PF_Masked;
			}
		}

		/*
		The per-draw blend path gates both enabling and disabling this vendor state on the same
		flag, so a value going false between an enable and its matching disable strands the device
		with the state stuck on, after which every later masked draw derives its coverage from
		alpha and sprites and overlays read as dithered speckle.
		*/
		if (!m_useAlphaToCoverageForMasked && m_alphaToCoverageEnabled) {
			m_alphaToCoverageEnabled = false;
			DisableAlphaToCoverageNoCheck();
		}
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

#ifdef UTGLR_INCLUDE_SSE_CODE
	m_useSSSE3 = (UseSSE2 != 0) && CPU_DetectSSSE3();
#else
	m_useSSSE3 = false;
#endif


	//Shader mode only.
	m_sevenBitMapRecon = m_sevenBitMapReconLoaded && UseFragmentProgram && ReduceBanding && !NoFiltering;

	if (UseFragmentProgram) {
		//Lightmap blend scale.
		m_fsBlendInfo[3] = (OneXBlending) ? 1.0f : 2.0f;

		if (!m_frameSkipped) {
			m_d3dDevice->SetPixelShaderConstantF(0, m_fsBlendInfo, 1);
		}

		if (m_sevenBitMapRecon && !m_frameSkipped) {
			m_d3dDevice->SetPixelShaderConstantF(SEVEN_BIT_MAP_CONST_REG, g_sevenBitMapParams, 5);
		}
		m_sevenBitMapSizesValid = false;
	}


	if (DeferredRecording && UseFragmentProgram && !m_deferredReady && !m_deferredGaveUp) {
		StartJobSystem();
	}
	/*
	Its own gate: nested inside the pool's,
	this could only ever run on the frame the pool came up.
	A reset taken while the pool was off releases the instance resources and returns early with the
	readiness flag still set.
	*/
	if (DeferredRecording && UseFragmentProgram && m_deferredReady &&
		!m_instancingAvailable && !m_instancingGaveUp && !m_deferredGaveUp && !m_frameSkipped) {
		InitInstanceResources();
	}


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
		m_pRenderPassesNoCheckSetupProc = &UD3D9RenderDevice::RenderPassesNoCheckSetup_FP;
		m_pRenderPassesNoCheckSetup_SingleOrDualTextureAndDetailTextureProc = &UD3D9RenderDevice::RenderPassesNoCheckSetup_SingleOrDualTextureAndDetailTexture_FP;
	} else {
		m_pRenderPassesNoCheckSetupProc = &UD3D9RenderDevice::RenderPassesNoCheckSetup;
		m_pRenderPassesNoCheckSetup_SingleOrDualTextureAndDetailTextureProc = &UD3D9RenderDevice::RenderPassesNoCheckSetup_SingleOrDualTextureAndDetailTexture;
	}

	m_pBufferDetailTextureDataProc = &UD3D9RenderDevice::BufferDetailTextureData;
#ifdef UTGLR_INCLUDE_SSE_CODE
	if (UseSSE2) {
		m_pBufferDetailTextureDataProc = &UD3D9RenderDevice::BufferDetailTextureData_SSE2;
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
		m_detailTextureColor4ub = 0x00408040;
	} else {
		m_detailTextureColor4ub = 0x00808080;
	}

	FlashScale = InFlashScale;
	FlashFog = InFlashFog;

	m_HitData = InHitData;
	m_HitSize = InHitSize;
	m_HitCount = 0;
	if (m_HitData) {
		m_HitBufSize = *m_HitSize;
		*m_HitSize = 0;

		m_gclip.SelectModeStart();

		//Clip planes are programmed once per frame and cleared at the end of every hit-test frame,
		//so the cached scene node key resets with them.
		appMemzero(&m_sceneNodeKey, sizeof(m_sceneNodeKey));
	}

	if (flushTextures) {
		InternalFlush();
	}

	unguard;
}
