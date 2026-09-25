/**
\file Draw/DeferredGouraud.cpp

Blended models held back and replayed back to front.
*/

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "../D3D10.h"
#include "../Geometry/VertexFormats.h"
#include "../Texture/TextureCache.h"
#include "../Shaders/Shader_GouraudPolygon.h"


/**
Until the run of models has ended.
\note Stored by texture cache id, FTextureInfo being good for one call.
*/
bool UD3D10RenderDevice::deferGouraudPolygon(FSceneNode *Frame, FTransTexture **Pts, int NumPts, DWORD flags, DWORD64 textureID, float multU, float multV, float alpha) {
	if (NumPts > MAX_DEFERRED_GOURAUD_POINTS)
		return false;

	if (numDeferredGouraudPolygons >= MAX_DEFERRED_GOURAUD_POLYGONS || numDeferredGouraudPoints + NumPts > MAX_DEFERRED_GOURAUD_POINTS)
		flushDeferredGouraudPolygons();

	if (numDeferredGouraudPolygons == 0)
		deferredGouraudFrame = Frame;

	DeferredGouraudPolygon &poly = deferredGouraudPolygons[numDeferredGouraudPolygons++];
	poly.flags = flags;
	poly.textureID = textureID;
	poly.multU = multU;
	poly.multV = multV;
	poly.alpha = alpha;
	poly.firstPoint = numDeferredGouraudPoints;
	poly.numPoints = NumPts;

	float depth = 0.0f;
	for (int i = 0; i < NumPts; i++) {
		deferredGouraudPoints[numDeferredGouraudPoints++] = *Pts[i];
		depth += Pts[i]->Point.Z; //View space already.
	}
	poly.depth = depth / NumPts;
	return true;
}

/**
Greatest depth first.
\note A stable merge sort, so equal depths keep the engine's order.
*/
void UD3D10RenderDevice::sortDeferredGouraudPolygons(int numPolygons) {
	for (int i = 0; i < numPolygons; i++)
		deferredGouraudOrder[i] = i;

	int *from = deferredGouraudOrder;
	int *to = deferredGouraudOrderScratch;
	for (int width = 1; width < numPolygons; width *= 2) {
		for (int lo = 0; lo < numPolygons; lo += 2 * width) {
			int mid = lo + width;
			int hi = lo + 2 * width;
			if (mid > numPolygons)
				mid = numPolygons;
			if (hi > numPolygons)
				hi = numPolygons;

			int a = lo, b = mid, o = lo;
			while (a < mid && b < hi) {
				//Strictly greater keeps it stable.
				to[o++] = (deferredGouraudPolygons[from[b]].depth > deferredGouraudPolygons[from[a]].depth) ? from[b++] : from[a++];
			}
			while (a < mid)
				to[o++] = from[a++];
			while (b < hi)
				to[o++] = from[b++];
		}
		int *swap = from;
		from = to;
		to = swap;
	}

	if (from != deferredGouraudOrder)
		appMemcpy(deferredGouraudOrder, from, numPolygons * sizeof(int));
}

/**
Furthest first.
\note The sort only orders the held polygons. They come in front by being drawn last.
*/
void UD3D10RenderDevice::flushDeferredGouraudPolygons() {
	if (numDeferredGouraudPolygons == 0)
		return;

	sortDeferredGouraudPolygons(numDeferredGouraudPolygons);

	//Counts reset first, so recursion finds nothing.
	const int numPolygons = numDeferredGouraudPolygons;
	numDeferredGouraudPolygons = 0;
	numDeferredGouraudPoints = 0;

	D3D::switchToShader(D3D::SHADER_GOURAUDPOLYGON);
	for (int i = 0; i < numPolygons; i++) {
		const DeferredGouraudPolygon &poly = deferredGouraudPolygons[deferredGouraudOrder[i]];

		if (!textureCache->setTexture(shader_GouraudPolygon, TextureCache::PASS_DIFFUSE, poly.textureID))
			continue; //Flushed from the cache since.

		shader_GouraudPolygon->setFlags(poly.flags);

		DynamicGeometryBuffer *buf = static_cast<DynamicGeometryBuffer *>(shader_GouraudPolygon->getGeometryBuffer());
		if (!buf->indexTriangleFan(poly.numPoints))
			continue;
		for (int p = 0; p < poly.numPoints; p++) {
			Vertex_GouraudPolygon *v = (Vertex_GouraudPolygon *)buf->getVertex();
			const FTransTexture *pt = &deferredGouraudPoints[poly.firstPoint + p];
			v->Pos = *(Vec3 *)&pt->Point.X;
			v->TexCoord.x = (pt->U) * poly.multU;
			v->TexCoord.y = (pt->V) * poly.multV;
			v->Color = *(Vec4 *)&pt->Light.X;
			v->Fog = *(Vec4 *)&pt->Fog.X;
			v->flags = poly.flags;
			v->Color.w = poly.alpha;
		}
	}
}

/**
On a scene node change.
\note By address.
*/
void UD3D10RenderDevice::flushDeferredGouraudPolygonsForFrame(FSceneNode *Frame) {
	if (numDeferredGouraudPolygons > 0 && Frame != deferredGouraudFrame)
		flushDeferredGouraudPolygons();
}

/** A frame that never starts, or a cache flush. */
void UD3D10RenderDevice::discardDeferredGouraudPolygons() {
	numDeferredGouraudPolygons = 0;
	numDeferredGouraudPoints = 0;
	deferredGouraudFrame = nullptr; //Not left to chance.
}
