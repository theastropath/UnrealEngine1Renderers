/*=============================================================================
	Globals.h: the file scope things more than one part of the driver needs.

	Include after D3D9.h.
=============================================================================*/

#pragma once

//Must match the class's package.class path.
//The engine keys its own config reads off that name.
extern const TCHAR *g_pSection;

extern const FLOAT g_sevenBitMapParams[5 * 4];
