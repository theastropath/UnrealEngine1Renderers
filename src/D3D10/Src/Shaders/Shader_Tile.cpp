#include "Shader_Tile.h"

Shader_Tile::Shader_Tile(bool instanced) :
	Shader_Unreal(),
	instanced(instanced) {
	this->topology = instanced
		? D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST
		: D3D10_PRIMITIVE_TOPOLOGY_POINTLIST;
	if (instanced)
		this->techniqueIndex = 1; //RenderInstanced
}

bool Shader_Tile::compile(const D3D10_SHADER_MACRO *macros, DWORD shaderFlags) {
	if (!Shader_Unreal::compile(macros, shaderFlags))
		return false;

	if (instanced) {

		//Slot 0 quad, slot 1 record.
		D3D10_INPUT_ELEMENT_DESC layoutDesc[] = {
			{ "TEXCOORD", 7, DXGI_FORMAT_R32G32_FLOAT, 0, D3D10_APPEND_ALIGNED_ELEMENT, D3D10_INPUT_PER_VERTEX_DATA, 0 },
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D10_APPEND_ALIGNED_ELEMENT, D3D10_INPUT_PER_INSTANCE_DATA, 1 },
			{ "POSITION", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D10_APPEND_ALIGNED_ELEMENT, D3D10_INPUT_PER_INSTANCE_DATA, 1 },
			{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D10_APPEND_ALIGNED_ELEMENT, D3D10_INPUT_PER_INSTANCE_DATA, 1 },
			{ "PSIZE", 0, DXGI_FORMAT_R32_FLOAT, 1, D3D10_APPEND_ALIGNED_ELEMENT, D3D10_INPUT_PER_INSTANCE_DATA, 1 },
			{ "BLENDINDICES", 0, DXGI_FORMAT_R32_UINT, 1, D3D10_APPEND_ALIGNED_ELEMENT, D3D10_INPUT_PER_INSTANCE_DATA, 1 },
		};
		return Shader_Unreal::compileUnrealShader(L"d3d10drv\\tile.fx", macros, shaderFlags, layoutDesc, sizeof(layoutDesc) / sizeof(layoutDesc[0])) != 0;
	}

	D3D10_INPUT_ELEMENT_DESC layoutDesc[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D10_APPEND_ALIGNED_ELEMENT, D3D10_INPUT_PER_VERTEX_DATA, 0 },
		{ "POSITION", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D10_APPEND_ALIGNED_ELEMENT, D3D10_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D10_APPEND_ALIGNED_ELEMENT, D3D10_INPUT_PER_VERTEX_DATA, 0 },
		{ "PSIZE", 0, DXGI_FORMAT_R32_FLOAT, 0, D3D10_APPEND_ALIGNED_ELEMENT, D3D10_INPUT_PER_VERTEX_DATA, 0 },
		{ "BLENDINDICES", 0, DXGI_FORMAT_R32_UINT, 0, D3D10_APPEND_ALIGNED_ELEMENT, D3D10_INPUT_PER_VERTEX_DATA, 0 },
	};

	if (!Shader_Unreal::compileUnrealShader(L"d3d10drv\\tile.fx", macros, shaderFlags, layoutDesc, sizeof(layoutDesc) / sizeof(layoutDesc[0])))
		return false;

	return true;
}

void Shader_Tile::bind() {
	Shader_Unreal::bind();

	if (instanced)
		bindInstanced();
}

void Shader_Tile::apply() {
	if (instanced) {
		applyInstanced();
		return;
	}
	Shader_Unreal::apply();
}
