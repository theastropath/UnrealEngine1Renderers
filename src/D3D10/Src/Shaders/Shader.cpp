/**
*/

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include "Shader.h"
#include "PolyFlags.h"

Shader::StateStruct Shader::states;
ID3D10Device *Shader::device;

/**
Blend and depth state last pushed to the device, null meaning unknown, and cached as state objects here
because which object a given set of flags maps to depends on the shader doing the asking as much as it
does on the flags themselves.
*/
static ID3D10BlendState *currBlendState = nullptr;
static ID3D10DepthStencilState *currDepthState = nullptr;

void Shader::saveStatics(Statics &out) {
	out.states = states;
	out.device = device;
	out.currBlendState = currBlendState;
	out.currDepthState = currDepthState;
}

void Shader::loadStatics(const Statics &in) {
	states = in.states;
	device = in.device;
	currBlendState = in.currBlendState;
	currDepthState = in.currDepthState;
}

/**
The HLSL compiler, looked up by name at run time so that a machine carrying only the older one still
works, the effect pool interface it replaces being missing altogether under Wine and Proton, where
these games are increasingly played.
*/
typedef HRESULT(WINAPI *D3DCompileFunc)(LPCVOID data, SIZE_T dataSize, LPCSTR sourceName,
	const D3D10_SHADER_MACRO *defines, ID3D10Include *include, LPCSTR entryPoint, LPCSTR target,
	UINT flags1, UINT flags2, ID3D10Blob **code, ID3D10Blob **errors);

//Once, on first use.
static HMODULE compilerLibrary = nullptr;
static D3DCompileFunc d3dCompile = nullptr;

/** Deliberately never unloaded. */
static bool loadCompiler() {
	if (d3dCompile)
		return true;

	static const wchar_t *const libraries[] = { L"d3dcompiler_47.dll", L"d3dcompiler_43.dll" };
	for (size_t i = 0; i < ARRAYSIZE(libraries); i++) {
		HMODULE library = LoadLibraryW(libraries[i]);
		if (!library)
			continue;

		D3DCompileFunc entryPoint = (D3DCompileFunc)GetProcAddress(library, "D3DCompile");
		if (entryPoint) {
			compilerLibrary = library;
			d3dCompile = entryPoint;
			return true;
		}
		FreeLibrary(library);
	}

	UD3D10RenderDevice::debugs("The HLSL compiler could not be loaded; neither d3dcompiler_47.dll nor d3dcompiler_43.dll is available.");
	return false;
}

