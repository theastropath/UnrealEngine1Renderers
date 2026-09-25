/*=============================================================================
	ShaderLoad.cpp: compiling the ARB programs and choosing them.

	Sources are in Shaders/ShaderPrograms.cpp.
	A program the driver would emulate counts as a failure here.
=============================================================================*/

#include "../OpenGLDrv.h"
#include "../OpenGL.h"
#include "ShaderPrograms.h"

bool UOpenGLRenderDevice::LoadVertexProgram(GLuint vpId, const char *pProgram, const char *pName) {
	GLint iErrorPos;

	if (DebugBit(DEBUG_BIT_BASIC)) {
		dbgPrintf("utglr: Loading vertex program \"%s\"\n", pName);
	}

	glBindProgramARB(GL_VERTEX_PROGRAM_ARB, vpId);
	glProgramStringARB(GL_VERTEX_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB, strlen(pProgram), pProgram);

	glGetIntegerv(GL_PROGRAM_ERROR_POSITION_ARB, &iErrorPos);

	if (DebugBit(DEBUG_BIT_BASIC)) {
		if (iErrorPos != -1) {
			dbgPrintf("utglr: Vertex program error at offset %d\n", iErrorPos);
			dbgPrintf("utglr: Vertex program text from error offset:\n%s\n", pProgram + iErrorPos);
		}
	}

	if (iErrorPos != -1) {
		return false;
	}

	return true;
}

bool UOpenGLRenderDevice::LoadFragmentProgram(GLuint fpId, const char *pProgram, const char *pName) {
	GLint iErrorPos;

	if (DebugBit(DEBUG_BIT_BASIC)) {
		dbgPrintf("utglr: Loading fragment program \"%s\"\n", pName);
	}

	glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, fpId);
	glProgramStringARB(GL_FRAGMENT_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB, strlen(pProgram), pProgram);

	glGetIntegerv(GL_PROGRAM_ERROR_POSITION_ARB, &iErrorPos);

	if (DebugBit(DEBUG_BIT_BASIC)) {
		if (iErrorPos != -1) {
			dbgPrintf("utglr: Fragment program error at offset %d\n", iErrorPos);
			dbgPrintf("utglr: Fragment program text from error offset:\n%s\n", pProgram + iErrorPos);
		}
	}

	if (iErrorPos != -1) {
		return false;
	}

	return true;
}

/*
As above, and it also refuses a program the hardware would emulate because
ARB_fragment_program guarantees only 48 ALU and 24 texture instructions while a cubic
filter taken as four bilinear taps is comfortably past that, and hardware short of it
still accepts the program and emulates it at a frame rate that reads as a hang, so asking
is the only way to tell.
*/
bool UOpenGLRenderDevice::LoadFragmentProgramNative(GLuint fpId, const char *pProgram, const char *pName) {
	if (!LoadFragmentProgram(fpId, pProgram, pName)) {
		return false;
	}

	GLint underNativeLimits = 1;
	glGetProgramivARB(GL_FRAGMENT_PROGRAM_ARB, GL_PROGRAM_UNDER_NATIVE_LIMITS_ARB, &underNativeLimits);
	if (underNativeLimits == 0) {
		if (DebugBit(DEBUG_BIT_BASIC)) {
			dbgPrintf("utglr: Fragment program \"%s\" exceeds the hardware's native limits\n", pName);
		}
		return false;
	}

	return true;
}


