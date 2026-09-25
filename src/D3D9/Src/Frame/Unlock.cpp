/*=============================================================================
	Unlock.cpp: the end of a frame, and the flush between levels.
=============================================================================*/

#include "../D3D9Drv.h"
#include "../D3D9.h"


//The final slice of the frame wait is spun because sleeping alone overshoots by the timer's
//granularity and the overshoot is visible as a stutter, while spinning the whole wait out burns
//a core for nothing, so the two are split at a margin wide enough to cover the granularity the
//timer was asked for, which is one millisecond for as long as the limiter is running.
static const FLOAT UTGLR_FRAME_LIMIT_SPIN_TIME = 0.006f;

void UD3D9RenderDevice::Unlock(UBOOL Blit) {
#ifdef UTGLR_DEBUG_SHOW_CALL_COUNTS
	{
		static int si;
		dout << L"utd3d9r: Unlock = " << si++ << std::endl;
	}
#endif
	guard(UD3D9RenderDevice::Unlock);

	EndDeferredGouraudPolys(); //Even with no HUD.
	FlushDeferred(); //Last chance.
	EndBuffering();

	if (!m_frameSkipped) {
		SetDefaultAAState();
		SetDefaultProjectionState();
		SetDefaultStreamState();
		SetDefaultTextureState();
	}

	check(LockCount == 1);

	if (!m_frameSkipped) {
		if (FAILED(m_d3dDevice->EndScene())) {
			appErrorf(TEXT("EndScene failed"));
		}
	}

	if (Blit && !m_frameSkipped) {
		HRESULT hResult;
		bool swapBuffersStatus;

		ApplyGammaPass();

		hResult = m_d3dDevice->Present(NULL, NULL, NULL, NULL);
		swapBuffersStatus = (FAILED(hResult)) ? false : true;
		if (hResult == D3DERR_DEVICELOST) swapBuffersStatus = true;

		//Tolerate an isolated failed present. Not two in a row.
		if (!swapBuffersStatus && !m_prevSwapBuffersStatus) {
			appErrorf(TEXT("Present failed (0x%08X)"), hResult);
		}
		m_prevSwapBuffersStatus = swapBuffersStatus;
	}

	--LockCount;

	if (m_HitData) {
		INT i;

		m_gclip.SelectModeEnd();

		*m_HitSize = m_HitCount;

		for (i = 0; i < 5; i++) {
			m_gclip.SetCpEnable(i, false);
		}
	}

	if (UseTexIdPool) {
		ScanForOldTextures();
	}

	EvictOverBudgetTextures();

	m_lightmapAtlas.NewFrame();

	m_currentFrameCount++;

	if (FrameRateLimit >= 20) {
		//DOUBLE to match the timestamp.
#if defined UTGLR_DX_BUILD || defined UTGLR_RUNE_BUILD || defined UTGLR_OLD_URENDERDEVICE || defined UTGLR_UNREAL_227_BUILD
		DOUBLE curFrameTimestamp;
		DOUBLE spinTimestamp;
#else
		FTime curFrameTimestamp;
		FTime spinTimestamp;
#endif
		float timeDiff;
		float rcpFrameRateLimit;

		InitFrameRateLimitTimerSafe();

		curFrameTimestamp = appSeconds();
		timeDiff = curFrameTimestamp - m_prevFrameTimestamp;
		m_prevFrameTimestamp = curFrameTimestamp;

		rcpFrameRateLimit = 1.0f / FrameRateLimit;
		if (timeDiff < rcpFrameRateLimit) {
			float waitTime;
			float sleepTime;

			//Clamped because a backwards clock step can make the elapsed time negative.
			waitTime = Clamp(rcpFrameRateLimit - timeDiff, 0.0f, rcpFrameRateLimit);

			//Sleep out all but the margin.
			sleepTime = waitTime - UTGLR_FRAME_LIMIT_SPIN_TIME;
			if (sleepTime > 0.0f) {
				appSleep(sleepTime);
			}
			do {
				spinTimestamp = appSeconds();
			} while ((spinTimestamp - curFrameTimestamp) < waitTime);

			m_prevFrameTimestamp = spinTimestamp;
		}
	}


#if 0
	dout << TEXT("VP enable count = ") << m_vpEnableCount << std::endl;
	dout << TEXT("VP switch count = ") << m_vpSwitchCount << std::endl;
	dout << TEXT("FP enable count = ") << m_fpEnableCount << std::endl;
	dout << TEXT("FP switch count = ") << m_fpSwitchCount << std::endl;
	dout << TEXT("AA switch count = ") << m_AASwitchCount << std::endl;
	dout << TEXT("Scene node count = ") << m_sceneNodeCount << std::endl;
	dout << TEXT("Scene node refresh count = ") << m_sceneNodeRefreshCount << std::endl;
	dout << TEXT("VB flush count = ") << m_vbFlushCount << std::endl;
	dout << TEXT("Stat 0 count = ") << m_stat0Count << std::endl;
	dout << TEXT("Stat 1 count = ") << m_stat1Count << std::endl;
#endif


	unguard;
}

