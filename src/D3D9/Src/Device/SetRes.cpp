/*=============================================================================
	SetRes.cpp: choosing a display mode and creating the Direct3D device.
=============================================================================*/

#include "../D3D9Drv.h"
#include "../D3D9.h"
#include "../Core/Globals.h"

UBOOL UD3D9RenderDevice::SetRes(INT NewX, INT NewY, INT NewColorBytes, UBOOL Fullscreen) {
	guard(UD3D9RenderDevice::SetRes);

	HRESULT hResult;
	bool saved_SetRes_isDeviceReset;

	{
		INT i = 0;
		if (!GConfig->GetInt(g_pSection, TEXT("DebugBits"), i)) i = 0;
		m_debugBits = i;
		m_numLoggedTiles = 0;
	}
	if (DebugBit(DEBUG_BIT_ANY)) dout << TEXT("utd3d9r: DebugBits = ") << m_debugBits << std::endl;


	debugf(TEXT("Enter SetRes()"));

	m_SetRes_NewX = NewX;
	m_SetRes_NewY = NewY;
	m_SetRes_NewColorBytes = NewColorBytes;
	m_SetRes_Fullscreen = Fullscreen;

	saved_SetRes_isDeviceReset = m_SetRes_isDeviceReset;
	m_SetRes_isDeviceReset = false;

	if (m_d3dDevice && !saved_SetRes_isDeviceReset && !Fullscreen && !WasFullscreen && (NewColorBytes == Viewport->ColorBytes)) {
		if (!Viewport->ResizeViewport(BLIT_HardwarePaint | BLIT_Direct3D, NewX, NewY, NewColorBytes)) {
			return 0;
		}

		FreePermanentResources();

		NewX = Viewport->SizeX;
		NewY = Viewport->SizeY;

		//Don't break editor and tiny windowed mode
		if (NewX < 16) NewX = 16;
		if (NewY < 16) NewY = 16;

		//Updated to the size actually adopted, a lost-device recovery replaying these.
		m_SetRes_NewX = NewX;
		m_SetRes_NewY = NewY;

		m_d3dpp.BackBufferWidth = NewX;
		m_d3dpp.BackBufferHeight = NewY;

		hResult = m_d3dDevice->Reset(&m_d3dpp);
		if (FAILED(hResult)) {
			/*
			A lost device is ordinary and recoverable, so a resize that happens to notice one
			first abandons what it was doing and lets the next per-frame recovery pick the device
			up again using the size saved just above, and it still returns 1 because the viewport
			itself did resize, where 0 would have the engine treat the whole mode change as
			refused.
			*/
			if (hResult == D3DERR_DEVICELOST) {
				debugf(TEXT("D3D device lost during window resize; deferring to the next frame"));
				m_deferredDeviceReset = true;
				return 1;
			}
			appErrorf(TEXT("Failed to reset D3D device for new window size (0x%08X)"), hResult);
		}

		InitPermanentResourcesAndRenderingState();

		//Reset drops the ramp.
		SetGamma(Viewport->GetOuterUClient()->Brightness);

		D3DVIEWPORT9 d3dViewport;
		d3dViewport.X = 0;
		d3dViewport.Y = 0;
		d3dViewport.Width = NewX;
		d3dViewport.Height = NewY;
		d3dViewport.MinZ = 0.0f;
		d3dViewport.MaxZ = 1.0f;
		m_d3dDevice->SetViewport(&d3dViewport);

		return 1;
	}


	if (m_d3d9) {
		debugf(TEXT("UnSetRes() -> m_d3d9 != NULL"));
		UnsetRes();
	}

	m_d3d9 = pDirect3DCreate9(D3D_SDK_VERSION);
	if (!m_d3d9) {
		appErrorf(TEXT("Direct3DCreate9 failed"));
	}

	//Snapping to the nearest enumerated mode would override the configured size every time.
	if (Fullscreen && !IsFullscreenModeSupported(NewX, NewY, NewColorBytes)) {
		INT FindX = NewX, FindY = NewY, BestError = MAXINT;
		for (INT i = 0; i < Modes.Num(); i++) {
			if (Modes(i).Z == NewColorBytes * 8) {
				INT Error = (Modes(i).X - FindX) * (Modes(i).X - FindX) + (Modes(i).Y - FindY) * (Modes(i).Y - FindY);
				if (Error < BestError) {
					NewX = Modes(i).X;
					NewY = Modes(i).Y;
					BestError = Error;
				}
			}
		}
		if ((NewX != FindX) || (NewY != FindY)) {
			debugf(TEXT("%ix%i is not an available fullscreen mode; using %ix%i"), FindX, FindY, NewX, NewY);
		}
	}

	UBOOL Result = Viewport->ResizeViewport(Fullscreen ? (BLIT_Fullscreen | BLIT_Direct3D) : (BLIT_HardwarePaint | BLIT_Direct3D), NewX, NewY, NewColorBytes);
	if (!Result) {
		m_d3d9->Release();
		m_d3d9 = NULL;
		return 0;
	}


	hResult = m_d3d9->GetDeviceCaps(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, &m_d3dCaps);
	if (FAILED(hResult)) {
		appErrorf(TEXT("GetDeviceCaps failed (0x%08X)"), hResult);
	}

	m_isATI = false;
	m_isNVIDIA = false;
	{
		D3DADAPTER_IDENTIFIER9 ident;

		hResult = m_d3d9->GetAdapterIdentifier(D3DADAPTER_DEFAULT, 0, &ident);
		if (FAILED(hResult)) {
			appErrorf(TEXT("GetAdapterIdentifier failed (0x%08X)"), hResult);
		}

		debugf(TEXT("D3D adapter driver      : %s"), appFromAnsi(ident.Driver));
		debugf(TEXT("D3D adapter description : %s"), appFromAnsi(ident.Description));
		debugf(TEXT("D3D adapter id          : 0x%04X:0x%04X"), ident.VendorId, ident.DeviceId);

		if (ident.VendorId == 0x1002) {
			m_isATI = true;
		} else if (ident.VendorId == 0x10DE) {
			m_isNVIDIA = true;
		}
	}


	D3DDISPLAYMODE d3ddm;
	hResult = m_d3d9->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &d3ddm);
	if (FAILED(hResult)) {
		appErrorf(TEXT("Failed to get current display mode (0x%08X)"), hResult);
	}

	if (saved_SetRes_isDeviceReset && !Fullscreen) {
		switch (d3ddm.Format) {
			case D3DFMT_R5G6B5: NewColorBytes = 2; break;
			case D3DFMT_X1R5G5B5: NewColorBytes = 2; break;
			case D3DFMT_A1R5G5B5: NewColorBytes = 2; break;
			default:
				NewColorBytes = 4;
		}
	}
	m_SetRes_NewColorBytes = NewColorBytes;

	//Don't break editor and tiny windowed mode
	if (NewX < 16) NewX = 16;
	if (NewY < 16) NewY = 16;

	//Set presentation parameters.
	appMemzero(&m_d3dpp, sizeof(m_d3dpp));
	m_d3dpp.Windowed = TRUE;
	m_d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
	m_d3dpp.BackBufferWidth = NewX;
	m_d3dpp.BackBufferHeight = NewY;
	m_d3dpp.BackBufferFormat = d3ddm.Format;
	m_d3dpp.EnableAutoDepthStencil = TRUE;

	if (Fullscreen) {
		m_d3dpp.Windowed = FALSE;
		m_d3dpp.BackBufferFormat = (NewColorBytes <= 2) ? D3DFMT_R5G6B5 : D3DFMT_X8R8G8B8;
	}

	//Most precise first.
	static const struct {
		D3DFORMAT Format;
		DWORD NumBits;
	} depthFormats[] = {
		{ D3DFMT_D32, 32 },
		{ D3DFMT_D24X8, 24 },
		{ D3DFMT_D24S8, 24 },
		{ D3DFMT_D16, 16 }
	};

	m_d3dpp.AutoDepthStencilFormat = D3DFMT_D16;
	m_numDepthBits = 16;
	for (INT i = 0; i < (INT)ARRAY_COUNT(depthFormats); i++) {
		if (CheckDepthFormat(d3ddm.Format, m_d3dpp.BackBufferFormat, depthFormats[i].Format)) {
			m_d3dpp.AutoDepthStencilFormat = depthFormats[i].Format;
			m_numDepthBits = depthFormats[i].NumBits;
			break;
		}
	}

	//The user's AA toggle is set once in the constructor and left alone here.
	m_usingAA = false;
	m_initNumAASamples = 0;

	UseAA = (NumAASamples != 0) ? 1 : 0;

	//The multisample enum values are themselves the sample counts.
	if (UseAA && (NumAASamples >= 2)) {
		INT requestedSamples = (NumAASamples > 16) ? 16 : NumAASamples;

		for (INT samples = requestedSamples; samples >= 2; samples--) {
			D3DMULTISAMPLE_TYPE multiSampleType = (D3DMULTISAMPLE_TYPE)samples;
			DWORD backBufferQualityLevels;
			DWORD depthBufferQualityLevels;

			hResult = m_d3d9->CheckDeviceMultiSampleType(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, m_d3dpp.BackBufferFormat, m_d3dpp.Windowed, multiSampleType, &backBufferQualityLevels);
			if (FAILED(hResult)) {
				continue;
			}
			hResult = m_d3d9->CheckDeviceMultiSampleType(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, m_d3dpp.AutoDepthStencilFormat, m_d3dpp.Windowed, multiSampleType, &depthBufferQualityLevels);
			if (FAILED(hResult)) {
				continue;
			}
			if ((backBufferQualityLevels < 1) || (depthBufferQualityLevels < 1)) {
				continue;
			}

			m_d3dpp.MultiSampleType = multiSampleType;
			m_d3dpp.MultiSampleQuality = 0;
			m_usingAA = true;
			m_initNumAASamples = samples;
			break;
		}

		if (!m_usingAA) {
			debugf(NAME_Init, TEXT("No supported multisample mode found, antialiasing disabled"));
		} else if ((INT)m_d3dpp.MultiSampleType != requestedSamples) {
			debugf(NAME_Init, TEXT("Antialiasing reduced from %i to %i samples"), requestedSamples, (INT)m_d3dpp.MultiSampleType);
		}
	}

	if (SwapInterval >= 0) {
		DWORD requestedInterval;

		switch (SwapInterval) {
			case 0: requestedInterval = D3DPRESENT_INTERVAL_IMMEDIATE; break;
			case 1: requestedInterval = D3DPRESENT_INTERVAL_ONE; break;
			case 2: requestedInterval = D3DPRESENT_INTERVAL_TWO; break;
			case 3: requestedInterval = D3DPRESENT_INTERVAL_THREE; break;
			case 4: requestedInterval = D3DPRESENT_INTERVAL_FOUR; break;
			default:
				requestedInterval = D3DPRESENT_INTERVAL_DEFAULT;
		}

		//Fullscreen only.
		if (m_d3dpp.Windowed && (SwapInterval >= 2) && (SwapInterval <= 4)) {
			debugf(NAME_Init, TEXT("SwapInterval %i needs a fullscreen device, using one instead"), SwapInterval);
			requestedInterval = D3DPRESENT_INTERVAL_ONE;
		}

		if (requestedInterval == D3DPRESENT_INTERVAL_DEFAULT) {
			debugf(NAME_Init, TEXT("SwapInterval %i is out of range, using the driver default"), SwapInterval);
		} else if (!(m_d3dCaps.PresentationIntervals & requestedInterval)) {
			debugf(NAME_Init, TEXT("SwapInterval %i is not supported, using the driver default"), SwapInterval);
		} else {
			m_d3dpp.PresentationInterval = requestedInterval;
		}
	}

	if ((FrameRateLimit > 0) && (FrameRateLimit < 20)) {
		debugf(NAME_Init, TEXT("Warning: FrameRateLimit %i is below the minimum of 20 and is being ignored. Use 0 to turn the limiter off."), FrameRateLimit);
	}


	if (UseTripleBuffering) {
		m_d3dpp.BackBufferCount = 2;
	}

	/*
	Each create attempt below retries without the extra back buffer, lowering the count in place
	to do it, and since the usual failure is an unsupported refresh rate the count is restored
	before every attempt, one failed attempt having otherwise cost the session its triple
	buffering for good.
	*/
	const UINT requestedBackBufferCount = m_d3dpp.BackBufferCount;

	m_doSoftwareVertexInit = false;
	if (UseSoftwareVertexProcessing) {
		m_doSoftwareVertexInit = true;
	}

	if (!m_doSoftwareVertexInit) {
		bool tryDefaultRefreshRate = true;
		//Without FPU_PRESERVE, D3D9 drops the shared x87 control word to single precision
		DWORD behaviorFlags = D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_FPU_PRESERVE;

		if (UsePureDevice && (m_d3dCaps.DevCaps & D3DDEVCAPS_PUREDEVICE)) {
			behaviorFlags |= D3DCREATE_PUREDEVICE;
		}

		if (!m_d3dpp.Windowed && (RefreshRate > 0)) {
			m_d3dpp.FullScreen_RefreshRateInHz = RefreshRate;
			m_d3dpp.BackBufferCount = requestedBackBufferCount;
			hResult = m_d3d9->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, m_hWnd, behaviorFlags, &m_d3dpp, &m_d3dDevice);
			//Retry without triple buffering.
			if (FAILED(hResult) && (m_d3dpp.BackBufferCount > 1)) {
				m_d3dpp.BackBufferCount = 1;
				hResult = m_d3d9->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, m_hWnd, behaviorFlags, &m_d3dpp, &m_d3dDevice);
			}
			//Only a success settles the refresh rate.
			if (SUCCEEDED(hResult)) {
				tryDefaultRefreshRate = false;
			}
		}

		if (tryDefaultRefreshRate) {
			m_d3dpp.FullScreen_RefreshRateInHz = D3DPRESENT_RATE_DEFAULT;
			m_d3dpp.BackBufferCount = requestedBackBufferCount;
			hResult = m_d3d9->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, m_hWnd, behaviorFlags, &m_d3dpp, &m_d3dDevice);
			//Retry without triple buffering.
			if (FAILED(hResult) && (m_d3dpp.BackBufferCount > 1)) {
				m_d3dpp.BackBufferCount = 1;
				hResult = m_d3d9->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, m_hWnd, behaviorFlags, &m_d3dpp, &m_d3dDevice);
			}
			if (FAILED(hResult)) {
				debugf(NAME_Init, TEXT("Failed to create D3D device with hardware vertex processing (0x%08X)"), hResult);
				m_doSoftwareVertexInit = true;
			}
		}
	}
	if (m_doSoftwareVertexInit) {
		bool tryDefaultRefreshRate = true;
		DWORD behaviorFlags = D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_FPU_PRESERVE;

		if (!m_d3dpp.Windowed && (RefreshRate > 0)) {
			m_d3dpp.FullScreen_RefreshRateInHz = RefreshRate;
			m_d3dpp.BackBufferCount = requestedBackBufferCount;
			hResult = m_d3d9->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, m_hWnd, behaviorFlags, &m_d3dpp, &m_d3dDevice);
			//Retry without triple buffering.
			if (FAILED(hResult) && (m_d3dpp.BackBufferCount > 1)) {
				m_d3dpp.BackBufferCount = 1;
				hResult = m_d3d9->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, m_hWnd, behaviorFlags, &m_d3dpp, &m_d3dDevice);
			}
			//Only a success settles the refresh rate.
			if (SUCCEEDED(hResult)) {
				tryDefaultRefreshRate = false;
			}
		}

		if (tryDefaultRefreshRate) {
			m_d3dpp.FullScreen_RefreshRateInHz = D3DPRESENT_RATE_DEFAULT;
			m_d3dpp.BackBufferCount = requestedBackBufferCount;
			hResult = m_d3d9->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, m_hWnd, behaviorFlags, &m_d3dpp, &m_d3dDevice);
			//Retry without triple buffering.
			if (FAILED(hResult) && (m_d3dpp.BackBufferCount > 1)) {
				m_d3dpp.BackBufferCount = 1;
				hResult = m_d3d9->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, m_hWnd, behaviorFlags, &m_d3dpp, &m_d3dDevice);
			}
			if (FAILED(hResult)) {
				appErrorf(TEXT("Failed to create D3D device (0x%08X)"), hResult);
			}
		}
	}


	m_prevSwapBuffersStatus = true;

	debugf(NAME_Init, TEXT("Depth bits: %u"), m_numDepthBits);
	if (m_numDepthBits < 24) {
		debugf(NAME_Init, TEXT("Only a %u bit depth buffer is available, so surfaces will z-fight"), m_numDepthBits);
	}

	if (DebugBit(DEBUG_BIT_BASIC)) {
#define UTGLR_DEBUG_SHOW_PARAM_REG(_name) DbgPrintInitParam(TEXT(#_name), _name)
#define UTGLR_DEBUG_SHOW_PARAM_DCV(_name) DbgPrintInitParam(TEXT(#_name), DCV._name)

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
		UTGLR_DEBUG_SHOW_PARAM_REG(UseMultiTexture);
#ifndef UTGLR_OLD_URENDERDEVICE
		UTGLR_DEBUG_SHOW_PARAM_REG(UsePrecache);
#endif
		UTGLR_DEBUG_SHOW_PARAM_REG(UseTrilinear);
//		UTGLR_DEBUG_SHOW_PARAM_REG(UseVertexSpecular);
#ifndef UTGLR_KLINGON_BUILD
		UTGLR_DEBUG_SHOW_PARAM_REG(UseS3TC);
#endif
		UTGLR_DEBUG_SHOW_PARAM_REG(Use16BitTextures);
		UTGLR_DEBUG_SHOW_PARAM_REG(Use565Textures);
		UTGLR_DEBUG_SHOW_PARAM_REG(NoFiltering);
		UTGLR_DEBUG_SHOW_PARAM_DCV(DetailMax);
		//		UTGLR_DEBUG_SHOW_PARAM_REG(UseDetailAlpha);
		UTGLR_DEBUG_SHOW_PARAM_REG(DetailClipping);
		UTGLR_DEBUG_SHOW_PARAM_REG(ColorizeDetailTextures);
		UTGLR_DEBUG_SHOW_PARAM_REG(SinglePassFog);
		UTGLR_DEBUG_SHOW_PARAM_DCV(SinglePassDetail);
		//		UTGLR_DEBUG_SHOW_PARAM_REG(BufferActorTris);
		//		UTGLR_DEBUG_SHOW_PARAM_REG(BufferClippedActorTris);
		UTGLR_DEBUG_SHOW_PARAM_REG(UseSSE);
		UTGLR_DEBUG_SHOW_PARAM_REG(UseSSE2);
		UTGLR_DEBUG_SHOW_PARAM_REG(UseTexIdPool);
		UTGLR_DEBUG_SHOW_PARAM_REG(UseTexPool);
		UTGLR_DEBUG_SHOW_PARAM_REG(CacheStaticMaps);
		UTGLR_DEBUG_SHOW_PARAM_REG(GenerateMipMaps);
		UTGLR_DEBUG_SHOW_PARAM_REG(TexCacheBudgetMegs);
		UTGLR_DEBUG_SHOW_PARAM_REG(DynamicTexIdRecycleLevel);
		UTGLR_DEBUG_SHOW_PARAM_REG(TexDXT1ToDXT3);
		UTGLR_DEBUG_SHOW_PARAM_DCV(UseFragmentProgram);
		UTGLR_DEBUG_SHOW_PARAM_REG(SwapInterval);
		UTGLR_DEBUG_SHOW_PARAM_REG(FrameRateLimit);
		UTGLR_DEBUG_SHOW_PARAM_REG(SmoothMaskedTextures);
		UTGLR_DEBUG_SHOW_PARAM_REG(UseTripleBuffering);
		UTGLR_DEBUG_SHOW_PARAM_REG(UsePureDevice);
		UTGLR_DEBUG_SHOW_PARAM_REG(UseSoftwareVertexProcessing);
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
	if (DebugBit(DEBUG_BIT_BASIC)) dout << TEXT("utd3d9r: UseSSE = ") << UseSSE << std::endl;
	if (DebugBit(DEBUG_BIT_BASIC)) dout << TEXT("utd3d9r: UseSSE2 = ") << UseSSE2 << std::endl;

	InitGammaResourcesSafe();

	SetGamma(Viewport->GetOuterUClient()->Brightness);

	PL_ClientBrightness = Viewport->GetOuterUClient()->Brightness;
	PL_GammaOffset = GammaOffset;
	PL_GammaOffsetRed = GammaOffsetRed;
	PL_GammaOffsetGreen = GammaOffsetGreen;
	PL_GammaOffsetBlue = GammaOffsetBlue;
	PL_Brightness = Brightness;

	if (DynamicTexIdRecycleLevel < 10) DynamicTexIdRecycleLevel = 10;

	UseVertexSpecular = 1;

	//Forced on this build.
#ifdef UTGLR_KLINGON_BUILD
	SupportsTC = 0;
#else
	SupportsTC = UseS3TC;
#endif

	BufferActorTris = 1;
	BufferClippedActorTris = 1;

	UseDetailAlpha = 1;


	m_dxt1TextureCap = true;
	hResult = m_d3d9->CheckDeviceFormat(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, d3ddm.Format, 0, D3DRTYPE_TEXTURE, D3DFMT_DXT1);
	if (FAILED(hResult)) {
		m_dxt1TextureCap = false;
	}
	m_dxt3TextureCap = true;
	hResult = m_d3d9->CheckDeviceFormat(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, d3ddm.Format, 0, D3DRTYPE_TEXTURE, D3DFMT_DXT3);
	if (FAILED(hResult)) {
		m_dxt3TextureCap = false;
	}
	m_dxt5TextureCap = true;
	hResult = m_d3d9->CheckDeviceFormat(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, d3ddm.Format, 0, D3DRTYPE_TEXTURE, D3DFMT_DXT5);
	if (FAILED(hResult)) {
		m_dxt5TextureCap = false;
	}

	m_16BitTextureCap = true;
	m_565TextureCap = true;
	hResult = m_d3d9->CheckDeviceFormat(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, d3ddm.Format, 0, D3DRTYPE_TEXTURE, D3DFMT_A1R5G5B5);
	if (FAILED(hResult)) {
		m_16BitTextureCap = false;
	}
	hResult = m_d3d9->CheckDeviceFormat(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, d3ddm.Format, 0, D3DRTYPE_TEXTURE, D3DFMT_X1R5G5B5);
	if (FAILED(hResult)) {
		m_16BitTextureCap = false;
	}
	hResult = m_d3d9->CheckDeviceFormat(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, d3ddm.Format, 0, D3DRTYPE_TEXTURE, D3DFMT_R5G6B5);
	if (FAILED(hResult)) {
		m_565TextureCap = false;
	}

	m_alphaTextureCap = true;
	hResult = m_d3d9->CheckDeviceFormat(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, d3ddm.Format, 0, D3DRTYPE_TEXTURE, D3DFMT_A8);
	if (FAILED(hResult)) {
		m_alphaTextureCap = false;
	}

	m_supportsAlphaToCoverage = false;
	//ATI advertises alpha to coverage on every DX9 class card; NVIDIA exposes it as a queryable format.
	if (m_d3dCaps.PixelShaderVersion >= D3DPS_VERSION(3, 0)) {
		if (m_isATI) {
			m_supportsAlphaToCoverage = true;
		} else if (m_isNVIDIA) {
			hResult = m_d3d9->CheckDeviceFormat(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL,
				D3DFMT_X8R8G8B8, 0, D3DRTYPE_SURFACE, (D3DFORMAT)MAKEFOURCC('A', 'T', 'O', 'C'));
			if (SUCCEEDED(hResult)) {
				m_supportsAlphaToCoverage = true;
			}
		}
	}


	if (!m_dxt1TextureCap) SupportsTC = 0;
#ifdef UTGLR_HAS_DXT3_DXT5
	if (!m_dxt3TextureCap) SupportsTC = 0;
	if (!m_dxt5TextureCap) SupportsTC = 0;
#endif

	ConfigValidate_RefreshDCV();

	ConfigValidate_RequiredExtensions();


	if (!MaxTMUnits || (MaxTMUnits > MAX_TMUNITS)) {
		MaxTMUnits = MAX_TMUNITS;
	}

	if (UseMultiTexture) {
		TMUnits = m_d3dCaps.MaxSimultaneousTextures;
		debugf(TEXT("%i Texture Mapping Units found"), TMUnits);
		if (TMUnits > MaxTMUnits) {
			TMUnits = MaxTMUnits;
		}
	} else {
		TMUnits = 1;
	}


	ConfigValidate_Main();


	if (MaxAnisotropy < 0) {
		MaxAnisotropy = 0;
	}
	if (MaxAnisotropy) {
		int iMaxAnisotropyLimit;
		iMaxAnisotropyLimit = m_d3dCaps.MaxAnisotropy;
		debugf(TEXT("MaxAnisotropy = %i"), iMaxAnisotropyLimit);
		if (MaxAnisotropy > iMaxAnisotropyLimit) {
			MaxAnisotropy = iMaxAnisotropyLimit;
		}
	}

	m_anisotropicMagFilterCap = (MaxAnisotropy != 0) && ((m_d3dCaps.TextureFilterCaps & D3DPTFILTERCAPS_MAGFANISOTROPIC) != 0);

	if (SupportsTC) {
		debugf(TEXT("Trying to use S3TC extension."));
	}

	if (MaxLogTextureSize <= 0) MaxLogTextureSize = 12;

	INT MaxTextureSize = Min(m_d3dCaps.MaxTextureWidth, m_d3dCaps.MaxTextureHeight);
	INT Dummy = -1;
	while (MaxTextureSize > 0) {
		MaxTextureSize >>= 1;
		Dummy++;
	}

	if ((MaxLogTextureSize > Dummy) || (SupportsTC)) MaxLogTextureSize = Dummy;
	if ((MinLogTextureSize < 2) || (SupportsTC)) MinLogTextureSize = 2;

	MaxLogUOverV = MaxLogTextureSize;
	MaxLogVOverU = MaxLogTextureSize;
	if (SupportsTC) {
		//No clamp when compressed.
	} else {
		INT MaxTextureAspectRatio = m_d3dCaps.MaxTextureAspectRatio;
		if (MaxTextureAspectRatio > 0) {
			INT MaxLogTextureAspectRatio = -1;
			while (MaxTextureAspectRatio > 0) {
				MaxTextureAspectRatio >>= 1;
				MaxLogTextureAspectRatio++;
			}
			if (MaxLogTextureAspectRatio < MaxLogUOverV) MaxLogUOverV = MaxLogTextureAspectRatio;
			if (MaxLogTextureAspectRatio < MaxLogVOverU) MaxLogVOverU = MaxLogTextureAspectRatio;
		}
	}

	debugf(TEXT("MinLogTextureSize = %i"), MinLogTextureSize);
	debugf(TEXT("MaxLogTextureSize = %i"), MaxLogTextureSize);

	debugf(TEXT("UseDetailAlpha = %i"), UseDetailAlpha);

	if (UseFragmentProgram) {
		debugf(TEXT("World rendering: shader model 3.0 (texture coordinates generated on GPU)"));
	} else {
		debugf(TEXT("World rendering: fixed function multipass (texture coordinates regenerated per pass on CPU)"));
		if (m_d3dCaps.VertexShaderVersion < D3DVS_VERSION(3, 0)) {
			debugf(TEXT("   vertex shader 3.0 is not supported"));
		}
		if (m_d3dCaps.PixelShaderVersion < D3DPS_VERSION(3, 0)) {
			debugf(TEXT("   pixel shader 3.0 is not supported"));
		}
	}


	MapDotArray = (FGLMapDot *)AlignMemPtr(m_MapDotArrayMem, VERTEX_ARRAY_ALIGN);


	check(MinLogTextureSize >= 0);
	check(MaxLogTextureSize >= 0);
	check(MinLogTextureSize <= MaxLogTextureSize);

	InternalFlush();

	m_pNoTexObj = NULL;
	m_pAlphaTexObj = NULL;

	InitPermanentResourcesAndRenderingState();


	//A float literal: this field's type has both float and double constructors on this build,
	//so an int literal would be ambiguous.
	m_prevFrameTimestamp = 0.0f;

	PL_DetailTextures = DetailTextures;
	PL_OneXBlending = OneXBlending;
	PL_ReduceBanding = ReduceBanding;
	PL_UseHardwareGamma = UseHardwareGamma;
	PL_MaxLogUOverV = MaxLogUOverV;
	PL_MaxLogVOverU = MaxLogVOverU;
	PL_MinLogTextureSize = MinLogTextureSize;
	PL_MaxLogTextureSize = MaxLogTextureSize;
	PL_NoFiltering = NoFiltering;
	PL_UseTrilinear = UseTrilinear;
	PL_Use16BitTextures = Use16BitTextures;
	PL_Use565Textures = Use565Textures;
	PL_TexDXT1ToDXT3 = TexDXT1ToDXT3;
	PL_GenerateMipMaps = GenerateMipMaps;
	PL_MaxAnisotropy = MaxAnisotropy;
	PL_SmoothMaskedTextures = SmoothMaskedTextures;
	PL_LODBias = LODBias;
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

void UD3D9RenderDevice::UnsetRes() {
	guard(UD3D9RenderDevice::UnsetRes);

	check(m_d3d9);

	InternalFlush();

	if (m_pNoTexObj) {
		m_pNoTexObj->Release();
		m_pNoTexObj = NULL;
	}
	if (m_pAlphaTexObj) {
		m_pAlphaTexObj->Release();
		m_pAlphaTexObj = NULL;
	}

	FreePermanentResources();

	if (m_d3dDevice) {
		m_d3dDevice->Release();
		m_d3dDevice = NULL;
	}

	m_d3d9->Release();
	m_d3d9 = NULL;

	unguard;
}


bool UD3D9RenderDevice::IsFullscreenModeSupported(INT NewX, INT NewY, INT NewColorBytes) {
	D3DFORMAT format = (NewColorBytes <= 2) ? D3DFMT_R5G6B5 : D3DFMT_X8R8G8B8;
	UINT numModes = m_d3d9->GetAdapterModeCount(D3DADAPTER_DEFAULT, format);

	for (UINT i = 0; i < numModes; i++) {
		D3DDISPLAYMODE mode;

		if (FAILED(m_d3d9->EnumAdapterModes(D3DADAPTER_DEFAULT, format, i, &mode))) {
			continue;
		}
		if (((INT)mode.Width == NewX) && ((INT)mode.Height == NewY)) {
			return true;
		}
	}

	return false;
}


bool UD3D9RenderDevice::CheckDepthFormat(D3DFORMAT adapterFormat, D3DFORMAT backBufferFormat, D3DFORMAT depthBufferFormat) {
	HRESULT hResult;

	hResult = m_d3d9->CheckDeviceFormat(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, adapterFormat, D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_SURFACE, depthBufferFormat);
	if (FAILED(hResult)) {
		return false;
	}

	hResult = m_d3d9->CheckDepthStencilMatch(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, adapterFormat, backBufferFormat, depthBufferFormat);
	if (FAILED(hResult)) {
		return false;
	}

	return true;
}
