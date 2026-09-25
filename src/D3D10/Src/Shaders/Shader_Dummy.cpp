#include "Shader_Dummy.h"

Shader_Dummy::Shader_Dummy() :
	Shader_Postprocess() {
}

bool Shader_Dummy::compile(const D3D10_SHADER_MACRO *macros, DWORD shaderFlags) {
	return Shader_Postprocess::compile(macros, shaderFlags);
}

bool Shader_Dummy::createRenderTargetViews(ID3D10RenderTargetView *backbuffer, const DXGI_SWAP_CHAIN_DESC &swapChainDesc, int multiSampleCount) {
	return true;
}

void Shader_Dummy::releaseRenderTargetViews() {
	//Borrowed from the preceding pass.
	shaderResourceView = nullptr;
}

void Shader_Dummy::setShaderResourceView(ID3D10ShaderResourceView *view) {
	shaderResourceView = view;
}

void Shader_Dummy::bind() {
}

void Shader_Dummy::apply() {
}