void UOpenGLRenderDevice::AllocateFragmentProgramNamesSafe(void) {
	if (m_allocatedShaderNames) {
		return;
	}

	glGenProgramsARB(1, &m_vpDefaultRenderingState);
	glGenProgramsARB(1, &m_vpDefaultRenderingStateWithFog);
	glGenProgramsARB(1, &m_vpDefaultRenderingStateWithLinearFog);
	glGenProgramsARB(MAX_TMUNITS, m_vpComplexSurface);
	glGenProgramsARB(1, &m_vpComplexSurfaceSingleTextureWithPos);
	glGenProgramsARB(1, &m_vpComplexSurfaceDualTextureWithPos);
	glGenProgramsARB(1, &m_vpComplexSurfaceTripleTextureWithPos);

	glGenProgramsARB(1, &m_fpDefaultRenderingState);
	glGenProgramsARB(1, &m_fpDefaultRenderingStateWithFog);
	glGenProgramsARB(1, &m_fpDefaultRenderingStateWithLinearFog);
	glGenProgramsARB(1, &m_fpComplexSurfaceSingleTexture);
	glGenProgramsARB(1, &m_fpComplexSurfaceDualTextureModulated);
	glGenProgramsARB(1, &m_fpComplexSurfaceTripleTextureModulated);
	glGenProgramsARB(1, &m_fpComplexSurfaceSingleTextureWithFog);
	glGenProgramsARB(1, &m_fpComplexSurfaceDualTextureModulatedWithFog);
	glGenProgramsARB(1, &m_fpComplexSurfaceTripleTextureModulatedWithFog);
	glGenProgramsARB(1, &m_fpDetailTexture);
	glGenProgramsARB(1, &m_fpDetailTextureTwoLayer);
	glGenProgramsARB(1, &m_fpSingleTextureAndDetailTexture);
	glGenProgramsARB(1, &m_fpSingleTextureAndDetailTextureTwoLayer);
	glGenProgramsARB(1, &m_fpDualTextureAndDetailTexture);
	glGenProgramsARB(1, &m_fpDualTextureAndDetailTextureTwoLayer);

	glGenProgramsARB(1, &m_fpComplexSurfaceDualTextureLightRecon);
	glGenProgramsARB(1, &m_fpComplexSurfaceTripleTextureLightRecon);
	glGenProgramsARB(1, &m_fpComplexSurfaceSingleTextureFogRecon);
	glGenProgramsARB(1, &m_fpComplexSurfaceDualTextureLightFogRecon);
	glGenProgramsARB(1, &m_fpComplexSurfaceDualTextureMacroFogRecon);
	glGenProgramsARB(1, &m_fpComplexSurfaceTripleTextureLightFogRecon);
	glGenProgramsARB(1, &m_fpDualTextureAndDetailTextureLightRecon);
	glGenProgramsARB(1, &m_fpDualTextureAndDetailTextureTwoLayerLightRecon);

	m_allocatedShaderNames = true;

	return;
}

void UOpenGLRenderDevice::FreeFragmentProgramNamesSafe(void) {
	if (!m_allocatedShaderNames) {
		return;
	}

	glDeleteProgramsARB(1, &m_vpDefaultRenderingState);
	glDeleteProgramsARB(1, &m_vpDefaultRenderingStateWithFog);
	glDeleteProgramsARB(1, &m_vpDefaultRenderingStateWithLinearFog);
	glDeleteProgramsARB(MAX_TMUNITS, m_vpComplexSurface);
	glDeleteProgramsARB(1, &m_vpComplexSurfaceSingleTextureWithPos);
	glDeleteProgramsARB(1, &m_vpComplexSurfaceDualTextureWithPos);
	glDeleteProgramsARB(1, &m_vpComplexSurfaceTripleTextureWithPos);

	glDeleteProgramsARB(1, &m_fpDefaultRenderingState);
	glDeleteProgramsARB(1, &m_fpDefaultRenderingStateWithFog);
	glDeleteProgramsARB(1, &m_fpDefaultRenderingStateWithLinearFog);
	glDeleteProgramsARB(1, &m_fpComplexSurfaceSingleTexture);
	glDeleteProgramsARB(1, &m_fpComplexSurfaceDualTextureModulated);
	glDeleteProgramsARB(1, &m_fpComplexSurfaceTripleTextureModulated);
	glDeleteProgramsARB(1, &m_fpComplexSurfaceSingleTextureWithFog);
	glDeleteProgramsARB(1, &m_fpComplexSurfaceDualTextureModulatedWithFog);
	glDeleteProgramsARB(1, &m_fpComplexSurfaceTripleTextureModulatedWithFog);
	glDeleteProgramsARB(1, &m_fpDetailTexture);
	glDeleteProgramsARB(1, &m_fpDetailTextureTwoLayer);
	glDeleteProgramsARB(1, &m_fpSingleTextureAndDetailTexture);
	glDeleteProgramsARB(1, &m_fpSingleTextureAndDetailTextureTwoLayer);
	glDeleteProgramsARB(1, &m_fpDualTextureAndDetailTexture);
	glDeleteProgramsARB(1, &m_fpDualTextureAndDetailTextureTwoLayer);

	glDeleteProgramsARB(1, &m_fpComplexSurfaceDualTextureLightRecon);
	glDeleteProgramsARB(1, &m_fpComplexSurfaceTripleTextureLightRecon);
	glDeleteProgramsARB(1, &m_fpComplexSurfaceSingleTextureFogRecon);
	glDeleteProgramsARB(1, &m_fpComplexSurfaceDualTextureLightFogRecon);
	glDeleteProgramsARB(1, &m_fpComplexSurfaceDualTextureMacroFogRecon);
	glDeleteProgramsARB(1, &m_fpComplexSurfaceTripleTextureLightFogRecon);
	glDeleteProgramsARB(1, &m_fpDualTextureAndDetailTextureLightRecon);
	glDeleteProgramsARB(1, &m_fpDualTextureAndDetailTextureTwoLayerLightRecon);

	//Cleared with the names they describe.
	m_sevenBitMapReconLoaded = false;
	m_sevenBitMapRecon = false;
	m_curSevenBitMapSizeValidBits = 0;

	m_allocatedShaderNames = false;

	return;
}

