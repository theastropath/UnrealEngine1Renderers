/**
\file Draw/Gouraud.cpp

Per-vertex lit geometry. Fans, plus indexed triangles for Harry Potter.
*/

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "../D3D10.h"
#include "../Geometry/VertexFormats.h"
#include "../Texture/TextureCache.h"
#include "../Texture/Precache.h"
#include "../Shaders/PolyFlags.h"
#include "../Shaders/Shader_GouraudPolygon.h"


/**
Models, decals and shadows, already transformed and lit.
\param Frame The scene.
\param Info Diffuse only.
\param Pts Fan array. Each point carries light and fog.
\param NumPts Points in the fan.
\param PolyFlags Flags for this model.
\param Span Probably for software renderers.
*/
void UD3D10RenderDevice::DrawGouraudPolygon(FSceneNode *Frame, FTextureInfo &Info, FTransTexture **Pts, int NumPts, DWORD PolyFlags, FSpanBuffer *Span) {
	makeContextCurrent();

	if (NumPts < 3)
		return;

	flushDeferredGouraudPolygonsForFrame(Frame);

	//Never announced.
	setSceneNodeIfChanged(Frame);

	//No update while geometry is buffered.
	D3D::switchToShader(D3D::SHADER_GOURAUDPOLYGON);

	TextureCache::CachedTexture *entry = precacheLayer(*this, *textureCache, Info, PolyFlags, false);
	if (!entry)
		return;
	const TextureCache::TextureMetaData &diffuse = entry->metadata;

	const DWORD flags = enginePolyFlags(PolyFlags) | diffuse.customPolyFlags;

#if RUNE
	//Absent reads as full opacity.
	const float alpha = ((PolyFlags & PF_AlphaBlend) && Info.Texture) ? Info.Texture->Alpha : 1.0f;
#else
	const float alpha = 1.0f;
#endif

	//Blended geometry writes no depth, so it is held back and replayed after this run. One too wide
	//for the store draws now and takes its chances.
	if (isBlendedGeometry(flags)) {
		if (deferGouraudPolygon(Frame, Pts, NumPts, flags, Info.CacheID, diffuse.multU, diffuse.multV, alpha))
			return;
	}

	if (!textureCache->setTexture(shader_GouraudPolygon, TextureCache::PASS_DIFFUSE, Info.CacheID, entry))
		return;

	shader_GouraudPolygon->setFlags(flags);

	DynamicGeometryBuffer *buf = static_cast<DynamicGeometryBuffer *>(shader_GouraudPolygon->getGeometryBuffer());
	if (!buf->indexTriangleFan(NumPts))
		return;
	for (INT i = 0; i < NumPts; i++) {
		Vertex_GouraudPolygon *v = (Vertex_GouraudPolygon *)buf->getVertex();
		const FTransTexture *pt = Pts[i];
		v->Pos = *(Vec3 *)&pt->Point.X;
		v->TexCoord.x = (pt->U) * diffuse.multU;
		v->TexCoord.y = (pt->V) * diffuse.multV;
		v->Color = *(Vec4 *)&pt->Light.X;
		v->Fog = *(Vec4 *)&pt->Fog.X;
		v->flags = flags;
		v->Color.w = alpha;
	}
}

#if HARRYPOTTER

/** Per indexed-triangle call. */
INT UD3D10RenderDevice::MaxVertices() {
	return 10000;
}

