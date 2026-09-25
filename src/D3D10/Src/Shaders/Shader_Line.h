#pragma once

#include "Shader_Unreal.h"

class Shader_Line : public Shader_Unreal {
private:
	bool instanced;

public:
	Shader_Line(bool instanced);
	bool compile(const D3D10_SHADER_MACRO *macros, DWORD shaderFlags) override;
	void bind() override;
	void apply() override;
};
