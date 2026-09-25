/** \class D3D Direct3D, independent of the renderer interface. */
#ifdef _DEBUG
#define _DEBUGDX
#endif

//#define _PERFHUD //nv perfhud support

#include <cstdio>
#include <dxgi.h>
#include <d3d10.h>
#include <d3dx10.h>
#include "../D3D10.h"
#include "D3D.h"
#include "../Shaders/Shader_GouraudPolygon.h"
#include "../Shaders/Shader_Tile.h"
#include "../Shaders/Shader_ComplexSurface.h"
#include "../Shaders/Shader_FogSurface.h"
#include "../Shaders/Shader_Line.h"
#include "../Shaders/Shader_FirstPass.h"
#include "../Shaders/Shader_HDR.h"
#include "../Shaders/Shader_PostAA.h"
#include "../Shaders/Shader_FinalPass.h"
#include "../Shaders/Shader_Dummy.h"
#include "../Geometry/Deferred.h"

static struct
{
	IDXGIOutput *output;
	ID3D10Device *device;
	IDXGISwapChain *swapChain;
	ID3D10RenderTargetView *backBufferRTV;
} D3DObjects;

static Shader *currentShader = 0;
static Shader *shaders[D3D::DUMMY_NUM_SHADERS];

static DeferredGeometry *deferredGeometry = nullptr;

static const DXGI_FORMAT BACKBUFFER_FORMAT = DXGI_FORMAT_R8G8B8A8_UNORM;
static const DXGI_FORMAT HDR_FORMAT = DXGI_FORMAT_R11G11B10_FLOAT;
static const DXGI_FORMAT WIDE_COMPOSITE_FORMAT = DXGI_FORMAT_R10G10B10A2_UNORM;

/**@name Swap chain pacing
DXGI queues three frames.
*/
//@{
static const UINT SWAP_CHAIN_BUFFERS = 2;
static const UINT MAX_FRAME_LATENCY = 1;
//@}

static D3D::Options options;
static HWND hWnd;
static int resX, resY;

static bool deviceLost = true;

/**@name Display transitions
WinDrv can ask for a resize part way through a full screen transition, DXGI dispatching window
messages from inside one, and DXGI then refuses the nested resize while the back buffer view it
creates stands as an outstanding reference that fails the interrupted call, which the engine takes
as fatal, so the request is recorded here and applied once the transition finishes.
*/
//@{
static bool inTransition = false;
static bool resizeRequested = false;
static int requestedX = 0, requestedY = 0;
static bool requestedFullScreen = false;
/** Answering one request can produce another. */
static const int MAX_DEFERRED_RESIZES = 4;

struct TransitionScope {
	TransitionScope() { inTransition = true; }
	~TransitionScope() { inTransition = false; }
};
//@}

static int currIndex = -1;
static bool wasDown = false;

static unsigned int bufferFlushes = 0;
static unsigned int bufferFlushesLastFrame = 0;

static unsigned int moverBiasSwitches = 0;
static unsigned int moverBiasSwitchesLastFrame = 0;

static ID3D10RenderTargetView *currRTV = nullptr;
static ID3D10DepthStencilView *currDSV = nullptr;
//Resolve leaves two nulls.
static bool renderTargetsCached = false;

static bool shadersReady() {
	for (int i = 0; i < D3D::DUMMY_NUM_SHADERS; i++) {
		if (!shaders[i])
			return false;
	}
	return true;
}

static bool viewPortCached = false;
static int oldViewX, oldViewY, oldViewLeft, oldViewTop;

/** Everything the module caches. */
static void invalidateCachedState() {
	viewPortCached = false;
	currRTV = nullptr;
	currDSV = nullptr;
	renderTargetsCached = false;
	Shader::invalidateCachedState();
	Shader_Unreal::invalidateCachedState();
}


/*-----------------------------------------------------------------------------
	Per device state.
-----------------------------------------------------------------------------*/
struct D3D::Context {
	IDXGIOutput *output;
	ID3D10Device *device;
	IDXGISwapChain *swapChain;
	ID3D10RenderTargetView *backBufferRTV;
	Shader *currentShader;
	Shader *shaders[D3D::DUMMY_NUM_SHADERS];
	DeferredGeometry *deferredGeometry;
	D3D::Options options;
	HWND hWnd;
	int resX, resY;
	bool deviceLost;
	int currIndex;
	bool wasDown;
	bool viewPortCached;
	int oldViewX, oldViewY, oldViewLeft, oldViewTop;
	unsigned int bufferFlushes, bufferFlushesLastFrame;
	unsigned int moverBiasSwitches, moverBiasSwitchesLastFrame;
	ID3D10RenderTargetView *currRTV;
	ID3D10DepthStencilView *currDSV;
	bool renderTargetsCached;

	Shader::Statics shaderStatics;
	Shader_Unreal::Statics unrealStatics;
	Shader_Postprocess::Statics postprocessStatics;
};

static D3D::Context *currentContext = nullptr;

//In step with the struct above.
static void storeContext(D3D::Context &c) {
	c.output = D3DObjects.output;
	c.device = D3DObjects.device;
	c.swapChain = D3DObjects.swapChain;
	c.backBufferRTV = D3DObjects.backBufferRTV;
	c.currentShader = currentShader;
	for (int i = 0; i < D3D::DUMMY_NUM_SHADERS; i++)
		c.shaders[i] = shaders[i];
	c.deferredGeometry = deferredGeometry;
	c.options = options;
	c.hWnd = hWnd;
	c.resX = resX;
	c.resY = resY;
	c.deviceLost = deviceLost;
	c.currIndex = currIndex;
	c.wasDown = wasDown;
	c.viewPortCached = viewPortCached;
	c.oldViewX = oldViewX;
	c.oldViewY = oldViewY;
	c.oldViewLeft = oldViewLeft;
	c.oldViewTop = oldViewTop;
	c.bufferFlushes = bufferFlushes;
	c.bufferFlushesLastFrame = bufferFlushesLastFrame;
	c.moverBiasSwitches = moverBiasSwitches;
	c.moverBiasSwitchesLastFrame = moverBiasSwitchesLastFrame;
	c.currRTV = currRTV;
	c.currDSV = currDSV;
	c.renderTargetsCached = renderTargetsCached;

	Shader::saveStatics(c.shaderStatics);
	Shader_Unreal::saveStatics(c.unrealStatics);
	Shader_Postprocess::saveStatics(c.postprocessStatics);
}

