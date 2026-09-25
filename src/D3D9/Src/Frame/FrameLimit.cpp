
#include "../D3D9Drv.h"
#include "../D3D9.h"

#ifdef WIN32
#include <mmsystem.h>
#endif


void UD3D9RenderDevice::InitFrameRateLimitTimerSafe(void) {
	if (m_frameRateLimitTimerInitialized) {
		return;
	}
	m_frameRateLimitTimerInitialized = true;

#ifdef WIN32
	timeBeginPeriod(1);
#endif

	return;
}

void UD3D9RenderDevice::ShutdownFrameRateLimitTimer(void) {
	if (!m_frameRateLimitTimerInitialized) {
		return;
	}
	m_frameRateLimitTimerInitialized = false;

#ifdef WIN32
	timeEndPeriod(1);
#endif

	return;
}
