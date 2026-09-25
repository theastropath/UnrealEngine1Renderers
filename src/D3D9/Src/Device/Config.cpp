/*=============================================================================
	Config.cpp: the renderer's settings.

	Registration with the engine's object system, defaults for the engine's own feature
	flags, and the validation that runs once the hardware's capabilities are known.
=============================================================================*/

#include "../D3D9Drv.h"
#include "../D3D9.h"
#include "../Core/Globals.h"

/*-----------------------------------------------------------------------------
	D3D9Drv.
-----------------------------------------------------------------------------*/

IMPLEMENT_CLASS(UD3D9RenderDevice);


#ifdef UTGLR_KLINGON_BUILD
//From the defaults buffer.
void UD3D9RenderDevice::StaticConstructor(UClass *Class) {
	m_scClass = Class;

	if (Class->Defaults.Num() >= (INT)sizeof(UD3D9RenderDevice)) {
		((UD3D9RenderDevice *)&Class->Defaults(0))->StaticConstructorBody();
	} else {
		BYTE *scratch = new BYTE[sizeof(UD3D9RenderDevice)];
		memset(scratch, 0, sizeof(UD3D9RenderDevice));
		((UD3D9RenderDevice *)scratch)->StaticConstructorBody();
		delete[] scratch;
	}
}
#else
void UD3D9RenderDevice::StaticConstructor() {
	m_scClass = GetClass();
	StaticConstructorBody();
}
#endif

