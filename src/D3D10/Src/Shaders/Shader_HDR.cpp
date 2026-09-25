#include "Shader_HDR.h"

Shader_HDR::Shader_HDR() :
	Shader_Postprocess(),
	sceneTexture(nullptr),
	sourceToLumTechnique(nullptr),
	downscaleLumTechnique(nullptr),
	brightPassTechnique(nullptr),
	blurTechnique(nullptr),
	finalTechnique(nullptr),
	adaptiveLumTechnique(nullptr),
	renderTargetSize(),
	adaptiveLumTexture(0),
	variables() {
	for (int i = 0; i < NUM_TONEMAP_TEXTURES; i++) {
		toneMapRTV[i] = nullptr;
		toneMapSRV[i] = nullptr;
	}
	for (int i = 0; i < NUM_BLOOM_TEXTURES; i++) {
		bloomRTV[i] = nullptr;
		bloomSRV[i] = nullptr;
	}
	for (int i = 0; i < NUM_ADAPTIVE_LUM_TEXTURES; i++) {
		adaptiveLumRTV[i] = nullptr;
		adaptiveLumSRV[i] = nullptr;
	}
	brightSRV = nullptr;
	brightRTV = nullptr;
}


//Whole chain or none.
bool Shader_HDR::compile(const D3D10_SHADER_MACRO *macros, DWORD shaderFlags) {
	if (!Shader_Postprocess::compile(macros, shaderFlags))
		return false;
	if (!Shader_Postprocess::compilePostProcessingShader(L"d3d10drv\\hdr.fx", macros, shaderFlags))
		return false;

	sourceToLumTechnique = effect->GetTechniqueByName("sourceToLum");
	downscaleLumTechnique = effect->GetTechniqueByName("downscaleLum");
	brightPassTechnique = effect->GetTechniqueByName("brightPass");
	blurTechnique = effect->GetTechniqueByName("blur");
	finalTechnique = effect->GetTechniqueByName("finalPass");
	adaptiveLumTechnique = effect->GetTechniqueByName("adaptiveLum");

	variables.luminanceTexture = effect->GetVariableByName("luminanceTex")->AsShaderResource();
	variables.bloomTexture = effect->GetVariableByName("bloomTex")->AsShaderResource();

	//A gap leaves later passes reading empty buffers.
	if (!checkTechnique(sourceToLumTechnique, "sourceToLum") || !checkTechnique(downscaleLumTechnique, "downscaleLum") || !checkTechnique(brightPassTechnique, "brightPass") || !checkTechnique(blurTechnique, "blur") || !checkTechnique(finalTechnique, "finalPass") || !checkTechnique(adaptiveLumTechnique, "adaptiveLum")) {
		return false;
	}
	checkVariable(variables.luminanceTexture, "luminanceTex");
	checkVariable(variables.bloomTexture, "bloomTex");

	return true;
}


//Only used through its views.
static bool createBuffer(ID3D10Device *device, const D3D10_TEXTURE2D_DESC &desc, ID3D10RenderTargetView **rtv, ID3D10ShaderResourceView **srv) {
	ID3D10Texture2D *texture = nullptr;
	if (FAILED(device->CreateTexture2D(&desc, nullptr, &texture)))
		return false;

	const bool created = SUCCEEDED(device->CreateRenderTargetView(texture, nullptr, rtv)) && SUCCEEDED(device->CreateShaderResourceView(texture, nullptr, srv));

	SAFE_RELEASE(texture);

	//Both left null on failure.
	if (!created) {
		SAFE_RELEASE(*rtv);
		SAFE_RELEASE(*srv);
	}

	return created;
}

