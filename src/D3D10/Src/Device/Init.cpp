/**
\file Device/Init.cpp

Init, SetRes and Exit, plus applyOptionChanges and rebuildRenderer.
*/

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>

#include <new>

#include "../D3D10.h"
#include "../Core/Misc.h"
#include "../Geometry/Deferred.h"
#include "../Texture/TextureCache.h"
#include "../Texture/TexConverter.h"
#include "../Shaders/Shader_GouraudPolygon.h"
#include "../Shaders/Shader_Tile.h"
#include "../Shaders/Shader_ComplexSurface.h"
#include "../Shaders/Shader_FogSurface.h"
#include "../Shaders/Shader_Line.h"


void UD3D10RenderDevice::makeContextCurrent() {
	if (d3dContext)
		D3D::makeCurrent(d3dContext);
}

//What the engine's fog math assumes.
void UD3D10RenderDevice::setViewDistance(int unlimited) {
	zFar = unlimited ? 65536.0f : 32760.0f;
}

/**
\param InViewport Includes the window handle.
\param NewX,NewY Viewport size.
\param NewColorBytes Color depth.
\param Fullscreen Fullscreen mode.
\return 1 on success; 0 errors out.
\note Ignores color depth.
*/
UBOOL UD3D10RenderDevice::Init(UViewport *InViewport, INT NewX, INT NewY, INT NewColorBytes, UBOOL Fullscreen) {
	UD3D10RenderDevice::debugs("Initializing Direct3D 10 renderer.");

	if (!QueryPerformanceFrequency(&perfCounterFreq) || perfCounterFreq.QuadPart <= 0) {
		UD3D10RenderDevice::debugs("No performance counter; frame timing will be wrong.");
		perfCounterFreq.QuadPart = 1;
	}
	lastFrameEnd.QuadPart = 0;

	URenderDevice::SpanBased = 0;
	URenderDevice::FullscreenOnly = 0;
	URenderDevice::SupportsFogMaps = 1;
#if (!KLINGON)
	URenderDevice::SupportsTC = 1;
	URenderDevice::SupportsLazyTextures = 0;
#endif
	URenderDevice::SupportsDistanceFog = 0;

#if (!UNREALGOLD && !UNREAL_224 && !KLINGON)
	URenderDevice::Description = TEXT("Direct3D 10");
	URenderDevice::DescFlags = 0;
#endif


	options.precache = getOption(TEXT("Precache"), 0, true);
	D3DOptions.samples = getOption(TEXT("Antialiasing"), 8, false);
	D3DOptions.aniso = getOption(TEXT("Anisotropy"), 8, false);
	//The limiter paces instead.
	D3DOptions.VSync = getOption(TEXT("VSync"), 0, true);
	D3DOptions.POM = getOption(TEXT("ParallaxOcclusionMapping"), 1, true);
	D3DOptions.LODBias = getOption(TEXT("LODBias"), 0, false);
	D3DOptions.bumpMapping = getOption(TEXT("BumpMapping"), 1, true);
	D3DOptions.classicLighting = getOption(TEXT("ClassicLighting"), 1, true);
	//Off everywhere, for the MODULATE2X look.
	D3DOptions.oneXBlending = getOption(TEXT("OneXBlending"), 0, true);
	D3DOptions.alphaToCoverage = getOption(TEXT("AlphaToCoverage"), 0, true);
	options.autoFOV = getOption(TEXT("AutoFOV"), 1, true);
	options.FPSLimit = getOption(TEXT("FrameRateLimit"), 120, false);
	options.singleCpuAffinity = getOption(TEXT("SingleCpuAffinity"), 0, true);
	options.textureCacheBudgetMegs = getOption(TEXT("TextureCacheBudgetMegs"), 512, false);
	//Off if edges misbehave.
	options.lightmapAtlas = getOption(TEXT("LightmapAtlas"), 1, true);
	if (!GConfig->GetFloat(TEXT("D3D10Drv.D3D10RenderDevice"), TEXT("GammaOffset"), options.gammaOffset)) {
		options.gammaOffset = 0.0f;
		GConfig->SetFloat(TEXT("D3D10Drv.D3D10RenderDevice"), TEXT("GammaOffset"), options.gammaOffset);
	}
	if (!(options.gammaOffset > -1e30f && options.gammaOffset < 1e30f)) {
		UD3D10RenderDevice::debugs("GammaOffset is not a finite number; ignoring it.");
		options.gammaOffset = 0.0f;
	}
	logTiles = getOption(TEXT("LogTiles"), 0, true) != 0;
	numLoggedTiles = 0;
	D3DOptions.filtering = getOption(TEXT("TextureFiltering"), D3D::FILTER_ANISOTROPIC, false);
	D3DOptions.postAA = getOption(TEXT("PostProcessAA"), 0, true);
	D3DOptions.reduceBanding = getOption(TEXT("ReduceBanding"), 1, true);
	D3DOptions.clipboardScreenshots = getOption(TEXT("ClipboardScreenshots"), 1, true);
	D3DOptions.decalDepthBias = getOption(TEXT("DecalDepthBias"), 32, false);
	D3DOptions.instancing = getOption(TEXT("Instancing"), 1, true);
	options.deferredRecording = getOption(TEXT("DeferredRecording"), 1, true);
	options.renderThreads = getOption(TEXT("RenderThreads"), 0, false);
	options.unlimitedViewDistance = getOption(TEXT("UnlimitedViewDistance"), 0, true);
	setViewDistance(options.unlimitedViewDistance);

	appliedOptions.unlimitedViewDistance = options.unlimitedViewDistance;
	appliedOptions.textureCacheBudgetMegs = options.textureCacheBudgetMegs;
	appliedOptions.gammaOffset = options.gammaOffset;

	URenderDevice::Viewport = InViewport;

	static bool affinityRestricted = false;
	if (options.singleCpuAffinity && !affinityRestricted) {
		affinityRestricted = true;
		if (SetProcessAffinityMask(GetCurrentProcess(), 0x1))
			UD3D10RenderDevice::debugs("Restricted the process to a single CPU core.");
		else
			UD3D10RenderDevice::debugs("Could not restrict the process to a single CPU core.");
	}

	d3dContext = D3D::createContext();
	if (!d3dContext) {
		errorOut().Log(TEXT("Init: Allocating the Direct3D context failed."));
		return 0;
	}
	makeContextCurrent();

	//Engine cleanup is unreliable.
	appliedD3DOptions = D3DOptions;
	if (!D3D::init((HWND)InViewport->GetWindow(), D3DOptions)) {
		errorOut().Log(TEXT("Init: Initializing Direct3D failed."));
		Exit();
		return 0;
	}

	if (!UD3D10RenderDevice::SetRes(NewX, NewY, NewColorBytes, Fullscreen)) {
		errorOut().Log(TEXT("Init: SetRes failed."));
		Exit();
		return 0;
	}

	textureCache = new (std::nothrow) TextureCache(D3D::getDevice());
	if (!textureCache) {
		errorOut().Log(TEXT("Error allocating texture cache."));
		Exit();
		return 0;
	}
	textureCache->setBudget(options.textureCacheBudgetMegs);

	texConverter = new (std::nothrow) TexConverter(textureCache);
	if (!texConverter) {
		errorOut().Log(TEXT("Error allocating texture converter."));
		Exit();
		return 0;
	}

	shader_GouraudPolygon = static_cast<Shader_GouraudPolygon *>(D3D::getShader(D3D::SHADER_GOURAUDPOLYGON));
	shader_Tile = static_cast<Shader_Tile *>(D3D::getShader(D3D::SHADER_TILE));
	shader_ComplexSurface = static_cast<Shader_ComplexSurface *>(D3D::getShader(D3D::SHADER_COMPLEXSURFACE));
	shader_FogSurface = static_cast<Shader_FogSurface *>(D3D::getShader(D3D::SHADER_FOGSURFACE));
	shader_Line = static_cast<Shader_Line *>(D3D::getShader(D3D::SHADER_LINE));

	//Not from DllMain.
	{
		DeferredGeometry *deferred = D3D::getDeferred();
		deferred->setEnabled(options.deferredRecording != 0);
		//Four viewports meant sixty idle threads.
		if (options.deferredRecording)
			deferred->start(options.renderThreads);
		char msg[128];
		_snprintf_s(msg, sizeof(msg), _TRUNCATE, "Deferred recording %s, %i build thread(s).",
			options.deferredRecording ? "on" : "off", deferred->numThreads());
		UD3D10RenderDevice::debugs(msg);
	}

	float brightness = 0.5f;
	GConfig->GetFloat(TEXT("WinDrv.WindowsClient"), TEXT("Brightness"), brightness);
	appliedBrightness = brightness;
	D3D::setBrightness(brightness + options.gammaOffset);
	updateBrightness(); //It may be newer.

	saveSystemGamma();
	neutralizeSystemGamma();

#if (!KLINGON)
	if (options.precache)
		URenderDevice::PrecacheOnFlip = 1;
#endif

	return 1;
}