static void loadContext(const D3D::Context &c) {
	D3DObjects.output = c.output;
	D3DObjects.device = c.device;
	D3DObjects.swapChain = c.swapChain;
	D3DObjects.backBufferRTV = c.backBufferRTV;
	currentShader = c.currentShader;
	for (int i = 0; i < D3D::DUMMY_NUM_SHADERS; i++)
		shaders[i] = c.shaders[i];
	deferredGeometry = c.deferredGeometry;
	options = c.options;
	hWnd = c.hWnd;
	resX = c.resX;
	resY = c.resY;
	deviceLost = c.deviceLost;
	currIndex = c.currIndex;
	wasDown = c.wasDown;
	viewPortCached = c.viewPortCached;
	oldViewX = c.oldViewX;
	oldViewY = c.oldViewY;
	oldViewLeft = c.oldViewLeft;
	oldViewTop = c.oldViewTop;
	bufferFlushes = c.bufferFlushes;
	bufferFlushesLastFrame = c.bufferFlushesLastFrame;
	moverBiasSwitches = c.moverBiasSwitches;
	moverBiasSwitchesLastFrame = c.moverBiasSwitchesLastFrame;
	currRTV = c.currRTV;
	currDSV = c.currDSV;
	renderTargetsCached = c.renderTargetsCached;

	Shader::loadStatics(c.shaderStatics);
	Shader_Unreal::loadStatics(c.unrealStatics);
	Shader_Postprocess::loadStatics(c.postprocessStatics);
}

static void blankContext(D3D::Context &c) {
	memset(&c, 0, sizeof(c));
	c.deviceLost = true;
	c.currIndex = -1;
	c.viewPortCached = false;
}

D3D::Context *D3D::createContext() {
	Context *c = new (std::nothrow) Context;
	if (!c)
		return nullptr;

	blankContext(*c);

	c->deferredGeometry = new (std::nothrow) DeferredGeometry;
	if (!c->deferredGeometry) {
		delete c;
		return nullptr;
	}

	return c;
}

void D3D::destroyContext(Context *context) {
	if (!context)
		return;

	if (currentContext == context) {
		Context blank;
		blankContext(blank);
		loadContext(blank);
		currentContext = nullptr;
	}

	delete context->deferredGeometry;
	delete context;
}

//A pointer comparison.
void D3D::makeCurrent(Context *context) {
	if (currentContext == context)
		return;

	if (currentContext)
		storeContext(*currentContext);

	currentContext = context;

	if (context)
		loadContext(*context);
	else {
		Context blank;
		blankContext(blank);
		loadContext(blank);
	}
}

