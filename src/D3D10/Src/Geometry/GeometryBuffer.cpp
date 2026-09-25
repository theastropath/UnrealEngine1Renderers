#include "GeometryBuffer.h"
#include "../D3D10.h"
#include <cstdio>
GeometryBuffer::GeometryBuffer(ID3D10Device *device) :
	numIndices(0),
	numUndrawnIndices(0),
	vertexBuffer(nullptr),
	indexBuffer(nullptr),
	device(device),
	stride(0) {
}

GeometryBuffer::~GeometryBuffer() {
	SAFE_RELEASE(vertexBuffer);
	SAFE_RELEASE(indexBuffer);
}


bool GeometryBuffer::create(unsigned int numIndices, UINT vertexSize, const D3D10_BUFFER_DESC *vertexBufferDesc, const D3D10_BUFFER_DESC *indexBufferDesc, const D3D10_SUBRESOURCE_DATA *initialVertexData, const D3D10_SUBRESOURCE_DATA *initialIndexData) {
	HRESULT hr;
	hr = device->CreateBuffer(vertexBufferDesc, initialVertexData, &vertexBuffer);
	if (FAILED(hr)) {
		UD3D10RenderDevice::debugs("GeometryBuffer: Failed to create vertex buffer.");
		return false;
	}


	hr = device->CreateBuffer(indexBufferDesc, initialIndexData, &indexBuffer);
	if (FAILED(hr)) {
		UD3D10RenderDevice::debugs("GeometryBuffer: Failed to create index buffer.");
		SAFE_RELEASE(vertexBuffer);
		return false;
	}

	this->numIndices = this->numUndrawnIndices = numIndices;
	this->stride = vertexSize;

	return true;
}

void GeometryBuffer::bind() {

	UINT offset = 0;

	device->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
	device->IASetIndexBuffer(indexBuffer, DXGI_FORMAT_R32_UINT, 0);
}

bool GeometryBuffer::hasContents() const {
	return (numUndrawnIndices > 0);
}

void GeometryBuffer::draw() {
	device->DrawIndexed(numIndices, 0, 0);
}

void GeometryBuffer::newFrame() {
}
