#include "Shader_ComplexSurface.h"

Shader_ComplexSurface::Shader_ComplexSurface() :
	Shader_Unreal(),
	useMoverBias(false),
	variables(),
	bstate_Translucent_ComplexSurface(nullptr),
	texturePassMask(0) {
}

Shader_ComplexSurface::~Shader_ComplexSurface() {
	SAFE_RELEASE(bstate_Translucent_ComplexSurface);
}


bool Shader_ComplexSurface::compile(const D3D10_SHADER_MACRO *macros, DWORD shaderFlags) {
	if (!Shader_Unreal::compile(macros, shaderFlags))
		return false;
	D3D10_INPUT_ELEMENT_DESC layoutDesc[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D10_APPEND_ALIGNED_ELEMENT, D3D10_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D10_APPEND_ALIGNED_ELEMENT, D3D10_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 1, DXGI_FORMAT_R32G32_FLOAT, 0, D3D10_APPEND_ALIGNED_ELEMENT, D3D10_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 2, DXGI_FORMAT_R32G32_FLOAT, 0, D3D10_APPEND_ALIGNED_ELEMENT, D3D10_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 3, DXGI_FORMAT_R32G32_FLOAT, 0, D3D10_APPEND_ALIGNED_ELEMENT, D3D10_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 4, DXGI_FORMAT_R32G32_FLOAT, 0, D3D10_APPEND_ALIGNED_ELEMENT, D3D10_INPUT_PER_VERTEX_DATA, 0 },
		{ "BLENDINDICES", 0, DXGI_FORMAT_R32_UINT, 0, D3D10_APPEND_ALIGNED_ELEMENT, D3D10_INPUT_PER_VERTEX_DATA, 0 },
		{ "BLENDINDICES", 1, DXGI_FORMAT_R32_UINT, 0, D3D10_APPEND_ALIGNED_ELEMENT, D3D10_INPUT_PER_VERTEX_DATA, 0 },
	};

	if (!Shader_Unreal::compileUnrealShader(L"d3d10drv\\complexsurface.fx", macros, shaderFlags, layoutDesc, sizeof(layoutDesc) / sizeof(layoutDesc[0])))
		return false;

	variables.textures = effect->GetVariableByName("textures")->AsShaderResource();
	checkVariable(variables.textures, "textures");

	ID3D10EffectBlendVariable *blendVariable = effect->GetVariableByName("bstate_Translucent_ComplexSurface")->AsBlend();
	if (!checkVariable(blendVariable, "bstate_Translucent_ComplexSurface") || FAILED(blendVariable->GetBlendState(0, &bstate_Translucent_ComplexSurface))) {
		bstate_Translucent_ComplexSurface = nullptr;
	}

	return true;
}

void Shader_ComplexSurface::switchPass(TextureCache::TexturePass pass, BOOL val) {
	//Diffuse is index zero; shifting by it is undefined.
	if (pass == TextureCache::PASS_DIFFUSE)
		return;

	if (val)
		texturePassMask |= (1u << (pass - 1));
	else
		texturePassMask &= ~(1u << (pass - 1));
}

void Shader_ComplexSurface::setMoverBias(bool moverBias) {
	if (moverBias != useMoverBias) {
		D3D::render();
		D3D::countMoverBiasSwitch();
		useMoverBias = moverBias;
	}
}

void Shader_ComplexSurface::apply() {
	effect->GetTechniqueByIndex(0)->GetPassByIndex(useMoverBias ? PASS_MOVER_BIAS : PASS_STANDARD)->Apply(0);
	geometryBuffer->draw();
}

void Shader_ComplexSurface::setTexture(int pass, ID3D10ShaderResourceView *texture) const {
	if (pass == 0)
		Shader_Unreal::setTexture(pass, texture);
	else
		variables.textures->SetResourceArray(&texture, pass - 1, 1);
}

ID3D10BlendState *Shader_ComplexSurface::translucentBlendState() const {
	if (bstate_Translucent_ComplexSurface)
		return bstate_Translucent_ComplexSurface;
	return Shader_Unreal::translucentBlendState();
}
