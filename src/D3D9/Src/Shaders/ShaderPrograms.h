/*=============================================================================
	ShaderPrograms.h: the shader programs, for the code that loads them.

	Defined in ShaderPrograms.cpp. extern is required on both: a const object at
	namespace scope has internal linkage whether or not static is written.
=============================================================================*/

#pragma once

extern const DWORD g_fpComplexSurfaceDualTextureLightDetailRecon[];
extern const DWORD g_fpComplexSurfaceDualTextureLightDetailTwoLayerRecon[];
extern const DWORD g_fpComplexSurfaceDualTextureLightFogRecon[];
extern const DWORD g_fpComplexSurfaceDualTextureLightRecon[];
extern const DWORD g_fpComplexSurfaceDualTextureMacroFogRecon[];
extern const DWORD g_fpComplexSurfaceDualTextureModulated[];
extern const DWORD g_fpComplexSurfaceDualTextureModulatedWithFog[];
extern const DWORD g_fpComplexSurfaceSingleTexture[];
extern const DWORD g_fpComplexSurfaceSingleTextureFogRecon[];
extern const DWORD g_fpComplexSurfaceSingleTextureWithFog[];
extern const DWORD g_fpComplexSurfaceTripleTextureLightFogRecon[];
extern const DWORD g_fpComplexSurfaceTripleTextureLightRecon[];
extern const DWORD g_fpComplexSurfaceTripleTextureModulated[];
extern const DWORD g_fpComplexSurfaceTripleTextureModulatedWithFog[];
extern const DWORD g_fpDefaultRenderingState[];
extern const DWORD g_fpDefaultRenderingStateWithFog[];
extern const DWORD g_fpDefaultRenderingStateWithLinearFog[];
extern const DWORD g_fpDetailTexture[];
extern const DWORD g_fpDetailTextureTwoLayer[];
extern const DWORD g_fpDualTextureAndDetailTexture[];
extern const DWORD g_fpDualTextureAndDetailTextureTwoLayer[];
extern const DWORD g_fpGammaCorrection[];
extern const DWORD g_fpGammaCorrectionDithered[];
extern const DWORD g_fpSingleTextureAndDetailTexture[];
extern const DWORD g_fpSingleTextureAndDetailTextureTwoLayer[];
extern const D3DVERTEXELEMENT9 g_oneColorStreamDef[];
extern const D3DVERTEXELEMENT9 *g_standardNTextureStreamDefs[MAX_TMUNITS];
extern const D3DVERTEXELEMENT9 g_twoColorSingleTextureStreamDef[];
extern const DWORD g_vpComplexSurfaceDualTexture[];
extern const DWORD g_vpComplexSurfaceDualTextureAndDetailTexture[];
extern const DWORD g_vpComplexSurfaceDualTextureCT[];
extern const DWORD g_vpComplexSurfaceQuadTexture[];
extern const DWORD g_vpComplexSurfaceQuadTextureCT[];
extern const DWORD g_vpComplexSurfaceSingleTexture[];
extern const DWORD g_vpComplexSurfaceSingleTextureAndDetailTexture[];
extern const DWORD g_vpComplexSurfaceSingleTextureCT[];
extern const DWORD g_vpComplexSurfaceTripleTexture[];
extern const DWORD g_vpComplexSurfaceTripleTextureCT[];
extern const DWORD g_vpDefaultRenderingState[];
extern const DWORD g_vpDefaultRenderingStateWithFog[];
extern const DWORD g_vpDefaultRenderingStateWithLinearFog[];
extern const DWORD g_vpDetailTexture[];
extern const DWORD g_vpGammaCorrection[];
