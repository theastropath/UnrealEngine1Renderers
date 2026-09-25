#pragma once
#include "Shader_Postprocess.h"


class Shader_Dummy : public Shader_Postprocess {
private:
public:
	Shader_Dummy();
	bool compile(const D3D10_SHADER_MACRO *macros, DWORD shaderFlags) override;
	bool createRenderTargetViews(ID3D10RenderTargetView *backbuffer, const DXGI_SWAP_CHAIN_DESC &swapChainDesc, int multiSampleCount) override;
	void releaseRenderTargetViews() override;
	void setShaderResourceView(ID3D10ShaderResourceView *view);
	void apply() override;
	void bind() override;
};
