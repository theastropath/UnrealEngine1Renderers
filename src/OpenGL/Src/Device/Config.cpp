/*=============================================================================
	Config.cpp: ini settings and their registration.
=============================================================================*/

#include "../OpenGLDrv.h"
#include "../OpenGL.h"
#include "../Core/Globals.h"

/*-----------------------------------------------------------------------------
	OpenGL1xDrv.
-----------------------------------------------------------------------------*/

IMPLEMENT_CLASS(UOpenGLRenderDevice);


#ifdef UTGLR_KLINGON_BUILD
/*
Defaults come straight from the class defaults buffer, and not every SDK generation
reliably provides a default object here, so the fallback path still registers the
properties and lets the settings round trip through the ini with only the initial values
lost, which one run puts right, and that is worth a branch because a renderer refusing to
load over its own missing defaults fails harder than one starting at zero.
*/
void UOpenGLRenderDevice::StaticConstructor(UClass *Class) {
	m_scClass = Class;

	if (Class->Defaults.Num() >= (INT)sizeof(UOpenGLRenderDevice)) {
		((UOpenGLRenderDevice *)&Class->Defaults(0))->StaticConstructorBody();
	} else {
		//Scratch, and unread.
		BYTE *scratch = new BYTE[sizeof(UOpenGLRenderDevice)];
		memset(scratch, 0, sizeof(UOpenGLRenderDevice));
		((UOpenGLRenderDevice *)scratch)->StaticConstructorBody();
		delete[] scratch;
	}
}
#else
void UOpenGLRenderDevice::StaticConstructor() {
	m_scClass = GetClass();
	StaticConstructorBody();
}
#endif

