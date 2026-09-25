/*=============================================================================
	Unlock.cpp: closing a frame, and the flush between levels.
=============================================================================*/

#include "../OpenGLDrv.h"
#include "../OpenGL.h"


//The last slice of the frame wait is spun.
//Sleeping alone overshoots by the timer's granularity.
//Spinning the whole wait would burn a core.
static const FLOAT UTGLR_FRAME_LIMIT_SPIN_TIME = 0.006f;
void UOpenGLRenderDevice::Unlock(UBOOL Blit) {
	UTGLR_DEBUG_CALL_COUNT(Unlock);
	guard(UOpenGLRenderDevice::Unlock);

	EndDeferredGouraudPolys(); //Even with no HUD.
	EndBuffering();

	SetDefaultAAState();
	SetDefaultProjectionState();
	SetDefaultColorState();
	SetDefaultShaderState();
	SetDefaultTextureState();

	check(LockCount == 1);

	//glFlush();

	if (Blit) {
		//The last thing drawn.
		if (UsingPostProcessGamma() && m_gammaRampInEffect) {
			DrawGammaPostProcess();
		}

		CheckGLErrorFlag(TEXT("please report this bug"));

#ifdef __UNIX__
		SDL_GL_SwapBuffers();
#else
		{
			bool SwapBuffersStatus;

			SwapBuffersStatus = (SwapBuffers(m_hDC)) ? true : false;

			if (!m_prevSwapBuffersStatus) {
				check(SwapBuffersStatus);
			}
			m_prevSwapBuffersStatus = SwapBuffersStatus;
		}
#endif
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

			//A backwards clock step inflates the wait.
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
	dbgPrintf("VP enable count = %u\n", m_vpEnableCount);
	dbgPrintf("VP switch count = %u\n", m_vpSwitchCount);
	dbgPrintf("FP enable count = %u\n", m_fpEnableCount);
	dbgPrintf("FP switch count = %u\n", m_fpSwitchCount);
	dbgPrintf("AA switch count = %u\n", m_AASwitchCount);
	dbgPrintf("Scene node count = %u\n", m_sceneNodeCount);
	dbgPrintf("Scene node refresh count = %u\n", m_sceneNodeRefreshCount);
#endif


	unguard;
}

#ifdef UTGLR_OLD_URENDERDEVICE
void UOpenGLRenderDevice::Flush()
#else
void UOpenGLRenderDevice::Flush(UBOOL AllowPrecache)
#endif
{
	guard(UOpenGLRenderDevice::Flush);
	unsigned int u;
	TArray<GLuint> Binds;

	//About to be released.
	DiscardDeferredGouraudPolys();

	for (u = 0; u < NUM_CTTree_TREES; u++) {
		DWORD_CTTree_t *zeroPrefixBindTree = &m_zeroPrefixBindTrees[u];
		for (DWORD_CTTree_t::node_t *zpbmPtr = zeroPrefixBindTree->begin(); zpbmPtr != zeroPrefixBindTree->end(); zpbmPtr = zeroPrefixBindTree->next_node(zpbmPtr)) {
			Binds.AddItem(zpbmPtr->data.Id);
		}
		zeroPrefixBindTree->clear(&m_DWORD_CTTree_Allocator);
	}

	for (u = 0; u < NUM_CTTree_TREES; u++) {
		QWORD_CTTree_t *nonZeroPrefixBindTree = &m_nonZeroPrefixBindTrees[u];
		for (QWORD_CTTree_t::node_t *nzpbmPtr = nonZeroPrefixBindTree->begin(); nzpbmPtr != nonZeroPrefixBindTree->end(); nzpbmPtr = nonZeroPrefixBindTree->next_node(nzpbmPtr)) {
			Binds.AddItem(nzpbmPtr->data.Id);
		}
		nonZeroPrefixBindTree->clear(&m_QWORD_CTTree_Allocator);
	}

	m_zeroPrefixBindChain->mark_as_clear();
	m_nonZeroPrefixBindChain->mark_as_clear();

	//Everything the counters track is deleted below.
	*m_texCacheBytes = 0;
	//A new level warns again.
	m_texCacheBudgetUnreachable = false;

	while (QWORD_CTTree_NodePool_t::node_t *nzptipPtr = m_nonZeroPrefixTexIdPool->try_remove()) {
		Binds.AddItem(nzptipPtr->data.Id);
		m_QWORD_CTTree_Allocator.free_node(nzptipPtr);
	}

	for (TexPoolMap_t::node_t *RGBA8TpPtr = m_RGBA8TexPool->begin(); RGBA8TpPtr != m_RGBA8TexPool->end(); RGBA8TpPtr = m_RGBA8TexPool->next_node(RGBA8TpPtr)) {
		while (QWORD_CTTree_NodePool_t::node_t *texPoolNodePtr = RGBA8TpPtr->data.try_remove()) {
			Binds.AddItem(texPoolNodePtr->data.Id);
			m_QWORD_CTTree_Allocator.free_node(texPoolNodePtr);
		}
	}
	m_RGBA8TexPool->clear(&m_TexPoolMap_Allocator);

	while (QWORD_CTTree_NodePool_t::node_t *nzpnpPtr = m_nonZeroPrefixNodePool.try_remove()) {
		m_QWORD_CTTree_Allocator.free_node(nzpnpPtr);
	}

	if (Binds.Num()) {
		glDeleteTextures(Binds.Num(), (GLuint *)&Binds(0));
	}
	*m_allocatedTextures = 0;

	for (u = 0; u < MAX_TMUNITS; u++) {
		TexInfo[u].CurrentCacheID = TEX_CACHE_ID_UNUSED;
		TexInfo[u].pBind = NULL;
	}

	//Any open batch names textures just deleted.
	BufferedVerts = 0;
	BufferedTileVerts = 0;
	BufferedLineVerts = 0;
	BufferedPointVerts = 0;

	//The 226 engine build has no AllowPrecache setting.
#ifndef UTGLR_OLD_URENDERDEVICE
	if (AllowPrecache && UsePrecache && !GIsEditor) {
		PrecacheOnFlip = 1;
	}
#endif

	//Gamma is left alone here.
	//The mode-set and per-frame paths already handle it.

	unguard;
}
