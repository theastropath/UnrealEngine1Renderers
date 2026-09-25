#pragma once

class Shader;

#include <d3d10.h>
#include <d3dx10.h>
#include "../D3D10.h"
#include "../Geometry/GeometryBuffer.h"

class Shader {

protected:
	static struct StateStruct {
		ID3D10DepthStencilState *dstate_Enable;
		ID3D10DepthStencilState *dstate_Disable;
		ID3D10BlendState *bstate_Alpha;
		ID3D10BlendState *bstate_Translucent;
		ID3D10BlendState *bstate_Modulate;
		ID3D10BlendState *bstate_NoBlend;
		ID3D10BlendState *bstate_Masked;
		ID3D10BlendState *bstate_Invis;
		ID3D10BlendState *bstate_Highlight;
	} states;

public:
	/** Per device. */
	struct Statics {
		StateStruct states;
		ID3D10Device *device;
		ID3D10BlendState *currBlendState;
		ID3D10DepthStencilState *currDepthState;
	};
	static void saveStatics(Statics &out);
	static void loadStatics(const Statics &in);

protected:
	GeometryBuffer *geometryBuffer;
	ID3D10RenderTargetView *renderTargetView;
	ID3D10DepthStencilView *depthStencilView;
	ID3D10InputLayout *vertexLayout;
	ID3D10ShaderResourceView *shaderResourceView;
	static ID3D10Device *device;
	ID3D10Effect *effect;
	D3D10_PRIMITIVE_TOPOLOGY topology;
	/** Nonzero where there is a choice. */
	int techniqueIndex;

	/**
	\param fileName Relative to System.
	\param out The effect, untouched on failure.
	*/
	static bool compileEffect(const wchar_t *fileName, const D3D10_SHADER_MACRO *macros, DWORD shaderFlags, UINT effectFlags, ID3D10EffectPool *pool, ID3D10Effect **out);
	/** Same, for an effect pool. */
	static bool compileEffectPool(const wchar_t *fileName, const D3D10_SHADER_MACRO *macros, DWORD shaderFlags, ID3D10EffectPool **out);
	bool createRenderTargetViews(DXGI_FORMAT format, DXGI_FORMAT depthFormat, float scaleX, float scaleY, int samples, const DXGI_SWAP_CHAIN_DESC &swapChainDesc);

	/**
	\brief A missing variable draws from a silent placeholder.
	\return Whether it can be used. Also false for one compiled out.
	*/
	static bool checkVariable(ID3D10EffectVariable *variable, const char *name);
	/** Same, for a technique. */
	static bool checkTechnique(ID3D10EffectTechnique *technique, const char *name);

	/** Overridable. */
	virtual ID3D10BlendState *translucentBlendState() const;

	/** Avoids a flush. */
	static void aliasIdenticalStates();

public:
	Shader();
	static bool initShaderSystem(ID3D10Device *device, const D3D10_SHADER_MACRO *macros, DWORD shaderFlags);
	virtual bool compile(const D3D10_SHADER_MACRO *macros, DWORD shaderFlags) = 0;
	virtual bool createRenderTargetViews(ID3D10RenderTargetView *backbuffer, const DXGI_SWAP_CHAIN_DESC &swapChainDesc, int multiSampleCount) = 0;
	virtual void releaseRenderTargetViews();
	virtual ~Shader();
	virtual void bind();
	virtual void apply();
	GeometryBuffer *getGeometryBuffer() const;
	ID3D10ShaderResourceView *getResourceView() const;
	virtual void setFlags(DWORD flags);
	virtual bool buffersGeometry() const;
	static void invalidateCachedState();
	static void releaseSharedResources();
};
