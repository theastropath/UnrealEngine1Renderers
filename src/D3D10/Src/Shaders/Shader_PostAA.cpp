
#include "Shader_PostAA.h"

bool Shader_PostAA::compile(const D3D10_SHADER_MACRO *macros, DWORD shaderFlags) {
	if (!Shader_Postprocess::compile(macros, shaderFlags))
		return false;
	return Shader_Postprocess::compilePostProcessingShader(L"d3d10drv\\postaa.fx", macros, shaderFlags);
}

bool Shader_PostAA::createRenderTargetViews(ID3D10RenderTargetView *backbuffer, const DXGI_SWAP_CHAIN_DESC &swapChainDesc, int multiSampleCount) {
	//As the input format.
	return Shader::createRenderTargetViews(D3D::getCompositeFormat(), DXGI_FORMAT_UNKNOWN, 1, 1, 1, swapChainDesc);
}
