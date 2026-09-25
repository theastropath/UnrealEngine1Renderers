/*=============================================================================
	Init.cpp: startup and shutdown.
=============================================================================*/

#include "../OpenGLDrv.h"
#include "../OpenGL.h"
#include "../Core/Globals.h"

UBOOL UOpenGLRenderDevice::FailedInitf(const TCHAR *Fmt, ...) {
	TCHAR TempStr[4096];
	GET_VARARGS(TempStr, ARRAY_COUNT(TempStr), Fmt);
	//Already formatted here.
	debugf(NAME_Init, TEXT("%s"), TempStr);
	Exit();
	return 0;
}

void UOpenGLRenderDevice::Exit() {
	guard(UOpenGLRenderDevice::Exit);

	//Nothing to undo.
	if (m_isUncountedDevice) {
		return;
	}

	//Idempotent, because the decrement below is not.
	if (m_exited) {
		return;
	}
	m_exited = true;

	check(NumDevices > 0);

	//Held for the process.
	//Must run before the platform split.
	FreeTexComposeBuffer();

#ifdef __UNIX__
	UnsetRes();

	if (--NumDevices == 0) {
		{
			unsigned int u;

			for (u = 0; u < NUM_CTTree_TREES; u++) {
				m_sharedZeroPrefixBindTrees[u].clear(&m_DWORD_CTTree_Allocator);
			}
			for (u = 0; u < NUM_CTTree_TREES; u++) {
				m_sharedNonZeroPrefixBindTrees[u].clear(&m_QWORD_CTTree_Allocator);
			}
		}
		m_sharedNonZeroPrefixBindChain.mark_as_clear();
		//The non zero prefix tex id pool is not freed.
		m_sharedRGBA8TexPool.clear(&m_TexPoolMap_Allocator);
	}
#else
	if (m_hRC) {
		UnsetRes();
	}

	ShutdownFrameRateLimitTimer();

	//May fail if the window was already destroyed.
	if (m_hDC) {
		ReleaseDC(m_hWnd, m_hDC);
		//A second call is safe.
		m_hDC = NULL;
	}

	if (--NumDevices == 0) {
		/*
		Only the last device restores the saved ramp because it is process wide and restoring it
		per device let closing one editor viewport pull the ramp out from under the others still
		running, and it goes through the desktop window's own device context so that a device
		whose own window has already gone can still put the desktop back.
		*/
		ResetGamma();

#if 0 // Broken on some drivers \
	// Free modules.
		if (hModuleGlMain)
			verify(FreeLibrary(hModuleGlMain));
#endif
		{
			unsigned int u;

			for (u = 0; u < NUM_CTTree_TREES; u++) {
				m_sharedZeroPrefixBindTrees[u].clear(&m_DWORD_CTTree_Allocator);
			}
			for (u = 0; u < NUM_CTTree_TREES; u++) {
				m_sharedNonZeroPrefixBindTrees[u].clear(&m_QWORD_CTTree_Allocator);
			}
		}
		m_sharedNonZeroPrefixBindChain.mark_as_clear();
		//The non zero prefix tex id pool is not freed.
		m_sharedRGBA8TexPool.clear(&m_TexPoolMap_Allocator);

		//Nothing left at exit.
		AllContexts.Empty();
	}
#endif
	unguard;
}

void UOpenGLRenderDevice::ShutdownAfterError() {
	guard(UOpenGLRenderDevice::ShutdownAfterError);

	debugf(NAME_Exit, TEXT("UOpenGLRenderDevice::ShutdownAfterError"));

	if (DebugBit(DEBUG_BIT_BASIC)) {
		dbgPrintf("utglr: ShutdownAfterError\n");
	}

#ifdef _WIN32
	//The normal teardown path is not reached here.
	if (WasFullscreen) {
		TCHAR_CALL_OS(ChangeDisplaySettingsW(NULL, 0), ChangeDisplaySettingsA(NULL, 0));
	}
#endif

	ResetGamma();

	//The high resolution timer is process wide.
	//Safe to call again.
	ShutdownFrameRateLimitTimer();

#ifdef _WIN32
	//As in the normal teardown.
	if (m_hDC) {
		ReleaseDC(m_hWnd, m_hDC);
		m_hDC = NULL;
	}
#endif

	unguard;
}