/**
Harry Potter's main actor geometry path.
\note Three encodings per call:
- Indices non-null. An indexed triangle list.
- Indices null, NumIndices equal to NumPts. A fan.
- Indices null otherwise. Points, three at a time.
*/
void UD3D10RenderDevice::DrawTriangles(FSceneNode *Frame, FTextureInfo &Info, FTransTexture **Pts, INT NumPts, _WORD *Indices, INT NumIndices, DWORD PolyFlags, FSpanBuffer *Span) {
	//The game marks Harry's lenses highlighted and translucent on a masked texture, and honoring that
	//combination literally draws them as two discs of opaque white, so the pair of flags comes straight
	//back off again before anything downstream gets a look at them.
	if ((PolyFlags & (PF_Highlighted | PF_Translucent)) == (PF_Highlighted | PF_Translucent) && (PolyFlags & PF_Masked))
		PolyFlags &= ~(PF_Highlighted | PF_Translucent);

	if (!Indices && NumIndices == NumPts) {
		DrawGouraudPolygon(Frame, Info, Pts, NumPts, PolyFlags, Span);
		return;
	}

	const INT numIndices = Indices ? NumIndices : NumPts;

	makeContextCurrent();

	if (numIndices < 3 || NumPts < 3)
		return;

	flushDeferredGouraudPolygonsForFrame(Frame);
	setSceneNodeIfChanged(Frame);
	D3D::switchToShader(D3D::SHADER_GOURAUDPOLYGON);

	TextureCache::CachedTexture *entry = precacheLayer(*this, *textureCache, Info, PolyFlags, false);
	if (!entry)
		return;
	const TextureCache::TextureMetaData &diffuse = entry->metadata;

	const DWORD flags = enginePolyFlags(PolyFlags) | diffuse.customPolyFlags;
	const float alpha = 1.0f;

	//A three-point fan.
	if (isBlendedGeometry(flags)) {
		for (INT i = 0; (i + 3) <= numIndices; i += 3) {
			if (!indexedTriangleInRange(Indices, i, NumPts))
				continue;

			FTransTexture *tri[3];
			tri[0] = Pts[Indices ? Indices[i + 0] : (i + 0)];
			tri[1] = Pts[Indices ? Indices[i + 1] : (i + 1)];
			tri[2] = Pts[Indices ? Indices[i + 2] : (i + 2)];

			deferGouraudPolygon(Frame, tri, 3, flags, Info.CacheID, diffuse.multU, diffuse.multV, alpha);
		}
		return;
	}

	if (!textureCache->setTexture(shader_GouraudPolygon, TextureCache::PASS_DIFFUSE, Info.CacheID, entry))
		return;

	shader_GouraudPolygon->setFlags(flags);

	DynamicGeometryBuffer *buf = static_cast<DynamicGeometryBuffer *>(shader_GouraudPolygon->getGeometryBuffer());

	//One reservation for the whole mesh with the indices rebased onto its starting vertex, and a refusal
	//retries behind a render and discard, a second refusal meaning the mesh is wider than the buffer and
	//has to go a triangle at a time instead.
	void *verts;
	int *indices;
	unsigned int baseVertex;
	bool reserved = buf->reserveRange(NumPts, numIndices, &verts, &indices, &baseVertex);
	if (!reserved) {
		D3D::render();
		buf->requestDiscard();
		reserved = buf->reserveRange(NumPts, numIndices, &verts, &indices, &baseVertex);
	}

	if (reserved) {
		for (INT i = 0; i < NumPts; i++) {
			Vertex_GouraudPolygon *v = ((Vertex_GouraudPolygon *)verts) + i;
			const FTransTexture *pt = Pts[i];
			v->Pos = *(Vec3 *)&pt->Point.X;
			v->TexCoord.x = (pt->U) * diffuse.multU;
			v->TexCoord.y = (pt->V) * diffuse.multV;
			v->Color = *(Vec4 *)&pt->Light.X;
			v->Fog = *(Vec4 *)&pt->Fog.X;
			v->flags = flags;
			v->Color.w = alpha;
		}

		for (INT i = 0; i < numIndices; i++) {
			const INT local = Indices ? Indices[i] : i;
			indices[i] = baseVertex + ((local < NumPts) ? local : 0);
		}

		return;
	}

	for (INT i = 0; (i + 3) <= numIndices; i += 3) {
		if (!indexedTriangleInRange(Indices, i, NumPts))
			continue;

		if (!buf->indexTriangleFan(3))
			continue;

		for (INT j = 0; j < 3; j++) {
			Vertex_GouraudPolygon *v = (Vertex_GouraudPolygon *)buf->getVertex();
			const FTransTexture *pt = Pts[Indices ? Indices[i + j] : (i + j)];
			v->Pos = *(Vec3 *)&pt->Point.X;
			v->TexCoord.x = (pt->U) * diffuse.multU;
			v->TexCoord.y = (pt->V) * diffuse.multV;
			v->Color = *(Vec4 *)&pt->Light.X;
			v->Fog = *(Vec4 *)&pt->Fog.X;
			v->flags = flags;
			v->Color.w = alpha;
		}
	}
}

#endif //HARRYPOTTER
