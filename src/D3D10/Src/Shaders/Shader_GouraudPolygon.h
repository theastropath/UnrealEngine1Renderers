#pragma once

#include "Shader_Unreal.h"

class Shader_GouraudPolygon : public Shader_Unreal {
private:
	enum Pass { PASS_STANDARD,
		PASS_DECAL_BIAS };

	bool useDecalBias;

public:
	struct
	{
		ID3D10EffectVectorVariable *fogColor;
		ID3D10EffectScalarVariable *fogDist;
	} variables;

	Shader_GouraudPolygon();
	bool compile(const D3D10_SHADER_MACRO *macros, DWORD shaderFlags) override;
	void setFlags(DWORD flags) override;
	void apply() override;
	void fog(float dist, Vec4 *color) const;
};