/** Freed by the caller. */
static bool readShaderFile(const wchar_t *fileName, void **contents, DWORD *size) {
	HANDLE file = CreateFileW(fileName, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (file == INVALID_HANDLE_VALUE)
		return false;

	bool read = false;
	const DWORD fileSize = GetFileSize(file, nullptr);
	if (fileSize != INVALID_FILE_SIZE) {
		void *buffer = malloc(fileSize ? fileSize : 1); //A zero sized allocation may return null
		if (buffer) {
			DWORD bytesRead = 0;
			read = ReadFile(file, buffer, fileSize, &bytesRead, nullptr) != FALSE && bytesRead == fileSize;
			if (read) {
				*contents = buffer;
				*size = fileSize;
			} else
				free(buffer);
		}
	}
	CloseHandle(file);
	return read;
}

static void reportEffectError(const wchar_t *fileName, const char *problem) {
	char message[MAX_PATH + 128];
	_snprintf_s(message, ARRAYSIZE(message), _TRUNCATE, "Effect file \"%S\" %s.", fileName, problem);
	UD3D10RenderDevice::debugs(message);
}

class EffectInclude : public ID3D10Include {
public:
	explicit EffectInclude(const wchar_t *directory) :
		directory(directory) {}

	HRESULT STDMETHODCALLTYPE Open(D3D10_INCLUDE_TYPE type, LPCSTR fileName, LPCVOID parentData, LPCVOID *contents, UINT *size) override {
		if (!fileName || !contents || !size)
			return E_INVALIDARG;

		wchar_t wideName[MAX_PATH];
		if (MultiByteToWideChar(CP_ACP, 0, fileName, -1, wideName, ARRAYSIZE(wideName)) == 0)
			return E_FAIL;

		wchar_t path[MAX_PATH];
		if (_snwprintf_s(path, ARRAYSIZE(path), _TRUNCATE, L"%s%s", directory, wideName) < 0)
			return E_FAIL;

		void *buffer;
		DWORD bytes;
		if (!readShaderFile(path, &buffer, &bytes))
			return E_FAIL;

		*contents = buffer;
		*size = bytes;
		return S_OK;
	}

	HRESULT STDMETHODCALLTYPE Close(LPCVOID contents) override {
		free(const_cast<void *>(contents));
		return S_OK;
	}

private:
	const wchar_t *directory;
};

/** \return The effect, released by the caller, or null. */
static ID3D10Blob *compileEffectFile(const wchar_t *fileName, const D3D10_SHADER_MACRO *macros, DWORD shaderFlags, UINT effectFlags) {
	if (!loadCompiler())
		return nullptr;

	void *source;
	DWORD sourceSize;
	if (!readShaderFile(fileName, &source, &sourceSize)) {
		reportEffectError(fileName, "could not be opened");
		UD3D10RenderDevice::debugs("The renderer's .fx and .fxh files belong in a \"d3d10drv\" directory inside the game's \"system\" directory.");
		return nullptr;
	}

	wchar_t directory[MAX_PATH];
	wcsncpy_s(directory, ARRAYSIZE(directory), fileName, _TRUNCATE);
	wchar_t *separator = wcsrchr(directory, L'\\');
	if (separator)
		separator[1] = L'\0';
	else
		directory[0] = L'\0';
	EffectInclude include(directory);

	char sourceName[MAX_PATH];
	if (WideCharToMultiByte(CP_ACP, 0, fileName, -1, sourceName, ARRAYSIZE(sourceName), nullptr, nullptr) == 0)
		sourceName[0] = '\0';

	ID3D10Blob *code = nullptr;
	ID3D10Blob *errors = nullptr;
	const HRESULT hr = d3dCompile(source, sourceSize, sourceName, macros, &include, "main", "fx_4_0", shaderFlags, effectFlags, &code, &errors);
	free(source);

	if (errors) //A successful compile can still produce warnings
	{
		UD3D10RenderDevice::debugs((const char *)errors->GetBufferPointer());
		errors->Release();
	}

	if (FAILED(hr)) {
		SAFE_RELEASE(code);
		reportEffectError(fileName, "could not be compiled");
		return nullptr;
	}
	return code;
}

/** \param pool Null for a standalone effect. */
bool Shader::compileEffect(const wchar_t *fileName, const D3D10_SHADER_MACRO *macros, DWORD shaderFlags, UINT effectFlags, ID3D10EffectPool *pool, ID3D10Effect **out) {
	ID3D10Blob *code = compileEffectFile(fileName, macros, shaderFlags, effectFlags);
	if (!code)
		return false;

	const HRESULT hr = D3D10CreateEffectFromMemory(code->GetBufferPointer(), code->GetBufferSize(), effectFlags, device, pool, out);
	code->Release();
	if (FAILED(hr)) {
		reportEffectError(fileName, "compiled but could not be loaded as an effect");
		return false;
	}
	return true;
}

bool Shader::compileEffectPool(const wchar_t *fileName, const D3D10_SHADER_MACRO *macros, DWORD shaderFlags, ID3D10EffectPool **out) {
	ID3D10Blob *code = compileEffectFile(fileName, macros, shaderFlags, 0);
	if (!code)
		return false;

	const HRESULT hr = D3D10CreateEffectPoolFromMemory(code->GetBufferPointer(), code->GetBufferSize(), 0, device, out);
	code->Release();
	if (FAILED(hr)) {
		reportEffectError(fileName, "compiled but could not be loaded as an effect pool");
		return false;
	}
	return true;
}

//compile() may still fail.
Shader::Shader() :
	geometryBuffer(nullptr),
	renderTargetView(nullptr),
	depthStencilView(nullptr),
	vertexLayout(nullptr),
	shaderResourceView(nullptr),
	effect(nullptr),
	topology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST),
	techniqueIndex(0) {
}