void UD3D9RenderDevice::StaticConstructorBody() {
	unsigned int u;

	guard(UD3D9RenderDevice::StaticConstructor);

	//Not registered on Klingon, which has nothing to negotiate compressed texture support through.
#ifndef UTGLR_KLINGON_BUILD
#if defined UTGLR_DX_BUILD || defined UTGLR_RUNE_BUILD
	const UBOOL UTGLR_DEFAULT_UseS3TC = 0;
#else
	const UBOOL UTGLR_DEFAULT_UseS3TC = 1;
#endif
#endif

#ifndef UTGLR_OLD_URENDERDEVICE
	const UBOOL UTGLR_DEFAULT_ZRangeHack = 1;
#endif

#define CPP_PROPERTY_LOCAL(_name) _name, CPP_PROPERTY(_name)
#define CPP_PROPERTY_LOCAL_DCV(_name) DCV._name, CPP_PROPERTY(DCV._name)

	SC_AddFloatConfigParam(TEXT("LODBias"), CPP_PROPERTY_LOCAL(LODBias), 0.0f);
	SC_AddBoolConfigParam(0, TEXT("ReduceBanding"), CPP_PROPERTY_LOCAL(ReduceBanding), 1);
	SC_AddFloatConfigParam(TEXT("GammaOffset"), CPP_PROPERTY_LOCAL(GammaOffset), 0.0f);
	SC_AddFloatConfigParam(TEXT("GammaOffsetRed"), CPP_PROPERTY_LOCAL(GammaOffsetRed), 0.0f);
	SC_AddFloatConfigParam(TEXT("GammaOffsetGreen"), CPP_PROPERTY_LOCAL(GammaOffsetGreen), 0.0f);
	SC_AddFloatConfigParam(TEXT("GammaOffsetBlue"), CPP_PROPERTY_LOCAL(GammaOffsetBlue), 0.0f);
	SC_AddIntConfigParam(TEXT("Brightness"), CPP_PROPERTY_LOCAL(Brightness), 0);
	SC_AddBoolConfigParam(1, TEXT("HardwareGamma"), CPP_PROPERTY_LOCAL(UseHardwareGamma), 0);
	//Off for every game, Deus Ex included: a world surface is modulated by its lightmap twice.
	SC_AddBoolConfigParam(0, TEXT("OneXBlending"), CPP_PROPERTY_LOCAL(OneXBlending), 0);
	SC_AddIntConfigParam(TEXT("MinLogTextureSize"), CPP_PROPERTY_LOCAL(MinLogTextureSize), 0);
	SC_AddIntConfigParam(TEXT("MaxLogTextureSize"), CPP_PROPERTY_LOCAL(MaxLogTextureSize), 12);
	//Only registered on the builds where they do something.
#ifdef UTGLR_OLD_URENDERDEVICE
	const DWORD scUsePrecacheBools = 0;
#else
	const DWORD scUsePrecacheBools = 1;
#endif
#ifdef UTGLR_KLINGON_BUILD
	const DWORD scUseS3TCBools = 0;
#else
	const DWORD scUseS3TCBools = 1;
#endif
	SC_AddBoolConfigParam(3 + scUsePrecacheBools + scUseS3TCBools, TEXT("MultiTexture"), CPP_PROPERTY_LOCAL(UseMultiTexture), 1);
#ifndef UTGLR_OLD_URENDERDEVICE
	SC_AddBoolConfigParam(3 + scUseS3TCBools, TEXT("Precache"), CPP_PROPERTY_LOCAL(UsePrecache), 0);
#else
	UsePrecache = 0;
#endif
	SC_AddBoolConfigParam(2 + scUseS3TCBools, TEXT("Trilinear"), CPP_PROPERTY_LOCAL(UseTrilinear), 1);
#ifndef UTGLR_KLINGON_BUILD
	SC_AddBoolConfigParam(2, TEXT("S3TC"), CPP_PROPERTY_LOCAL(UseS3TC), UTGLR_DEFAULT_UseS3TC);
#else
	UseS3TC = 0;
#endif
	SC_AddBoolConfigParam(1, TEXT("16BitTextures"), CPP_PROPERTY_LOCAL(Use16BitTextures), 0);
	SC_AddBoolConfigParam(0, TEXT("565Textures"), CPP_PROPERTY_LOCAL(Use565Textures), 0);
	SC_AddIntConfigParam(TEXT("Anisotropy"), CPP_PROPERTY_LOCAL(MaxAnisotropy), 8);
	SC_AddBoolConfigParam(0, TEXT("NoFiltering"), CPP_PROPERTY_LOCAL(NoFiltering), 0);
	SC_AddIntConfigParam(TEXT("MaxTMUnits"), CPP_PROPERTY_LOCAL(MaxTMUnits), 0);
	SC_AddIntConfigParam(TEXT("RefreshRate"), CPP_PROPERTY_LOCAL(RefreshRate), 0);
	SC_AddIntConfigParam(TEXT("DetailMax"), CPP_PROPERTY_LOCAL_DCV(DetailMax), 0);
	SC_AddBoolConfigParam(8, TEXT("DetailClipping"), CPP_PROPERTY_LOCAL(DetailClipping), 0);
	SC_AddBoolConfigParam(7, TEXT("DebugDrawDetailTextures"), CPP_PROPERTY_LOCAL(ColorizeDetailTextures), 0);
	SC_AddBoolConfigParam(6, TEXT("SinglePassFog"), CPP_PROPERTY_LOCAL(SinglePassFog), 1);
	SC_AddBoolConfigParam(5, TEXT("SinglePassDetail"), CPP_PROPERTY_LOCAL_DCV(SinglePassDetail), 0);
	SC_AddBoolConfigParam(4, TEXT("UseSSE"), CPP_PROPERTY_LOCAL(UseSSE), 1);
	SC_AddBoolConfigParam(3, TEXT("UseSSE2"), CPP_PROPERTY_LOCAL(UseSSE2), 1);
	SC_AddBoolConfigParam(2, TEXT("TexIdPool"), CPP_PROPERTY_LOCAL(UseTexIdPool), 1);
	SC_AddBoolConfigParam(1, TEXT("TexPool"), CPP_PROPERTY_LOCAL(UseTexPool), 1);
	SC_AddBoolConfigParam(0, TEXT("CacheStaticMaps"), CPP_PROPERTY_LOCAL(CacheStaticMaps), 1);
	SC_AddIntConfigParam(TEXT("TextureCacheBudgetMegs"), CPP_PROPERTY_LOCAL(TexCacheBudgetMegs), 512);
	SC_AddBoolConfigParam(2, TEXT("GenerateMipMaps"), CPP_PROPERTY_LOCAL(GenerateMipMaps), 1);
	//Only useful together. Neighbouring surfaces sharing a texture still bind their own lightmap.
	SC_AddBoolConfigParam(1, TEXT("LightmapAtlas"), CPP_PROPERTY_LOCAL(LightmapAtlas), 1);
	SC_AddBoolConfigParam(0, TEXT("SurfaceBatching"), CPP_PROPERTY_LOCAL(UseSurfaceBatching), 1);
	SC_AddIntConfigParam(TEXT("RenderThreads"), CPP_PROPERTY_LOCAL(RenderThreads), 0);
	SC_AddBoolConfigParam(0, TEXT("DeferredRecording"), CPP_PROPERTY_LOCAL(DeferredRecording), 1);
	SC_AddIntConfigParam(TEXT("DynamicTexIdRecycleLevel"), CPP_PROPERTY_LOCAL(DynamicTexIdRecycleLevel), 100);
	SC_AddBoolConfigParam(1, TEXT("TexDXT1ToDXT3"), CPP_PROPERTY_LOCAL(TexDXT1ToDXT3), 0);
	//The shader path collapses the fixed function multipass into one.
	SC_AddBoolConfigParam(0, TEXT("FragmentProgram"), CPP_PROPERTY_LOCAL_DCV(UseFragmentProgram), 1);
	//1 is vsync, 2 half rate, and a negative value - the default - leaves the driver's own choice alone.
	SC_AddIntConfigParam(TEXT("VSync"), CPP_PROPERTY_LOCAL(SwapInterval), -1);
	SC_AddIntConfigParam(TEXT("FrameRateLimit"), CPP_PROPERTY_LOCAL(FrameRateLimit), 120);
	SC_AddBoolConfigParam(3, TEXT("SmoothMaskedTextures"), CPP_PROPERTY_LOCAL(SmoothMaskedTextures), 0);
	//Off by default, following from vsync being off: the extra back buffer absorbs a frame that overruns
	//the vblank. Turn both on together.
	SC_AddBoolConfigParam(2, TEXT("TripleBuffering"), CPP_PROPERTY_LOCAL(UseTripleBuffering), 0);
	SC_AddBoolConfigParam(1, TEXT("PureDevice"), CPP_PROPERTY_LOCAL(UsePureDevice), 1);
	SC_AddBoolConfigParam(0, TEXT("SoftwareVertexProcessing"), CPP_PROPERTY_LOCAL(UseSoftwareVertexProcessing), 0);
	/*
	Doubles as the on/off switch: under two samples reads as off.
	Any value is safe to ask for - the mode set path clamps to what the back and depth buffers accept.

	4 by default.
	*/
	SC_AddIntConfigParam(TEXT("Antialiasing"), CPP_PROPERTY_LOCAL(NumAASamples), 4);
	SC_AddBoolConfigParam(1, TEXT("NoAATiles"), CPP_PROPERTY_LOCAL(NoAATiles), 1);
	//The two share one slot and are never both registered.
#ifdef UTGLR_OLD_URENDERDEVICE
	SC_AddBoolConfigParam(0, TEXT("DetailTextures"), CPP_PROPERTY_LOCAL(DetailTextures), 1);
#else
	SC_AddBoolConfigParam(0, TEXT("ZRangeHack"), CPP_PROPERTY_LOCAL(ZRangeHack), UTGLR_DEFAULT_ZRangeHack);
#endif

#undef CPP_PROPERTY_LOCAL
#undef CPP_PROPERTY_LOCAL_DCV

	InitEngineFeatureFlags();

	SpanBased = 0;
	SupportsFogMaps = 1;
#ifdef UTGLR_RUNE_BUILD
	SupportsDistanceFog = 1;
#else
	SupportsDistanceFog = 0;
#endif
	FullscreenOnly = 0;

#ifndef UTGLR_KLINGON_BUILD
	SupportsLazyTextures = 0;
#endif
#if !defined UTGLR_UNREAL_224_BUILD && !defined UTGLR_KLINGON_BUILD
	PrefersDeferredLoad = 0;
#endif

	m_d3d9 = NULL;
	m_d3dDevice = NULL;

	m_pNoTexObj = NULL;
	m_pAlphaTexObj = NULL;

	m_d3dVertexColorBuffer = NULL;
	m_d3dSecondaryColorBuffer = NULL;
	for (u = 0; u < MAX_TMUNITS; u++) {
		m_d3dTexCoordBuffer[u] = NULL;
	}
	m_d3dIndexBuffer = NULL;

	m_oneColorVertexDecl = NULL;
	for (u = 0; u < MAX_TMUNITS; u++) {
		m_standardNTextureVertexDecl[u] = NULL;
	}
	m_twoColorSingleTextureVertexDecl = NULL;

	m_pGammaTexObj = NULL;
	m_pGammaTexSurface = NULL;
	m_pBackBufferSurface = NULL;
	m_gammaPassAvailable = false;
	m_gammaPassFailCount = 0;
	m_gammaPassGaveUp = false;

	m_vpDefaultRenderingState = NULL;
	m_vpDefaultRenderingStateWithFog = NULL;
	m_vpDefaultRenderingStateWithLinearFog = NULL;
	for (u = 0; u < MAX_TMUNITS; u++) {
		m_vpComplexSurface[u] = NULL;
		m_vpComplexSurfaceCT[u] = NULL;
	}
	m_vpDetailTexture = NULL;
	m_vpComplexSurfaceSingleTextureAndDetailTexture = NULL;
	m_vpComplexSurfaceDualTextureAndDetailTexture = NULL;
	m_vpGammaCorrection = NULL;

	m_fpDefaultRenderingState = NULL;
	m_fpDefaultRenderingStateWithFog = NULL;
	m_fpDefaultRenderingStateWithLinearFog = NULL;
	m_fpComplexSurfaceSingleTexture = NULL;
	m_fpComplexSurfaceDualTextureModulated = NULL;
	m_fpComplexSurfaceTripleTextureModulated = NULL;
	m_fpComplexSurfaceSingleTextureWithFog = NULL;
	m_fpComplexSurfaceDualTextureModulatedWithFog = NULL;
	m_fpComplexSurfaceTripleTextureModulatedWithFog = NULL;
	m_fpDetailTexture = NULL;
	m_fpDetailTextureTwoLayer = NULL;
	m_fpSingleTextureAndDetailTexture = NULL;
	m_fpSingleTextureAndDetailTextureTwoLayer = NULL;
	m_fpDualTextureAndDetailTexture = NULL;
	m_fpDualTextureAndDetailTextureTwoLayer = NULL;
	m_fpGammaCorrection = NULL;

	m_fpComplexSurfaceDualTextureLightRecon = NULL;
	m_fpComplexSurfaceTripleTextureLightRecon = NULL;
	m_fpComplexSurfaceSingleTextureFogRecon = NULL;
	m_fpComplexSurfaceDualTextureLightFogRecon = NULL;
	m_fpComplexSurfaceDualTextureMacroFogRecon = NULL;
	m_fpComplexSurfaceTripleTextureLightFogRecon = NULL;
	m_fpComplexSurfaceDualTextureLightDetailRecon = NULL;
	m_fpComplexSurfaceDualTextureLightDetailTwoLayerRecon = NULL;

	m_sevenBitMapReconLoaded = false;
	m_sevenBitMapRecon = false;
	m_sevenBitMapSizesValid = false;

	TMUnits = 0;

	m_useSSSE3 = false;

	m_csBatchOpen = false;
	m_csBatchLocked = false;
	m_csBatchLockedLayers = 0;
	m_csBatchLockVertexPos = 0;
	m_csBatchLockIndexPos = 0;
	m_csBatchVertexBase = 0;
	m_csBatchIndexBase = 0;
	m_csBatchVertexCount = 0;
	m_csBatchIndexCount = 0;
	m_csBatchPassCount = 0;
	m_csBatchSevenBitMask = 0;
	m_csBatchDepthBias = false;

	m_SetRes_isDeviceReset = false;

	m_deferredDeviceReset = false;

	m_frameRateLimitTimerInitialized = false;

	unguard;
}