int D3D::init(HWND _hWnd, const D3D::Options &createOptions) {
	hWnd = _hWnd;
	HRESULT hr;

	deviceLost = true;
	invalidateCachedState();

	options = createOptions;
	//Everything below is clamped.
	CLAMP(options.samples, 1, D3D10_MAX_MULTISAMPLE_SAMPLE_COUNT);
	//Fed to the sampler as an effect macro, which rejects 0, and the demotion below is what makes an
	//aniso of 0 mean off the way it does in the other two renderers, in place of a refused setting.
	CLAMP(options.aniso, 1, 16);
	CLAMP(options.VSync, 0, 1);
	CLAMP(options.LODBias, -10, 10);
	CLAMP(options.filtering, 0, D3D::DUMMY_NUM_FILTERING - 1);
	if (options.aniso < 2 && options.filtering == D3D::FILTER_ANISOTROPIC)
		options.filtering = D3D::FILTER_LINEAR;
	CLAMP(options.postAA, 0, 1);
	CLAMP(options.reduceBanding, 0, 1);
	CLAMP(options.clipboardScreenshots, 0, 1);
	//Each is stringified into a shader macro or compared against a literal, so a merely non-zero value
	//reads as off in one place and on in the other, and the property system writes the engine's own bit
	//mask, which past the first flag in a run is not 1.
	CLAMP(options.POM, 0, 1);
	CLAMP(options.bumpMapping, 0, 1);
	CLAMP(options.classicLighting, 0, 1);
	CLAMP(options.oneXBlending, 0, 1);
	CLAMP(options.alphaToCoverage, 0, 1);
	CLAMP(options.instancing, 0, 1);
	CLAMP(options.decalDepthBias, -65535, 65535);
	UD3D10RenderDevice::debugs("Initializing Direct3D.");

	IDXGIAdapter *selectedAdapter = nullptr;
	D3D10_DRIVER_TYPE driverType = D3D10_DRIVER_TYPE_HARDWARE;
#ifdef _PERFHUD
	{
		IDXGIFactory *pDXGIFactory = nullptr;
		if (SUCCEEDED(CreateDXGIFactory(__uuidof(IDXGIFactory), (void **)&pDXGIFactory))) {
			UINT nAdapter = 0;
			IDXGIAdapter *adapter = nullptr;

			while (pDXGIFactory->EnumAdapters(nAdapter, &adapter) != DXGI_ERROR_NOT_FOUND) {
				DXGI_ADAPTER_DESC adaptDesc;
				if (SUCCEEDED(adapter->GetDesc(&adaptDesc)) && wcscmp(adaptDesc.Description, L"NVIDIA PerfHUD") == 0) {
					puts("Using PerfHUD.");
					selectedAdapter = adapter; //Released after device creation.
					driverType = D3D10_DRIVER_TYPE_REFERENCE;
					break;
				}
				SAFE_RELEASE(adapter); //A new reference each call.
				++nAdapter;
			}
			SAFE_RELEASE(pDXGIFactory);
		}
	}
#endif

	UINT flags = D3D10_CREATE_DEVICE_SINGLETHREADED;
#ifdef _DEBUGDX
	flags |= D3D10_CREATE_DEVICE_DEBUG;
#endif

	DXGI_SWAP_CHAIN_DESC sd;
	ZeroMemory(&sd, sizeof(sd));
	sd.BufferCount = SWAP_CHAIN_BUFFERS;
	//sd.BufferDesc.Width = Window::getWidth();
	//sd.BufferDesc.Height = Window::getHeight();
	sd.BufferDesc.Format = BACKBUFFER_FORMAT;
	//sd.BufferDesc.RefreshRate.Numerator = 60;
	//sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.OutputWindow = hWnd;
	sd.SampleDesc.Count = 1;
	sd.SampleDesc.Quality = 0;
	sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
	sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
	sd.Windowed = TRUE;


	hr = D3D10CreateDeviceAndSwapChain(selectedAdapter, driverType, NULL, flags, D3D10_SDK_VERSION, &sd, &D3DObjects.swapChain, &D3DObjects.device);
	SAFE_RELEASE(selectedAdapter); //PerfHUD builds only.
	if (FAILED(hr)) {
		UD3D10RenderDevice::debugs("Error creating swap chain");
		return 0;
	}

	{
		IDXGIDevice1 *dxgiDevice = nullptr;
		if (SUCCEEDED(D3DObjects.device->QueryInterface(__uuidof(IDXGIDevice1), (void **)&dxgiDevice))) {
			dxgiDevice->SetMaximumFrameLatency(MAX_FRAME_LATENCY);
			SAFE_RELEASE(dxgiDevice);
		} else
			UD3D10RenderDevice::debugs("Could not cap the DXGI frame latency; frame pacing is left to the default of three queued frames.");
	}

	{
		IDXGIFactory *factory = nullptr;
		if (SUCCEEDED(D3DObjects.swapChain->GetParent(__uuidof(IDXGIFactory), (void **)&factory))) {
			factory->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER);
			SAFE_RELEASE(factory);
		}
	}

	if (FAILED(D3DObjects.swapChain->GetContainingOutput(&D3DObjects.output))) {
		D3DObjects.output = nullptr;
		UD3D10RenderDevice::debugs("Could not get containing output; the game's own resolution list will be used.");
	}

	if (!D3D::findAALevel()) //Must precede initShaders.
	{
		uninit();
		return 0;
	}

	if (!initShaders()) {
		uninit();
		return 0;
	}

#ifdef _DEBUGDX
	ID3D10InfoQueue *pInfoQueue = nullptr;
	D3DObjects.device->QueryInterface(__uuidof(ID3D10InfoQueue), (void **)&pInfoQueue);

	//Raised deliberately.
	D3D10_MESSAGE_ID messageIDs[] = {
		D3D10_MESSAGE_ID_DEVICE_DRAW_SHADERRESOURCEVIEW_NOT_SET,
		D3D10_MESSAGE_ID_PSSETSHADERRESOURCES_UNBINDDELETINGOBJECT,
		D3D10_MESSAGE_ID_OMSETRENDERTARGETS_UNBINDDELETINGOBJECT,
		D3D10_MESSAGE_ID_CHECKFORMATSUPPORT_FORMAT_DEPRECATED
	};

	D3D10_INFO_QUEUE_FILTER filter = { 0 };
	filter.DenyList.NumIDs = ARRAYSIZE(messageIDs); //A count of entries
	filter.DenyList.pIDList = messageIDs;

	if (pInfoQueue) {
		pInfoQueue->AddStorageFilterEntries(&filter);
		SAFE_RELEASE(pInfoQueue);
	}

#endif

	deviceLost = false; //Drawable from here on
	return 1;
}

