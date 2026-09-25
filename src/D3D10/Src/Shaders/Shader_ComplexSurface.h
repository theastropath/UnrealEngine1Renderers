#pragma once

#include "Shader_Unreal.h"
#include "../Texture/TextureCache.h"

class Shader_ComplexSurface : public Shader_Unreal {
private:
	enum Pass { PASS_STANDARD,
		PASS_MOVER_BIAS };

	bool useMoverBias;

	struct
	{
		ID3D10EffectShaderResourceVariable *textures;
	} variables;
	ID3D10BlendState *bstate_Translucent_ComplexSurface;
	static const int numPasses = TextureCache::DUMMY_NUM_TEXTURE_PASSES - 1; //-1 because diffuse is always enabled
	//Must fit the vertex field.
	static_assert(numPasses <= 32, "texturePassMask cannot hold a bit per texture pass");
	/** Secondary passes in use, per vertex. */
	DWORD texturePassMask;

protected:
	ID3D10BlendState *translucentBlendState() const override;

public:
	Shader_ComplexSurface();
	~Shader_ComplexSurface();
	bool compile(const D3D10_SHADER_MACRO *macros, DWORD shaderFlags) override;
	void switchPass(TextureCache::TexturePass pass, BOOL val);
	void setMoverBias(bool moverBias);
	void apply() override;
	void setTexture(int pass, ID3D10ShaderResourceView *texture) const override;
	DWORD getTexturePassMask() const { return texturePassMask; }
};