bool Shader_HDR::createRenderTargetViews(ID3D10RenderTargetView *backbuffer, const DXGI_SWAP_CHAIN_DESC &swapChainDesc, int multiSampleCount) {

	renderTargetSize.x = swapChainDesc.BufferDesc.Width;
	renderTargetSize.y = swapChainDesc.BufferDesc.Height;

	D3D10_TEXTURE2D_DESC desc;
	desc.CPUAccessFlags = 0;
	desc.MiscFlags = 0;
	desc.ArraySize = 1;
	desc.BindFlags = D3D10_BIND_RENDER_TARGET | D3D10_BIND_SHADER_RESOURCE;
	desc.Usage = D3D10_USAGE_DEFAULT;
	desc.MipLevels = 1;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;


	//Luminance buffers are read before they are first written, so they cannot start out as whatever
	//happened to be in memory, and the initial value tone maps neutrally.
	const float initialLuminance[4] = { 0.07f, 0.07f, 0.07f, 0.07f };

	//Tone mapping textures
	int nSampleLen = TONEMAP_BASE_SIZE;
	for (int i = 0; i < Shader_HDR::NUM_TONEMAP_TEXTURES; i++) {

		desc.Format = DXGI_FORMAT_R32_FLOAT;
		desc.Width = nSampleLen;
		desc.Height = nSampleLen;

		//The caller just gives up.
		if (!createBuffer(device, desc, &toneMapRTV[i], &toneMapSRV[i])) {
			Shader_HDR::releaseRenderTargetViews();
			return false;
		}

		device->ClearRenderTargetView(toneMapRTV[i], initialLuminance);

		nSampleLen /= TONEMAP_RESIZE;
	}

	//Adaptive luminance textures
	for (int i = 0; i < Shader_HDR::NUM_ADAPTIVE_LUM_TEXTURES; i++) {

		desc.Format = DXGI_FORMAT_R32_FLOAT;
		desc.Width = 1;
		desc.Height = 1;

		if (!createBuffer(device, desc, &adaptiveLumRTV[i], &adaptiveLumSRV[i])) {
			Shader_HDR::releaseRenderTargetViews();
			return false;
		}

		device->ClearRenderTargetView(adaptiveLumRTV[i], initialLuminance);
	}

	//Zero fails creation.
	desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.Width = max(1u, (UINT)renderTargetSize.x / BLOOM_SCALE);
	desc.Height = max(1u, (UINT)renderTargetSize.y / BLOOM_SCALE);
	if (!createBuffer(device, desc, &brightRTV, &brightSRV)) {
		Shader_HDR::releaseRenderTargetViews();
		return false;
	}

	//Bloom textures
	for (int i = 0; i < Shader_HDR::NUM_BLOOM_TEXTURES; i++) {
		desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.Width = max(1u, swapChainDesc.BufferDesc.Width / BLOOM_SCALE);
		desc.Height = max(1u, swapChainDesc.BufferDesc.Height / BLOOM_SCALE);

		if (!createBuffer(device, desc, &bloomRTV[i], &bloomSRV[i])) {
			Shader_HDR::releaseRenderTargetViews();
			return false;
		}
	}


	//8 bit bands.
	if (!Shader::createRenderTargetViews(D3D::getCompositeFormat(), DXGI_FORMAT_UNKNOWN, 1, 1, 1, swapChainDesc)) {
		Shader_HDR::releaseRenderTargetViews();
		return false;
	}

	return true;
}

//All null-safe.
void Shader_HDR::releaseRenderTargetViews() {
	for (int i = 0; i < Shader_HDR::NUM_TONEMAP_TEXTURES; i++) {
		SAFE_RELEASE(toneMapRTV[i]);
		SAFE_RELEASE(toneMapSRV[i]);
	}

	SAFE_RELEASE(brightRTV);
	SAFE_RELEASE(brightSRV);

	for (int i = 0; i < Shader_HDR::NUM_BLOOM_TEXTURES; i++) {
		SAFE_RELEASE(bloomRTV[i]);
		SAFE_RELEASE(bloomSRV[i]);
	}

	for (int i = 0; i < Shader_HDR::NUM_ADAPTIVE_LUM_TEXTURES; i++) {
		SAFE_RELEASE(adaptiveLumRTV[i]);
		SAFE_RELEASE(adaptiveLumSRV[i]);
	}
	Shader_Postprocess::releaseRenderTargetViews();
}

