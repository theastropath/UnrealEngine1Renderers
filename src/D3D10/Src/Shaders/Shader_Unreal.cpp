/**
Base class for the Unreal geometry pipeline shaders. They share a pool and a geometry buffer.
\note All must use equal-sized vertices.
*/

#include <new>
#include "Shader_Unreal.h"
#include "../Geometry/DynamicGeometryBuffer.h"
#include <xnamath.h>

DynamicGeometryBuffer *Shader_Unreal::dynamicGeometryBuffer;
ID3D10EffectPool *Shader_Unreal::pool;
Shader_Unreal::_variables Shader_Unreal::variables;
ID3D10RenderTargetView *Shader_Unreal::unrealRTV;
ID3D10DepthStencilView *Shader_Unreal::unrealDSV;
ID3D10ShaderResourceView *Shader_Unreal::unrealSRV;
ID3D10DepthStencilView *Shader_Unreal::noMSAADSV;
ID3D10Buffer *Shader_Unreal::cornerBuffer;
ID3D10Buffer *Shader_Unreal::unitIndexBuffer;

static const int BUFFER_SIZE = 20000; //Vertices of engine geometry

static bool projectionCached = false;
static float oldPrjXM, oldPrjXP, oldPrjYM, oldPrjYP, oldZNear, oldZFar;

void Shader_Unreal::saveStatics(Statics &out) {
	out.dynamicGeometryBuffer = dynamicGeometryBuffer;
	out.pool = pool;
	out.unrealRTV = unrealRTV;
	out.unrealDSV = unrealDSV;
	out.unrealSRV = unrealSRV;
	out.noMSAADSV = noMSAADSV;
	out.cornerBuffer = cornerBuffer;
	out.unitIndexBuffer = unitIndexBuffer;
	out.variables = variables;
	out.projectionCached = projectionCached;
	out.oldPrjXM = oldPrjXM;
	out.oldPrjXP = oldPrjXP;
	out.oldPrjYM = oldPrjYM;
	out.oldPrjYP = oldPrjYP;
	out.oldZNear = oldZNear;
	out.oldZFar = oldZFar;
}

void Shader_Unreal::loadStatics(const Statics &in) {
	dynamicGeometryBuffer = in.dynamicGeometryBuffer;
	pool = in.pool;
	unrealRTV = in.unrealRTV;
	unrealDSV = in.unrealDSV;
	unrealSRV = in.unrealSRV;
	noMSAADSV = in.noMSAADSV;
	cornerBuffer = in.cornerBuffer;
	unitIndexBuffer = in.unitIndexBuffer;
	variables = in.variables;
	projectionCached = in.projectionCached;
	oldPrjXM = in.oldPrjXM;
	oldPrjXP = in.oldPrjXP;
	oldPrjYM = in.oldPrjYM;
	oldPrjYP = in.oldPrjYP;
	oldZNear = in.oldZNear;
	oldZFar = in.oldZFar;
}

Shader_Unreal::Shader_Unreal() :
	Shader() {
}

Shader_Unreal::~Shader_Unreal() {
	geometryBuffer = nullptr;
}

/**
Releases the members shared by every Unreal shader.
\note The pool outlives the child effects compiled against it.
*/
void Shader_Unreal::releaseSharedResources() {
	SAFE_RELEASE(cornerBuffer);
	SAFE_RELEASE(unitIndexBuffer);

	SAFE_RELEASE(pool);

	variables.projection = nullptr;
	variables.viewportHeight = nullptr;
	variables.viewportWidth = nullptr;
	variables.diffuseTexture = nullptr;

	delete dynamicGeometryBuffer;
	dynamicGeometryBuffer = nullptr;
}