#ifdef UTGLR_OLD_URENDERDEVICE
void UD3D9RenderDevice::Flush()
#else
void UD3D9RenderDevice::Flush(UBOOL AllowPrecache)
#endif
{
	guard(UD3D9RenderDevice::Flush);
	unsigned int u;

	if (!m_d3dDevice) {
		return;
	}

	//Those textures are going.
	DiscardDeferredGouraudPolys();

	//Recorded geometry too.
	FlushDeferred();

	//Any open batch still holds a texture binding and buffer locks about to go invalid.
	EndBuffering();
	m_lightmapAtlas.Reset();

	for (u = 0; u < (DWORD)TMUnits; u++) {
		m_d3dDevice->SetTexture(u, NULL);
	}

	for (u = 0; u < NUM_CTTree_TREES; u++) {
		DWORD_CTTree_t *zeroPrefixBindTree = &m_zeroPrefixBindTrees[u];
		for (DWORD_CTTree_t::node_t *zpbmPtr = zeroPrefixBindTree->begin(); zpbmPtr != zeroPrefixBindTree->end(); zpbmPtr = zeroPrefixBindTree->next_node(zpbmPtr)) {
			if (zpbmPtr->data.pTexObj) {
				zpbmPtr->data.pTexObj->Release();
			}
		}
		zeroPrefixBindTree->clear(&m_DWORD_CTTree_Allocator);
	}

	for (u = 0; u < NUM_CTTree_TREES; u++) {
		QWORD_CTTree_t *nonZeroPrefixBindTree = &m_nonZeroPrefixBindTrees[u];
		for (QWORD_CTTree_t::node_t *nzpbmPtr = nonZeroPrefixBindTree->begin(); nzpbmPtr != nonZeroPrefixBindTree->end(); nzpbmPtr = nonZeroPrefixBindTree->next_node(nzpbmPtr)) {
			if (nzpbmPtr->data.pTexObj) {
				nzpbmPtr->data.pTexObj->Release();
			}
		}
		nonZeroPrefixBindTree->clear(&m_QWORD_CTTree_Allocator);
	}

	m_zeroPrefixBindChain->mark_as_clear();
	m_nonZeroPrefixBindChain->mark_as_clear();

	for (TexPoolMap_t::node_t *RGBA8TpPtr = m_RGBA8TexPool->begin(); RGBA8TpPtr != m_RGBA8TexPool->end(); RGBA8TpPtr = m_RGBA8TexPool->next_node(RGBA8TpPtr)) {
		while (QWORD_CTTree_NodePool_t::node_t *texPoolNodePtr = RGBA8TpPtr->data.try_remove()) {
			//A failed texture creation is kept out of the pool elsewhere, so this should be unreachable.
			if (texPoolNodePtr->data.pTexObj) {
				texPoolNodePtr->data.pTexObj->Release();
			}
			m_QWORD_CTTree_Allocator.free_node(texPoolNodePtr);
		}
	}
	m_RGBA8TexPool->clear(&m_TexPoolMap_Allocator);

	while (QWORD_CTTree_NodePool_t::node_t *nzpnpPtr = m_nonZeroPrefixNodePool.try_remove()) {
		m_QWORD_CTTree_Allocator.free_node(nzpnpPtr);
	}

	AllocatedTextures = 0;
	m_texCacheBytes = 0;
	//The cache the warning was about is gone.
	m_texCacheBudgetUnreachable = false;

	for (u = 0; u < MAX_TMUNITS; u++) {
		TexInfo[u].CurrentCacheID = TEX_CACHE_ID_UNUSED;
		TexInfo[u].pBind = NULL;
	}

	//The 226 engine build has no AllowPrecache setting.
#ifndef UTGLR_OLD_URENDERDEVICE
	if (AllowPrecache && UsePrecache && !GIsEditor) {
		PrecacheOnFlip = 1;
	}
#endif

	//Gamma is deliberately left alone: the mode-set, device-reset and per-frame brightness paths
	//already handle it.

	unguard;
}
