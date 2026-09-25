/*=============================================================================
	ShaderLoad.cpp: creating the shader objects, and deciding whether to use them.

	If any shader in the set fails to create, fragment program mode is abandoned whole:
	the fixed function path is complete where a mixture would not be.
=============================================================================*/

#include "../D3D9Drv.h"
#include "../D3D9.h"
#include "ShaderPrograms.h"

bool UD3D9RenderDevice::LoadVertexProgram(IDirect3DVertexShader9 **ppShader, const DWORD *pFunction, const TCHAR *pName) {
	HRESULT hResult;

	//A false return leaves the out pointer NULL.
	*ppShader = NULL;

	if (DebugBit(DEBUG_BIT_BASIC)) {
		dout << TEXT("utd3d9r: Loading vertex program \"") << pName << TEXT("\"") << std::endl;
	}

	hResult = m_d3dDevice->CreateVertexShader(pFunction, ppShader);
	if (FAILED(hResult)) {
		if (DebugBit(DEBUG_BIT_BASIC)) {
			dout << TEXT("utd3d9r: Vertex program load error") << std::endl;
		}

		return false;
	}

	return true;
}

bool UD3D9RenderDevice::LoadFragmentProgram(IDirect3DPixelShader9 **ppShader, const DWORD *pFunction, const TCHAR *pName) {
	HRESULT hResult;

	//As above. A false return leaves the out pointer NULL
	*ppShader = NULL;

	if (DebugBit(DEBUG_BIT_BASIC)) {
		dout << TEXT("utd3d9r: Loading fragment program \"") << pName << TEXT("\"") << std::endl;
	}

	hResult = m_d3dDevice->CreatePixelShader(pFunction, ppShader);
	if (FAILED(hResult)) {
		if (DebugBit(DEBUG_BIT_BASIC)) {
			dout << TEXT("utd3d9r: Fragment program load error") << std::endl;
		}

		return false;
	}

	return true;
}


