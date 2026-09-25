#pragma once

#include "Shader_Postprocess.h"
#include "../Texture/TextureCache.h"

class Shader_FinalPass : public Shader_Postprocess {
private:
	struct
	{
		ID3D10EffectScalarVariable *brightness;
		ID3D10EffectVectorVariable *flash;
	} variables;


public:
	Shader_FinalPass();
	bool compile(const D3D10_SHADER_MACRO *macros, DWORD shaderFlags) override;
	bool createRenderTargetViews(ID3D10RenderTargetView *backbuffer, const DXGI_SWAP_CHAIN_DESC &swapChainDesc, int multiSampleCount) override;
	void releaseRenderTargetViews() override;
	void setBrightness(float brightness) const;
	void flash(Vec4 &color) const;
};