void UOpenGLRenderDevice::StaticConstructorBody() {
	guard(UOpenGLRenderDevice::StaticConstructor);

	//Not registered on Klingon.
	//Deus Ex and Rune ship uncompressed content.
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
	//Dithers the gamma pass.
	SC_AddBoolConfigParam(0, TEXT("ReduceBanding"), CPP_PROPERTY_LOCAL(ReduceBanding), 1);
	SC_AddFloatConfigParam(TEXT("GammaOffset"), CPP_PROPERTY_LOCAL(GammaOffset), 0.0f);
	SC_AddFloatConfigParam(TEXT("GammaOffsetRed"), CPP_PROPERTY_LOCAL(GammaOffsetRed), 0.0f);
	SC_AddFloatConfigParam(TEXT("GammaOffsetGreen"), CPP_PROPERTY_LOCAL(GammaOffsetGreen), 0.0f);
	SC_AddFloatConfigParam(TEXT("GammaOffsetBlue"), CPP_PROPERTY_LOCAL(GammaOffsetBlue), 0.0f);
	//Registered, so edits are seen.
	SC_AddIntConfigParam(TEXT("Brightness"), CPP_PROPERTY_LOCAL(Brightness), 0);
	//Named as in the D3D9 renderer.
	//Off selects the in-render gamma pass.
	SC_AddBoolConfigParam(1, TEXT("HardwareGamma"), CPP_PROPERTY_LOCAL(UseHardwareGamma), 0);
	//Off everywhere, Deus Ex included.
	SC_AddBoolConfigParam(0, TEXT("OneXBlending"), CPP_PROPERTY_LOCAL(OneXBlending), 0);
	SC_AddIntConfigParam(TEXT("MinLogTextureSize"), CPP_PROPERTY_LOCAL(MinLogTextureSize), 0);
	SC_AddIntConfigParam(TEXT("MaxLogTextureSize"), CPP_PROPERTY_LOCAL(MaxLogTextureSize), 12);
	//Only registered where they do something.
	//The offsets have to be computed.
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
	SC_AddBoolConfigParam(7 + scUsePrecacheBools + scUseS3TCBools, TEXT("ZTrick"), CPP_PROPERTY_LOCAL(UseZTrick), 0);
	SC_AddBoolConfigParam(6 + scUsePrecacheBools + scUseS3TCBools, TEXT("BGRATextures"), CPP_PROPERTY_LOCAL(UseBGRATextures), 1);
	SC_AddBoolConfigParam(5 + scUsePrecacheBools + scUseS3TCBools, TEXT("MultiTexture"), CPP_PROPERTY_LOCAL(UseMultiTexture), 1);
	SC_AddBoolConfigParam(4 + scUsePrecacheBools + scUseS3TCBools, TEXT("Palette"), CPP_PROPERTY_LOCAL(UsePalette), 0);
	SC_AddBoolConfigParam(3 + scUsePrecacheBools + scUseS3TCBools, TEXT("ShareLists"), CPP_PROPERTY_LOCAL(ShareLists), 0);
#ifndef UTGLR_OLD_URENDERDEVICE
	SC_AddBoolConfigParam(3 + scUseS3TCBools, TEXT("Precache"), CPP_PROPERTY_LOCAL(UsePrecache), 0);
#else
	//Nothing else sets it.
	UsePrecache = 0;
#endif
	//Trilinear and anisotropic default on.
	//Both clamp to the hardware limits.
	//Changing either flushes the texture cache.
	SC_AddBoolConfigParam(2 + scUseS3TCBools, TEXT("Trilinear"), CPP_PROPERTY_LOCAL(UseTrilinear), 1);
	SC_AddBoolConfigParam(1 + scUseS3TCBools, TEXT("AlphaPalette"), CPP_PROPERTY_LOCAL(UseAlphaPalette), 0);
#ifndef UTGLR_KLINGON_BUILD
	SC_AddBoolConfigParam(1, TEXT("S3TC"), CPP_PROPERTY_LOCAL(UseS3TC), UTGLR_DEFAULT_UseS3TC);
#else
	//Forced off on this build.
	UseS3TC = 0;
#endif
	SC_AddBoolConfigParam(0, TEXT("16BitTextures"), CPP_PROPERTY_LOCAL(Use16BitTextures), 0);
	SC_AddIntConfigParam(TEXT("Anisotropy"), CPP_PROPERTY_LOCAL(MaxAnisotropy), 8);
	SC_AddBoolConfigParam(0, TEXT("NoFiltering"), CPP_PROPERTY_LOCAL(NoFiltering), 0);
	SC_AddIntConfigParam(TEXT("MaxTMUnits"), CPP_PROPERTY_LOCAL(MaxTMUnits), 0);
	SC_AddIntConfigParam(TEXT("RefreshRate"), CPP_PROPERTY_LOCAL(RefreshRate), 0);
	SC_AddIntConfigParam(TEXT("DetailMax"), CPP_PROPERTY_LOCAL_DCV(DetailMax), 0);
	SC_AddBoolConfigParam(9, TEXT("DetailClipping"), CPP_PROPERTY_LOCAL(DetailClipping), 0);
	SC_AddBoolConfigParam(8, TEXT("DebugDrawDetailTextures"), CPP_PROPERTY_LOCAL(ColorizeDetailTextures), 0);
	SC_AddBoolConfigParam(7, TEXT("SinglePassFog"), CPP_PROPERTY_LOCAL_DCV(SinglePassFog), 1);
	SC_AddBoolConfigParam(6, TEXT("SinglePassDetail"), CPP_PROPERTY_LOCAL_DCV(SinglePassDetail), 0);
	//On by default.
	//Tiles carry the HUD, sprites and particles.
	SC_AddBoolConfigParam(5, TEXT("BufferTileQuads"), CPP_PROPERTY_LOCAL(BufferTileQuads), 1);
	SC_AddBoolConfigParam(4, TEXT("UseSSE"), CPP_PROPERTY_LOCAL(UseSSE), 1);
	SC_AddBoolConfigParam(3, TEXT("UseSSE2"), CPP_PROPERTY_LOCAL(UseSSE2), 1);
	SC_AddBoolConfigParam(2, TEXT("TexIdPool"), CPP_PROPERTY_LOCAL(UseTexIdPool), 1);
	SC_AddBoolConfigParam(1, TEXT("TexPool"), CPP_PROPERTY_LOCAL(UseTexPool), 1);
	SC_AddBoolConfigParam(0, TEXT("CacheStaticMaps"), CPP_PROPERTY_LOCAL(CacheStaticMaps), 1);
	SC_AddIntConfigParam(TEXT("TextureCacheBudgetMegs"), CPP_PROPERTY_LOCAL(TexCacheBudgetMegs), 512);
	SC_AddIntConfigParam(TEXT("DynamicTexIdRecycleLevel"), CPP_PROPERTY_LOCAL(DynamicTexIdRecycleLevel), 100);
	SC_AddBoolConfigParam(3, TEXT("TexDXT1ToDXT3"), CPP_PROPERTY_LOCAL(TexDXT1ToDXT3), 0);
	SC_AddBoolConfigParam(2, TEXT("MultiDrawArrays"), CPP_PROPERTY_LOCAL(UseMultiDrawArrays), 1);
	SC_AddBoolConfigParam(1, TEXT("VBO"), CPP_PROPERTY_LOCAL(UseVBO), 1);
	SC_AddBoolConfigParam(0, TEXT("FragmentProgram"), CPP_PROPERTY_LOCAL_DCV(UseFragmentProgram), 1);
	//An interval, unlike the D3D10 renderer's key of the same name.
	//1 is vsync, 2 is half rate.
	//Negative leaves the driver's own choice alone.
	SC_AddIntConfigParam(TEXT("VSync"), CPP_PROPERTY_LOCAL(SwapInterval), -1);
	SC_AddIntConfigParam(TEXT("FrameRateLimit"), CPP_PROPERTY_LOCAL(FrameRateLimit), 120);
	SC_AddBoolConfigParam(0, TEXT("SmoothMaskedTextures"), CPP_PROPERTY_LOCAL(SmoothMaskedTextures), 0);
	//Doubles as the on/off switch.
	//The mode set path halves the count on each refusal.
	SC_AddIntConfigParam(TEXT("Antialiasing"), CPP_PROPERTY_LOCAL(NumAASamples), 8);
	//No translucent variant of this hack.
	SC_AddBoolConfigParam(1, TEXT("NoAATiles"), CPP_PROPERTY_LOCAL(NoAATiles), 1);
	//These two share one slot.
	//Never both registered.
#ifdef UTGLR_OLD_URENDERDEVICE
	SC_AddBoolConfigParam(0, TEXT("DetailTextures"), CPP_PROPERTY_LOCAL(DetailTextures), 1);
#else
	SC_AddBoolConfigParam(0, TEXT("ZRangeHack"), CPP_PROPERTY_LOCAL(ZRangeHack), UTGLR_DEFAULT_ZRangeHack);
#endif

#undef CPP_PROPERTY_LOCAL
#undef CPP_PROPERTY_LOCAL_DCV

	//Has to happen here.
	InitEngineFeatureFlags();

	SpanBased = 0;
	SupportsFogMaps = 1;
#ifdef UTGLR_RUNE_BUILD
	SupportsDistanceFog = 1;
#else
	SupportsDistanceFog = 0;
#endif
	FullscreenOnly = 0;

	//Never read on this SDK.
#ifndef UTGLR_KLINGON_BUILD
	SupportsLazyTextures = 0;
#endif
	//The flag does not exist on older builds.
#if !defined UTGLR_UNREAL_224_BUILD && !defined UTGLR_KLINGON_BUILD
	PrefersDeferredLoad = 0;
#endif

	m_noTextureId = 0;
	m_alphaTextureId = 0;

	m_allocatedShaderNames = false;

	//False until the fragment programs load.
	m_sevenBitMapReconLoaded = false;
	m_sevenBitMapRecon = false;
	m_curSevenBitMapSizeValidBits = 0;

	m_fpGammaRamp = 0;
	m_gammaSceneTexId = 0;
	m_gammaRampTexId = 0;
	m_gammaSceneTexWidth = 0;
	m_gammaSceneTexHeight = 0;
	m_gammaPostProcessSupported = false;
	m_gammaRampTexOutOfDate = false;
	m_gammaRampIsIdentity = false;
	m_gammaRampInEffect = false;

	m_frameRateLimitTimerInitialized = false;

	unguard;
}


