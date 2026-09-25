#include <new>
#include "Shader_Postprocess.h"
#include "../Geometry/GeometryBuffer.h"

GeometryBuffer *Shader_Postprocess::quadGeometryBuffer; /**< Geometry buffer that holds a full-screen quad */
ID3D10EffectPool *Shader_Postprocess::pool;
Shader_Postprocess::_variables Shader_Postprocess::variables;
ID3D10InputLayout *Shader_Postprocess::quadInputLayout;

//Per device.
void Shader_Postprocess::saveStatics(Statics &out) {
	out.quadGeometryBuffer = quadGeometryBuffer;
	out.quadInputLayout = quadInputLayout;
	out.pool = pool;
	out.variables = variables;
}

void Shader_Postprocess::loadStatics(const Statics &in) {
	quadGeometryBuffer = in.quadGeometryBuffer;
	quadInputLayout = in.quadInputLayout;
	pool = in.pool;
	variables = in.variables;
}

Shader_Postprocess::Shader_Postprocess() :
	Shader() {
	topology = D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
}

Shader_Postprocess::~Shader_Postprocess() {
	//Released separately.
	geometryBuffer = 0;
	vertexLayout = 0;
}

/**
Releases the members shared by every post-processing
shader.
\note Not in a destructor: the pool must outlive the child effects, released first.
*/
void Shader_Postprocess::releaseSharedResources() {
	SAFE_RELEASE(pool);

	//Owned by the pool.
	variables.inputTexture = nullptr;
	variables.viewPort = nullptr;
	variables.elapsedTime = nullptr;

	SAFE_RELEASE(quadInputLayout);

	delete quadGeometryBuffer;
	quadGeometryBuffer = nullptr;
}

bool Shader_Postprocess::compile(const D3D10_SHADER_MACRO *macros, DWORD shaderFlags) {

	if (quadGeometryBuffer == nullptr) {
		//Geometry buffer

		/*	Vertex_Simple quadVerts[]={ {{-1,-1,0},{0,1}},
									{{-1,1,0},{0,0}},
									{{1,-1,0},{1,1}},
									{{1,1,0},{1,0}}};*/

		Vertex_Simple quadVerts[] = { { -1, -1, 0 },
			{ -1, 1, 0 },
			{ 1, -1, 0 },
			{ 1, 1, 0 } };
		//D3D10 ignores the pitches.
		D3D10_SUBRESOURCE_DATA quadVertData;
		quadVertData.pSysMem = quadVerts;
		quadVertData.SysMemPitch = 0;
		quadVertData.SysMemSlicePitch = 0;
		D3D10_BUFFER_DESC vBufferDesc;
		vBufferDesc.Usage = D3D10_USAGE_IMMUTABLE;
		vBufferDesc.CPUAccessFlags = 0;
		vBufferDesc.ByteWidth = sizeof(Vertex_Simple) * 4;
		vBufferDesc.BindFlags = D3D10_BIND_VERTEX_BUFFER;
		vBufferDesc.MiscFlags = 0;


		unsigned int quadIndices[] = { 0, 1, 2, 3 };
		D3D10_SUBRESOURCE_DATA quadIndexData;
		quadIndexData.pSysMem = quadIndices;
		quadIndexData.SysMemPitch = 0;
		quadIndexData.SysMemSlicePitch = 0;
		D3D10_BUFFER_DESC iBufferDesc;
		iBufferDesc.Usage = D3D10_USAGE_IMMUTABLE;
		iBufferDesc.CPUAccessFlags = 0;
		iBufferDesc.ByteWidth = sizeof(unsigned int) * 4;
		iBufferDesc.BindFlags = D3D10_BIND_INDEX_BUFFER;
		iBufferDesc.MiscFlags = 0;

		quadGeometryBuffer = new (std::nothrow) GeometryBuffer(device);
		if (!quadGeometryBuffer || !quadGeometryBuffer->create(4, sizeof(Vertex_Simple), &vBufferDesc, &iBufferDesc, &quadVertData, &quadIndexData)) {
			UD3D10RenderDevice::debugs("Failed to create quad geometry buffer.");
			return false;
		}
	}
	if (pool == nullptr) {
		if (!compileEffectPool(L"d3d10drv\\postprocessing.fxh", macros, shaderFlags, &pool))
			return false;

		variables.inputTexture = pool->AsEffect()->GetVariableByName("inputTexture")->AsShaderResource();
		variables.viewPort = pool->AsEffect()->GetVariableByName("viewPort")->AsVector();
		variables.elapsedTime = pool->AsEffect()->GetVariableByName("elapsedTime")->AsScalar();
		checkVariable(variables.inputTexture, "inputTexture");
		checkVariable(variables.viewPort, "viewPort");
		checkVariable(variables.elapsedTime, "elapsedTime");
	}

	this->geometryBuffer = quadGeometryBuffer;
	return true;
}

bool Shader_Postprocess::compilePostProcessingShader(const wchar_t *filename, const D3D10_SHADER_MACRO *macros, DWORD shaderFlags) {
	if (!compileEffect(filename, macros, shaderFlags, D3D10_EFFECT_COMPILE_CHILD_EFFECT, pool, &effect))
		return 0;

	//The first compiled creates it.
	if (quadInputLayout == nullptr) {
		D3D10_PASS_DESC passDesc;
		ID3D10EffectTechnique *t = effect->GetTechniqueByIndex(0);
		if (!t->IsValid()) {
			UD3D10RenderDevice::debugs("Failed to find technique 0.");
			return 0;
		}
		ID3D10EffectPass *p = t->GetPassByIndex(0);
		if (!p->IsValid()) {
			UD3D10RenderDevice::debugs("Failed to find pass 0.");
			return 0;
		}
		p->GetDesc(&passDesc);

		D3D10_INPUT_ELEMENT_DESC simpleLayoutDesc[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D10_APPEND_ALIGNED_ELEMENT, D3D10_INPUT_PER_VERTEX_DATA, 0 },
			//	{ "TEXCOORD",   0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D10_APPEND_ALIGNED_ELEMENT,   D3D10_INPUT_PER_VERTEX_DATA, 0 },
		};
		int numElements = sizeof(simpleLayoutDesc) / sizeof(simpleLayoutDesc[0]);
		const HRESULT hr = device->CreateInputLayout(simpleLayoutDesc, numElements, passDesc.pIAInputSignature, passDesc.IAInputSignatureSize, &quadInputLayout);
		if (FAILED(hr)) {
			UD3D10RenderDevice::debugs("Error creating input layout.");
			return 0;
		}
	}
	vertexLayout = quadInputLayout;

	return 1;
}


void Shader_Postprocess::setInputTexture(ID3D10ShaderResourceView *texture) {
	variables.inputTexture->SetResource(texture);
}

//A shader constant.
void Shader_Postprocess::setViewPort(int x, int y) const {
	//A uint4 for the setter.
	int viewPort[4] = { x, y, 0, 0 };
	variables.viewPort->SetIntVector(viewPort);
}

void Shader_Postprocess::setElapsedTime(float t) {
	variables.elapsedTime->SetFloat(t);
}
