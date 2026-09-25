#include "Shader_FirstPass.h"

Shader_FirstPass::Shader_FirstPass() :
	Shader_Postprocess(),
	variables(),
	hardwareResolve(false),
	resolveTarget(nullptr) {
}

bool Shader_FirstPass::compile(const D3D10_SHADER_MACRO *macros, DWORD shaderFlags) {
	if (!Shader_Postprocess::compile(macros, shaderFlags))
		return false;
	if (!Shader_Postprocess::compilePostProcessingShader(L"d3d10drv\\firstpass.fx", macros, shaderFlags))
		return false;

	variables.sceneBuffer = effect->GetVariableByName("sceneBuffer")->AsShaderResource();
	checkVariable(variables.sceneBuffer, "sceneBuffer");

	return true;
}

bool Shader_FirstPass::formatResolvable(DXGI_FORMAT format) {
	UINT support = 0;
	if (FAILED(device->CheckFormatSupport(format, &support)))
		return false;
	return (support & D3D10_FORMAT_SUPPORT_MULTISAMPLE_RESOLVE) != 0;
}

bool Shader_FirstPass::createRenderTargetViews(ID3D10RenderTargetView *backbuffer, const DXGI_SWAP_CHAIN_DESC &swapChainDesc, int multiSampleCount) {
	if (!Shader::createRenderTargetViews(D3D::getDrawPassFormat(), DXGI_FORMAT_UNKNOWN, 1, 1, 1, swapChainDesc))
		return false;

	hardwareResolve = multiSampleCount > 1 && formatResolvable(D3D::getDrawPassFormat());
	if (hardwareResolve)
		renderTargetView->GetResource(&resolveTarget);

	return true;
}

void Shader_FirstPass::releaseRenderTargetViews() {
	SAFE_RELEASE(resolveTarget);
	hardwareResolve = false;
	Shader_Postprocess::releaseRenderTargetViews();
}

void Shader_FirstPass::setInputTexture(ID3D10ShaderResourceView *texture) {
	variables.sceneBuffer->SetResource(texture);
}

void Shader_FirstPass::apply() {
	if (hardwareResolve)
		return;
	Shader_Postprocess::apply();
}

void Shader_FirstPass::resolveScene(ID3D10Resource *scene) const {
	if (!hardwareResolve || !scene || !resolveTarget)
		return;
	device->ResolveSubresource(resolveTarget, 0, scene, 0, D3D::getDrawPassFormat());
}