/*
Each bool takes one bit of a shared field and the offset is how many bools registered
after it fall in the same uninterrupted run, so a wrong count silently points the setting
at a bit the engine never reads and inserting or removing a run member renumbers the rest
of that run, which is why the validation pass below exists at all given that the failure
has no symptom beyond a setting that will not change.
*/
void UOpenGLRenderDevice::SC_AddBoolConfigParam(DWORD BitMaskOffset, const TCHAR *pName, UBOOL &param, ECppProperty EC_CppProperty, INT InOffset, UBOOL defaultValue) {
	param = (((defaultValue) != 0) ? 1 : 0) << BitMaskOffset; //Pre-shifted.
	new (m_scClass, pName, RF_Public) UBoolProperty(EC_CppProperty, InOffset, TEXT("Options"), CPF_Config);
	SC_RecordBoolConfigParam(pName, BitMaskOffset);
}

//Same, for BITFIELD parameters.
void UOpenGLRenderDevice::SC_AddBoolConfigParam(DWORD BitMaskOffset, const TCHAR *pName, BITFIELD &param, ECppProperty EC_CppProperty, INT InOffset, UBOOL defaultValue) {
	param = (((defaultValue) != 0) ? 1 : 0) << BitMaskOffset;
	new (m_scClass, pName, RF_Public) UBoolProperty(EC_CppProperty, InOffset, TEXT("Options"), CPF_Config);
	SC_RecordBoolConfigParam(pName, BitMaskOffset);
}