/*
Each bool parameter occupies one bit of a shared field, numbered by how many bools registered
after it fall in the same uninterrupted run, and the wrong count maps a setting onto a bit the
engine does not expect, at which point it stops responding to the ini and says nothing about it,
which is why the runs are counted here and not written out by hand.
*/
void UD3D9RenderDevice::SC_AddBoolConfigParam(DWORD BitMaskOffset, const TCHAR *pName, UBOOL &param, ECppProperty EC_CppProperty, INT InOffset, UBOOL defaultValue) {
	param = (((defaultValue) != 0) ? 1 : 0) << BitMaskOffset;
	new (m_scClass, pName, RF_Public) UBoolProperty(EC_CppProperty, InOffset, TEXT("Options"), CPF_Config);
	SC_RecordBoolConfigParam(pName, BitMaskOffset);
}

void UD3D9RenderDevice::SC_AddBoolConfigParam(DWORD BitMaskOffset, const TCHAR *pName, BITFIELD &param, ECppProperty EC_CppProperty, INT InOffset, UBOOL defaultValue) {
	param = (((defaultValue) != 0) ? 1 : 0) << BitMaskOffset;
	new (m_scClass, pName, RF_Public) UBoolProperty(EC_CppProperty, InOffset, TEXT("Options"), CPF_Config);
	SC_RecordBoolConfigParam(pName, BitMaskOffset);
}

