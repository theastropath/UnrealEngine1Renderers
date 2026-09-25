/*=============================================================================
	PixelFormat.cpp: choosing a Win32 pixel format and rendering context.

	Multisampled formats need an extension that itself needs a context.
=============================================================================*/

#include "../OpenGLDrv.h"
#include "../OpenGL.h"

#ifdef _WIN32
void UOpenGLRenderDevice::InitARBPixelFormat(INT NewColorBytes, InitARBPixelFormatRet_t *pRet) {
	guard(UOpenGLRenderDevice::InitARBPixelFormat);

	HINSTANCE hInstance = TCHAR_CALL_OS(GetModuleHandleW(NULL), GetModuleHandleA(NULL));
	struct tagWNDCLASSA wcA;
	struct tagWNDCLASSW wcW;
	const CHAR *pClassNameA = "UOpenGLRenderDevice::InitARBPixelFormat";
	const WCHAR *pClassNameW = L"UOpenGLRenderDevice::InitARBPixelFormat";
	InitARBPixelFormatWndProcParams_t initParams;
	HWND hWnd;

	wcA.style = wcW.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
	wcA.lpfnWndProc = wcW.lpfnWndProc = UOpenGLRenderDevice::InitARBPixelFormatWndProc;
	wcA.cbClsExtra = wcW.cbClsExtra = 0;
	wcA.cbWndExtra = wcW.cbWndExtra = 0;
	wcA.hInstance = wcW.hInstance = hInstance;
	wcA.hIcon = wcW.hIcon = NULL;
	wcA.hCursor = wcW.hCursor = TCHAR_CALL_OS(LoadCursorW(NULL, MAKEINTRESOURCEW(32512) /*IDC_ARROW*/), LoadCursorA(NULL, MAKEINTRESOURCEA(32512) /*IDC_ARROW*/));
	wcA.hbrBackground = wcW.hbrBackground = NULL;
	wcA.lpszMenuName = NULL;
	wcW.lpszMenuName = NULL;
	wcA.lpszClassName = pClassNameA;
	wcW.lpszClassName = pClassNameW;

	if (TCHAR_CALL_OS(RegisterClassW(&wcW), RegisterClassA(&wcA)) == 0) {
		return;
	}

	initParams.NewColorBytes = NewColorBytes;
	initParams.p_wglChoosePixelFormatARB = NULL;
	initParams.haveWGLMultisampleARB = false;

	hWnd = TCHAR_CALL_OS(
		CreateWindowW(
			pClassNameW,
			pClassNameW,
			WS_POPUP | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
			0, 0,
			20, 20,
			NULL,
			NULL,
			hInstance,
			&initParams),
		CreateWindowA(
			pClassNameA,
			pClassNameA,
			WS_POPUP | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
			0, 0,
			20, 20,
			NULL,
			NULL,
			hInstance,
			&initParams));
	if (hWnd == NULL) {
		TCHAR_CALL_OS(UnregisterClassW(pClassNameW, hInstance), UnregisterClassA(pClassNameA, hInstance));
		return;
	}

	DestroyWindow(hWnd);

	TCHAR_CALL_OS(UnregisterClassW(pClassNameW, hInstance), UnregisterClassA(pClassNameA, hInstance));

	pRet->p_wglChoosePixelFormatARB = initParams.p_wglChoosePixelFormatARB;
	pRet->haveWGLMultisampleARB = initParams.haveWGLMultisampleARB;

	return;

	unguard;
}

