/*=============================================================================
	Init.cpp: bringing the device up, and taking it down.

	d3d9.dll is loaded by name at runtime, so a machine without it fails here with
	something the user can act on.
=============================================================================*/

#include "../D3D9Drv.h"
#include "../D3D9.h"

#ifdef UTD3D9R_USE_DEBUG_D3D9_DLL
static const char *g_d3d9DllName = "d3d9d.dll";
#else
static const char *g_d3d9DllName = "d3d9.dll";
#endif

UBOOL UD3D9RenderDevice::FailedInitf(const TCHAR *Fmt, ...) {
	TCHAR TempStr[4096];
	GET_VARARGS(TempStr, ARRAY_COUNT(TempStr), Fmt);
	debugf(NAME_Init, TEXT("%s"), TempStr);
	Exit();
	return 0;
}

void UD3D9RenderDevice::Exit() {
	guard(UD3D9RenderDevice::Exit);

	if (m_isUncountedDevice) {
		return;
	}

	if (m_exited) {
		return;
	}
	m_exited = true;

	check(NumDevices > 0);

	//Process wide state the first device fills and only this clears,
	//so one device leaving while others still run would put the desktop's ramp back under them.
	if (NumDevices == 1) {
		ResetGamma();
	}

	ShutdownDeferred();

	if (m_d3d9) {
		UnsetRes();
	}

	if (m_hDC) {
		ReleaseDC(m_hWnd, m_hDC);
		m_hDC = NULL;
	}

	ShutdownFrameRateLimitTimer();

	if (--NumDevices == 0) {
		/*
		d3d9.dll is deliberately left loaded, because it pulls in the display driver's own DLL,
		and the renderer can be re-created within one session anyway.
		The dead code below stays so the intent is not mistaken for an oversight.
		*/
#if 0
		//Free modules.
		if (hModuleD3d9) {
			verify(FreeLibrary(hModuleD3d9));
			hModuleD3d9 = NULL;
		}
#endif
	}

	unguard;
}

void UD3D9RenderDevice::ShutdownAfterError() {
	guard(UD3D9RenderDevice::ShutdownAfterError);

	debugf(NAME_Exit, TEXT("UD3D9RenderDevice::ShutdownAfterError"));

	if (DebugBit(DEBUG_BIT_BASIC)) {
		dout << TEXT("utd3d9r: ShutdownAfterError") << std::endl;
	}

	ResetGamma();

	ShutdownDeferred();

	//The high resolution timer is process wide, so it must be lowered here too.
	//Safe to call again; this has its own once flag.
	ShutdownFrameRateLimitTimer();

	/*
	An exclusive fullscreen device would hide the engine's error box behind the primary surface,
	hence the mode change back to windowed.
	A terminal unwind hands it all back anyway,
	but the ordering is a trap for anyone who makes this path recoverable.
	*/
	if (m_d3dDevice) {
		if (!m_d3dpp.Windowed) {
			UnsetRes();
			ChangeDisplaySettings(NULL, 0);
		}
	}

	if (m_hDC) {
		ReleaseDC(m_hWnd, m_hDC);
		m_hDC = NULL;
	}

	unguard;
}


UBOOL UD3D9RenderDevice::Init(UViewport *InViewport, INT NewX, INT NewY, INT NewColorBytes, UBOOL Fullscreen) {
	guard(UD3D9RenderDevice::Init);

	debugf(TEXT("Initializing D3D9Drv..."));

	ValidateBoolConfigParamOffsets();

	if (NumDevices == 0) {
		g_haveOriginalGammaRamp = false;
	}

	for (INT i = 0;; i++) {
		UBOOL UnicodeOS;

#if defined(NO_UNICODE_OS_SUPPORT) || !defined(UNICODE)
		UnicodeOS = 0;
#elif defined(NO_ANSI_OS_SUPPORT)
		UnicodeOS = 1;
#else
		UnicodeOS = GUnicodeOS;
#endif

		if (!UnicodeOS) {
#if defined(NO_UNICODE_OS_SUPPORT) || !defined(UNICODE) || !defined(NO_ANSI_OS_SUPPORT)
			DEVMODEA Tmp;
			appMemzero(&Tmp, sizeof(Tmp));
			Tmp.dmSize = sizeof(Tmp);
			if (!EnumDisplaySettingsA(NULL, i, &Tmp)) {
				break;
			}
			Modes.AddUniqueItem(FPlane(Tmp.dmPelsWidth, Tmp.dmPelsHeight, Tmp.dmBitsPerPel, Tmp.dmDisplayFrequency));
#endif
		} else {
#if !defined(NO_UNICODE_OS_SUPPORT) && defined(UNICODE)
			DEVMODEW Tmp;
			appMemzero(&Tmp, sizeof(Tmp));
			Tmp.dmSize = sizeof(Tmp);
			if (!EnumDisplaySettingsW(NULL, i, &Tmp)) {
				break;
			}
			Modes.AddUniqueItem(FPlane(Tmp.dmPelsWidth, Tmp.dmPelsHeight, Tmp.dmBitsPerPel, Tmp.dmDisplayFrequency));
#endif
		}
	}

	if (!hModuleD3d9) {
		hModuleD3d9 = LoadLibraryA(g_d3d9DllName);
		if (!hModuleD3d9) {
			debugf(NAME_Init, TEXT("Failed to load %s"), appFromAnsi(g_d3d9DllName));
			m_isUncountedDevice = true;
			return 0;
		}
		pDirect3DCreate9 = (LPDIRECT3DCREATE9)GetProcAddress(hModuleD3d9, "Direct3DCreate9");
		if (!pDirect3DCreate9) {
			debugf(NAME_Init, TEXT("Failed to load function from %s"), appFromAnsi(g_d3d9DllName));
			FreeLibrary(hModuleD3d9);
			hModuleD3d9 = NULL;
			m_isUncountedDevice = true;
			return 0;
		}
	}

	NumDevices++;

	/*
	After both early-return failure paths above.
	Process attach is too early: it runs under the loader lock, cannot create threads,
	and would take its floating point control word from the wrong thread.
	*/
	InitDeferred();
	StartJobSystem();

	m_zeroPrefixBindTrees = m_localZeroPrefixBindTrees;
	m_nonZeroPrefixBindTrees = m_localNonZeroPrefixBindTrees;
	m_zeroPrefixBindChain = &m_localZeroPrefixBindChain;
	m_nonZeroPrefixBindChain = &m_localNonZeroPrefixBindChain;
	m_RGBA8TexPool = &m_localRGBA8TexPool;
	m_texCacheBytes = 0;
	m_texCacheBudgetUnreachable = false;

	Viewport = InViewport;


	m_hWnd = (HWND)InViewport->GetWindow();
	check(m_hWnd);
	m_hDC = GetDC(m_hWnd);
	check(m_hDC);

	if (!SetRes(NewX, NewY, NewColorBytes, Fullscreen)) {
		return FailedInitf(LocalizeError("ResFailed"));
	}

	return 1;
	unguard;
}