bool Shader_Unreal::compile(const D3D10_SHADER_MACRO *macros, DWORD shaderFlags) {

	if (dynamicGeometryBuffer == nullptr) {

		dynamicGeometryBuffer = new (std::nothrow) DynamicGeometryBuffer(device);
		if (!dynamicGeometryBuffer || !dynamicGeometryBuffer->create(BUFFER_SIZE, sizeof(Vertex_ComplexSurface))) {
			UD3D10RenderDevice::debugs("Failed to create dynamic geometry buffer.");
			return false;
		}
	}
	if (pool == nullptr) {
		if (!compileEffectPool(L"d3d10drv\\unrealpool.fxh", macros, shaderFlags, &pool))
			return false;

		variables.projection = pool->AsEffect()->GetVariableByName("projection")->AsMatrix();
		variables.viewportHeight = pool->AsEffect()->GetVariableByName("viewportHeight")->AsScalar();
		variables.viewportWidth = pool->AsEffect()->GetVariableByName("viewportWidth")->AsScalar();
		variables.diffuseTexture = pool->AsEffect()->GetVariableByName("texDiffuse")->AsShaderResource();
		checkVariable(variables.projection, "projection");
		checkVariable(variables.viewportHeight, "viewportHeight");
		checkVariable(variables.viewportWidth, "viewportWidth");
		checkVariable(variables.diffuseTexture, "texDiffuse");
	}
	if (cornerBuffer == nullptr) {
		//The unit quad the instanced techniques draw, its corner order matching the triangle strip it
		//replaces, which is why the second triangle's indices run out of ascending order as v2, v1, v3 and
		//have to be left that way.
		static const float corners[INSTANCE_CORNER_COUNT * 2] = {
			0.0f, 0.0f,
			0.0f, 1.0f,
			1.0f, 0.0f,
			1.0f, 1.0f
		};
		static const unsigned int indices[INSTANCE_INDEX_COUNT] = { 0, 1, 2, 2, 1, 3 };

		D3D10_BUFFER_DESC desc;
		desc.Usage = D3D10_USAGE_IMMUTABLE;
		desc.ByteWidth = sizeof(corners);
		desc.BindFlags = D3D10_BIND_VERTEX_BUFFER;
		desc.CPUAccessFlags = 0;
		desc.MiscFlags = 0;
		D3D10_SUBRESOURCE_DATA data;
		data.pSysMem = corners;
		data.SysMemPitch = 0;
		data.SysMemSlicePitch = 0;
		if (FAILED(device->CreateBuffer(&desc, &data, &cornerBuffer))) {
			UD3D10RenderDevice::debugs("Failed to create the instancing corner buffer.");
			return false;
		}

		desc.ByteWidth = sizeof(indices);
		desc.BindFlags = D3D10_BIND_INDEX_BUFFER;
		data.pSysMem = indices;
		if (FAILED(device->CreateBuffer(&desc, &data, &unitIndexBuffer))) {
			UD3D10RenderDevice::debugs("Failed to create the instancing index buffer.");
			SAFE_RELEASE(cornerBuffer);
			return false;
		}
	}

	this->geometryBuffer = dynamicGeometryBuffer;
	return true;
}


/**
Slot 0 quad, slot 1 records.
\note Replaces the ordinary bind.
*/
void Shader_Unreal::bindInstanced() {
	ID3D10Buffer *buffers[2] = { cornerBuffer, dynamicGeometryBuffer->getVertexBuffer() };
	UINT strides[2] = { sizeof(float) * 2, dynamicGeometryBuffer->getStride() };
	UINT offsets[2] = { 0, 0 };

	device->IASetVertexBuffers(0, 2, buffers, strides, offsets);
	device->IASetIndexBuffer(unitIndexBuffer, DXGI_FORMAT_R32_UINT, 0);
}

/** One instance each. */
bool Shader_Unreal::applyInstanced() {
	unsigned int firstRecord, numRecords;
	if (!dynamicGeometryBuffer->takeUndrawnRecords(firstRecord, numRecords))
		return false;

	effect->GetTechniqueByIndex(techniqueIndex)->GetPassByIndex(0)->Apply(0);
	//Instance offset only.
	device->DrawIndexedInstanced(INSTANCE_INDEX_COUNT, numRecords, 0, 0, firstRecord);
	return true;
}

bool Shader_Unreal::buffersGeometry() const {
	return true;
}

bool Shader_Unreal::compileUnrealShader(const wchar_t *filename, const D3D10_SHADER_MACRO *macros, DWORD shaderFlags, const D3D10_INPUT_ELEMENT_DESC *elementDesc, int numElements) {
	if (!compileEffect(filename, macros, shaderFlags, D3D10_EFFECT_COMPILE_CHILD_EFFECT, pool, &effect))
		return 0;

	//An extra per-vertex element.
	D3D10_PASS_DESC passDesc;
	ID3D10EffectTechnique *t = effect->GetTechniqueByIndex(techniqueIndex);
	if (!t->IsValid()) {
		UD3D10RenderDevice::debugs("Failed to find the requested technique.");
		return 0;
	}
	ID3D10EffectPass *p = t->GetPassByIndex(0);
	if (!p->IsValid()) {
		UD3D10RenderDevice::debugs("Failed to find pass 0.");
		return 0;
	}
	p->GetDesc(&passDesc);
	const HRESULT hr = device->CreateInputLayout(elementDesc, numElements, passDesc.pIAInputSignature, passDesc.IAInputSignatureSize, &vertexLayout);
	if (FAILED(hr)) {
		UD3D10RenderDevice::debugs("Error creating input layout.");
		return 0;
	}

	return 1;
}

