#include "../OpenGLDrv.h"
#include "../OpenGL.h"

static void FASTCALL CopyStatsResult(TCHAR *pDest, const TCHAR *pSrc) {
	enum { STATS_RESULT_MAX_CHARS = 256 };
	INT i = 0;
	while ((i < (STATS_RESULT_MAX_CHARS - 1)) && (pSrc[i] != 0)) {
		pDest[i] = pSrc[i];
		i++;
	}
	pDest[i] = 0;
}

void UOpenGLRenderDevice::GetStats(TCHAR *Result) {
	guard(UOpenGLRenderDevice::GetStats);

	if (!Result) {
		return;
	}

	TCHAR statsStr[512];

	double msPerCycle = GSecondsPerCycle * 1000.0f;
	appSprintf(
		statsStr,
		TEXT("OpenGL stats: Bind=%04.1f Image=%04.1f Complex=%04.1f Gouraud=%04.1f Tile=%04.1f"),
		msPerCycle * BindCycles,
		msPerCycle * ImageCycles,
		msPerCycle * ComplexCycles,
		msPerCycle * GouraudCycles,
		msPerCycle * TileCycles);

	CopyStatsResult(Result, statsStr);

	unguard;
}
