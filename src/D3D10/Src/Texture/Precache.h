/** \file Texture/Precache.h */

#pragma once

#include "TextureCache.h"

class UD3D10RenderDevice;

/**
\param cache For ids that failed conversion.
\param entry Cached entry, or null.
*/
bool textureNeedsPrecache(const TextureCache &cache, const TextureCache::CachedTexture *entry, const FTextureInfo &Info, DWORD PolyFlags);

/**
\param lightmap Take the atlas path.
\return Null when uncached.
*/
TextureCache::CachedTexture *precacheLayer(UD3D10RenderDevice &renderDevice, TextureCache &cache,
	FTextureInfo &Info, DWORD PolyFlags, bool lightmap);
