#include "../OpenGLDrv.h"
#include "../OpenGL.h"
#include "Globals.h"

//Must match the class's package.class path; the engine keys config reads off it.
const TCHAR *g_pSection = TEXT("OpenGL1xDrv.OpenGLRenderDevice");


INT UOpenGLRenderDevice::NumDevices = 0;
INT UOpenGLRenderDevice::LockCount = 0;
#ifdef _WIN32
HGLRC UOpenGLRenderDevice::hCurrentRC = NULL;
HMODULE UOpenGLRenderDevice::hModuleGlMain = NULL;
TArray<HGLRC> UOpenGLRenderDevice::AllContexts;
#else
UBOOL UOpenGLRenderDevice::GLLoaded = false;
#endif

bool UOpenGLRenderDevice::g_gammaFirstTime = false;
bool UOpenGLRenderDevice::g_haveOriginalGammaRamp = false;
#ifdef _WIN32
UOpenGLRenderDevice::FGammaRamp UOpenGLRenderDevice::g_originalGammaRamp;
#endif

UOpenGLRenderDevice::DWORD_CTTree_t UOpenGLRenderDevice::m_sharedZeroPrefixBindTrees[NUM_CTTree_TREES];
UOpenGLRenderDevice::QWORD_CTTree_t UOpenGLRenderDevice::m_sharedNonZeroPrefixBindTrees[NUM_CTTree_TREES];
CCachedTextureChain UOpenGLRenderDevice::m_sharedZeroPrefixBindChain;
CCachedTextureChain UOpenGLRenderDevice::m_sharedNonZeroPrefixBindChain;
DWORD UOpenGLRenderDevice::m_sharedTexCacheBytes;
INT UOpenGLRenderDevice::m_sharedAllocatedTextures;
UOpenGLRenderDevice::QWORD_CTTree_NodePool_t UOpenGLRenderDevice::m_sharedNonZeroPrefixTexIdPool;
UOpenGLRenderDevice::TexPoolMap_t UOpenGLRenderDevice::m_sharedRGBA8TexPool;

#define GL1_PROC(ret, func, params) ret(STDCALL *UOpenGLRenderDevice::func) params;
#include "../Device/OpenGL1Funcs.h"
#undef GL1_PROC

#define GL_EXT_NAME(name) bool UOpenGLRenderDevice::SUPPORTS##name = 0;
#define GL_EXT_PROC(ext, ret, func, params) ret(STDCALL *UOpenGLRenderDevice::func) params;
#include "../Device/OpenGLExtFuncs.h"
#undef GL_EXT_NAME
#undef GL_EXT_PROC

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
