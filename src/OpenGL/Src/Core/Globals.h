/*=============================================================================
	Globals.h: the file scope things more than one part of the driver needs.

	Include after OpenGL.h.
=============================================================================*/

#pragma once

extern const TCHAR *g_pSection;


inline const char *GLStringOrEmpty(const GLubyte *pStr) {
	return (pStr != NULL) ? (const char *)pStr : "";
}
