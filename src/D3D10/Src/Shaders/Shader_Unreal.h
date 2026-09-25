#pragma once

#include "Shader.h"
#include "../Geometry/DynamicGeometryBuffer.h"

class Shader_Unreal : public Shader {
private:
	static DynamicGeometryBuffer *dynamicGeometryBuffer;
	static ID3D10EffectPool *pool;
	static ID3D10RenderTargetView *unrealRTV;
	static ID3D10DepthStencilView *unrealDSV;
	static ID3D10ShaderResourceView *unrealSRV;
	static ID3D10DepthStencilView *noMSAADSV;

	/**@name Instancing resources
	One unit quad, shared by every shader that draws one record per primitive, its corner order matching
	the triangle strip it replaces so that the index order and the vertex order stay in step with one
	another.
	*/
	//@{
	static ID3D10Buffer *cornerBuffer;
	static ID3D10Buffer *unitIndexBuffer;
	//@}

	struct _variables {
		ID3D10EffectMatrixVariable *projection;
		ID3D10EffectShaderResourceVariable *diffuseTexture;
		ID3D10EffectScalarVariable *viewportHeight; /**< Viewport height in pixels */
		ID3D10EffectScalarVariable *viewportWidth; /**< Viewport width in pixels */
	};


protected:
	/** The unit quad above. */
	enum { INSTANCE_CORNER_COUNT = 4,
		INSTANCE_INDEX_COUNT = 6 };

	/** Slot 0 the quad, slot 1 the records. */
	void bindInstanced();

	/** Whether anything drew. */
	bool applyInstanced();

public:
	enum BUFFERS { BUFFER_MULTIPASS,
		BUFFER_HUD };

	static _variables variables;

	/**
	Per-device statics: effect pool, geometry
	buffer, scene targets and instancing quad.
	*/
	struct Statics {
		DynamicGeometryBuffer *dynamicGeometryBuffer;
		ID3D10EffectPool *pool;
		ID3D10RenderTargetView *unrealRTV;
		ID3D10DepthStencilView *unrealDSV;
		ID3D10ShaderResourceView *unrealSRV;
		ID3D10DepthStencilView *noMSAADSV;
		ID3D10Buffer *cornerBuffer;
		ID3D10Buffer *unitIndexBuffer;
		_variables variables;
		//Per-device, like the projection.
		bool projectionCached;
		float oldPrjXM, oldPrjXP, oldPrjYM, oldPrjYP, oldZNear, oldZFar;
	};
	static void saveStatics(Statics &out);
	static void loadStatics(const Statics &in);

	Shader_Unreal();
	virtual ~Shader_Unreal();

	bool compile(const D3D10_SHADER_MACRO *macros, DWORD shaderFlags) override;
	bool buffersGeometry() const override;

	bool compileUnrealShader(const wchar_t *filename, const D3D10_SHADER_MACRO *macros, DWORD shaderFlags, const D3D10_INPUT_ELEMENT_DESC *elementDesc, int numElements);
	bool createRenderTargetViews(ID3D10RenderTargetView *backbuffer, const DXGI_SWAP_CHAIN_DESC &swapChainDesc, int multiSampleCount) override;
	void releaseRenderTargetViews() override;
	void setProjection(float prjXM, float prjXP, float prjYM, float prjYP, float zNear, float zFar) const;
	static void invalidateCachedState();
	void setViewportSize(float x, float y) const;
	virtual void setTexture(int pass, ID3D10ShaderResourceView *texture) const;
	void clear(Vec4 &clearColor) const;
	void clearDepth() const;
	void switchBuffers(enum BUFFERS buffer);
	static void releaseSharedResources();
};
