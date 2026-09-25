#pragma once

#include "Shader_Postprocess.h"
#include "../Texture/TextureCache.h"

/** Resolves the multisampled scene buffer. On a hardware resolve, this pass owns the destination. */
class Shader_FirstPass : public Shader_Postprocess {
private:
	struct
	{
		ID3D10EffectShaderResourceVariable *sceneBuffer;
	} variables;

	/** Set, this pass draws nothing. */
	bool hardwareResolve;
	/** Behind the view. */
	ID3D10Resource *resolveTarget;

	static bool formatResolvable(DXGI_FORMAT format);

public:
	bool compile(const D3D10_SHADER_MACRO *macros, DWORD shaderFlags) override;
	Shader_FirstPass();
	bool createRenderTargetViews(ID3D10RenderTargetView *backbuffer, const DXGI_SWAP_CHAIN_DESC &swapChainDesc, int multiSampleCount) override;
	void releaseRenderTargetViews() override;
	void setInputTexture(ID3D10ShaderResourceView *texture) override;
	void apply() override;

	bool resolvesInHardware() const { return hardwareResolve; }
	/**
	\note Hardware resolve only, no render target bound.
	\param scene The multisampled texture.
	*/
	void resolveScene(ID3D10Resource *scene) const;
};
