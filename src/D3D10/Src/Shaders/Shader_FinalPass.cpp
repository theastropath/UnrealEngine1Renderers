
#include "Shader_FinalPass.h"

Shader_FinalPass::Shader_FinalPass() :
	Shader_Postprocess(),
	variables() {
}

bool Shader_FinalPass::compile(const D3D10_SHADER_MACRO *macros, DWORD shaderFlags) {
	if (!Shader_Postprocess::compile(macros, shaderFlags))
		return false;
	if (!Shader_Postprocess::compilePostProcessingShader(L"d3d10drv\\finalpass.fx", macros, shaderFlags))
		return false;

	variables.brightness = effect->GetVariableByName("brightness")->AsScalar();
	variables.flash = effect->GetVariableByName("flash")->AsVector();
	checkVariable(variables.brightness, "brightness");
	checkVariable(variables.flash, "flash");

	return true;
}

bool Shader_FinalPass::createRenderTargetViews(ID3D10RenderTargetView *backbuffer, const DXGI_SWAP_CHAIN_DESC &swapChainDesc, int multiSampleCount) {
	renderTargetView = backbuffer;
	return 1;
}

void Shader_FinalPass::releaseRenderTargetViews() {
	//Owned elsewhere.
	renderTargetView = nullptr;
}

void Shader_FinalPass::setBrightness(float brightness) const {
	variables.brightness->SetFloat(brightness);
}

void Shader_FinalPass::flash(Vec4 &color) const {
	variables.flash->SetFloatVector((float *)&color);
}
