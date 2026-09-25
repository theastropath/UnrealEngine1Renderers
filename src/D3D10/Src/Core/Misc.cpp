/** \namespace misc API-independent helpers. */

#include <cmath>
#include "Misc.h"

static const float PI = 3.1415926535897932f;

/** Code from http://emsai.net/projects/widescreen/fovcalc/ */
int Misc::getFov(int defaultFOV, int resX, int resY) {
	if (resX <= 0 || resY <= 0)
		return defaultFOV;

	float aspect = (float)resX / (float)resY;
	float fov = (float)(atan(tan(defaultFOV * PI / 360.0) * (aspect / (4.0 / 3.0))) * 360.0) / PI;
	return (int)(fov + 0.5f);
}