Shader::~Shader() {
	delete geometryBuffer;
	SAFE_RELEASE(effect);
	SAFE_RELEASE(vertexLayout);
}

//Static; the first destroyed would take them.
void Shader::releaseSharedResources() {
	SAFE_RELEASE(states.dstate_Enable);
	SAFE_RELEASE(states.dstate_Disable);
	SAFE_RELEASE(states.bstate_NoBlend);
	SAFE_RELEASE(states.bstate_Translucent);
	SAFE_RELEASE(states.bstate_Modulate);
	SAFE_RELEASE(states.bstate_Alpha);
	SAFE_RELEASE(states.bstate_Masked);
	SAFE_RELEASE(states.bstate_Invis);
	SAFE_RELEASE(states.bstate_Highlight);

}

/**
\return false if the effect has no such state.
\note Fatal, unlike a missing variable, since the device reads a null blend state as a request for its
	own default and would go on drawing everything unblended without a word of it, which is a good deal
	harder to spot than a missing texture.
*/
static bool getBlendState(ID3D10Effect *effect, const char *name, ID3D10BlendState **out) {
	ID3D10EffectBlendVariable *variable = effect->GetVariableByName(name)->AsBlend();
	if (!variable->IsValid() || FAILED(variable->GetBlendState(0, out))) {
		UD3D10RenderDevice::debugs("states.fxh does not define a blend state it should:");
		UD3D10RenderDevice::debugs(name);
		return false;
	}
	return true;
}

static bool getDepthStencilState(ID3D10Effect *effect, const char *name, ID3D10DepthStencilState **out) {
	ID3D10EffectDepthStencilVariable *variable = effect->GetVariableByName(name)->AsDepthStencil();
	if (!variable->IsValid() || FAILED(variable->GetDepthStencilState(0, out))) {
		UD3D10RenderDevice::debugs("states.fxh does not define a depth stencil state it should:");
		UD3D10RenderDevice::debugs(name);
		return false;
	}
	return true;
}

/** Global shader initialization */
bool Shader::initShaderSystem(ID3D10Device *device, const D3D10_SHADER_MACRO *macros, DWORD shaderFlags) {
	Shader::device = device;

	ID3D10Effect *tempEffect;
	if (!compileEffect(L"d3d10drv\\states.fxh", macros, shaderFlags, 0, nullptr, &tempEffect))
		return false;

	const bool haveStates =
		getDepthStencilState(tempEffect, "dstate_Enable", &states.dstate_Enable) && getDepthStencilState(tempEffect, "dstate_Disable", &states.dstate_Disable) && getBlendState(tempEffect, "bstate_Translucent", &states.bstate_Translucent) && getBlendState(tempEffect, "bstate_Modulate", &states.bstate_Modulate) && getBlendState(tempEffect, "bstate_NoBlend", &states.bstate_NoBlend) && getBlendState(tempEffect, "bstate_Masked", &states.bstate_Masked) && getBlendState(tempEffect, "bstate_Alpha", &states.bstate_Alpha) && getBlendState(tempEffect, "bstate_Invis", &states.bstate_Invis) && getBlendState(tempEffect, "bstate_Highlight", &states.bstate_Highlight);
	SAFE_RELEASE(tempEffect);

	if (haveStates)
		aliasIdenticalStates();

	return haveStates;
}

