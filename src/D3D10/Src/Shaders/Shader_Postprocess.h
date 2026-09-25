#pragma once

#include "Shader.h"
#include "../Geometry/GeometryBuffer.h"
#include "../Geometry/VertexFormats.h"

class Shader_Postprocess : public Shader {
private:
	static GeometryBuffer *quadGeometryBuffer;
	static ID3D10InputLayout *quadInputLayout;
	static ID3D10EffectPool *pool;

	struct _variables {
		ID3D10EffectShaderResourceVariable *inputTexture;
		ID3D10EffectVectorVariable *viewPort;
		ID3D10EffectScalarVariable *elapsedTime;
	};

public:
	static _variables variables;

	struct Statics {
		GeometryBuffer *quadGeometryBuffer;
		ID3D10InputLayout *quadInputLayout;
		ID3D10EffectPool *pool;
		_variables variables;
	};
	static void saveStatics(Statics &out);
	static void loadStatics(const Statics &in);

	Shader_Postprocess();
	~Shader_Postprocess();

	bool compile(const D3D10_SHADER_MACRO *macros, DWORD shaderFlags) override;

	bool compilePostProcessingShader(const wchar_t *filename, const D3D10_SHADER_MACRO *macros, DWORD shaderFlags);
	virtual void setInputTexture(ID3D10ShaderResourceView *texture);
	void setViewPort(int x, int y) const;
	void setElapsedTime(float t);
	static void releaseSharedResources();
};