/**
Runs the tone-mapping chain from 81x81 down to 1x1.
\note Two different "viewport" concepts share a
	name here: the shader constant used for UV
	scaling, and the rasterizer viewport, which
	stays at full resolution throughout. The math
	only holds because the rasterizer viewport is
	never smaller than any render target bound
	below.
*/
void Shader_HDR::apply() {

	int targetSize = TONEMAP_BASE_SIZE;


	//Through the tracked setter.
	D3D::setRenderTargets(toneMapRTV[0], nullptr);
	setViewPort(targetSize, targetSize);
	sourceToLumTechnique->GetPassByIndex(0)->Apply(0);
	geometryBuffer->draw();

	//Downscale luminance
	for (int i = 1; i < NUM_TONEMAP_TEXTURES; i++) {
		targetSize /= TONEMAP_RESIZE;
		D3D::setRenderTargets(toneMapRTV[i], nullptr);
		Shader_Postprocess::setInputTexture(toneMapSRV[i - 1]);
		setViewPort(targetSize, targetSize);
		downscaleLumTechnique->GetPassByIndex(0)->Apply(0);
		geometryBuffer->draw();
	}

	//There can be more than one.
	Shader_Postprocess::setInputTexture(adaptiveLumSRV[adaptiveLumTexture]);
	D3D::setRenderTargets(adaptiveLumRTV[(adaptiveLumTexture + 1) % NUM_ADAPTIVE_LUM_TEXTURES], nullptr);
	variables.luminanceTexture->SetResource(toneMapSRV[NUM_TONEMAP_TEXTURES - 1]);
	adaptiveLumTechnique->GetPassByIndex(0)->Apply(0);
	geometryBuffer->draw();
	adaptiveLumTexture = (adaptiveLumTexture + 1) % NUM_ADAPTIVE_LUM_TEXTURES;

	//Clamped like its texture; zero would divide UV.
	setViewPort(max(1, renderTargetSize.x / BLOOM_SCALE), max(1, renderTargetSize.y / BLOOM_SCALE));
	Shader_Postprocess::setInputTexture(sceneTexture);
	variables.luminanceTexture->SetResource(adaptiveLumSRV[adaptiveLumTexture]);
	D3D::setRenderTargets(brightRTV, nullptr);
	brightPassTechnique->GetPassByIndex(0)->Apply(0);
	geometryBuffer->draw();

	//Bloom pass
	setViewPort(max(1, renderTargetSize.x / BLOOM_SCALE), max(1, renderTargetSize.y / BLOOM_SCALE));
	Shader_Postprocess::setInputTexture(brightSRV);
	D3D::setRenderTargets(bloomRTV[0], nullptr);
	blurTechnique->GetPassByIndex(0)->Apply(0);
	geometryBuffer->draw();
	Shader_Postprocess::setInputTexture(bloomSRV[0]);
	D3D::setRenderTargets(bloomRTV[1], nullptr);
	blurTechnique->GetPassByIndex(1)->Apply(0);
	geometryBuffer->draw();

	//Final pass
	Shader_Postprocess::setViewPort(renderTargetSize.x, renderTargetSize.y);
	D3D::setRenderTargets(renderTargetView, nullptr);
	Shader_Postprocess::setInputTexture(sceneTexture);
	variables.bloomTexture->SetResource(bloomSRV[1]);
	variables.luminanceTexture->SetResource(adaptiveLumSRV[adaptiveLumTexture]);
	finalTechnique->GetPassByIndex(0)->Apply(0);
	geometryBuffer->draw();
}

//Read again later.
void Shader_HDR::setInputTexture(ID3D10ShaderResourceView *texture) {
	sceneTexture = texture;
	Shader_Postprocess::setInputTexture(texture);
}
