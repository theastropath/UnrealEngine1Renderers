#include "../D3D9Drv.h"
#include "../D3D9.h"

//The engine hands over a pointer with no size, so the only safe length is comfortably under 256 TCHARs.
static void FASTCALL CopyStatsResult(TCHAR *pDest, const TCHAR *pSrc) {
	enum { STATS_RESULT_MAX_CHARS = 256 };
	INT i = 0;
	while ((i < (STATS_RESULT_MAX_CHARS - 1)) && (pSrc[i] != 0)) {
		pDest[i] = pSrc[i];
		i++;
	}
	pDest[i] = 0;
}

void UD3D9RenderDevice::GetStats(TCHAR *Result) {
	guard(UD3D9RenderDevice::GetStats);

	if (!Result) {
		return;
	}

	double msPerCycle = GSecondsPerCycle * 1000.0f;
	//Surf/Draw: world surfaces batched against draw calls issued. Cmd/Run/Calls/Build:
	//the same for the whole frame - primitives submitted, runs collapsed into,
	//draw calls issued, worker-pool build time. Low Calls with high Build wants wider jobs.
	//Tex: video memory the budget tracks,
	//then the total including atlas pages.
	//Atlas: pages held, then tiles packed, rewritten and refused this frame.
	const FLightmapAtlas::FStats &atlasStats = m_lightmapAtlas.Stats();

	TCHAR statsStr[512];
	appSprintf(
		statsStr,
		TEXT("D3D9 stats: Bind=%04.1f Image=%04.1f Complex=%04.1f Gouraud=%04.1f Tile=%04.1f Surf=%u Draw=%u Cmd=%u Run=%u Calls=%u Jobs=%u Build=%04.2fms Tex=%u/%uMB Atlas=%u/%u/%u/%u"),
		msPerCycle * BindCycles,
		msPerCycle * ImageCycles,
		msPerCycle * ComplexCycles,
		msPerCycle * GouraudCycles,
		msPerCycle * TileCycles,
		m_csBatchSurfaceCount,
		m_csBatchDrawCount,
		m_recordedCmdCount,
		m_runCount,
		m_drawCallCount,
		m_buildJobCount,
		(FLOAT)m_buildMicroseconds / 1000.0f,
		m_texCacheBytes / (1024 * 1024),
		(m_texCacheBytes + m_lightmapAtlas.Bytes()) / (1024 * 1024),
		m_lightmapAtlas.PageCount(),
		atlasStats.Placed,
		atlasStats.Rewritten,
		atlasStats.Failed);

	CopyStatsResult(Result, statsStr);

	unguard;
}