LRESULT CALLBACK UOpenGLRenderDevice::InitARBPixelFormatWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch (uMsg) {
		case WM_CREATE: {
			InitARBPixelFormatWndProcParams_t *pInitParams = (InitARBPixelFormatWndProcParams_t *)((LPCREATESTRUCT)lParam)->lpCreateParams;

			HDC hDC = GetDC(hWnd);

			INT NewColorBytes = pInitParams->NewColorBytes;
			INT nPixelFormat;
			BYTE DesiredColorBits = (NewColorBytes <= 2) ? 16 : 24;
			BYTE DesiredDepthBits = 32;
			BYTE DesiredStencilBits = 0;
			PIXELFORMATDESCRIPTOR pfd = {
				sizeof(PIXELFORMATDESCRIPTOR),
				1,
				PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
				PFD_TYPE_RGBA,
				DesiredColorBits,
				0, 0, 0, 0, 0, 0,
				0, 0,
				0, 0, 0, 0, 0,
				DesiredDepthBits,
				DesiredStencilBits,
				0,
				PFD_MAIN_PLANE,
				0,
				0, 0, 0
			};

			nPixelFormat = ChoosePixelFormat(hDC, &pfd);
			if (!nPixelFormat) {
				pfd.cDepthBits = 24;
				nPixelFormat = ChoosePixelFormat(hDC, &pfd);
			}
			if (!nPixelFormat) {
				pfd.cDepthBits = 16;
				nPixelFormat = ChoosePixelFormat(hDC, &pfd);
			}
			if (nPixelFormat == 0) {
				break;
			}

			if (SetPixelFormat(hDC, nPixelFormat, &pfd) == FALSE) {
				break;
			}
			HGLRC hGLRC = wglCreateContext(hDC);
			if (hGLRC == NULL) {
				break;
			}
			if (wglMakeCurrent(hDC, hGLRC) == FALSE) {
				wglDeleteContext(hGLRC);
				break;
			}

			PFNWGLGETEXTENSIONSSTRINGARBPROC p_wglGetExtensionsStringARB = reinterpret_cast<PFNWGLGETEXTENSIONSSTRINGARBPROC>(wglGetProcAddress("wglGetExtensionsStringARB"));
			if (p_wglGetExtensionsStringARB != NULL) {
				const char *pWGLExtensions = p_wglGetExtensionsStringARB(hDC);
				if (pWGLExtensions != NULL) {
					pInitParams->haveWGLMultisampleARB = IsGLExtensionSupported(pWGLExtensions, "WGL_ARB_multisample");
				}
			}
			pInitParams->p_wglChoosePixelFormatARB = reinterpret_cast<PFNWGLCHOOSEPIXELFORMATARBPROC>(wglGetProcAddress("wglChoosePixelFormatARB"));

			wglMakeCurrent(NULL, NULL);
			wglDeleteContext(hGLRC);
		} break;

		default:
			return TCHAR_CALL_OS(DefWindowProcW(hWnd, uMsg, wParam, lParam), DefWindowProcA(hWnd, uMsg, wParam, lParam));
	}

	return 0;
}


void UOpenGLRenderDevice::SetBasicPixelFormat(INT NewColorBytes) {
	INT nPixelFormat;
	BYTE DesiredColorBits = (NewColorBytes <= 2) ? 16 : 24;
	BYTE DesiredDepthBits = 32;
	BYTE DesiredStencilBits = 0;
	PIXELFORMATDESCRIPTOR pfd = {
		sizeof(PIXELFORMATDESCRIPTOR),
		1,
		PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
		PFD_TYPE_RGBA,
		DesiredColorBits,
		0, 0, 0, 0, 0, 0,
		0, 0,
		0, 0, 0, 0, 0,
		DesiredDepthBits,
		DesiredStencilBits,
		0,
		PFD_MAIN_PLANE,
		0,
		0, 0, 0
	};

	if (DebugBit(DEBUG_BIT_BASIC)) dbgPrintf("utglr: BasicInit\n");

	nPixelFormat = ChoosePixelFormat(m_hDC, &pfd);
	if (!nPixelFormat) {
		pfd.cDepthBits = 24;
		nPixelFormat = ChoosePixelFormat(m_hDC, &pfd);
	}
	if (!nPixelFormat) {
		pfd.cDepthBits = 16;
		nPixelFormat = ChoosePixelFormat(m_hDC, &pfd);
	}

	Parse(appCmdLine(), TEXT("PIXELFORMAT="), nPixelFormat);
	debugf(NAME_Init, TEXT("Using pixel format %i"), nPixelFormat);
	check(nPixelFormat);

	verify(SetPixelFormat(m_hDC, nPixelFormat, &pfd));
	m_hRC = wglCreateContext(m_hDC);
	check(m_hRC);

	if (DescribePixelFormat(m_hDC, nPixelFormat, sizeof(pfd), &pfd)) {
		m_numDepthBits = pfd.cDepthBits;
	}

	MakeCurrent();

	return;
}

