/**
\file Shaders/PolyFlags.h
\note Flags are passed explicitly, because FTextureInfo's own cannot be trusted.
Blending, first match wins:
- PF_Modulated: DEST_COLOR, SRC_COLOR.
- PF_Translucent: ONE, INV_SRC_COLOR.
- PF_Highlighted: ONE, INV_SRC_ALPHA. Editor selection.
- PF_AlphaBlend: SRC_ALPHA, INV_SRC_ALPHA. Rune only.
- PF_Invisible: ZERO, ONE.
- Otherwise none.
Depth writes stay on for PF_Occlude and for unblended surfaces, which is what still occludes a corona drawn at Z=0.
PF_RenderFog alone draws the model's fog color. PF_Masked alpha tests at 0.5.
PF_NoSmooth drops filtering for UI.
*/

#pragma once

#include "polyflags.fxh"



/**
Filter engine polyflags down to the ones this renderer may act
on.

0x1000 is PF_AlphaBlend in Rune and PF_BigWavy elsewhere, so honouring it
everywhere would alpha blend every wavy surface.
*/
static inline DWORD enginePolyFlags(DWORD polyFlags) {
//#if not #ifndef; /D NAME defines it as 1.
#if !RUNE
	//Non-Rune headers don't declare it.
	const DWORD PF_AlphaBlend_Bit = 0x00001000;
	polyFlags &= ~PF_AlphaBlend_Bit;
#endif
	return polyFlags;
}

/** Depth writes off, so it is held back. */
static inline bool isBlendedGeometry(DWORD polyFlags) {
	return (polyFlags & (PF_Translucent | PF_Modulated)) != 0 && !(polyFlags & PF_Occlude);
}