void UD3D9RenderDevice::SC_AddIntConfigParam(const TCHAR *pName, INT &param, ECppProperty EC_CppProperty, INT InOffset, INT defaultValue) {
	param = defaultValue;
	new (m_scClass, pName, RF_Public) UIntProperty(EC_CppProperty, InOffset, TEXT("Options"), CPF_Config);
}

void UD3D9RenderDevice::SC_AddFloatConfigParam(const TCHAR *pName, FLOAT &param, ECppProperty EC_CppProperty, INT InOffset, FLOAT defaultValue) {
	param = defaultValue;
	new (m_scClass, pName, RF_Public) UFloatProperty(EC_CppProperty, InOffset, TEXT("Options"), CPF_Config);
}

UD3D9RenderDevice::FBoolConfigParam UD3D9RenderDevice::m_boolConfigParams[UD3D9RenderDevice::MAX_BOOL_CONFIG_PARAMS];
DWORD UD3D9RenderDevice::m_numBoolConfigParams = 0;
DWORD UD3D9RenderDevice::m_numDroppedBoolConfigParams = 0;
UClass *UD3D9RenderDevice::m_scClass = NULL;

void UD3D9RenderDevice::SC_RecordBoolConfigParam(const TCHAR *pName, DWORD BitMaskOffset) {
	if (m_numBoolConfigParams >= MAX_BOOL_CONFIG_PARAMS) {
		m_numDroppedBoolConfigParams++;
		return;
	}

	m_boolConfigParams[m_numBoolConfigParams].pName = pName;
	m_boolConfigParams[m_numBoolConfigParams].BitMaskOffset = BitMaskOffset;
	m_numBoolConfigParams++;
}

