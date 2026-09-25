/** \file Frame/FrameLimit.cpp */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "../D3D10.h"


/**@name Frame rate limiter
Sleeps, then spins.
*/
//@{

//Missing pre-1803.
#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif

static const float FRAME_LIMIT_SPIN_TIME = 0.006f;

HANDLE UD3D10RenderDevice::getFrameTimer() {
	if (!frameTimerCreated) {
		frameTimerCreated = true;
		frameTimer = CreateWaitableTimerExW(nullptr, nullptr,
			CREATE_WAITABLE_TIMER_MANUAL_RESET | CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
	}
	return frameTimer;
}

void UD3D10RenderDevice::releaseFrameTimer() {
	if (frameTimer)
		CloseHandle(frameTimer);
	frameTimer = nullptr;
	frameTimerCreated = false;
}

void UD3D10RenderDevice::limitFrameRate(const LARGE_INTEGER &startTime, float targetFrameTime) {
	HANDLE timer = getFrameTimer();
	const float counterFreq = (float)perfCounterFreq.QuadPart;

	for (;;) {
		LARGE_INTEGER time;
		QueryPerformanceCounter(&time);
		const float elapsed = (time.QuadPart - startTime.QuadPart) / counterFreq;
		const float remaining = targetFrameTime - elapsed;

		if (remaining <= 0.0f)
			return;
		if (remaining <= FRAME_LIMIT_SPIN_TIME)
			continue;

		if (timer) {
			LARGE_INTEGER due;
			//Relative, in 100ns units.
			due.QuadPart = -(LONGLONG)((remaining - FRAME_LIMIT_SPIN_TIME) * 10000000.0f);
			if (SetWaitableTimer(timer, &due, 0, nullptr, nullptr, FALSE))
				WaitForSingleObject(timer, INFINITE);
			else
				Sleep(1);
		} else {
			Sleep(1);
		}
	}
}
//@}
