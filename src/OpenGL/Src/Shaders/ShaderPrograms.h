/*=============================================================================
	ShaderPrograms.h: the shader program sources, for the code that loads them.

	extern is spelled out because a const object at namespace scope already has
	internal linkage; dropping static alone would leave these unreachable.
=============================================================================*/

#pragma once

extern const char *g_fpComplexSurfaceDualTextureLightFogRecon;
extern const char *g_fpComplexSurfaceDualTextureLightRecon;
extern const char *g_fpComplexSurfaceDualTextureMacroFogRecon;
extern const char *g_fpComplexSurfaceDualTextureModulated;
extern const char *g_fpComplexSurfaceDualTextureModulatedWithFog;
extern const char *g_fpComplexSurfaceSingleTexture;
extern const char *g_fpComplexSurfaceSingleTextureFogRecon;
extern const char *g_fpComplexSurfaceSingleTextureWithFog;
extern const char *g_fpComplexSurfaceTripleTextureLightFogRecon;
extern const char *g_fpComplexSurfaceTripleTextureLightRecon;
extern const char *g_fpComplexSurfaceTripleTextureModulated;
extern const char *g_fpComplexSurfaceTripleTextureModulatedWithFog;
extern const char *g_fpDefaultRenderingState;
extern const char *g_fpDefaultRenderingStateWithFog;
extern const char *g_fpDefaultRenderingStateWithLinearFog;
extern const char *g_fpDetailTexture;
extern const char *g_fpDetailTextureTwoLayer;
extern const char *g_fpDualTextureAndDetailTexture;
extern const char *g_fpDualTextureAndDetailTextureLightRecon;
extern const char *g_fpDualTextureAndDetailTextureTwoLayer;
extern const char *g_fpDualTextureAndDetailTextureTwoLayerLightRecon;
extern const char *g_fpGammaRamp;
extern const char *g_fpGammaRampDithered;
extern const char *g_fpSingleTextureAndDetailTexture;
extern const char *g_fpSingleTextureAndDetailTextureTwoLayer;
extern const char *g_vpComplexSurfaceDualTexture;
extern const char *g_vpComplexSurfaceDualTextureWithPos;
extern const char *g_vpComplexSurfaceQuadTexture;
extern const char *g_vpComplexSurfaceSingleTexture;
extern const char *g_vpComplexSurfaceSingleTextureWithPos;
extern const char *g_vpComplexSurfaceTripleTexture;
extern const char *g_vpComplexSurfaceTripleTextureWithPos;
extern const char *g_vpDefaultRenderingState;
extern const char *g_vpDefaultRenderingStateWithFog;
extern const char *g_vpDefaultRenderingStateWithLinearFog;