bool UOpenGLRenderDevice::InitializeFragmentPrograms(void) {
	bool initOk = true;


	initOk &= LoadVertexProgram(m_vpDefaultRenderingState, g_vpDefaultRenderingState,
		"Default rendering state");

	initOk &= LoadVertexProgram(m_vpDefaultRenderingStateWithFog, g_vpDefaultRenderingStateWithFog,
		"Default rendering state with fog");

	initOk &= LoadVertexProgram(m_vpDefaultRenderingStateWithLinearFog, g_vpDefaultRenderingStateWithLinearFog,
		"Default rendering state with linear fog");


	initOk &= LoadVertexProgram(m_vpComplexSurface[0], g_vpComplexSurfaceSingleTexture,
		"Complex surface single texture");

	initOk &= LoadVertexProgram(m_vpComplexSurface[1], g_vpComplexSurfaceDualTexture,
		"Complex surface dual texture");

	initOk &= LoadVertexProgram(m_vpComplexSurface[2], g_vpComplexSurfaceTripleTexture,
		"Complex surface triple texture");

	initOk &= LoadVertexProgram(m_vpComplexSurface[3], g_vpComplexSurfaceQuadTexture,
		"Complex surface quad texture");


	initOk &= LoadVertexProgram(m_vpComplexSurfaceSingleTextureWithPos, g_vpComplexSurfaceSingleTextureWithPos,
		"Complex surface single texture with position");

	initOk &= LoadVertexProgram(m_vpComplexSurfaceDualTextureWithPos, g_vpComplexSurfaceDualTextureWithPos,
		"Complex surface dual texture with position");

	initOk &= LoadVertexProgram(m_vpComplexSurfaceTripleTextureWithPos, g_vpComplexSurfaceTripleTextureWithPos,
		"Complex surface triple texture with position");


	glBindProgramARB(GL_VERTEX_PROGRAM_ARB, 0);
	glDisable(GL_VERTEX_PROGRAM_ARB);
	m_vpCurrent = 0;


	initOk &= LoadFragmentProgram(m_fpDefaultRenderingState, g_fpDefaultRenderingState,
		"Default rendering state");

	initOk &= LoadFragmentProgram(m_fpDefaultRenderingStateWithFog, g_fpDefaultRenderingStateWithFog,
		"Default rendering state with fog");

	initOk &= LoadFragmentProgram(m_fpDefaultRenderingStateWithLinearFog, g_fpDefaultRenderingStateWithLinearFog,
		"Default rendering state with linear fog");


	initOk &= LoadFragmentProgram(m_fpComplexSurfaceSingleTexture, g_fpComplexSurfaceSingleTexture,
		"Complex surface single texture");

	initOk &= LoadFragmentProgram(m_fpComplexSurfaceDualTextureModulated, g_fpComplexSurfaceDualTextureModulated,
		"Complex surface dual texture modulated");

	initOk &= LoadFragmentProgram(m_fpComplexSurfaceTripleTextureModulated, g_fpComplexSurfaceTripleTextureModulated,
		"Complex surface triple texture modulated");


	initOk &= LoadFragmentProgram(m_fpComplexSurfaceSingleTextureWithFog, g_fpComplexSurfaceSingleTextureWithFog,
		"Complex surface single texture with fog");

	initOk &= LoadFragmentProgram(m_fpComplexSurfaceDualTextureModulatedWithFog, g_fpComplexSurfaceDualTextureModulatedWithFog,
		"Complex surface dual texture modulated with fog");

	initOk &= LoadFragmentProgram(m_fpComplexSurfaceTripleTextureModulatedWithFog, g_fpComplexSurfaceTripleTextureModulatedWithFog,
		"Complex surface triple texture modulated with fog");


	initOk &= LoadFragmentProgram(m_fpDetailTexture, g_fpDetailTexture,
		"Detail texture");

	initOk &= LoadFragmentProgram(m_fpDetailTextureTwoLayer, g_fpDetailTextureTwoLayer,
		"Detail texture two layer");

	initOk &= LoadFragmentProgram(m_fpSingleTextureAndDetailTexture, g_fpSingleTextureAndDetailTexture,
		"Complex surface single texture and detail texture");

	initOk &= LoadFragmentProgram(m_fpSingleTextureAndDetailTextureTwoLayer, g_fpSingleTextureAndDetailTextureTwoLayer,
		"Complex surface single texture and detail texture two layer");

	initOk &= LoadFragmentProgram(m_fpDualTextureAndDetailTexture, g_fpDualTextureAndDetailTexture,
		"Complex surface dual texture and detail texture");

	initOk &= LoadFragmentProgram(m_fpDualTextureAndDetailTextureTwoLayer, g_fpDualTextureAndDetailTextureTwoLayer,
		"Complex surface dual texture and detail texture two layer");


	//Loaded whether or not ReduceBanding asks for them now.
	//Their failure stays out of initOk.
	//These only improve a program that already works.
	{
		bool reconOk = true;

		reconOk &= LoadFragmentProgramNative(m_fpComplexSurfaceDualTextureLightRecon, g_fpComplexSurfaceDualTextureLightRecon,
			"Complex surface dual texture, reconstructed lightmap");

		reconOk &= LoadFragmentProgramNative(m_fpComplexSurfaceTripleTextureLightRecon, g_fpComplexSurfaceTripleTextureLightRecon,
			"Complex surface triple texture, reconstructed lightmap");

		reconOk &= LoadFragmentProgramNative(m_fpComplexSurfaceSingleTextureFogRecon, g_fpComplexSurfaceSingleTextureFogRecon,
			"Complex surface single texture, reconstructed fog map");

		reconOk &= LoadFragmentProgramNative(m_fpComplexSurfaceDualTextureLightFogRecon, g_fpComplexSurfaceDualTextureLightFogRecon,
			"Complex surface dual texture, reconstructed lightmap and fog map");

		reconOk &= LoadFragmentProgramNative(m_fpComplexSurfaceDualTextureMacroFogRecon, g_fpComplexSurfaceDualTextureMacroFogRecon,
			"Complex surface dual texture, reconstructed fog map");

		reconOk &= LoadFragmentProgramNative(m_fpComplexSurfaceTripleTextureLightFogRecon, g_fpComplexSurfaceTripleTextureLightFogRecon,
			"Complex surface triple texture, reconstructed lightmap and fog map");

		reconOk &= LoadFragmentProgramNative(m_fpDualTextureAndDetailTextureLightRecon, g_fpDualTextureAndDetailTextureLightRecon,
			"Complex surface dual texture and detail texture, reconstructed lightmap");

		reconOk &= LoadFragmentProgramNative(m_fpDualTextureAndDetailTextureTwoLayerLightRecon, g_fpDualTextureAndDetailTextureTwoLayerLightRecon,
			"Complex surface dual texture and detail texture two layer, reconstructed lightmap");

		m_sevenBitMapReconLoaded = reconOk;
		if (!reconOk) {
			debugf(TEXT("Seven bit map reconstruction programs unavailable; lightmaps will be sampled bilinearly"));
		}
	}

	//The environment parameters belong to the fragment program target.
	//The cache is dropped here, on a fresh context.
	m_curSevenBitMapSizeValidBits = 0;


	glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, 0);
	glDisable(GL_FRAGMENT_PROGRAM_ARB);
	m_fpCurrent = 0;
	//Cheap and idempotent.
	ResyncTextureEnables();

	return initOk;
}