//Baked in as macros.
bool D3D::initShaders() {
	DWORD dwShaderFlags = D3D10_SHADER_ENABLE_STRICTNESS;

#define OPTION_TO_STRING(x) \
	char buf##x[12]; \
	_itoa_s(options.##x, buf##x, 12, 10);
#define VALUE_TO_STRING(name, value) \
	char buf##name[12]; \
	_itoa_s(value, buf##name, 12, 10);
#define OPTIONSTRING_TO_SHADERVAR(x, y) { y, buf##x }
	OPTION_TO_STRING(aniso);
	OPTION_TO_STRING(LODBias);
	OPTION_TO_STRING(samples);
	OPTION_TO_STRING(POM);
	OPTION_TO_STRING(bumpMapping);
	OPTION_TO_STRING(classicLighting);
	OPTION_TO_STRING(oneXBlending);
	OPTION_TO_STRING(filtering);
	OPTION_TO_STRING(decalDepthBias);
	OPTION_TO_STRING(reduceBanding);

	VALUE_TO_STRING(alphaToCoverage, (options.alphaToCoverage && options.samples >= 4) ? 1 : 0);

	/*
	What a lightmap or fog map texel's channels get multiplied by to reach eight bits, every SDK here
	naming format 0x01 TEXF_RGBA7, seven significant bits, so that a texel stands for twice what it
	actually holds, with Klingon's build the one exception to all of it.
	*/
#if KLINGON
	VALUE_TO_STRING(bgra7Expand, 1);
#else
	VALUE_TO_STRING(bgra7Expand, 2);
#endif

	D3D10_SHADER_MACRO macros[] = {
		OPTIONSTRING_TO_SHADERVAR(aniso, "NUM_ANISO"),
		OPTIONSTRING_TO_SHADERVAR(LODBias, "LODBIAS"),
		OPTIONSTRING_TO_SHADERVAR(samples, "SAMPLES"),
		OPTIONSTRING_TO_SHADERVAR(POM, "POM_ENABLED"),
		OPTIONSTRING_TO_SHADERVAR(alphaToCoverage, "ALPHA_TO_COVERAGE_ENABLED"),
		OPTIONSTRING_TO_SHADERVAR(bumpMapping, "BUMPMAPPING_ENABLED"),
		OPTIONSTRING_TO_SHADERVAR(classicLighting, "CLASSIC_LIGHTING"),
		OPTIONSTRING_TO_SHADERVAR(oneXBlending, "ONEX_BLENDING"),
		OPTIONSTRING_TO_SHADERVAR(filtering, "TEXTURE_FILTERING"),
		OPTIONSTRING_TO_SHADERVAR(decalDepthBias, "DECAL_DEPTH_BIAS"),
		OPTIONSTRING_TO_SHADERVAR(reduceBanding, "REDUCE_BANDING"),
		OPTIONSTRING_TO_SHADERVAR(bgra7Expand, "BGRA7_EXPAND"),
		NULL
	};


	if (!Shader::initShaderSystem(D3DObjects.device, macros, dwShaderFlags))
		return 0;

	try {
		shaders[D3D::SHADER_GOURAUDPOLYGON] = new Shader_GouraudPolygon();
		shaders[D3D::SHADER_TILE] = new Shader_Tile(options.instancing != 0);
		shaders[D3D::SHADER_COMPLEXSURFACE] = new Shader_ComplexSurface();
		shaders[D3D::SHADER_FOGSURFACE] = new Shader_FogSurface();
		shaders[D3D::SHADER_LINE] = new Shader_Line(options.instancing != 0);
		if (options.samples > 1)
			shaders[D3D::SHADER_FIRSTPASS] = new Shader_FirstPass();
		else
			shaders[D3D::SHADER_FIRSTPASS] = new Shader_Dummy();
		if (!options.classicLighting)
			shaders[D3D::SHADER_HDR] = new Shader_HDR();
		else
			shaders[D3D::SHADER_HDR] = new Shader_Dummy();
		if (options.postAA)
			shaders[D3D::SHADER_POSTAA] = new Shader_PostAA();
		else
			shaders[D3D::SHADER_POSTAA] = new Shader_Dummy();
		shaders[D3D::SHADER_FINALPASS] = new Shader_FinalPass();
	} catch (std::bad_alloc &) {
		UD3D10RenderDevice::debugs("Error creating shader instances.");
		return 0;
	}
	for (int i = 0; i < D3D::DUMMY_NUM_SHADERS; i++) {
		if (!shaders[i]->compile(macros, dwShaderFlags))
			return false;
	}

	return 1;
}

//No reference the swap chain needs back.
int D3D::createRenderTargetViews() {
	HRESULT hr;

	ID3D10Texture2D *pBuffer;
	hr = D3DObjects.swapChain->GetBuffer(0, __uuidof(ID3D10Texture2D), (LPVOID *)&pBuffer);
	if (FAILED(hr)) {
		UD3D10RenderDevice::debugs("Error getting swap chain buffer.");
		return 0;
	}

	//For direct writes.
	hr = D3DObjects.device->CreateRenderTargetView(pBuffer, nullptr, &D3DObjects.backBufferRTV);
	if (FAILED(hr)) {
		UD3D10RenderDevice::debugs("Error creating render target view (backbuffer).");
		SAFE_RELEASE(pBuffer);
		return 0;
	}

	DXGI_SWAP_CHAIN_DESC scd;
	hr = D3DObjects.swapChain->GetDesc(&scd);
	if (FAILED(hr)) {
		UD3D10RenderDevice::debugs("Error getting swap chain description.");
		SAFE_RELEASE(pBuffer);
		return 0;
	}

	for (int i = 0; i < D3D::DUMMY_NUM_SHADERS; i++) {
		if (!shaders[i]->createRenderTargetViews(D3DObjects.backBufferRTV, scd, options.samples)) {
			UD3D10RenderDevice::debugs("Error creating render target views.");
			SAFE_RELEASE(pBuffer);
			return 0;
		}
	}
	if (options.samples <= 1)
		static_cast<Shader_Dummy *>(shaders[D3D::SHADER_FIRSTPASS])->setShaderResourceView(shaders[D3D::SHADER_FIRSTPASS - 1]->getResourceView());
	if (options.classicLighting)
		static_cast<Shader_Dummy *>(shaders[D3D::SHADER_HDR])->setShaderResourceView(shaders[D3D::SHADER_HDR - 1]->getResourceView());
	if (!options.postAA)
		static_cast<Shader_Dummy *>(shaders[D3D::SHADER_POSTAA])->setShaderResourceView(shaders[D3D::SHADER_POSTAA - 1]->getResourceView());

	SAFE_RELEASE(pBuffer);

	return 1;
}


//Safe on a part-built device, and safe twice.
void D3D::uninit() {
	UD3D10RenderDevice::debugs("Uninit.");

	TransitionScope transition;

	deviceLost = true; //Until a new device exists.
	switchToShader(-1); //A later init would find it set.
	invalidateCachedState();

	if (D3DObjects.swapChain)
		D3DObjects.swapChain->SetFullscreenState(FALSE, nullptr); //Windowed, so it releases.
	if (D3DObjects.device) {
		D3DObjects.device->Flush();
		D3DObjects.device->ClearState();
	}

	for (int i = 0; i < D3D::DUMMY_NUM_SHADERS; i++) {
		if (!shaders[i]) //Possibly unset.
			continue;
		shaders[i]->releaseRenderTargetViews();
		delete shaders[i];
		shaders[i] = nullptr;
	}

	Shader_Unreal::releaseSharedResources();
	Shader_Postprocess::releaseSharedResources();
	Shader::releaseSharedResources();

	SAFE_RELEASE(D3DObjects.backBufferRTV);

	SAFE_RELEASE(D3DObjects.swapChain);
	SAFE_RELEASE(D3DObjects.device);
	SAFE_RELEASE(D3DObjects.output);
	resizeRequested = false;
	UD3D10RenderDevice::debugs("Bye.");
}

//Null until init has run.
ID3D10Device *D3D::getDevice() {
	return D3DObjects.device;
}

/** Order matters here. */
int D3D::resizeSwapChain(int X, int Y, bool fullScreen) {

#ifdef _DEBUG
	printf("%d %d %d\n", X, Y, fullScreen);
#endif
	HRESULT hr;
	DXGI_SWAP_CHAIN_DESC sd;

	if (deviceLost || !D3DObjects.swapChain || !shadersReady()) {
		UD3D10RenderDevice::debugs("Cannot resize; there is no usable device.");
		return 0;
	}

	switchToShader(-1);

	hr = D3DObjects.swapChain->GetDesc(&sd);
	if (FAILED(hr)) {
		UD3D10RenderDevice::debugs("Failed to get swap chain description.");
		return 0;
	}
	sd.BufferDesc.Width = X; //For the closest-matching-mode lookup.
	sd.BufferDesc.Height = Y;
	sd.Flags &= ~DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	SAFE_RELEASE(D3DObjects.backBufferRTV);
	for (int i = 0; i < D3D::DUMMY_NUM_SHADERS; i++) {
		if (!shaders[i]) //As above.
			continue;
		shaders[i]->releaseRenderTargetViews();
	}

	invalidateCachedState();

	bool wentFullScreen = false;
	if (fullScreen) {
		DXGI_MODE_DESC fullscreenMode = sd.BufferDesc;
		//hr = D3DObjects.output->FindClosestMatchingMode(&sd.BufferDesc,&fullscreenMode,D3DObjects.device);
		hr = D3DObjects.swapChain->ResizeTarget(&fullscreenMode);
		if (FAILED(hr)) {
			UD3D10RenderDevice::debugs("Failed to set full-screen resolution.");
			deviceLost = true;
			return 0;
		}
		hr = D3DObjects.swapChain->SetFullscreenState(TRUE, nullptr);
		if (FAILED(hr)) {
			UD3D10RenderDevice::debugs("Failed to switch to full-screen; continuing windowed.");
		} else {
			wentFullScreen = true;

			//MS recommends doing this
			fullscreenMode.RefreshRate.Denominator = 0;
			fullscreenMode.RefreshRate.Numerator = 0;
			hr = D3DObjects.swapChain->ResizeTarget(&fullscreenMode);
			if (FAILED(hr)) {
				UD3D10RenderDevice::debugs("Failed to set full-screen resolution.");
				deviceLost = true;
				return 0;
			}
			sd.BufferDesc = fullscreenMode;
			sd.Flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
		}
	}
	if (!wentFullScreen) {
		BOOL isFullScreen = FALSE;
		if (SUCCEEDED(D3DObjects.swapChain->GetFullscreenState(&isFullScreen, nullptr)) && isFullScreen) {
			hr = D3DObjects.swapChain->SetFullscreenState(FALSE, nullptr);
			if (FAILED(hr))
				UD3D10RenderDevice::debugs("Failed to leave full-screen.");
		}
	}


	hr = D3DObjects.swapChain->ResizeBuffers(sd.BufferCount, X, Y, sd.BufferDesc.Format, sd.Flags);
	if (FAILED(hr)) {
		UD3D10RenderDevice::debugs("Failed to resize back buffer.");
		deviceLost = true;
		return 0;
	}
	if (!createRenderTargetViews()) {
		deviceLost = true;
		return 0;
	}

	setViewPort(X, Y, 0, 0);

	resX = X;
	resY = Y;
	return 1;
}

/** \return 1 on success, or a recorded request. 0 is fatal. */
int D3D::resize(int X, int Y, bool fullScreen) {
	if (inTransition) {
		requestedX = X;
		requestedY = Y;
		requestedFullScreen = fullScreen;
		resizeRequested = true;
		UD3D10RenderDevice::debugs("Resize requested during a display change; it is applied once that finishes.");
		return 1;
	}

	TransitionScope transition;
	int result = resizeSwapChain(X, Y, fullScreen);
	int appliedX = X, appliedY = Y;
	bool appliedFullScreen = fullScreen;

	for (int i = 0; result && resizeRequested && i < MAX_DEFERRED_RESIZES; i++) {
		resizeRequested = false;
		if (requestedX == appliedX && requestedY == appliedY && requestedFullScreen == appliedFullScreen)
			break; //Already applied above
		appliedX = requestedX;
		appliedY = requestedY;
		appliedFullScreen = requestedFullScreen;
		result = resizeSwapChain(appliedX, appliedY, appliedFullScreen);
	}
	resizeRequested = false;

	return result;
}

bool D3D::optionsNeedRebuild(const Options &a, const Options &b) {
	return a.samples != b.samples || a.aniso != b.aniso || a.LODBias != b.LODBias || a.POM != b.POM || a.bumpMapping != b.bumpMapping || a.alphaToCoverage != b.alphaToCoverage || a.classicLighting != b.classicLighting || a.oneXBlending != b.oneXBlending || a.filtering != b.filtering || a.postAA != b.postAA || a.reduceBanding != b.reduceBanding || a.decalDepthBias != b.decalDepthBias || a.instancing != b.instancing;
}

void D3D::setRuntimeOptions(const Options &newOptions) {
	options.VSync = newOptions.VSync;
	options.clipboardScreenshots = newOptions.clipboardScreenshots;
	CLAMP(options.VSync, 0, 1);
	CLAMP(options.clipboardScreenshots, 0, 1);
}

DXGI_FORMAT D3D::getCompositeFormat() {
	if (options.reduceBanding)
		return WIDE_COMPOSITE_FORMAT;
	else
		return BACKBUFFER_FORMAT;
}

DXGI_FORMAT D3D::getDrawPassFormat() {
	if (options.classicLighting)
		return getCompositeFormat(); //No HDR pass, so the composite.
	else
		return HDR_FORMAT;
}

unsigned int D3D::getBufferFlushCount() {
	return bufferFlushesLastFrame;
}

unsigned int D3D::getMoverBiasSwitchCount() {
	return moverBiasSwitchesLastFrame;
}

void D3D::countMoverBiasSwitch() {
	moverBiasSwitches++;
}

void D3D::setRenderTargets(ID3D10RenderTargetView *renderTargetView, ID3D10DepthStencilView *depthStencilView) {
	if (renderTargetsCached && renderTargetView == currRTV && depthStencilView == currDSV)
		return;

	if (renderTargetView)
		D3DObjects.device->OMSetRenderTargets(1, &renderTargetView, depthStencilView);
	else
		D3DObjects.device->OMSetRenderTargets(0, nullptr, depthStencilView);

	currRTV = renderTargetView;
	currDSV = depthStencilView;
	renderTargetsCached = true;
}

ID3D10RenderTargetView *D3D::getRenderTarget() {
	return currRTV;
}

//Everything the module owns.
void D3D::newFrame(float time) {
	if (!shadersReady())
		return;

	for (int i = 0; i < D3D::DUMMY_NUM_SHADERS; i++)
		shaders[i]->getGeometryBuffer()->newFrame();

	deferredGeometry->newFrame();

	static_cast<Shader_Postprocess *>(shaders[SHADER_FIRSTPASS])->setElapsedTime(time);

	bufferFlushesLastFrame = bufferFlushes;
	bufferFlushes = 0;
	moverBiasSwitchesLastFrame = moverBiasSwitches;
	moverBiasSwitches = 0;
}


void D3D::render() {
	if (deviceLost || !shadersReady() || deferredGeometry == nullptr)
		return;

	if (deferredGeometry->hasCommands()) {
		if (currentShader == shaders[SHADER_COMPLEXSURFACE])
			bufferFlushes += deferredGeometry->build(currentShader);
		else
			deferredGeometry->discard(); //Not a new frame yet.
	}

	if (currentShader && currentShader->getGeometryBuffer()->hasContents()) {
		currentShader->apply();
		if (currentShader->buffersGeometry())
			bufferFlushes++;
	}
}

//Null before any device exists.
DeferredGeometry *D3D::getDeferred() {
	return deferredGeometry;
}


void D3D::switchToShader(int index) {
	if (index != currIndex) {
		if (index < 0)
			currentShader = 0;
		else {
			render();
			currentShader = shaders[index];
			currentShader->bind();
		}
		currIndex = index;
	}
}

Shader *D3D::getShader(int index) {
	return shaders[index];
}

//Each pass reads the last one's output.
void D3D::postprocess() {
	if (deviceLost || !shadersReady())
		return;

	ID3D10ShaderResourceView *texture;
	if (currentShader)
		texture = currentShader->getResourceView();
	else
		texture = shaders[SHADER_GOURAUDPOLYGON] ? shaders[SHADER_GOURAUDPOLYGON]->getResourceView() : nullptr;

	shaders[SHADER_FIRSTPASS]->setFlags(0); //No blending, no depth.
	setViewPort(resX, resY, 0, 0);

	if (options.samples > 1 && texture) {
		Shader_FirstPass *firstPass = static_cast<Shader_FirstPass *>(shaders[SHADER_FIRSTPASS]);
		if (firstPass->resolvesInHardware()) {
			render();
			setRenderTargets(nullptr, nullptr);
			ID3D10Resource *scene = nullptr;
			texture->GetResource(&scene); //AddRef'd
			firstPass->resolveScene(scene);
			SAFE_RELEASE(scene);
		}
	}

	for (int i = D3D::SHADER_FIRSTPASS; i < D3D::SHADER_FINALPASS; i++) {
		switchToShader(i);
		Shader_Postprocess *pps = static_cast<Shader_Postprocess *>(currentShader);
		pps->setInputTexture(texture);
		pps->setViewPort(resX, resY);
		texture = currentShader->getResourceView(); //Input for the next pass.
		D3D::render();
		switchToShader(-1);
	}
}

/** Copies the back buffer onto the clipboard as a DIB, because Print Screen goes through GDI and never
sees the swap chain, and it has to happen before the frame presents while the back buffer still holds
what the player was looking at. */
static void backBufferToClipboard() {
	ID3D10Texture2D *backBuffer = nullptr;
	if (FAILED(D3DObjects.swapChain->GetBuffer(0, __uuidof(ID3D10Texture2D), (LPVOID *)&backBuffer)))
		return;

	D3D10_TEXTURE2D_DESC desc;
	backBuffer->GetDesc(&desc);
	desc.BindFlags = 0;
	desc.MiscFlags = 0;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.CPUAccessFlags = D3D10_CPU_ACCESS_READ;
	desc.Usage = D3D10_USAGE_STAGING;

	ID3D10Texture2D *staging = nullptr;
	if (FAILED(D3DObjects.device->CreateTexture2D(&desc, nullptr, &staging))) {
		SAFE_RELEASE(backBuffer);
		return;
	}
	D3DObjects.device->CopySubresourceRegion(staging, 0, 0, 0, 0, backBuffer, 0, nullptr);

	D3D10_MAPPED_TEXTURE2D mapped;
	if (SUCCEEDED(staging->Map(0, D3D10_MAP_READ, 0, &mapped))) {
		const SIZE_T rowBytes = desc.Width * sizeof(Vec4_byte);
		HGLOBAL handle = GlobalAlloc(GMEM_MOVEABLE, sizeof(BITMAPINFOHEADER) + rowBytes * desc.Height);
		if (handle) {
			BITMAPINFOHEADER *header = (BITMAPINFOHEADER *)GlobalLock(handle);
			if (header) {
				ZeroMemory(header, sizeof(BITMAPINFOHEADER));
				header->biSize = sizeof(BITMAPINFOHEADER);
				header->biWidth = desc.Width;
				header->biHeight = desc.Height; //A DIB is stored bottom-up.
				header->biPlanes = 1;
				header->biBitCount = 32;
				header->biCompression = BI_RGB;
				header->biSizeImage = (DWORD)(rowBytes * desc.Height);

				Vec4_byte *rowSrc = (Vec4_byte *)mapped.pData;
				Vec4_byte *pixels = (Vec4_byte *)(header + 1);
				for (unsigned int row = 0; row < desc.Height; row++) {
					Vec4_byte *rowDst = pixels + (desc.Height - 1 - row) * desc.Width;
					for (unsigned int col = 0; col < desc.Width; col++) {
						rowDst[col].x = rowSrc[col].z;
						rowDst[col].y = rowSrc[col].y;
						rowDst[col].z = rowSrc[col].x;
						rowDst[col].w = 255; //Not meaningful.
					}
					rowSrc += (mapped.RowPitch / sizeof(Vec4_byte));
				}
				GlobalUnlock(handle);

				bool placed = false;
				if (OpenClipboard(hWnd)) {
					EmptyClipboard();
					placed = (SetClipboardData(CF_DIB, handle) != nullptr);
					CloseClipboard();
				}
				if (!placed)
					GlobalFree(handle); //Only passes on success.
			} else {
				GlobalFree(handle);
			}
		}
		staging->Unmap(0);
	}

	SAFE_RELEASE(staging);
	SAFE_RELEASE(backBuffer);
}

static void handlePrintScreen() {
	if (!options.clipboardScreenshots)
		return;

	SHORT state = GetAsyncKeyState(VK_SNAPSHOT);
	bool isDown = (state & 0x8000) != 0;
	bool pressed = ((state & 0x0001) != 0) || (isDown && !wasDown);
	wasDown = isDown;

	if (pressed && GetForegroundWindow() == hWnd)
		backBufferToClipboard();
}

static void handleDeviceLoss(HRESULT presentResult) {
	if (deviceLost)
		return;
	deviceLost = true;

	const HRESULT reason = D3DObjects.device ? D3DObjects.device->GetDeviceRemovedReason() : presentResult;
	switch (reason) {
		case DXGI_ERROR_DEVICE_HUNG:
			UD3D10RenderDevice::debugs("Device lost: the graphics device hung (invalid commands or a driver timeout).");
			break;
		case DXGI_ERROR_DEVICE_RESET:
			UD3D10RenderDevice::debugs("Device lost: the graphics device was reset.");
			break;
		case DXGI_ERROR_DEVICE_REMOVED:
			UD3D10RenderDevice::debugs("Device lost: the graphics device was removed, or its driver was updated.");
			break;
		case DXGI_ERROR_DRIVER_INTERNAL_ERROR:
			UD3D10RenderDevice::debugs("Device lost: internal driver error.");
			break;
		default:
			UD3D10RenderDevice::debugs("Device lost.");
			break;
	}
}

//At frame start, where a rebuild is safe.
bool D3D::isDeviceLost() {
	return deviceLost;
}

//All in by now.
void D3D::present() {
	HRESULT hr;

	if (deviceLost || !D3DObjects.swapChain || !shadersReady())
		return;

	shaders[SHADER_FINALPASS]->setFlags(0);
	ID3D10ShaderResourceView *texture = shaders[D3D::SHADER_FINALPASS - 1]->getResourceView(); //Output of the draw passes
	setViewPort(resX, resY, 0, 0);
	switchToShader(D3D::SHADER_FINALPASS);
	Shader_Postprocess *pps = static_cast<Shader_Postprocess *>(currentShader);
	pps->setInputTexture(texture);
	D3D::render();
	switchToShader(-1);

	handlePrintScreen(); //Before the present.

	hr = D3DObjects.swapChain->Present((options.VSync != 0), 0);
	if (FAILED(hr)) {
		//The recoverable state.
		handleDeviceLoss(hr);
		return;
	}
}

/** Sets up the viewport, the shader's own copy of its size included, and buffered polygons have to be
flushed first or glitches show up, Deus Ex's security cameras being the ones that found this, so the
flush goes ahead unconditionally. */
void D3D::setViewPort(int X, int Y, int left, int top) {
	if (!viewPortCached || X != oldViewX || Y != oldViewY || left != oldViewLeft || top != oldViewTop) {

		render();
		D3D10_VIEWPORT vp;
		vp.Width = X;
		vp.Height = Y;
		vp.MinDepth = 0.0;
		vp.MaxDepth = 1.0;
		vp.TopLeftX = left;
		vp.TopLeftY = top;

		D3DObjects.device->RSSetViewports(1, &vp);
	}
	oldViewLeft = left;
	oldViewTop = top;
	oldViewX = X;
	oldViewY = Y;
	viewPortCached = true;
}


/** \note Capped to 16 for older games. */
TCHAR *D3D::getModes() {
	TCHAR *out;
	const int resStringLength = 14;

	if (!D3DObjects.output) {
		out = new (std::nothrow) TCHAR[1];
		if (out)
			out[0] = 0;
		return out;
	}

	UINT num = 0;
	if (FAILED(D3DObjects.output->GetDisplayModeList(BACKBUFFER_FORMAT, 0, &num, nullptr)))
		num = 0;
	out = new (std::nothrow) TCHAR[num * resStringLength + 1];
	if (!out)
		return nullptr;
	out[0] = 0;

	DXGI_MODE_DESC *descs = new (std::nothrow) DXGI_MODE_DESC[num];
	if (!descs)
		num = 0;
	else if (num > 0 && FAILED(D3DObjects.output->GetDisplayModeList(BACKBUFFER_FORMAT, 0, &num, descs)))
		num = 0;

#if (DEUSEX || UNREALGOLD || NERF || UNREAL_224 || KLINGON)
	const int maxItems = 16;
	int h[maxItems];
	int w[maxItems];
	h[0] = 0;
	w[0] = 0;
	int slot = maxItems - 1;
	for (int i = (int)num; i > 0 && slot >= 0; i--) {
		if (slot == maxItems - 1 || w[slot + 1] != descs[i - 1].Width || h[slot + 1] != descs[i - 1].Height) {
			w[slot] = descs[i - 1].Width;
			h[slot] = descs[i - 1].Height;
#ifdef _DEBUG
			printf("%d\n", w[slot]);
#endif
			slot--;
		}
	}
	for (int i = slot + 1; i < maxItems; i++) {
		TCHAR curr[resStringLength + 1];
		_sntprintf_s(curr, resStringLength + 1, _TRUNCATE, TEXT("%dx%d "), w[i], h[i]);
		_tcscat_s(out, num * resStringLength + 1, curr);
	}
#else
	int height = 0;
	int width = 0;
	for (unsigned int i = 0; i < num; i++) {
		if (width != descs[i].Width || height != descs[i].Height) {
			width = descs[i].Width;
			height = descs[i].Height;
			TCHAR curr[resStringLength + 1];
			_sntprintf_s(curr, resStringLength + 1, _TRUNCATE, TEXT("%dx%d "), width, height);
			_tcscat_s(out, num * resStringLength + 1, curr);
		}
	}
#endif

	size_t outLen = _tcslen(out);
	if (outLen > 0)
		out[outLen - 1] = 0;
	delete[] descs;
	return out;
}

/** \note A failure leaves the buffer unchanged. */
void D3D::getScreenshot(Vec4_byte *buf, int width, int height) {
	if (!buf || width <= 0 || height <= 0 || deviceLost || !D3DObjects.swapChain)
		return;

	ID3D10Texture2D *backBuffer = nullptr;
	if (FAILED(D3DObjects.swapChain->GetBuffer(0, __uuidof(ID3D10Texture2D), (LPVOID *)&backBuffer))) {
		UD3D10RenderDevice::debugs("Error getting the back buffer for a screenshot.");
		return;
	}

	D3D10_TEXTURE2D_DESC desc;
	backBuffer->GetDesc(&desc);
	desc.BindFlags = 0;
	desc.MiscFlags = 0;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;

	ID3D10Texture2D *tstaging = nullptr;
	desc.CPUAccessFlags = D3D10_CPU_ACCESS_READ;
	desc.Usage = D3D10_USAGE_STAGING;
	if (FAILED(D3DObjects.device->CreateTexture2D(&desc, nullptr, &tstaging))) {
		UD3D10RenderDevice::debugs("Error creating a staging texture for a screenshot.");
		SAFE_RELEASE(backBuffer);
		return;
	}

	D3DObjects.device->CopySubresourceRegion(tstaging, 0, 0, 0, 0, backBuffer, 0, nullptr);


	D3D10_MAPPED_TEXTURE2D tempMapped;
	if (FAILED(tstaging->Map(0, D3D10_MAP_READ, 0, &tempMapped))) {
		UD3D10RenderDevice::debugs("Error mapping a screenshot.");
		SAFE_RELEASE(tstaging);
		SAFE_RELEASE(backBuffer);
		return;
	}

	const unsigned int rows = min((unsigned int)height, desc.Height);
	const unsigned int cols = min((unsigned int)width, desc.Width);
	Vec4_byte *rowSrc = (Vec4_byte *)tempMapped.pData;
	Vec4_byte *rowDst = buf;
	for (unsigned int row = 0; row < rows; row++) {
		for (unsigned int col = 0; col < cols; col++) {
			rowDst[col].x = rowSrc[col].z;
			rowDst[col].y = rowSrc[col].y;
			rowDst[col].z = rowSrc[col].x;
			rowDst[col].w = rowSrc[col].w;
		}
		rowSrc += (tempMapped.RowPitch / sizeof(Vec4_byte));
		rowDst += width;
	}

	tstaging->Unmap(0);
	SAFE_RELEASE(backBuffer);
	SAFE_RELEASE(tstaging);
	UD3D10RenderDevice::debugs("Done.");
}

/**
\return 1 on success.
*/
int D3D::findAALevel() {
	HRESULT hr;

	UINT levels = 0;
	int count;
	for (count = options.samples; levels == 0 && count > 0; count--) {
		hr = D3DObjects.device->CheckMultisampleQualityLevels(D3D::getDrawPassFormat(), count, &levels);
		if (FAILED(hr)) {
			UD3D10RenderDevice::debugs("Error getting MSAA support level.");
			return 0;
		}
		if (levels != 0)
			break;
	}
	if (count < 1) //Only safe fallback.
	{
		UD3D10RenderDevice::debugs("No supported anti aliasing level found; anti aliasing disabled.");
		count = 1;
	}
	if (count != options.samples) {
		UD3D10RenderDevice::debugs("Anti aliasing setting decreased; requested setting unsupported.");
		options.samples = count;
	}
	return 1;
}

void D3D::setBrightness(float brightness) {
	if (!shaders[D3D::SHADER_FINALPASS])
		return;
	CLAMP(brightness, 0.1f, 1.0f);
	static_cast<Shader_FinalPass *>(shaders[D3D::SHADER_FINALPASS])->setBrightness(brightness);
}


//Underwater blue, damage red.
void D3D::flash(Vec4 &color) {
	if (!shaders[D3D::SHADER_FINALPASS])
		return;
	static_cast<Shader_FinalPass *>(shaders[D3D::SHADER_FINALPASS])->flash(color);
}