//A mismatch silently breaks the setting.
void UD3D9RenderDevice::ValidateBoolConfigParamOffsets(void) {
	guard(UD3D9RenderDevice::ValidateBoolConfigParamOffsets);

	static bool validated = false;
	if (validated) {
		return;
	}
	validated = true;

	if (m_numDroppedBoolConfigParams != 0) {
		debugf(NAME_Init, TEXT("Warning: %u bool config parameter(s) past the limit of %u were not recorded and are unchecked; raise MAX_BOOL_CONFIG_PARAMS."),
			m_numDroppedBoolConfigParams, (DWORD)MAX_BOOL_CONFIG_PARAMS);
	}

	for (DWORD u = 0; u < m_numBoolConfigParams; u++) {
		const TCHAR *pName = m_boolConfigParams[u].pName;

		UBoolProperty *pProperty = FindField<UBoolProperty>(GetClass(), pName);
		if (pProperty == NULL) {
			debugf(NAME_Init, TEXT("Warning: bool config parameter %s was registered but cannot be found."), pName);
			continue;
		}

		BITFIELD expected = ((BITFIELD)1) << m_boolConfigParams[u].BitMaskOffset;
		if (pProperty->BitMask != expected) {
			debugf(NAME_Init, TEXT("Warning: %s was registered with bit offset %u but the engine assigned bit mask 0x%08X; the ini cannot control it. Correct the offset in StaticConstructor."),
				pName, m_boolConfigParams[u].BitMaskOffset, (DWORD)pProperty->BitMask);
		}
	}

	unguard;
}
/*
The engine never defaults its own feature flags, so a missing ini key leaves one sitting at zero,
which is visible in the game for ShinySurfaces because the engine checks it before submitting any
reflection at all, and adding the key only when it is absent puts the default back without
overriding an entry somebody wrote deliberately.
*/
void UD3D9RenderDevice::InitEngineFeatureFlagSafe(const TCHAR *pName, BITFIELD &flag) {
	guard(UD3D9RenderDevice::InitEngineFeatureFlagSafe);

	UBoolProperty *pProperty = FindField<UBoolProperty>(m_scClass, pName);
	if (pProperty != NULL) {
		flag |= pProperty->BitMask;
	}

	TCHAR configValue[80];
	if (GConfig->GetString(g_pSection, pName, configValue, ARRAY_COUNT(configValue))) {
		return;
	}

	GConfig->SetBool(g_pSection, pName, 1);

	unguard;
}