//The basic path matches nearest and can land on 16 depth bits silently.
//This one treats the request as a lower bound.
bool UOpenGLRenderDevice::SetARBPixelFormat(INT NewColorBytes) {
	static const int depthBitsToTry[3] = { 32, 24, 16 };
	InitARBPixelFormatRet_t iapfRet;
	int iFormats[1];
	int iAttributes[12];
	UINT nNumFormats;
	PIXELFORMATDESCRIPTOR tempPfd;
	unsigned int u;
	bool haveFormat;

	iapfRet.p_wglChoosePixelFormatARB = NULL;
	iapfRet.haveWGLMultisampleARB = false;
	InitARBPixelFormat(NewColorBytes, &iapfRet);

	//No multisampling requested.
	if (iapfRet.p_wglChoosePixelFormatARB == NULL) {
		return false;
	}

	if (DebugBit(DEBUG_BIT_BASIC)) dbgPrintf("utglr: ARBInit\n");

	iAttributes[0] = WGL_SUPPORT_OPENGL_ARB;
	iAttributes[1] = GL_TRUE;
	iAttributes[2] = WGL_DRAW_TO_WINDOW_ARB;
	iAttributes[3] = GL_TRUE;
	iAttributes[4] = WGL_COLOR_BITS_ARB;
	iAttributes[5] = (NewColorBytes <= 2) ? 16 : 24;
	iAttributes[6] = WGL_DEPTH_BITS_ARB;
	iAttributes[7] = 32;
	iAttributes[8] = WGL_DOUBLE_BUFFER_ARB;
	iAttributes[9] = GL_TRUE;
	iAttributes[10] = 0;
	iAttributes[11] = 0;

	haveFormat = false;
	for (u = 0; u < ARRAY_COUNT(depthBitsToTry); u++) {
		iAttributes[7] = depthBitsToTry[u];
		if (iapfRet.p_wglChoosePixelFormatARB(m_hDC, iAttributes, NULL, 1, iFormats, &nNumFormats) && (nNumFormats != 0)) {
			haveFormat = true;
			break;
		}
	}
	if (!haveFormat) {
		if (DebugBit(DEBUG_BIT_BASIC)) dbgPrintf("utglr: ARBInit failed\n");
		return false;
	}

	appMemzero(&tempPfd, sizeof(tempPfd));
	tempPfd.nSize = sizeof(tempPfd);
	verify(SetPixelFormat(m_hDC, iFormats[0], &tempPfd));
	m_hRC = wglCreateContext(m_hDC);
	check(m_hRC);

	MakeCurrent();

	//A safe lower bound.
	m_numDepthBits = depthBitsToTry[u];

	PFNWGLGETPIXELFORMATATTRIBIVARBPROC p_wglGetPixelFormatAttribivARB = reinterpret_cast<PFNWGLGETPIXELFORMATATTRIBIVARBPROC>(wglGetProcAddress("wglGetPixelFormatAttribivARB"));
	if (p_wglGetPixelFormatAttribivARB != NULL) {
		int iAttribute;
		int iValue;

		iAttribute = WGL_DEPTH_BITS_ARB;
		iValue = m_numDepthBits;
		if (p_wglGetPixelFormatAttribivARB(m_hDC, iFormats[0], 0, 1, &iAttribute, &iValue)) {
			m_numDepthBits = iValue;
		}
	}

	return true;
}

