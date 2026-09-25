/*=============================================================================
	PixelFormatTypes.h: parameters and results for the pixel format search.
=============================================================================*/

#pragma once

#ifdef _WIN32
typedef struct {
	PFNWGLCHOOSEPIXELFORMATARBPROC p_wglChoosePixelFormatARB;
	bool haveWGLMultisampleARB;
} InitARBPixelFormatRet_t;

typedef struct {
	INT NewColorBytes;
	PFNWGLCHOOSEPIXELFORMATARBPROC p_wglChoosePixelFormatARB;
	bool haveWGLMultisampleARB;
} InitARBPixelFormatWndProcParams_t;
#endif
