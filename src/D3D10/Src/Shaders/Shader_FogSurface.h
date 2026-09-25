#pragma once

#include "Shader_Unreal.h"

class Shader_FogSurface : public Shader_Unreal {
private:
public:
	Shader_FogSurface();
	bool compile(const D3D10_SHADER_MACRO *macros, DWORD shaderFlags) override;
};
