/** \file Device/Exec.cpp */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "../D3D10.h"


/**
Console commands from the game.
\param Cmd The command.
- GetRes Should return a list of resolutions in string form "HxW HxW" etc.
\param Ar Log responses here.
\note Deus Ex ignores resolutions it doesn't like. Brightness is polled instead.
*/
UBOOL UD3D10RenderDevice::Exec(const TCHAR *Cmd, FOutputDevice &Ar) {
	makeContextCurrent();
//Nothing to chain to.
#if (!OLD_URENDERDEVICE)
	if (URenderDevice::Exec(Cmd, Ar)) {
		return 1;
	} else
#endif
		if (ParseCommand(&Cmd, TEXT("GetRes"))) {
		UD3D10RenderDevice::debugs("Getting modelist...");
		TCHAR *resolutions = D3D::getModes();
		//Null on failure.
		if (resolutions) {
			Ar.Log(resolutions);
			delete[] resolutions;
		}
		UD3D10RenderDevice::debugs("Done.");
		return 1;
	}
	return 0;
}
