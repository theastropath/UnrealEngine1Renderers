/**
\file Frame/Gamma.cpp

The engine writes brightness into the adapter's gamma ramp, which is system wide and outlives the
process, so this renderer applies brightness in the final shader pass instead and hands the ramp
back the way it found it.
*/

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <math.h>

#include "../D3D10.h"


/**@name System gamma neutralization */
//@{
static const int GAMMA_RAMP_SIZE = 256;
//As Epic's own renderers do it.
static const float GAMMA_BRIGHTNESS_SCALE = 2.5f;
//Twice 8-bit quantization.
static const int GAMMA_RAMP_TOLERANCE = 512;

static void buildGammaRamp(WORD ramp[3][GAMMA_RAMP_SIZE], float gamma) {
	const float exponent = 1.0f / gamma;
	for (int i = 0; i < GAMMA_RAMP_SIZE; i++) {
		int entry = (int)(powf(i / (float)(GAMMA_RAMP_SIZE - 1), exponent) * 65535.0f + 0.5f);
		CLAMP(entry, 0, 65535);
		ramp[0][i] = ramp[1][i] = ramp[2][i] = (WORD)entry;
	}
}

static bool gammaRampsMatch(const WORD a[3][GAMMA_RAMP_SIZE], const WORD b[3][GAMMA_RAMP_SIZE], int tolerance) {
	for (int channel = 0; channel < 3; channel++) {
		for (int i = 0; i < GAMMA_RAMP_SIZE; i++) {
			int difference = (int)a[channel][i] - (int)b[channel][i];
			if (difference < 0)
				difference = -difference;
			if (difference > tolerance)
				return false;
		}
	}
	return true;
}

static HWND getGammaWindow(UViewport *viewport) {
	if (!viewport)
		return nullptr;
	return (HWND)viewport->GetWindow();
}
//@}

void UD3D10RenderDevice::updateBrightness() {
	if (!URenderDevice::Viewport)
		return;
	UClient *client = URenderDevice::Viewport->GetOuterUClient();
	if (!client)
		return;

	if (client->Brightness == appliedBrightness)
		return;

	appliedBrightness = client->Brightness;
	D3D::setBrightness(appliedBrightness + options.gammaOffset); //Clamped there.

	neutralizeSystemGamma();
}

void UD3D10RenderDevice::saveSystemGamma() {
	static_assert(::GAMMA_RAMP_SIZE == (int)UD3D10RenderDevice::GAMMA_RAMP_SIZE,
		"The file scope GAMMA_RAMP_SIZE must match the member enum savedGammaRamp is sized from");

	gammaRampSaved = false;
	gammaRampNeutralized = false;

	HWND window = getGammaWindow(URenderDevice::Viewport);
	HDC dc = GetDC(window);
	if (!dc) {
		UD3D10RenderDevice::debugs("Could not get a device context; leaving system gamma alone.");
		return;
	}
	gammaRampSaved = (GetDeviceGammaRamp(dc, savedGammaRamp) != FALSE);
	ReleaseDC(window, dc);

	if (!gammaRampSaved)
		UD3D10RenderDevice::debugs("Could not read the display gamma ramp; leaving system gamma alone.");
}

void UD3D10RenderDevice::neutralizeSystemGamma() {
	if (!gammaRampSaved) //No baseline, no way to tell.
		return;

	HWND window = getGammaWindow(URenderDevice::Viewport);
	HDC dc = GetDC(window);
	if (!dc)
		return;

	WORD currentRamp[3][GAMMA_RAMP_SIZE];
	if (GetDeviceGammaRamp(dc, currentRamp)) {
		WORD neutralRamp[3][GAMMA_RAMP_SIZE];
		buildGammaRamp(neutralRamp, 1.0f);

		if (!gammaRampNeutralized) {
			WORD engineRamp[3][GAMMA_RAMP_SIZE];
			buildGammaRamp(engineRamp, GAMMA_BRIGHTNESS_SCALE * appliedBrightness);

			if (!gammaRampsMatch(engineRamp, neutralRamp, GAMMA_RAMP_TOLERANCE) && gammaRampsMatch(currentRamp, engineRamp, GAMMA_RAMP_TOLERANCE)) {
				UD3D10RenderDevice::debugs("Engine gamma ramp is in effect; neutralizing it as brightness is applied in-shader.");
				gammaRampNeutralized = true;
			}
		}

		if (gammaRampNeutralized && !gammaRampsMatch(currentRamp, neutralRamp, GAMMA_RAMP_TOLERANCE))
			SetDeviceGammaRamp(dc, neutralRamp);
	}

	ReleaseDC(window, dc);
}

void UD3D10RenderDevice::restoreSystemGamma() {
	if (gammaRampSaved && gammaRampNeutralized) {
		HWND window = getGammaWindow(URenderDevice::Viewport);
		HDC dc = GetDC(window);
		if (dc) {
			SetDeviceGammaRamp(dc, savedGammaRamp);
			ReleaseDC(window, dc);
		}
	}
	gammaRampSaved = false;
	gammaRampNeutralized = false;
}
