#include "Shader_GouraudPolygon.h"
#include "PolyFlags.h"

Shader_GouraudPolygon::Shader_GouraudPolygon() :
	Shader_Unreal(),
	useDecalBias(false) {
}

bool Shader_GouraudPolygon::compile(const D3D10_SHADER_MACRO *macros, DWORD shaderFlags) {
	if (!Shader_Unreal::compile(macros, shaderFlags))
		return false;
	D3D10_INPUT_ELEMENT_DESC layoutDesc[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D10_APPEND_ALIGNED_ELEMENT, D3D10_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D10_APPEND_ALIGNED_ELEMENT, D3D10_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D10_APPEND_ALIGNED_ELEMENT, D3D10_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D10_APPEND_ALIGNED_ELEMENT, D3D10_INPUT_PER_VERTEX_DATA, 0 },
		{ "BLENDINDICES", 0, DXGI_FORMAT_R32_UINT, 0, D3D10_APPEND_ALIGNED_ELEMENT, D3D10_INPUT_PER_VERTEX_DATA, 0 },
	};

	if (!Shader_Unreal::compileUnrealShader(L"d3d10drv\\gouraudpolygon.fx", macros, shaderFlags, layoutDesc, sizeof(layoutDesc) / sizeof(layoutDesc[0])))
		return false;

	variables.fogColor = effect->GetVariableByName("fogColor")->AsVector();
	variables.fogDist = effect->GetVariableByName("fogDist")->AsScalar();
	checkVariable(variables.fogColor, "fogColor");
	checkVariable(variables.fogDist, "fogDist");
	return true;
}

/**
\note Keyed on the modulated flag, which is how the engine marks geometry drawn coplanar with what is
	already there, and the reason an unmodulated model never picks up the bias and so cannot end up
	poking out through the wall behind it.
*/
void Shader_GouraudPolygon::setFlags(DWORD flags) {
	const bool decalBias = (flags & PF_Modulated) != 0;
	if (decalBias != useDecalBias) {
		D3D::render();
		useDecalBias = decalBias;
	}
	Shader::setFlags(flags);
}

void Shader_GouraudPolygon::apply() {
	effect->GetTechniqueByIndex(0)->GetPassByIndex(useDecalBias ? PASS_DECAL_BIAS : PASS_STANDARD)->Apply(0);
	geometryBuffer->draw();
}

void Shader_GouraudPolygon::fog(float dist, Vec4 *color) const {
	variables.fogDist->SetFloat(dist);
	if (dist > 0) {
		variables.fogColor->SetFloatVector((float *)color);
	}
}