void UOpenGLRenderDevice::SC_AddIntConfigParam(const TCHAR *pName, INT &param, ECppProperty EC_CppProperty, INT InOffset, INT defaultValue) {
	param = defaultValue;
	new (m_scClass, pName, RF_Public) UIntProperty(EC_CppProperty, InOffset, TEXT("Options"), CPF_Config);
}

void UOpenGLRenderDevice::SC_AddFloatConfigParam(const TCHAR *pName, FLOAT &param, ECppProperty EC_CppProperty, INT InOffset, FLOAT defaultValue) {
	param = defaultValue;
	new (m_scClass, pName, RF_Public) UFloatProperty(EC_CppProperty, InOffset, TEXT("Options"), CPF_Config);
}

UOpenGLRenderDevice::FBoolConfigParam UOpenGLRenderDevice::m_boolConfigParams[UOpenGLRenderDevice::MAX_BOOL_CONFIG_PARAMS];
UClass *UOpenGLRenderDevice::m_scClass = NULL;
DWORD UOpenGLRenderDevice::m_numBoolConfigParams = 0;
//Counted separately here.
DWORD UOpenGLRenderDevice::m_numDroppedBoolConfigParams = 0;

void UOpenGLRenderDevice::SC_RecordBoolConfigParam(const TCHAR *pName, DWORD BitMaskOffset) {
	if (m_numBoolConfigParams >= MAX_BOOL_CONFIG_PARAMS) {
		m_numDroppedBoolConfigParams++;
		return;
	}

	m_boolConfigParams[m_numBoolConfigParams].pName = pName;
	m_boolConfigParams[m_numBoolConfigParams].BitMaskOffset = BitMaskOffset;
	m_numBoolConfigParams++;
}