bool Shader_Unreal::createRenderTargetViews(ID3D10RenderTargetView *backbuffer, const DXGI_SWAP_CHAIN_DESC &swapChainDesc, int multiSampleCount) {

	if (unrealRTV == nullptr) {
		if (!Shader::createRenderTargetViews(D3D::getDrawPassFormat(), DXGI_FORMAT_D32_FLOAT, 1, 1, multiSampleCount, swapChainDesc))
			return false;
		unrealRTV = renderTargetView;
		unrealDSV = depthStencilView;
		unrealSRV = shaderResourceView;

		if (multiSampleCount > 1) {
			//The base create publishes into three view pointers and releases all three on failure, by which
			//point they are the scene targets, so detaching them first keeps that unwind off views the statics
			//still hold and leaves the scene intact through the failure.
			ID3D10RenderTargetView *savedRTV = renderTargetView;
			ID3D10DepthStencilView *savedDSV = depthStencilView;
			ID3D10ShaderResourceView *savedSRV = shaderResourceView;
			renderTargetView = nullptr;
			depthStencilView = nullptr;
			shaderResourceView = nullptr;

			const bool ok = Shader::createRenderTargetViews(DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_D32_FLOAT, 1, 1, 1, swapChainDesc);
			noMSAADSV = depthStencilView; //Null on failure.

			renderTargetView = savedRTV;
			depthStencilView = savedDSV;
			shaderResourceView = savedSRV;

			if (!ok)
				return false;
		}
	}

	renderTargetView = unrealRTV;
	depthStencilView = unrealDSV;
	shaderResourceView = unrealSRV;

	return 1;
}

void Shader_Unreal::releaseRenderTargetViews() {
	SAFE_RELEASE(unrealRTV);
	renderTargetView = NULL;

	SAFE_RELEASE(unrealDSV);
	depthStencilView = NULL;

	SAFE_RELEASE(unrealSRV);
	shaderResourceView = NULL;

	SAFE_RELEASE(noMSAADSV); //Nulls its argument

	Shader::releaseRenderTargetViews();
}


/**
From the node's own projection edges.
\param prjXM Left edge (X over Z); prjYM is the top edge, because engine Y runs downwards.
\note The depth buffer is reversed, near mapping to 1 for precision, which is why zFar goes in as the
	near plane and zNear as the far one.
*/
void Shader_Unreal::setProjection(float prjXM, float prjXP, float prjYM, float prjYP, float zNear, float zFar) const {
	if (projectionCached && prjXM == oldPrjXM && prjXP == oldPrjXP && prjYM == oldPrjYM && prjYP == oldPrjYP && zNear == oldZNear && zFar == oldZFar)
		return;

	D3D::render();

	//The vertex shaders flip Y.
	XMMATRIX m = XMMatrixPerspectiveOffCenterLH(-prjXM * zFar, prjXP * zFar, -prjYM * zFar, prjYP * zFar, zFar, zNear); //Same as a standard off-center frustum.
	variables.projection->SetMatrix(&m.m[0][0]);

	oldPrjXM = prjXM;
	oldPrjXP = prjXP;
	oldPrjYM = prjYM;
	oldPrjYP = prjYP;
	oldZNear = zNear;
	oldZFar = zFar;
	projectionCached = true;
}

/**
Forgets the cached projection, since the pool and its matrix variable are recreated on every renderer
init and a stale cache would leave the new one unwritten, which shows up as a blank screen the first
time anything tries to draw.
*/
void Shader_Unreal::invalidateCachedState() {
	projectionCached = false;
}

void Shader_Unreal::setViewportSize(float x, float y) const {
	variables.viewportWidth->SetFloat(x);
	variables.viewportHeight->SetFloat(y);
}

void Shader_Unreal::setTexture(int pass, ID3D10ShaderResourceView *texture) const {
	if (pass == 0)
		variables.diffuseTexture->SetResourceArray(&texture, pass, 1);
}

void Shader_Unreal::clear(Vec4 &clearColor) const {
	if (renderTargetView)
		device->ClearRenderTargetView(renderTargetView, (float *)&clearColor);
}

void Shader_Unreal::clearDepth() const {
	if (depthStencilView) {
		device->ClearDepthStencilView(depthStencilView, D3D10_CLEAR_DEPTH, 0.0, 0);
	}
}

/** Later draws land on the last target. */
void Shader_Unreal::switchBuffers(enum BUFFERS buffer) {
	if (buffer == Shader_Unreal::BUFFER_MULTIPASS) {
		renderTargetView = unrealRTV;
		depthStencilView = unrealDSV;
	} else {
		renderTargetView = 0;
		if (noMSAADSV)
			depthStencilView = noMSAADSV;
		else
			depthStencilView = unrealDSV;
	}
}
