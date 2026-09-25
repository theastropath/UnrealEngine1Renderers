#pragma once

#include "GeometryBuffer.h"


class DynamicGeometryBuffer : public GeometryBuffer {
private:
	bool clear;
	unsigned int numVerts;
	/** Not derivable when mixed. */
	unsigned int drawnVerts;
	void *mappedVBuffer;
	int *mappedIBuffer;
	size_t vertexCapacity;
	size_t indexCapacity;
	bool map();
	bool unmap();

public:
	DynamicGeometryBuffer(ID3D10Device *device);

	enum { INDICES_PER_VERTEX = 3 };

	bool create(size_t size, size_t vertexSize);
	void draw() override;
	void newFrame() override;

	bool indexTriangleFan(int num);
	bool indexSingleVertex();
	void *getVertex();

	/**
	One index per vertex.
	\return false when nothing is undrawn.
	*/
	bool takeUndrawnRecords(unsigned int &firstRecord, unsigned int &numRecords);

	ID3D10Buffer *getVertexBuffer() const { return vertexBuffer; }
	UINT getStride() const { return stride; }

	/**@name Bulk reservation */
	//@{
	bool wouldFit(unsigned int wantVerts, unsigned int wantIndices) const;

	/**
	Does not flush.
	\param outBaseVertex Indices are relative to this.
	\return false if it doesn't fit or cannot be mapped.
	*/
	bool reserveRange(unsigned int wantVerts, unsigned int wantIndices,
		void **outVerts, int **outIndices, unsigned int *outBaseVertex);

	/** Must follow its reserveRange. */
	void releaseRange(unsigned int haveVerts, unsigned int haveIndices);

	void requestDiscard() { clear = true; }
	//@}
};