UBOOL UOpenGLRenderDevice::Init(UViewport *InViewport, INT NewX, INT NewY, INT NewColorBytes, UBOOL Fullscreen) {
	guard(UOpenGLRenderDevice::Init);

	debugf(TEXT("Initializing OpenGL1xDrv..."));

	//Cleared before any return.
	m_isUncountedDevice = false;

	ValidateBoolConfigParamOffsets();

	if (NumDevices == 0) {
		g_gammaFirstTime = true;
		g_haveOriginalGammaRamp = false;
	}

#ifdef __UNIX__
	if (NumDevices == 0) {
		bool loadRet;

		FString OpenGLLibName;
		if (!GConfig->GetString(g_pSection, TEXT("OpenGLLibName"), OpenGLLibName)) {
			OpenGLLibName = TEXT("libGL.so.1");
		}

		if (!GLLoaded) {
			// Only call it once as succeeding calls will 'fail'.
			debugf(TEXT("binding %s"), *OpenGLLibName);
			if (SDL_GL_LoadLibrary(*OpenGLLibName) == -1) {
				appErrorf(TEXT(SDL_GetError()));
			}
			GLLoaded = true;
		}

		loadRet = GetGL1Procs();
		if (!loadRet) {
			//Set before the count increment below.
			m_isUncountedDevice = true;
			return 0;
		}
	}
#else
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

	if (NumDevices == 0) {
		bool loadRet;

		hModuleGlMain = LoadLibraryA(GL_DLL);
		if (!hModuleGlMain) {
			debugf(NAME_Init, LocalizeError("NoFindGL"), appFromAnsi(GL_DLL));
			//Not counted on failure.
			m_isUncountedDevice = true;
			return 0;
		}

		loadRet = GetGL1Procs();
		if (!loadRet) {
			//Not counted either, for the same reason.
			m_isUncountedDevice = true;
			return 0;
		}
	}
#endif

	/*
	The engine probes compatibility with a NULL viewport and with no window to attach a
	context to, loading GL is as far as this goes, the probe only wanting to know whether the
	DLL answers for itself at all, and everything past this point assumes a window so the
	early return is the whole of the probe's support.
	*/
	if (!InViewport) {
		//This device never counts itself below.
		//Teardown decrements unconditionally.
		m_isUncountedDevice = true;
		return 1;
	}

	NumDevices++;

	m_zeroPrefixBindTrees = ShareLists ? m_sharedZeroPrefixBindTrees : m_localZeroPrefixBindTrees;
	m_nonZeroPrefixBindTrees = ShareLists ? m_sharedNonZeroPrefixBindTrees : m_localNonZeroPrefixBindTrees;
	m_zeroPrefixBindChain = ShareLists ? &m_sharedZeroPrefixBindChain : &m_localZeroPrefixBindChain;
	m_nonZeroPrefixBindChain = ShareLists ? &m_sharedNonZeroPrefixBindChain : &m_localNonZeroPrefixBindChain;
	m_nonZeroPrefixTexIdPool = ShareLists ? &m_sharedNonZeroPrefixTexIdPool : &m_localNonZeroPrefixTexIdPool;
	m_RGBA8TexPool = ShareLists ? &m_sharedRGBA8TexPool : &m_localRGBA8TexPool;
	m_texCacheBytes = ShareLists ? &m_sharedTexCacheBytes : &m_localTexCacheBytes;
	m_allocatedTextures = ShareLists ? &m_sharedAllocatedTextures : &m_localAllocatedTextures;
	m_texCacheBudgetUnreachable = false;

	Viewport = InViewport;

#ifndef __UNIX__
	m_hWnd = (HWND)InViewport->GetWindow();
	check(m_hWnd);
	m_hDC = GetDC(m_hWnd);
	check(m_hDC);
#endif

#if 0
	{
		//Print all PFD's exposed.
		INT pf;
		INT pfCount = DescribePixelFormat(m_hDC, 0, 0, NULL);
		for (pf = 1; pf <= pfCount; pf++) {
			PrintFormat(m_hDC, pf);
		}
	}
#endif

	if (!SetRes(NewX, NewY, NewColorBytes, Fullscreen)) {
		return FailedInitf(LocalizeError("ResFailed"));
	}

	return 1;
	unguard;
}