//Safe to call multiple times.
//A repeat call reloads the programs.
void UOpenGLRenderDevice::TryInitializeFragmentProgramMode(void) {
	AllocateFragmentProgramNamesSafe();

	if (InitializeFragmentPrograms() == false) {
		FreeFragmentProgramNamesSafe();

		DCV.UseFragmentProgram = 0;
		UseFragmentProgram = 0;
		PL_UseFragmentProgram = 0;

		//Once per device.
		debugf(TEXT("Fragment program initialization failed; falling back to fixed function multipass"));
		if (DebugBit(DEBUG_BIT_BASIC)) dbgPrintf("utglr: Fragment program initialization failed\n");
	}

	return;
}

//Safe when uninitialized.
void UOpenGLRenderDevice::ShutdownFragmentProgramMode(void) {
	FreeFragmentProgramNamesSafe();

	if (m_vpCurrent != 0) {
		glBindProgramARB(GL_VERTEX_PROGRAM_ARB, 0);
		glDisable(GL_VERTEX_PROGRAM_ARB);
		m_vpCurrent = 0;
	}

	if (m_fpCurrent != 0) {
		glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, 0);
		glDisable(GL_FRAGMENT_PROGRAM_ARB);
		m_fpCurrent = 0;
		//Left by hand, so the texture enables are reinstated.
		ResyncTextureEnables();
	}

	return;
}