/** \return True if it can draw again. */
bool UD3D10RenderDevice::rebuildRenderer() {
	if (!URenderDevice::Viewport)
		return false;

	appliedD3DOptions = D3DOptions;

	lastFrameEnd.QuadPart = 0;

	if (textureCache)
		textureCache->flush();
	delete textureCache;
	textureCache = nullptr;
	delete texConverter;
	texConverter = nullptr;
	shader_GouraudPolygon = nullptr;
	shader_Tile = nullptr;
	shader_ComplexSurface = nullptr;
	shader_FogSurface = nullptr;
	shader_Line = nullptr;
	D3D::uninit();

	if (!D3D::init((HWND)URenderDevice::Viewport->GetWindow(), D3DOptions)) {
		UD3D10RenderDevice::debugs("Could not recreate the Direct3D device.");
		return false;
	}

	textureCache = new (std::nothrow) TextureCache(D3D::getDevice());
	texConverter = new (std::nothrow) TexConverter(textureCache);
	if (!textureCache || !texConverter) {
		UD3D10RenderDevice::debugs("Error reallocating the texture cache.");
		delete textureCache;
		textureCache = nullptr;
		delete texConverter;
		texConverter = nullptr;
		return false;
	}
	textureCache->setBudget(options.textureCacheBudgetMegs);

	shader_GouraudPolygon = static_cast<Shader_GouraudPolygon *>(D3D::getShader(D3D::SHADER_GOURAUDPOLYGON));
	shader_Tile = static_cast<Shader_Tile *>(D3D::getShader(D3D::SHADER_TILE));
	shader_ComplexSurface = static_cast<Shader_ComplexSurface *>(D3D::getShader(D3D::SHADER_COMPLEXSURFACE));
	shader_FogSurface = static_cast<Shader_FogSurface *>(D3D::getShader(D3D::SHADER_FOGSURFACE));
	shader_Line = static_cast<Shader_Line *>(D3D::getShader(D3D::SHADER_LINE));

	if (!D3D::resize(URenderDevice::Viewport->SizeX, URenderDevice::Viewport->SizeY, URenderDevice::Viewport->IsFullscreen() != 0)) {
		UD3D10RenderDevice::debugs("Could not resize the recreated device's buffers.");
		return false;
	}

	D3D::setBrightness(appliedBrightness + options.gammaOffset); //Went with the old device.

	UD3D10RenderDevice::debugs("Renderer rebuilt.");
	return true;
}

