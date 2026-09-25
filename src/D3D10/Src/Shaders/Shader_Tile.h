#pragma once

#include "Shader_Unreal.h"

class Shader_Tile : public Shader_Unreal {
private:
	bool instanced;

public:
	Shader_Tile(bool instanced);
	bool compile(const D3D10_SHADER_MACRO *macros, DWORD shaderFlags) override;
	void bind() override;
	void apply() override;
};
