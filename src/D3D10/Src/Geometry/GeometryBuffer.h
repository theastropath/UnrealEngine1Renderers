#pragma once

#include <d3d10.h>


class GeometryBuffer {


protected:
	unsigned int numIndices;
	unsigned int numUndrawnIndices;
	ID3D10Buffer *vertexBuffer;
	ID3D10Buffer *indexBuffer;
	ID3D10Device *device;
	UINT stride;

public:
	GeometryBuffer(ID3D10Device *device);
	bool create(unsigned int numIndices, UINT vertexSize, const D3D10_BUFFER_DESC *vertexBufferDesc, const D3D10_BUFFER_DESC *indexBufferDesc, const D3D10_SUBRESOURCE_DATA *initialVertexData, const D3D10_SUBRESOURCE_DATA *initialIndexData);
	virtual void bind();
	virtual ~GeometryBuffer();
	bool hasContents() const;
	virtual void draw();
	virtual void newFrame();
};