bool UD3D9RenderDevice::InitializeFragmentPrograms(void) {
	bool initOk = true;


//Create vertex programs if not already created
#define UTGLR_VS_CONDITIONAL_LOAD(_var, _vp, _name) \
	if (!_var) { \
		initOk &= LoadVertexProgram(&_var, _vp, _name); \
	}


	UTGLR_VS_CONDITIONAL_LOAD(m_vpDefaultRenderingState, g_vpDefaultRenderingState,
		TEXT("Default rendering state"));

	UTGLR_VS_CONDITIONAL_LOAD(m_vpDefaultRenderingStateWithFog, g_vpDefaultRenderingStateWithFog,
		TEXT("Default rendering state with fog"));

	UTGLR_VS_CONDITIONAL_LOAD(m_vpDefaultRenderingStateWithLinearFog, g_vpDefaultRenderingStateWithLinearFog,
		TEXT("Default rendering state with linear fog"));


	UTGLR_VS_CONDITIONAL_LOAD(m_vpComplexSurface[0], g_vpComplexSurfaceSingleTexture,
		TEXT("Complex surface single texture"));

	if (TMUnits >= 2) {
		UTGLR_VS_CONDITIONAL_LOAD(m_vpComplexSurface[1], g_vpComplexSurfaceDualTexture,
			TEXT("Complex surface dual texture"));
	}

	if (TMUnits >= 3) {
		UTGLR_VS_CONDITIONAL_LOAD(m_vpComplexSurface[2], g_vpComplexSurfaceTripleTexture,
			TEXT("Complex surface triple texture"));
	}

	if (TMUnits >= 4) {
		UTGLR_VS_CONDITIONAL_LOAD(m_vpComplexSurface[3], g_vpComplexSurfaceQuadTexture,
			TEXT("Complex surface quad texture"));
	}


	//The pass-through set the batched path checks for.
	//A missing entry falls back to per-surface.
	UTGLR_VS_CONDITIONAL_LOAD(m_vpComplexSurfaceCT[0], g_vpComplexSurfaceSingleTextureCT,
		TEXT("Complex surface single texture, CPU coordinates"));

	if (TMUnits >= 2) {
		UTGLR_VS_CONDITIONAL_LOAD(m_vpComplexSurfaceCT[1], g_vpComplexSurfaceDualTextureCT,
			TEXT("Complex surface dual texture, CPU coordinates"));
	}

	if (TMUnits >= 3) {
		UTGLR_VS_CONDITIONAL_LOAD(m_vpComplexSurfaceCT[2], g_vpComplexSurfaceTripleTextureCT,
			TEXT("Complex surface triple texture, CPU coordinates"));
	}

	if (TMUnits >= 4) {
		UTGLR_VS_CONDITIONAL_LOAD(m_vpComplexSurfaceCT[3], g_vpComplexSurfaceQuadTextureCT,
			TEXT("Complex surface quad texture, CPU coordinates"));
	}


	if (DetailTextures) {
		UTGLR_VS_CONDITIONAL_LOAD(m_vpDetailTexture, g_vpDetailTexture,
			TEXT("Detail texture"));

		UTGLR_VS_CONDITIONAL_LOAD(m_vpComplexSurfaceSingleTextureAndDetailTexture, g_vpComplexSurfaceSingleTextureAndDetailTexture,
			TEXT("Complex surface single texture and detail texture"));

		UTGLR_VS_CONDITIONAL_LOAD(m_vpComplexSurfaceDualTextureAndDetailTexture, g_vpComplexSurfaceDualTextureAndDetailTexture,
			TEXT("Complex surface dual texture and detail texture"));
	}


#undef UTGLR_VS_CONDITIONAL_LOAD


//Create fragment programs if not already created
#define UTGLR_PS_CONDITIONAL_LOAD(_var, _fp, _name) \
	if (!_var) { \
		initOk &= LoadFragmentProgram(&_var, _fp, _name); \
	}


	UTGLR_PS_CONDITIONAL_LOAD(m_fpDefaultRenderingState, g_fpDefaultRenderingState,
		TEXT("Default rendering state"));

	UTGLR_PS_CONDITIONAL_LOAD(m_fpDefaultRenderingStateWithFog, g_fpDefaultRenderingStateWithFog,
		TEXT("Default rendering state with fog"));

	UTGLR_PS_CONDITIONAL_LOAD(m_fpDefaultRenderingStateWithLinearFog, g_fpDefaultRenderingStateWithLinearFog,
		TEXT("Default rendering state with linear fog"));


	UTGLR_PS_CONDITIONAL_LOAD(m_fpComplexSurfaceSingleTexture, g_fpComplexSurfaceSingleTexture,
		TEXT("Complex surface single texture"));

	UTGLR_PS_CONDITIONAL_LOAD(m_fpComplexSurfaceDualTextureModulated, g_fpComplexSurfaceDualTextureModulated,
		TEXT("Complex surface dual texture modulated"));

	UTGLR_PS_CONDITIONAL_LOAD(m_fpComplexSurfaceTripleTextureModulated, g_fpComplexSurfaceTripleTextureModulated,
		TEXT("Complex surface triple texture modulated"));


	UTGLR_PS_CONDITIONAL_LOAD(m_fpComplexSurfaceSingleTextureWithFog, g_fpComplexSurfaceSingleTextureWithFog,
		TEXT("Complex surface single texture with fog"));

	UTGLR_PS_CONDITIONAL_LOAD(m_fpComplexSurfaceDualTextureModulatedWithFog, g_fpComplexSurfaceDualTextureModulatedWithFog,
		TEXT("Complex surface dual texture modulated with fog"));

	UTGLR_PS_CONDITIONAL_LOAD(m_fpComplexSurfaceTripleTextureModulatedWithFog, g_fpComplexSurfaceTripleTextureModulatedWithFog,
		TEXT("Complex surface triple texture modulated with fog"));


	if (DetailTextures) {
		UTGLR_PS_CONDITIONAL_LOAD(m_fpDetailTexture, g_fpDetailTexture,
			TEXT("Detail texture"));

		UTGLR_PS_CONDITIONAL_LOAD(m_fpDetailTextureTwoLayer, g_fpDetailTextureTwoLayer,
			TEXT("Detail texture two layer"));

		UTGLR_PS_CONDITIONAL_LOAD(m_fpSingleTextureAndDetailTexture, g_fpSingleTextureAndDetailTexture,
			TEXT("Complex surface single texture and detail texture"));

		UTGLR_PS_CONDITIONAL_LOAD(m_fpSingleTextureAndDetailTextureTwoLayer, g_fpSingleTextureAndDetailTextureTwoLayer,
			TEXT("Complex surface single texture and detail texture two layer"));

		UTGLR_PS_CONDITIONAL_LOAD(m_fpDualTextureAndDetailTexture, g_fpDualTextureAndDetailTexture,
			TEXT("Complex surface dual texture and detail texture"));

		UTGLR_PS_CONDITIONAL_LOAD(m_fpDualTextureAndDetailTextureTwoLayer, g_fpDualTextureAndDetailTextureTwoLayer,
			TEXT("Complex surface dual texture and detail texture two layer"));
	}


	/*
	The reconstruction combiners, created whether or not ReduceBanding asks for them now.
	Their failure stays out of initOk: these only improve a combiner that already works.
	*/
	{
		bool reconOk = true;

#define UTGLR_PS_CONDITIONAL_LOAD_RECON(_var, _fp, _name) \
	if (!_var) { \
		reconOk &= LoadFragmentProgram(&_var, _fp, _name); \
	}

		UTGLR_PS_CONDITIONAL_LOAD_RECON(m_fpComplexSurfaceDualTextureLightRecon, g_fpComplexSurfaceDualTextureLightRecon,
			TEXT("Complex surface dual texture, reconstructed lightmap"));

		UTGLR_PS_CONDITIONAL_LOAD_RECON(m_fpComplexSurfaceTripleTextureLightRecon, g_fpComplexSurfaceTripleTextureLightRecon,
			TEXT("Complex surface triple texture, reconstructed lightmap"));

		UTGLR_PS_CONDITIONAL_LOAD_RECON(m_fpComplexSurfaceSingleTextureFogRecon, g_fpComplexSurfaceSingleTextureFogRecon,
			TEXT("Complex surface single texture, reconstructed fog map"));

		UTGLR_PS_CONDITIONAL_LOAD_RECON(m_fpComplexSurfaceDualTextureLightFogRecon, g_fpComplexSurfaceDualTextureLightFogRecon,
			TEXT("Complex surface dual texture, reconstructed lightmap and fog map"));

		UTGLR_PS_CONDITIONAL_LOAD_RECON(m_fpComplexSurfaceDualTextureMacroFogRecon, g_fpComplexSurfaceDualTextureMacroFogRecon,
			TEXT("Complex surface dual texture, reconstructed fog map"));

		UTGLR_PS_CONDITIONAL_LOAD_RECON(m_fpComplexSurfaceTripleTextureLightFogRecon, g_fpComplexSurfaceTripleTextureLightFogRecon,
			TEXT("Complex surface triple texture, reconstructed lightmap and fog map"));

		if (DetailTextures) {
			UTGLR_PS_CONDITIONAL_LOAD_RECON(m_fpComplexSurfaceDualTextureLightDetailRecon, g_fpComplexSurfaceDualTextureLightDetailRecon,
				TEXT("Complex surface dual texture and detail texture, reconstructed lightmap"));

			UTGLR_PS_CONDITIONAL_LOAD_RECON(m_fpComplexSurfaceDualTextureLightDetailTwoLayerRecon, g_fpComplexSurfaceDualTextureLightDetailTwoLayerRecon,
				TEXT("Complex surface dual texture and detail texture two layer, reconstructed lightmap"));
		}

#undef UTGLR_PS_CONDITIONAL_LOAD_RECON

		if (reconOk) {
			m_sevenBitMapReconLoaded = true;
		} else {
			debugf(TEXT("Seven bit map reconstruction shaders failed to load; lightmaps will be sampled bilinearly"));
		}
	}


#undef UTGLR_PS_CONDITIONAL_LOAD

	return initOk;
}

