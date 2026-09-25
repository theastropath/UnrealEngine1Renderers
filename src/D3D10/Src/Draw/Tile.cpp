/** \file Draw/Tile.cpp */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>

#include "../D3D10.h"
#include "../Geometry/VertexFormats.h"
#include "../Texture/TextureCache.h"
#include "../Texture/Precache.h"
#include "../Shaders/PolyFlags.h"
#include "../Shaders/Shader_Tile.h"


/**
2D UI elements and smoke.
\param Frame The scene.
\param Info The quad's texture.
\param X,Y Screen-space position.
\param XL,YL Size in pixels.
\param U,V Top left corner.
\param UL,VL Added for the opposite corner.
\param Span Probably for software renderers.
\param Z Depth coordinate.
\param Color Color.
\param Fog Unused.
\param PolyFlags Tile flags.
\note Deus Ex letterboxes go unreported.
*/
void UD3D10RenderDevice::DrawTile(FSceneNode *Frame, FTextureInfo &Info, FLOAT X, FLOAT Y, FLOAT XL, FLOAT YL, FLOAT U, FLOAT V, FLOAT UL, FLOAT VL, class FSpanBuffer *Span, FLOAT Z, FPlane Color, FPlane Fog, DWORD PolyFlags) {
	makeContextCurrent();
	flushDeferredGouraudPolygons(); //Before EndFlash switches buffers.

#if DEUSEX
	//Deus Ex never reliably ends the world pass before its UI, so the UI's start is inferred from the first
	//unblended tile at the near plane, and narrowing that to textures named 'Corona' again only means some
	//other flare ends the world pass early and paints over the finished frame.
	if (Z == 1 && !drawingHUD && !(PolyFlags & (PF_Translucent | PF_Modulated))) {
		EndFlash();
	}
#endif
	D3D::switchToShader(D3D::SHADER_TILE);
	setSceneNodeIfChanged(Frame);

	TextureCache::CachedTexture *entry = precacheLayer(*this, *textureCache, Info, PolyFlags, false);
	const TextureCache::TextureMetaData *diffuse = nullptr;
	if (!(diffuse = textureCache->setTexture(shader_Tile, TextureCache::PASS_DIFFUSE, Info.CacheID, entry)))
		return;

	//if(TexInfoRealtimeChanged(Info)) //DEUS EX: use this  to catch zyme, toxins etc
	{}

	DWORD flags = enginePolyFlags(PolyFlags) | diffuse->customPolyFlags;
	/*
	Palette entry 128 not opaque marks a font or UI page, submitted with no blend flag, so left alone it
	paints the whole glyph cell and occludes later HUD tiles.
	*/
	if (Info.Palette && Info.Palette[128].A != 255 && !(flags & PF_Translucent))
		flags |= PF_Highlighted;

	if (logTiles)
		logTileOnce(Info, PolyFlags, flags);

	shader_Tile->setFlags(flags);
	DynamicGeometryBuffer *buf = static_cast<DynamicGeometryBuffer *>(shader_Tile->getGeometryBuffer());
	if (!buf->indexSingleVertex())
		return;
	Vertex_Tile *v = (Vertex_Tile *)buf->getVertex();

	v->XYWH.x = X;
	v->XYWH.y = Y;
	v->XYWH.z = XL;
	v->XYWH.w = YL;
	v->UVWH.x = U * diffuse->multU;
	v->UVWH.y = V * diffuse->multV;
	v->UVWH.z = UL * diffuse->multU;
	v->UVWH.w = VL * diffuse->multV;
	v->z = Z;
	v->Color = *((Vec4 *)&Color.X);

#if RUNE
	//Absent reads as full opacity.
	if ((PolyFlags & PF_AlphaBlend) && Info.Texture) {
		v->Color.w = (Info.Texture->Alpha);
	} else {
#endif
		v->Color.w = 1.0f;
#if RUNE
	}
#endif

	v->flags = flags;
}

/**
One line per distinct texture.
\note The cache id prints as two halves.
*/
void UD3D10RenderDevice::logTileOnce(const FTextureInfo &Info, DWORD polyFlags, DWORD flags) {
	const QWORD cacheID = (QWORD)Info.CacheID;
	for (int i = 0; i < numLoggedTiles; i++) {
		if (loggedTileIds[i] == cacheID)
			return;
	}
	if (numLoggedTiles >= MAX_LOGGED_TILES)
		return;
	loggedTileIds[numLoggedTiles++] = cacheID;

	char msg[160];
	_snprintf_s(msg, sizeof(msg), _TRUNCATE,
		"DrawTile id %08X%08X format %i palette %i marker %i PolyFlags %08X drawn %08X",
		(DWORD)(cacheID >> 32), (DWORD)cacheID,
		(int)Info.Format,
		Info.Palette ? 1 : 0,
		(Info.Palette && Info.Palette[128].A != 255) ? 1 : 0,
		polyFlags, flags);
	UD3D10RenderDevice::debugs(msg);
}