/**
Rebuilds for a shader-compiled setting.
\return False on failure.
*/
bool UD3D10RenderDevice::applyOptionChanges() {
	D3D::setRuntimeOptions(D3DOptions);
	appliedD3DOptions.VSync = D3DOptions.VSync;
	appliedD3DOptions.clipboardScreenshots = D3DOptions.clipboardScreenshots;

	if (options.unlimitedViewDistance != appliedOptions.unlimitedViewDistance) {
		appliedOptions.unlimitedViewDistance = options.unlimitedViewDistance;
		setViewDistance(options.unlimitedViewDistance);
	}

	if (options.textureCacheBudgetMegs != appliedOptions.textureCacheBudgetMegs) {
		appliedOptions.textureCacheBudgetMegs = options.textureCacheBudgetMegs;
		if (textureCache)
			textureCache->setBudget(options.textureCacheBudgetMegs);
	}

	if (options.gammaOffset != appliedOptions.gammaOffset) {
		appliedOptions.gammaOffset = options.gammaOffset;
		D3D::setBrightness(appliedBrightness + options.gammaOffset);
	}

	if (!D3D::optionsNeedRebuild(D3DOptions, appliedD3DOptions))
		return true;

	UD3D10RenderDevice::debugs("Options changed; rebuilding the renderer.");
	return rebuildRenderer();
}

/**
Resizes buffers and viewport.
\return 1 on success; 0 errors out.
\note Fullscreen may hold values past 0/1.
\note Must call the viewport's resize method.
*/
UBOOL UD3D10RenderDevice::SetRes(INT NewX, INT NewY, INT NewColorBytes, UBOOL Fullscreen) {
	makeContextCurrent();

	//Without it the fullscreen switch flickers.
	UBOOL Result = URenderDevice::Viewport->ResizeViewport(Fullscreen ? (BLIT_Fullscreen | BLIT_Direct3D) : (BLIT_HardwarePaint | BLIT_Direct3D), NewX, NewY, NewColorBytes);
	if (!Result) {
		errorOut().Log(TEXT("SetRes: Error resizing viewport."));
		return 0;
	}
	if (!D3D::resize(NewX, NewY, (Fullscreen != 0))) {
		errorOut().Log(TEXT("SetRes: D3D::Resize failed."));
		return 0;
	}

//Reapplied at frame start.
#if (RUNE || DEUSEX)
	int defaultFOV = 75;
#elif (UNREALGOLD || UNREAL_224 || UNREALTOURNAMENT || NERF || HARRYPOTTER || KLINGON)
	int defaultFOV = 90;
#else
	int defaultFOV = 90; //No game macro defined.
#endif
	customFOV = Misc::getFov(defaultFOV, Viewport->SizeX, Viewport->SizeY);

	neutralizeSystemGamma();

	return 1;
}

void UD3D10RenderDevice::Exit() {
	UD3D10RenderDevice::debugs("Direct3D 10 renderer exiting.");
	restoreSystemGamma();

	if (d3dContext) {
		makeContextCurrent();
		D3D::getDeferred()->stop();
		if (textureCache)
			textureCache->flush();
		delete textureCache;
		textureCache = nullptr;
		delete texConverter;
		texConverter = nullptr;
		D3D::uninit();

		D3D::destroyContext(d3dContext);
		d3dContext = nullptr;
	}

	releaseFrameTimer();
	//FreeConsole();
}