//Safe to call repeatedly.
void UD3D9RenderDevice::TryInitializeFragmentProgramMode(void) {
	if (!InitializeFragmentPrograms()) {
		ShutdownFragmentProgramMode();

		DCV.UseFragmentProgram = 0;
		UseFragmentProgram = 0;
		PL_UseFragmentProgram = 0;

		//Logged unconditionally. Falls back to the slower fixed-function path, at most once per device.
		debugf(TEXT("Fragment program initialization failed; falling back to fixed function multipass"));
		if (DebugBit(DEBUG_BIT_BASIC)) dout << TEXT("utd3d9r: Fragment program initialization failed") << std::endl;
	}

	return;
}

//Safe either way.
void UD3D9RenderDevice::ShutdownFragmentProgramMode(void) {
	//May be called during shutdown after the device is gone.
	//The fallback branch then only clears state.
	if (m_d3dDevice) {
		SetVertexShaderNoCheck(NULL);

		SetPixelShaderNoCheck(NULL);
	} else {
		m_curVertexShader = NULL;
		m_curPixelShader = NULL;
	}


#define UTGLR_VS_RELEASE(_var) \
	if (_var) { \
		_var->Release(); \
		_var = NULL; \
	}

	//Free vertex programs if they were created.
	UTGLR_VS_RELEASE(m_vpDefaultRenderingState);

	UTGLR_VS_RELEASE(m_vpDefaultRenderingStateWithFog);

	UTGLR_VS_RELEASE(m_vpDefaultRenderingStateWithLinearFog);


	UTGLR_VS_RELEASE(m_vpComplexSurface[0]);

	UTGLR_VS_RELEASE(m_vpComplexSurface[1]);

	UTGLR_VS_RELEASE(m_vpComplexSurface[2]);

	UTGLR_VS_RELEASE(m_vpComplexSurface[3]);


	UTGLR_VS_RELEASE(m_vpComplexSurfaceCT[0]);
	UTGLR_VS_RELEASE(m_vpComplexSurfaceCT[1]);
	UTGLR_VS_RELEASE(m_vpComplexSurfaceCT[2]);
	UTGLR_VS_RELEASE(m_vpComplexSurfaceCT[3]);


	UTGLR_VS_RELEASE(m_vpDetailTexture);

	UTGLR_VS_RELEASE(m_vpComplexSurfaceSingleTextureAndDetailTexture);

	UTGLR_VS_RELEASE(m_vpComplexSurfaceDualTextureAndDetailTexture);


#undef UTGLR_VS_RELEASE


#define UTGLR_PS_RELEASE(_var) \
	if (_var) { \
		_var->Release(); \
		_var = NULL; \
	}

	//Free fragment programs if they were created.
	UTGLR_PS_RELEASE(m_fpDefaultRenderingState);

	UTGLR_PS_RELEASE(m_fpDefaultRenderingStateWithFog);

	UTGLR_PS_RELEASE(m_fpDefaultRenderingStateWithLinearFog);


	UTGLR_PS_RELEASE(m_fpComplexSurfaceSingleTexture);

	UTGLR_PS_RELEASE(m_fpComplexSurfaceDualTextureModulated);

	UTGLR_PS_RELEASE(m_fpComplexSurfaceTripleTextureModulated);


	UTGLR_PS_RELEASE(m_fpComplexSurfaceSingleTextureWithFog);

	UTGLR_PS_RELEASE(m_fpComplexSurfaceDualTextureModulatedWithFog);

	UTGLR_PS_RELEASE(m_fpComplexSurfaceTripleTextureModulatedWithFog);


	UTGLR_PS_RELEASE(m_fpDetailTexture);

	UTGLR_PS_RELEASE(m_fpDetailTextureTwoLayer);

	UTGLR_PS_RELEASE(m_fpSingleTextureAndDetailTexture);

	UTGLR_PS_RELEASE(m_fpSingleTextureAndDetailTextureTwoLayer);

	UTGLR_PS_RELEASE(m_fpDualTextureAndDetailTexture);

	UTGLR_PS_RELEASE(m_fpDualTextureAndDetailTextureTwoLayer);


	UTGLR_PS_RELEASE(m_fpComplexSurfaceDualTextureLightRecon);

	UTGLR_PS_RELEASE(m_fpComplexSurfaceTripleTextureLightRecon);

	UTGLR_PS_RELEASE(m_fpComplexSurfaceSingleTextureFogRecon);

	UTGLR_PS_RELEASE(m_fpComplexSurfaceDualTextureLightFogRecon);

	UTGLR_PS_RELEASE(m_fpComplexSurfaceDualTextureMacroFogRecon);

	UTGLR_PS_RELEASE(m_fpComplexSurfaceTripleTextureLightFogRecon);

	UTGLR_PS_RELEASE(m_fpComplexSurfaceDualTextureLightDetailRecon);

	UTGLR_PS_RELEASE(m_fpComplexSurfaceDualTextureLightDetailTwoLayerRecon);

	//Cleared with the shaders they describe.
	m_sevenBitMapReconLoaded = false;
	m_sevenBitMapRecon = false;
	m_sevenBitMapSizesValid = false;


#undef UTGLR_PS_RELEASE

	return;
}
