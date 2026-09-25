/**
\file Draw/ComplexSurface.cpp

A BSP surface, five layers, as fans.
*/

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "../D3D10.h"
#include "../Geometry/VertexFormats.h"
#include "../Geometry/Deferred.h"
#include "../Texture/TextureCache.h"
#include "../Texture/Precache.h"
#include "../Shaders/PolyFlags.h"
#include "../Shaders/Shader_ComplexSurface.h"


/**
Whether a complex surface belongs to a mover, which matters because a mover is not CSG'd into the
world and one sitting flush against a wall ends up fighting it for the same pixels unless it is given
a depth bias of its own.
*/
static bool isNonWorldModelSurface(const FSurfaceInfo &Surface) {
	if (Surface.LightMap == nullptr || Surface.Level == nullptr || Surface.Level->Model == nullptr)
		return false;
	const DWORD owner = (DWORD)((QWORD)Surface.LightMap->CacheID >> 32);
	if (owner == 0)
		return false;
	if (owner == (DWORD)Surface.Level->Model->GetIndex())
		return false;
	return owner != (DWORD)Surface.Level->GetIndex();
}

/**
Map geometry. Facets of triangle-fan polys.
\param Frame The scene.
\param Surface Texture passes for this surface.
- Texture: the diffuse texture.
- LightMap: precalculated lighting; drawn with a -.5 pan offset.
- MacroTexture: a detail texture for far surfaces; rarely used.
\param Facet Map coordinates, and a linked list of polys.
\note DetailTexture and FogMap are mutually exclusive. Handled in the shader.
*/
void UD3D10RenderDevice::DrawComplexSurface(FSceneNode *Frame, FSurfaceInfo &Surface, FSurfaceFacet &Facet) {
	makeContextCurrent();

	DWORD flags;

	//A replay wants gouraud.
	flushDeferredGouraudPolygonsForFrame(Frame);

	setSceneNodeIfChanged(Frame);

	D3D::switchToShader(D3D::SHADER_COMPLEXSURFACE);

	shader_ComplexSurface->setMoverBias(isNonWorldModelSurface(Surface));

	const TextureCache::TextureMetaData *diffuse = nullptr, *lightMap = nullptr, *detail = nullptr, *fogMap = nullptr, *macro = nullptr;

	if (Surface.Texture == nullptr)
		return;

	TextureCache::CachedTexture *diffuseEntry = precacheLayer(*this, *textureCache, *Surface.Texture, Surface.PolyFlags, false);
	if (!(diffuse = textureCache->setTexture(shader_ComplexSurface, TextureCache::PASS_DIFFUSE, Surface.Texture->CacheID, diffuseEntry)))
		return;

	flags = enginePolyFlags(Surface.PolyFlags);
	flags |= diffuse->customPolyFlags;

	shader_ComplexSurface->setFlags(flags);

	if (Surface.LightMap) {
		TextureCache::CachedTexture *entry = precacheLayer(*this, *textureCache, *Surface.LightMap, 0, true);
		if (!(lightMap = textureCache->setTexture(shader_ComplexSurface, TextureCache::PASS_LIGHT, Surface.LightMap->CacheID, entry)))
			return;
		shader_ComplexSurface->switchPass(TextureCache::PASS_LIGHT, 1);
	} else {
		shader_ComplexSurface->switchPass(TextureCache::PASS_LIGHT, 0);
	}

	//Shares the diffuse pan.
	const FTextureInfo *detailInfo = nullptr;
	if (diffuse && diffuse->externalTextures[TextureCache::EXTRA_TEX_DETAIL]) {
		if (!(detail = textureCache->setTexture(shader_ComplexSurface, TextureCache::PASS_DETAIL, Surface.Texture->CacheID, diffuseEntry, TextureCache::EXTRA_TEX_DETAIL)))
			return;
		detailInfo = Surface.Texture;
		shader_ComplexSurface->switchPass(TextureCache::PASS_DETAIL, 1);
	} else if (Surface.DetailTexture) {
		TextureCache::CachedTexture *entry = precacheLayer(*this, *textureCache, *Surface.DetailTexture, 0, false);
		if (!(detail = textureCache->setTexture(shader_ComplexSurface, TextureCache::PASS_DETAIL, Surface.DetailTexture->CacheID, entry)))
			return;
		detailInfo = Surface.DetailTexture;
		shader_ComplexSurface->switchPass(TextureCache::PASS_DETAIL, 1);
	} else {
		shader_ComplexSurface->switchPass(TextureCache::PASS_DETAIL, 0);
	}

	if (Surface.FogMap) {
		TextureCache::CachedTexture *entry = precacheLayer(*this, *textureCache, *Surface.FogMap, 0, false);
		if (!(fogMap = textureCache->setTexture(shader_ComplexSurface, TextureCache::PASS_FOG, Surface.FogMap->CacheID, entry)))
			return;
		shader_ComplexSurface->switchPass(TextureCache::PASS_FOG, 1);
	} else {
		shader_ComplexSurface->switchPass(TextureCache::PASS_FOG, 0);
	}
	if (Surface.MacroTexture) {
		TextureCache::CachedTexture *entry = precacheLayer(*this, *textureCache, *Surface.MacroTexture, 0, false);
		if (!(macro = textureCache->setTexture(shader_ComplexSurface, TextureCache::PASS_MACRO, Surface.MacroTexture->CacheID, entry)))
			return;
		shader_ComplexSurface->switchPass(TextureCache::PASS_MACRO, 1);
	} else {
		shader_ComplexSurface->switchPass(TextureCache::PASS_MACRO, 0);
	}

	if (diffuse && diffuse->externalTextures[TextureCache::EXTRA_TEX_BUMP]) {
		if (!textureCache->setTexture(shader_ComplexSurface, TextureCache::PASS_BUMP, Surface.Texture->CacheID, diffuseEntry, TextureCache::EXTRA_TEX_BUMP))
			return;
		shader_ComplexSurface->switchPass(TextureCache::PASS_BUMP, 1);
	}
#if KLINGON
	//Already locked.
	else if (Surface.BumpMap) {
		//No polyflags here.
		TextureCache::CachedTexture *entry = precacheLayer(*this, *textureCache, *Surface.BumpMap, 0, false);
		if (!textureCache->setTexture(shader_ComplexSurface, TextureCache::PASS_BUMP, Surface.BumpMap->CacheID, entry))
			return;
		shader_ComplexSurface->switchPass(TextureCache::PASS_BUMP, 1);
	}
#else
	//The inner UTexture* is optional.
	else if (Surface.Texture->Texture && Surface.Texture->Texture->BumpMap) {
		FTextureInfo texInfo;
#if UT_URENDERDEVICE
		Surface.Texture->Texture->BumpMap->Lock(texInfo, FTime(), 0, this);
#else
		Surface.Texture->Texture->BumpMap->Lock(texInfo, 0, 0, this);
#endif
		//As above.
		TextureCache::CachedTexture *entry = precacheLayer(*this, *textureCache, texInfo, 0, false);
		const bool bound = textureCache->setTexture(shader_ComplexSurface, TextureCache::PASS_BUMP, texInfo.CacheID, entry) != nullptr;
		Surface.Texture->Texture->BumpMap->Unlock(texInfo); //Even on a cache miss.
		if (!bound)
			return;
		shader_ComplexSurface->switchPass(TextureCache::PASS_BUMP, 1);
	}
#endif

	else {
		shader_ComplexSurface->switchPass(TextureCache::PASS_BUMP, 0);
	}

	if (diffuse && diffuse->externalTextures[TextureCache::EXTRA_TEX_HEIGHT]) {
		if (!textureCache->setTexture(shader_ComplexSurface, TextureCache::PASS_HEIGHT, Surface.Texture->CacheID, diffuseEntry, TextureCache::EXTRA_TEX_HEIGHT))
			return;
		shader_ComplexSurface->switchPass(TextureCache::PASS_HEIGHT, 1);
	} else {
		shader_ComplexSurface->switchPass(TextureCache::PASS_HEIGHT, 0);
	}

	FLOAT UDot = Facet.MapCoords.XAxis | Facet.MapCoords.Origin;
	FLOAT VDot = Facet.MapCoords.YAxis | Facet.MapCoords.Origin;

	const DWORD texturePassMask = shader_ComplexSurface->getTexturePassMask();

	FSurfaceLayer layers[5];
	unsigned int layerMask = 0;
	memset(layers, 0, sizeof(layers));

	layers[0].PanU = Surface.Texture->Pan.X;
	layers[0].PanV = Surface.Texture->Pan.Y;
	layers[0].MultU = diffuse->multU;
	layers[0].MultV = diffuse->multV;
	layerMask |= 1u << 0;

	if (Surface.LightMap) {
		layers[1].PanU = Surface.LightMap->Pan.X - 0.5f * Surface.LightMap->UScale;
		layers[1].PanV = Surface.LightMap->Pan.Y - 0.5f * Surface.LightMap->VScale;
		layers[1].MultU = lightMap->multU;
		layers[1].MultV = lightMap->multV;
		layers[1].OffsetU = lightMap->offsetU;
		layers[1].OffsetV = lightMap->offsetV;
		layerMask |= 1u << 1;
	}
	if (detailInfo) {
		layers[2].PanU = detailInfo->Pan.X;
		layers[2].PanV = detailInfo->Pan.Y;
		layers[2].MultU = detail->multU;
		layers[2].MultV = detail->multV;
		layerMask |= 1u << 2;
	}
	if (Surface.FogMap) {
		//Fogmaps require pan correction of -.5
		layers[3].PanU = Surface.FogMap->Pan.X - 0.5f * Surface.FogMap->UScale;
		layers[3].PanV = Surface.FogMap->Pan.Y - 0.5f * Surface.FogMap->VScale;
		layers[3].MultU = fogMap->multU;
		layers[3].MultV = fogMap->multV;
		layerMask |= 1u << 3;
	}
	if (Surface.MacroTexture) {
		layers[4].PanU = Surface.MacroTexture->Pan.X;
		layers[4].PanV = Surface.MacroTexture->Pan.Y;
		layers[4].MultU = macro->multU;
		layers[4].MultV = macro->multV;
		layerMask |= 1u << 4;
	}

	DeferredGeometry *deferred = D3D::getDeferred();
	//A repeating fault is worse than a one-frame hole.
	if (deferred->enabled() && !deferred->gaveUp()) {
		if (deferred->record(Facet, UDot, VDot, layers, layerMask, flags, texturePassMask))
			return;
		D3D::render();
	}

	//Constant over a surface.
	unsigned int activeLayers[5];
	unsigned int numActiveLayers = 0;
	for (unsigned int t = 0; t < 5; t++) {
		if (layerMask & (1u << t))
			activeLayers[numActiveLayers++] = t;
	}

	DynamicGeometryBuffer *buf = static_cast<DynamicGeometryBuffer *>(shader_ComplexSurface->getGeometryBuffer());
	for (FSavedPoly *Poly = Facet.Polys; Poly; Poly = Poly->Next) {
		if (Poly->NumPts < 3)
			continue;

		if (!buf->indexTriangleFan(Poly->NumPts))
			continue; //No storage.
		for (INT i = 0; i < Poly->NumPts; i++) {
			Vertex_ComplexSurface *v = (Vertex_ComplexSurface *)buf->getVertex();
			const FVector &point = Poly->Pts[i]->Point;

			v->Pos = *(Vec3 *)&point.X;

			FLOAT U = Facet.MapCoords.XAxis | point;
			FLOAT V = Facet.MapCoords.YAxis | point;
			FLOAT UCoord = U - UDot;
			FLOAT VCoord = V - VDot;

			for (unsigned int a = 0; a < numActiveLayers; a++) {
				const unsigned int t = activeLayers[a];
				const FSurfaceLayer &layer = layers[t];
				v->TexCoord[t].x = (UCoord - layer.PanU) * layer.MultU + layer.OffsetU;
				v->TexCoord[t].y = (VCoord - layer.PanV) * layer.MultV + layer.OffsetV;
			}

			v->flags = flags;
			v->texturePasses = texturePassMask;
		}
	}
}
