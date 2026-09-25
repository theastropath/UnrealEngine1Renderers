#include "../OpenGLDrv.h"
#include "../OpenGL.h"

#ifdef _WIN32
#include <mmsystem.h>
#endif


void UOpenGLRenderDevice::SetSwapIntervalSafe(void) {
#ifdef _WIN32
	if ((SwapInterval < 0) || (SwapInterval > 10)) {
		return;
	}

	PFNWGLGETEXTENSIONSSTRINGARBPROC p_wglGetExtensionsStringARB = reinterpret_cast<PFNWGLGETEXTENSIONSSTRINGARBPROC>(wglGetProcAddress("wglGetExtensionsStringARB"));
	if (p_wglGetExtensionsStringARB == NULL) {
		return;
	}

	const char *pWGLExtensions = p_wglGetExtensionsStringARB(m_hDC);
	if (pWGLExtensions == NULL) {
		return;
	}
	if (!IsGLExtensionSupported(pWGLExtensions, "WGL_EXT_swap_control")) {
		return;
	}

	PFNWGLSWAPINTERVALEXTPROC p_wglSwapIntervalEXT = reinterpret_cast<PFNWGLSWAPINTERVALEXTPROC>(wglGetProcAddress("wglSwapIntervalEXT"));
	if (p_wglSwapIntervalEXT == NULL) {
		return;
	}

	p_wglSwapIntervalEXT(SwapInterval);
#else
#endif

	return;
}


void UOpenGLRenderDevice::InitFrameRateLimitTimerSafe(void) {
	if (m_frameRateLimitTimerInitialized) {
		return;
	}
	m_frameRateLimitTimerInitialized = true;

#ifdef _WIN32
	timeBeginPeriod(1);
#endif

	return;
}

void UOpenGLRenderDevice::ShutdownFrameRateLimitTimer(void) {
	if (!m_frameRateLimitTimerInitialized) {
		return;
	}
	m_frameRateLimitTimerInitialized = false;

#ifdef _WIN32
	timeEndPeriod(1);
#endif

	return;
}