void UD3D9RenderDevice::InitEngineFeatureFlags(void) {
	guard(UD3D9RenderDevice::InitEngineFeatureFlags);

	InitEngineFeatureFlagSafe(TEXT("Coronas"), Coronas);
	InitEngineFeatureFlagSafe(TEXT("ShinySurfaces"), ShinySurfaces);
	InitEngineFeatureFlagSafe(TEXT("VolumetricLighting"), VolumetricLighting);
	InitEngineFeatureFlagSafe(TEXT("HighDetailActors"), HighDetailActors);
#ifndef UTGLR_OLD_URENDERDEVICE
	InitEngineFeatureFlagSafe(TEXT("DetailTextures"), DetailTextures);
#endif

	unguard;
}


void UD3D9RenderDevice::ConfigValidate_RefreshDCV(void) {
#define UTGLR_REFRESH_DCV(_name) _name = DCV._name

	UTGLR_REFRESH_DCV(SinglePassDetail);
	UTGLR_REFRESH_DCV(UseFragmentProgram);
	UTGLR_REFRESH_DCV(DetailMax);

#undef UTGLR_REFRESH_DCV

	return;
}

void UD3D9RenderDevice::ConfigValidate_RequiredExtensions(void) {
	if (m_d3dCaps.PixelShaderVersion < D3DPS_VERSION(3, 0)) UseFragmentProgram = 0;
	if (m_d3dCaps.VertexShaderVersion < D3DVS_VERSION(3, 0)) UseFragmentProgram = 0;
	if (!(m_d3dCaps.TextureOpCaps & D3DTEXOPCAPS_BLENDCURRENTALPHA)) DetailTextures = 0;
	if (!(m_d3dCaps.TextureOpCaps & D3DTEXOPCAPS_BLENDCURRENTALPHA)) UseDetailAlpha = 0;
	if (!m_alphaTextureCap) UseDetailAlpha = 0;
	if (!(m_d3dCaps.TextureFilterCaps & D3DPTFILTERCAPS_MINFANISOTROPIC)) MaxAnisotropy = 0;
	if (!(m_d3dCaps.RasterCaps & D3DPRASTERCAPS_MIPMAPLODBIAS)) LODBias = 0;
	if (!m_dxt3TextureCap) TexDXT1ToDXT3 = 0;
	if (!m_16BitTextureCap) Use16BitTextures = 0;
	if (!m_565TextureCap) Use565Textures = 0;

	if (!(m_d3dCaps.TextureOpCaps & D3DTEXOPCAPS_MODULATEINVALPHA_ADDCOLOR)) SinglePassFog = 0;

	if (!(m_d3dCaps.TextureOpCaps & D3DTEXOPCAPS_MODULATE2X)) OneXBlending = 0x1; //Must use proper bit offset for Bool param

	return;
}

void UD3D9RenderDevice::ConfigValidate_Main(void) {
	if (TMUnits < 2) UseDetailAlpha = 0;

	if (TMUnits < 4) SinglePassDetail = 0;
	if (!UseDetailAlpha) SinglePassDetail = 0;

	if (DetailMax > 3) DetailMax = 3;
	//The shader path chooses between a one and a two layer detail pass,
	//so a third has nowhere to go; the fixed function path loops and can draw three.
	if (UseFragmentProgram && (DetailMax > 2)) DetailMax = 2;

	return;
}
