#pragma once

#include "Shader_Postprocess.h"

class Shader_PostAA : public Shader_Postprocess {
public:
	bool compile(const D3D10_SHADER_MACRO *macros, DWORD shaderFlags) override;
	bool createRenderTargetViews(ID3D10RenderTargetView *backbuffer, const DXGI_SWAP_CHAIN_DESC &swapChainDesc, int multiSampleCount) override;
};