bool UOpenGLRenderDevice::SetAAPixelFormat(INT NewColorBytes) {
	static const int depthBitsToTry[3] = { 32, 24, 16 };
	InitARBPixelFormatRet_t iapfRet;
	int iFormats[1];
	int iAttributes[30];
	UINT nNumFormats;
	PIXELFORMATDESCRIPTOR tempPfd;
	INT trySamples;
	//Outside the retry loop, so the granted depth stays readable.
	unsigned int depthIndex;
	bool haveFormat;

	iapfRet.p_wglChoosePixelFormatARB = NULL;
	iapfRet.haveWGLMultisampleARB = false;
	InitARBPixelFormat(NewColorBytes, &iapfRet);

	if ((iapfRet.p_wglChoosePixelFormatARB == NULL) || (iapfRet.haveWGLMultisampleARB != true)) {
		return false;
	}

	if (DebugBit(DEBUG_BIT_BASIC)) dbgPrintf("utglr: AAInit\n");

	iAttributes[0] = WGL_SUPPORT_OPENGL_ARB;
	iAttributes[1] = GL_TRUE;
	iAttributes[2] = WGL_DRAW_TO_WINDOW_ARB;
	iAttributes[3] = GL_TRUE;
	iAttributes[4] = WGL_COLOR_BITS_ARB;
	iAttributes[5] = (NewColorBytes <= 2) ? 16 : 24;
	iAttributes[6] = WGL_DEPTH_BITS_ARB;
	iAttributes[7] = 32;
	iAttributes[8] = WGL_DOUBLE_BUFFER_ARB;
	iAttributes[9] = GL_TRUE;
	iAttributes[10] = WGL_SAMPLE_BUFFERS_ARB;
	iAttributes[11] = GL_TRUE;
	iAttributes[12] = WGL_SAMPLES_ARB;
	iAttributes[13] = NumAASamples;
	iAttributes[14] = 0;
	iAttributes[15] = 0;

	//Halve on refusal.
	haveFormat = false;
	trySamples = NumAASamples;
	for (;;) {
		iAttributes[13] = trySamples;
		for (depthIndex = 0; depthIndex < ARRAY_COUNT(depthBitsToTry); depthIndex++) {
			iAttributes[7] = depthBitsToTry[depthIndex];
			if (iapfRet.p_wglChoosePixelFormatARB(m_hDC, iAttributes, NULL, 1, iFormats, &nNumFormats) && (nNumFormats != 0)) {
				haveFormat = true;
				break;
			}
		}

		if (haveFormat || (trySamples <= 2)) {
			break;
		}
		trySamples /= 2;
	}
	if (!haveFormat) {
		if (DebugBit(DEBUG_BIT_BASIC)) dbgPrintf("utglr: AAInit failed\n");
		return false;
	}
	m_initNumAASamples = trySamples;
	/*
	Again a safe lower bound, because without it the count keeps its placeholder whenever the
	readback below is unavailable and a false low-depth warning fires on hardware that granted
	more than was asked for, which is exactly the hardware nobody needs warning about, so the
	bound is fixed and the warning kept for the case it was written for.
	*/
	m_numDepthBits = depthBitsToTry[depthIndex];

	appMemzero(&tempPfd, sizeof(tempPfd));
	tempPfd.nSize = sizeof(tempPfd);
	verify(SetPixelFormat(m_hDC, iFormats[0], &tempPfd));
	m_hRC = wglCreateContext(m_hDC);
	check(m_hRC);

	MakeCurrent();

	PFNWGLGETPIXELFORMATATTRIBIVARBPROC p_wglGetPixelFormatAttribivARB = reinterpret_cast<PFNWGLGETPIXELFORMATATTRIBIVARBPROC>(wglGetProcAddress("wglGetPixelFormatAttribivARB"));
	if (p_wglGetPixelFormatAttribivARB != NULL) {
		int iAttribute;
		int iValue;

		iAttribute = WGL_DEPTH_BITS_ARB;
		iValue = m_numDepthBits;
		if (p_wglGetPixelFormatAttribivARB(m_hDC, iFormats[0], 0, 1, &iAttribute, &iValue)) {
			m_numDepthBits = iValue;
		}

		//The request is a lower bound.
		iAttribute = WGL_SAMPLES_ARB;
		iValue = m_initNumAASamples;
		if (p_wglGetPixelFormatAttribivARB(m_hDC, iFormats[0], 0, 1, &iAttribute, &iValue)) {
			m_initNumAASamples = iValue;
		}
	}

	return true;
}
#endif