/**
Point the masked blend state at the unblended one when the two describe the same state, so that a
masked to solid transition does not break a batch, which matters because levels alternate between the
two constantly and a state flush on every change costs a good deal more than the comparison here.
*/
void Shader::aliasIdenticalStates() {
	if (states.bstate_Masked == states.bstate_NoBlend)
		return;

	D3D10_BLEND_DESC maskedDesc, noBlendDesc;
	states.bstate_Masked->GetDesc(&maskedDesc);
	states.bstate_NoBlend->GetDesc(&noBlendDesc);
	if (memcmp(&maskedDesc, &noBlendDesc, sizeof(D3D10_BLEND_DESC)) != 0)
		return;

	//Both get released.
	states.bstate_Masked->Release();
	states.bstate_NoBlend->AddRef();
	states.bstate_Masked = states.bstate_NoBlend;

	UD3D10RenderDevice::debugs("Masked and unblended geometry share one blend state; they will not break batches between them.");
}

bool Shader::checkVariable(ID3D10EffectVariable *variable, const char *name) {
	if (variable && variable->IsValid())
		return true;

	UD3D10RenderDevice::debugs("Effect variable missing or of an unexpected type:");
	UD3D10RenderDevice::debugs(name);
	return false;
}

bool Shader::checkTechnique(ID3D10EffectTechnique *technique, const char *name) {
	if (technique && technique->IsValid())
		return true;

	UD3D10RenderDevice::debugs("Effect technique missing:");
	UD3D10RenderDevice::debugs(name);
	return false;
}

/**
Create render target, depth and resource views.
\param samples Multisample count, which need not match the game's.
\note A null format skips that buffer and view.
\note Every failure path unwinds the three views created here.
*/
bool Shader::createRenderTargetViews(DXGI_FORMAT format, DXGI_FORMAT depthFormat, float scaleX, float scaleY, int samples, const DXGI_SWAP_CHAIN_DESC &swapChainDesc) {

	ID3D10Texture2D *tex = nullptr;
	if (format != DXGI_FORMAT_UNKNOWN) {

		D3D10_TEXTURE2D_DESC texDesc;
		texDesc.ArraySize = 1;
		texDesc.BindFlags = D3D10_BIND_RENDER_TARGET | D3D10_BIND_SHADER_RESOURCE;
		texDesc.CPUAccessFlags = 0;
		texDesc.Format = format;
		//Zero fails creation.
		texDesc.Height = max(1u, (UINT)(swapChainDesc.BufferDesc.Height * scaleY));
		texDesc.Width = max(1u, (UINT)(swapChainDesc.BufferDesc.Width * scaleX));
		texDesc.MipLevels = 1;
		texDesc.MiscFlags = 0;
		texDesc.SampleDesc.Count = samples;
		texDesc.SampleDesc.Quality = 0;
		texDesc.Usage = D3D10_USAGE_DEFAULT;

		if (FAILED(device->CreateTexture2D(&texDesc, 0, &tex))) {
			UD3D10RenderDevice::debugs("Error creating buffer texture.");
			Shader::releaseRenderTargetViews();
			return 0;
		}

		const HRESULT hr = device->CreateRenderTargetView(tex, nullptr, &renderTargetView);
		if (FAILED(hr)) {
			UD3D10RenderDevice::debugs("Error creating render target view.");
			SAFE_RELEASE(tex);
			Shader::releaseRenderTargetViews();
			return 0;
		}

		if (FAILED(device->CreateShaderResourceView(tex, nullptr, &shaderResourceView))) {
			UD3D10RenderDevice::debugs("Error creating shader resource view.");
			SAFE_RELEASE(tex);
			Shader::releaseRenderTargetViews();
			return 0;
		}

		SAFE_RELEASE(tex);
	}

	if (depthFormat != DXGI_FORMAT_UNKNOWN) {
		D3D10_TEXTURE2D_DESC descDepth;

		ID3D10Texture2D *depthTexInternal = nullptr;
		descDepth.Width = max(1u, (UINT)(swapChainDesc.BufferDesc.Width * scaleX));
		descDepth.Height = max(1u, (UINT)(swapChainDesc.BufferDesc.Height * scaleY));
		descDepth.MipLevels = 1;
		descDepth.ArraySize = 1;
		descDepth.Format = depthFormat;
		descDepth.SampleDesc.Count = samples;
		descDepth.SampleDesc.Quality = 0;
		descDepth.Usage = D3D10_USAGE_DEFAULT;
		descDepth.BindFlags = D3D10_BIND_DEPTH_STENCIL;
		descDepth.CPUAccessFlags = 0;
		descDepth.MiscFlags = 0;
		if (FAILED(device->CreateTexture2D(&descDepth, nullptr, &depthTexInternal))) {
			UD3D10RenderDevice::debugs("Depth texture creation failed.");
			Shader::releaseRenderTargetViews();
			return 0;
		}

		if (FAILED(device->CreateDepthStencilView(depthTexInternal, nullptr, &depthStencilView))) {
			UD3D10RenderDevice::debugs("Error creating render target view (depth).");
			SAFE_RELEASE(depthTexInternal);
			Shader::releaseRenderTargetViews();
			return 0;
		}
		SAFE_RELEASE(depthTexInternal);
	}
	return 1;
}

