/**
\file Device/Config.cpp

Registering and reading settings.
*/

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>

#include <new>

#include "../D3D10.h"
#include "../Texture/TextureCache.h"

IMPLEMENT_CLASS(UD3D10RenderDevice);

/** Also stdout in debug. */
void UD3D10RenderDevice::debugs(const char *s) {
	if (!s)
		return;

#ifdef _DEBUG
	puts(s);
#endif

#if KLINGON
	logOut().Log(s);
	return;
#else
	const size_t length = strlen(s);
	TCHAR *buf = new (std::nothrow) TCHAR[length + 1];
	if (buf) {
		size_t n;
		if (mbstowcs_s(&n, buf, length + 1, s, length) == 0)
			logOut().Log(buf);
		delete[] buf;
		return;
	}
#endif

	logOut().Log(TEXT("D3D10Drv: message could not be logged."));
}

/**
Writes the default first if the key is missing.
\param name Config key.
\param defaultVal Written and returned when absent.
\param isBool Bool or int.
\return The value.
*/
int UD3D10RenderDevice::getOption(const TCHAR *name, int defaultVal, bool isBool) {
	const TCHAR *Section = TEXT("D3D10Drv.D3D10RenderDevice");
	int out;
	if (isBool) {
		if (!GConfig->GetBool(Section, name, (INT &)out)) {
			GConfig->SetBool(Section, name, defaultVal);
			out = defaultVal;
		}
	} else {
		if (!GConfig->GetInt(Section, name, (INT &)out)) {
			GConfig->SetInt(Section, name, defaultVal);
			out = defaultVal;
		}
	}
	return out;
}

/**
Defaults an engine feature flag on, because the engine itself never does and a missing key leaves the
flag clear, which quietly turns off coronas, detail textures and the rest for anyone who has not gone
and edited their ini by hand.
\param name Config key, also the property name.
\param flag The feature flag being defaulted.
*/
void UD3D10RenderDevice::setEngineFeatureFlagDefault(const TCHAR *name, BITFIELD &flag) {
	UBoolProperty *property = FindField<UBoolProperty>(scClass, name);
	if (property)
		flag |= property->BitMask;

	getOption(name, 1, true);
}

/** On first creation. */
#if KLINGON
/* This SDK hands over the class where the others hand over a default object, so the defaults are
recovered directly and only once the size confirms they exist, a release build crashing outright if
they are read any earlier than that. */
void UD3D10RenderDevice::StaticConstructor(UClass *Class) {
	scClass = Class;

	if (Class->Defaults.Num() >= (INT)sizeof(UD3D10RenderDevice)) {
		((UD3D10RenderDevice *)&Class->Defaults(0))->staticConstructorBody();
	} else {
		//No exception handler here.
		BYTE *scratch = new (std::nothrow) BYTE[sizeof(UD3D10RenderDevice)];
		if (scratch) {
			memset(scratch, 0, sizeof(UD3D10RenderDevice));
			((UD3D10RenderDevice *)scratch)->staticConstructorBody();
			delete[] scratch;
		}
	}
}
#else
void UD3D10RenderDevice::StaticConstructor() {
	scClass = GetClass();
	staticConstructorBody();
}
#endif

UClass *UD3D10RenderDevice::scClass = NULL;

