#include "../OpenGLDrv.h"
#include "../OpenGL.h"
#include "../Core/Globals.h"

bool UOpenGLRenderDevice::IsGLExtensionSupported(const char *pExtensionsString, const char *pExtensionName) {
	const char *pStart;
	const char *pWhere, *pTerminator;

	if ((pExtensionsString == NULL) || (pExtensionName == NULL)) {
		return false;
	}

	pStart = pExtensionsString;
	while (1) {
		pWhere = strstr(pStart, pExtensionName);
		if (pWhere == NULL) {
			break;
		}
		pTerminator = pWhere + strlen(pExtensionName);
		if ((pWhere == pStart) || (*(pWhere - 1) == ' ')) {
			if ((*pTerminator == ' ') || (*pTerminator == '\0')) {
				return true;
			}
		}
		pStart = pTerminator;
	}

	return false;
}

bool UOpenGLRenderDevice::GetGL1Proc(void *&ProcAddress, const char *pName) {
	guard(UOpenGLRenderDevice::GetGL1Proc);

#ifdef __UNIX__
	ProcAddress = (void *)SDL_GL_GetProcAddress(pName);
#else
	ProcAddress = GetProcAddress(hModuleGlMain, pName);
#endif
	if (!ProcAddress) {
		debugf(TEXT("   Missing function '%s' for '%s' support"), appFromAnsi(pName), TEXT("OpenGL 1.x"));
		dbgPrintf("Missing function '%s' for '%s' support\n", pName, "OpenGL 1.x");
		return false;
	}

	return true;

	unguard;
}

bool UOpenGLRenderDevice::GetGL1Procs(void) {
	guard(UOpenGLRenderDevice::GetGL1Procs);

	bool loadOk = true;

#define GL1_PROC(ret, func, params) loadOk &= GetGL1Proc(*(void **)&func, #func);
#include "OpenGL1Funcs.h"
#undef GL1_PROC

	return loadOk;

	unguard;
}

bool UOpenGLRenderDevice::FindGLExt(const char *pName) {
	guard(UOpenGLRenderDevice::FindGLExt);

	bool bRet = IsGLExtensionSupported(GLStringOrEmpty(glGetString(GL_EXTENSIONS)), pName);
	if (bRet) {
		debugf(NAME_Init, TEXT("Device supports: %s"), appFromAnsi(pName));
	}
	if (DebugBit(DEBUG_BIT_BASIC)) {
		dbgPrintf("utglr: GL_EXT: %s = %d\n", pName, bRet);
	}

	return bRet;
	unguard;
}

void UOpenGLRenderDevice::GetGLExtProc(void *&ProcAddress, const char *pName, const char *pSupportName, bool &Supports) {
	guard(UOpenGLRenderDevice::GetGLExtProc);

	if (!Supports) {
		return;
	}

#ifdef __UNIX__
	ProcAddress = (void *)SDL_GL_GetProcAddress(pName);
#else
	ProcAddress = wglGetProcAddress(pName);
#endif
	if (!ProcAddress) {
		Supports = false;

		debugf(TEXT("   Missing function '%s' for '%s' support"), appFromAnsi(pName), appFromAnsi(pSupportName));
		dbgPrintf("Missing function '%s' for '%s' support\n", pName, pSupportName);
	}

	unguard;
}

void UOpenGLRenderDevice::GetGLExtProcs(void) {
	guard(UOpenGLRenderDevice::GetGLExtProcs);
#define GL_EXT_NAME(name) SUPPORTS##name = FindGLExt(#name + 1);
#define GL_EXT_PROC(ext, ret, func, params) GetGLExtProc(*(void **)&func, #func, #ext + 1, SUPPORTS##ext);
#include "OpenGLExtFuncs.h"
#undef GL_EXT_NAME
#undef GL_EXT_PROC
	unguard;
}
