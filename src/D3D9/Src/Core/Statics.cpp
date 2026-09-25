
#include "../D3D9Drv.h"
#include "../D3D9.h"
#include "Globals.h"


const TCHAR *g_pSection = TEXT("D3D9Drv.D3D9RenderDevice");


INT UD3D9RenderDevice::NumDevices = 0;
INT UD3D9RenderDevice::LockCount = 0;

HMODULE UD3D9RenderDevice::hModuleD3d9 = NULL;
LPDIRECT3DCREATE9 UD3D9RenderDevice::pDirect3DCreate9 = NULL;

bool UD3D9RenderDevice::g_haveOriginalGammaRamp = false;
D3DGAMMARAMP UD3D9RenderDevice::g_originalGammaRamp;

BYTE UD3D9RenderDevice::m_skippedFrameScratch[VERTEX_RING_SIZE * 16];