//Resize and shutdown.
void Shader::releaseRenderTargetViews() {
	SAFE_RELEASE(renderTargetView);
	SAFE_RELEASE(depthStencilView);
	SAFE_RELEASE(shaderResourceView);
}


void Shader::bind() {
	if (geometryBuffer == nullptr)
		return;

	geometryBuffer->bind();
	device->IASetPrimitiveTopology(topology);
	device->IASetInputLayout(vertexLayout);
	Shader::setFlags(0);
	ID3D10RenderTargetView *target = renderTargetView ? renderTargetView : D3D::getRenderTarget();
	D3D::setRenderTargets(target, depthStencilView);
}

/** Draws its buffer. */
void Shader::apply() {
	if (effect == nullptr || geometryBuffer == nullptr)
		return;

	effect->GetTechniqueByIndex(techniqueIndex)->GetPassByIndex(0)->Apply(0);
	geometryBuffer->draw();
}

//Per family. See Shader_Unreal.
GeometryBuffer *Shader::getGeometryBuffer() const {
	return geometryBuffer;
}

ID3D10ShaderResourceView *Shader::getResourceView() const {
	return shaderResourceView;
}

/** Handle flags that change depth or blend state.
\param flags Unreal polyflags.
\note Bottleneck. Only flush on a real difference.
**/
void Shader::setFlags(DWORD flags) {
	if (!(flags & (PF_Translucent | PF_Modulated))) //Neither flag means occlude.
	{
		flags |= PF_Occlude;
	}

	//Original precedence.
	ID3D10BlendState *blendState;
	if (flags & PF_Invisible) {
		blendState = states.bstate_Invis;
	} else if (flags & PF_Translucent) {
		blendState = translucentBlendState();
	} else if (flags & PF_Modulated) {
		blendState = states.bstate_Modulate;
	} else if (flags & PF_Highlighted) {
		blendState = states.bstate_Highlight;
	} else if (flags & PF_AlphaBlend) {
		blendState = states.bstate_Alpha;
	} else if (flags & PF_Masked) {
		blendState = states.bstate_Masked;
	} else {
		blendState = states.bstate_NoBlend;
	}

	ID3D10DepthStencilState *depthState = (flags & PF_Occlude) ? states.dstate_Enable : states.dstate_Disable;

	const bool blendChanged = blendState != currBlendState;
	const bool depthChanged = depthState != currDepthState;
	if (!blendChanged && !depthChanged)
		return;

	D3D::render(); //Under the outgoing state.

	if (blendChanged) {
		device->OMSetBlendState(blendState, nullptr, 0xffffffff);
		currBlendState = blendState;
	}
	if (depthChanged) {
		device->OMSetDepthStencilState(depthState, 1);
		currDepthState = depthState;
	}
}

//Complex surfaces override this.
ID3D10BlendState *Shader::translucentBlendState() const {
	return states.bstate_Translucent;
}

/** Only such a buffer empties early. */
bool Shader::buffersGeometry() const {
	return false;
}

void Shader::invalidateCachedState() {
	currBlendState = nullptr;
	currDepthState = nullptr;
}