void UD3D10RenderDevice::staticConstructorBody() {
	new (scClass, TEXT("Precache"), RF_Public) UBoolProperty(CPP_PROPERTY(options.precache), TEXT("Options"), CPF_Config);
	new (scClass, TEXT("Antialiasing"), RF_Public) UIntProperty(CPP_PROPERTY(D3DOptions.samples), TEXT("Options"), CPF_Config);
	new (scClass, TEXT("Anisotropy"), RF_Public) UIntProperty(CPP_PROPERTY(D3DOptions.aniso), TEXT("Options"), CPF_Config);
	//0 point, 1 linear, 2 anisotropic.
	new (scClass, TEXT("TextureFiltering"), RF_Public) UIntProperty(CPP_PROPERTY(D3DOptions.filtering), TEXT("Options"), CPF_Config);
	new (scClass, TEXT("VSync"), RF_Public) UBoolProperty(CPP_PROPERTY(D3DOptions.VSync), TEXT("Options"), CPF_Config);
	new (scClass, TEXT("ParallaxOcclusionMapping"), RF_Public) UBoolProperty(CPP_PROPERTY(D3DOptions.POM), TEXT("Options"), CPF_Config);
	new (scClass, TEXT("LODBias"), RF_Public) UIntProperty(CPP_PROPERTY(D3DOptions.LODBias), TEXT("Options"), CPF_Config);
	new (scClass, TEXT("AlphaToCoverage"), RF_Public) UBoolProperty(CPP_PROPERTY(D3DOptions.alphaToCoverage), TEXT("Options"), CPF_Config);
	new (scClass, TEXT("PostProcessAA"), RF_Public) UBoolProperty(CPP_PROPERTY(D3DOptions.postAA), TEXT("Options"), CPF_Config);
	new (scClass, TEXT("ReduceBanding"), RF_Public) UBoolProperty(CPP_PROPERTY(D3DOptions.reduceBanding), TEXT("Options"), CPF_Config);
	new (scClass, TEXT("BumpMapping"), RF_Public) UBoolProperty(CPP_PROPERTY(D3DOptions.bumpMapping), TEXT("Options"), CPF_Config);
	new (scClass, TEXT("ClassicLighting"), RF_Public) UBoolProperty(CPP_PROPERTY(D3DOptions.classicLighting), TEXT("Options"), CPF_Config);
	new (scClass, TEXT("OneXBlending"), RF_Public) UBoolProperty(CPP_PROPERTY(D3DOptions.oneXBlending), TEXT("Options"), CPF_Config);
	new (scClass, TEXT("ClipboardScreenshots"), RF_Public) UBoolProperty(CPP_PROPERTY(D3DOptions.clipboardScreenshots), TEXT("Options"), CPF_Config);
	new (scClass, TEXT("DecalDepthBias"), RF_Public) UIntProperty(CPP_PROPERTY(D3DOptions.decalDepthBias), TEXT("Options"), CPF_Config);
	new (scClass, TEXT("AutoFOV"), RF_Public) UBoolProperty(CPP_PROPERTY(options.autoFOV), TEXT("Options"), CPF_Config);
	new (scClass, TEXT("FrameRateLimit"), RF_Public) UIntProperty(CPP_PROPERTY(options.FPSLimit), TEXT("Options"), CPF_Config);
	new (scClass, TEXT("SingleCpuAffinity"), RF_Public) UBoolProperty(CPP_PROPERTY(options.singleCpuAffinity), TEXT("Options"), CPF_Config);
	new (scClass, TEXT("TextureCacheBudgetMegs"), RF_Public) UIntProperty(CPP_PROPERTY(options.textureCacheBudgetMegs), TEXT("Options"), CPF_Config);
	new (scClass, TEXT("GammaOffset"), RF_Public) UFloatProperty(CPP_PROPERTY(options.gammaOffset), TEXT("Options"), CPF_Config);
	//The blend state stays on. An old ini key of that name is ignored.
	new (scClass, TEXT("UnlimitedViewDistance"), RF_Public) UBoolProperty(CPP_PROPERTY(options.unlimitedViewDistance), TEXT("Options"), CPF_Config);

	//Next launch only.
	new (scClass, TEXT("LightmapAtlas"), RF_Public) UBoolProperty(CPP_PROPERTY(options.lightmapAtlas), TEXT("Options"), CPF_Config);
	new (scClass, TEXT("Instancing"), RF_Public) UBoolProperty(CPP_PROPERTY(D3DOptions.instancing), TEXT("Options"), CPF_Config);
	new (scClass, TEXT("DeferredRecording"), RF_Public) UBoolProperty(CPP_PROPERTY(options.deferredRecording), TEXT("Options"), CPF_Config);
	new (scClass, TEXT("RenderThreads"), RF_Public) UIntProperty(CPP_PROPERTY(options.renderThreads), TEXT("Options"), CPF_Config);

	setEngineFeatureFlagDefault(TEXT("Coronas"), Coronas);
	setEngineFeatureFlagDefault(TEXT("HighDetailActors"), HighDetailActors);
	setEngineFeatureFlagDefault(TEXT("VolumetricLighting"), VolumetricLighting);
	setEngineFeatureFlagDefault(TEXT("ShinySurfaces"), ShinySurfaces);
#if (!OLD_URENDERDEVICE)
	setEngineFeatureFlagDefault(TEXT("DetailTextures"), DetailTextures);
#else
	getOption(TEXT("DetailTextures"), 1, true);
#endif

#ifdef _DEBUG
	AllocConsole(); //Harmless if present.
	FILE *consoleOut;
	freopen_s(&consoleOut, "CONOUT$", "w", stdout);
#endif
}
