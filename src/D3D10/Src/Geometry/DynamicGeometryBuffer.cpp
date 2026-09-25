#include "DynamicGeometryBuffer.h"
#include "../D3D10.h"

DynamicGeometryBuffer::DynamicGeometryBuffer(ID3D10Device *device) :
	GeometryBuffer(device),
	clear(true),
	numVerts(0),
	drawnVerts(0),
	mappedVBuffer(nullptr),
	mappedIBuffer(nullptr),
	vertexCapacity(0),
	indexCapacity(0) {
}

bool DynamicGeometryBuffer::create(size_t size, size_t vertexSize) {
	this->vertexCapacity = size;
	this->indexCapacity = size * INDICES_PER_VERTEX;

	D3D10_BUFFER_DESC vertexBufferDesc;
	vertexBufferDesc.Usage = D3D10_USAGE_DYNAMIC;
	vertexBufferDesc.ByteWidth = vertexSize * vertexCapacity;
	vertexBufferDesc.BindFlags = D3D10_BIND_VERTEX_BUFFER;
	vertexBufferDesc.CPUAccessFlags = D3D10_CPU_ACCESS_WRITE;
	vertexBufferDesc.MiscFlags = 0;

	D3D10_BUFFER_DESC indexBufferDesc;
	indexBufferDesc.Usage = D3D10_USAGE_DYNAMIC;
	indexBufferDesc.ByteWidth = sizeof(int) * indexCapacity;
	indexBufferDesc.BindFlags = D3D10_BIND_INDEX_BUFFER;
	indexBufferDesc.CPUAccessFlags = D3D10_CPU_ACCESS_WRITE;
	indexBufferDesc.MiscFlags = 0;

	return GeometryBuffer::create(0, vertexSize, &vertexBufferDesc, &indexBufferDesc, nullptr, nullptr);
}

bool DynamicGeometryBuffer::map() {
	HRESULT hr, hr2;
	if (mappedIBuffer != nullptr && mappedVBuffer != nullptr) {
		//UD3D10RenderDevice::debugs("map() without unmap");
		if (!clear)
			return true;
		unmap();
	}

	D3D10_MAP m;
	if (clear) {
		numVerts = 0;
		numIndices = 0;
		numUndrawnIndices = 0;
		drawnVerts = 0;
		m = D3D10_MAP_WRITE_DISCARD;
		clear = false;

	} else {
		m = D3D10_MAP_WRITE_NO_OVERWRITE;
	}


	hr = vertexBuffer->Map(m, 0, (void **)&mappedVBuffer);
	hr2 = indexBuffer->Map(m, 0, (void **)&mappedIBuffer);
	if (FAILED(hr) || FAILED(hr2)) {
		UD3D10RenderDevice::debugs("Failed to map index and/or vertex buffer.");
		if (SUCCEEDED(hr)) {
			vertexBuffer->Unmap();
			mappedVBuffer = nullptr;
		}
		if (SUCCEEDED(hr2)) {
			indexBuffer->Unmap();
			mappedIBuffer = nullptr;
		}
		clear = true;
		return false;
	}
	return true;
}

bool DynamicGeometryBuffer::unmap() {
	if (mappedVBuffer == nullptr || mappedIBuffer == nullptr) {
		return false;
	}
	vertexBuffer->Unmap();
	mappedVBuffer = nullptr;
	indexBuffer->Unmap();
	mappedIBuffer = nullptr;

	return true;
}

bool DynamicGeometryBuffer::indexTriangleFan(int num) {
	if (num < 3)
		return false;

	int newIndices = (num - 2) * 3;

	if (newIndices > (int)indexCapacity || num > (int)vertexCapacity)
	{
		UD3D10RenderDevice::debugs("Geometry buffer too small for a submitted polygon.");
		return false;
	}

	if ((clear ? 0 : numIndices) + newIndices > indexCapacity || (clear ? 0 : numVerts) + num > vertexCapacity) {
		D3D::render();
		clear = true;
	}
	if (!map())
		return false;

	for (int i = 1; i < num - 1; i++) {
		mappedIBuffer[numIndices++] = numVerts;
		mappedIBuffer[numIndices++] = numVerts + i;
		mappedIBuffer[numIndices++] = numVerts + i + 1;
	}

	numUndrawnIndices += newIndices;
	return true;
}

bool DynamicGeometryBuffer::indexSingleVertex() {
	int newIndices = 1;

	if ((clear ? 0 : numIndices) + newIndices > indexCapacity || (clear ? 0 : numVerts) + 1 > vertexCapacity) {
		D3D::render();
		clear = true;
	}
	if (!map())
		return false;

	mappedIBuffer[numIndices++] = numVerts;

	numUndrawnIndices += newIndices;
	return true;
}

void *DynamicGeometryBuffer::getVertex() {
	return (void *)((char *)(mappedVBuffer) + stride * numVerts++);
}

void DynamicGeometryBuffer::draw() {
	if (mappedVBuffer == nullptr || mappedIBuffer == nullptr || numUndrawnIndices == 0)
		return;
	unmap();
	device->DrawIndexed(numUndrawnIndices, numIndices - numUndrawnIndices, 0);
	numUndrawnIndices = 0;
	drawnVerts = numVerts;
}

bool DynamicGeometryBuffer::wouldFit(unsigned int wantVerts, unsigned int wantIndices) const {
	const unsigned int usedVerts = clear ? 0 : numVerts;
	const unsigned int usedIndices = clear ? 0 : numIndices;
	return (usedVerts + wantVerts <= vertexCapacity) && (usedIndices + wantIndices <= indexCapacity);
}

bool DynamicGeometryBuffer::reserveRange(unsigned int wantVerts, unsigned int wantIndices,
	void **outVerts, int **outIndices, unsigned int *outBaseVertex) {
	if (!wouldFit(wantVerts, wantIndices))
		return false;
	if (!map()) //Resets a pending discard.
		return false;
	if (numVerts + wantVerts > vertexCapacity || numIndices + wantIndices > indexCapacity)
		return false;

	*outBaseVertex = numVerts;
	*outVerts = (void *)((char *)mappedVBuffer + (size_t)stride * numVerts);
	*outIndices = mappedIBuffer + numIndices;

	numVerts += wantVerts;
	numIndices += wantIndices;
	numUndrawnIndices += wantIndices;
	return true;
}

void DynamicGeometryBuffer::releaseRange(unsigned int haveVerts, unsigned int haveIndices) {
	if (haveVerts > numVerts || haveIndices > numIndices || haveIndices > numUndrawnIndices)
		return;

	numVerts -= haveVerts;
	numIndices -= haveIndices;
	numUndrawnIndices -= haveIndices;
}

bool DynamicGeometryBuffer::takeUndrawnRecords(unsigned int &firstRecord, unsigned int &numRecords) {
	if (mappedVBuffer == nullptr || mappedIBuffer == nullptr || numUndrawnIndices == 0)
		return false;
	unmap();
	firstRecord = drawnVerts;
	numRecords = numVerts - drawnVerts;
	numUndrawnIndices = 0;
	drawnVerts = numVerts;
	return numRecords != 0;
}

void DynamicGeometryBuffer::newFrame() {
	unmap();
	numUndrawnIndices = 0;
	drawnVerts = 0;
	clear = true;
}