//Compares each registered offset against the linked bit.
//A mismatch breaks the setting silently.
void UOpenGLRenderDevice::ValidateBoolConfigParamOffsets(void) {
	guard(UOpenGLRenderDevice::ValidateBoolConfigParamOffsets);

	//Once is enough.
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
The engine never defaults its own feature flags, so a missing ini key leaves one off and
ShinySurfaces is the visible case because the engine checks it before submitting any
reflection at all, and the class default supplies the real value while writing the key
only when absent keeps the setting visible without overriding an explicit entry, which
matters because a player who turned one of these off deliberately should not find it back
on after an upgrade.
*/
void UOpenGLRenderDevice::InitEngineFeatureFlagSafe(const TCHAR *pName, BITFIELD &flag) {
	guard(UOpenGLRenderDevice::InitEngineFeatureFlagSafe);

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

void UOpenGLRenderDevice::InitEngineFeatureFlags(void) {
	guard(UOpenGLRenderDevice::InitEngineFeatureFlags);

	InitEngineFeatureFlagSafe(TEXT("Coronas"), Coronas);
	InitEngineFeatureFlagSafe(TEXT("ShinySurfaces"), ShinySurfaces);
	InitEngineFeatureFlagSafe(TEXT("VolumetricLighting"), VolumetricLighting);
	InitEngineFeatureFlagSafe(TEXT("HighDetailActors"), HighDetailActors);
	//The 226 parent class has no DetailTextures field.
#ifndef UTGLR_OLD_URENDERDEVICE
	InitEngineFeatureFlagSafe(TEXT("DetailTextures"), DetailTextures);
#endif

	unguard;
}


void UOpenGLRenderDevice::ConfigValidate_RefreshDCV(void) {
#define UTGLR_REFRESH_DCV(_name) _name = DCV._name

	UTGLR_REFRESH_DCV(SinglePassFog);
	UTGLR_REFRESH_DCV(SinglePassDetail);
	UTGLR_REFRESH_DCV(UseFragmentProgram);
	UTGLR_REFRESH_DCV(DetailMax);

#undef UTGLR_REFRESH_DCV

	return;
}

void UOpenGLRenderDevice::ConfigValidate_RequiredExtensions(void) {
	if (!SUPPORTS_GL_ARB_vertex_program) UseFragmentProgram = 0;
	if (!SUPPORTS_GL_ARB_fragment_program) UseFragmentProgram = 0;
	if (!SUPPORTS_GL_EXT_bgra) UseBGRATextures = 0;
	if (!SUPPORTS_GL_EXT_multi_draw_arrays) UseMultiDrawArrays = 0;
	if (!SUPPORTS_GL_ARB_vertex_buffer_object) UseVBO = 0;
	if (!SUPPORTS_GL_EXT_paletted_texture) UsePalette = 0;
	if (!SUPPORTS_GL_EXT_texture_env_combine) DetailTextures = 0;
	if (!SUPPORTS_GL_EXT_texture_env_combine) UseDetailAlpha = 0;
	if (!SUPPORTS_GL_EXT_texture_filter_anisotropic) MaxAnisotropy = 0;
	if (!SUPPORTS_GL_EXT_texture_lod_bias) LODBias = 0;

	if (!SUPPORTS_GL_ATI_texture_env_combine3 && !SUPPORTS_GL_NV_texture_env_combine4) SinglePassFog = 0;

	if (!SUPPORTS_GL_EXT_texture_env_combine) OneXBlending = 0x1; //Must use the proper bit offset.

	return;
}

void UOpenGLRenderDevice::ConfigValidate_Main(void) {
	if (TMUnits < 2) UseDetailAlpha = 0;

	if (TMUnits < 4) SinglePassDetail = 0;
	if (!UseDetailAlpha) SinglePassDetail = 0;

	if (DetailMax > 3) DetailMax = 3;
	//The ARB path has only one and two layer detail programs.
	//The fixed function path loops the configured count.
	//Both clamps hit a working copy re-seeded every pass.
	if (UseFragmentProgram && (DetailMax > 2)) DetailMax = 2;

	return;
}
