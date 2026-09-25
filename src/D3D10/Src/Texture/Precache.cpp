/** \file Texture/Precache.cpp */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "../D3D10.h"
#include "Precache.h"
#include "TextureCache.h"
#include "TexConverter.h"
#include "../Shaders/PolyFlags.h"


/**
\param cache For ids that failed conversion.
\param entry Or null.
*/
bool textureNeedsPrecache(const TextureCache &cache, const TextureCache::CachedTexture *entry, const FTextureInfo &Info, DWORD PolyFlags) {
	if (entry == nullptr) {
		//"Not cached" and "cannot be cached" look alike here.
		return !cache.conversionFailed(Info.CacheID);
	}
	if (TexInfoRealtimeChanged(Info))
		return true;
	//Masking is set unreliably, so rebuild when the surface's masked flag disagrees with the cached
	//one, and check both ways round because a wrong guess is permanent.
	return ((PolyFlags & PF_Masked) != 0) != entry->metadata.masked;
}

/**
\param lightmap Take the atlas path.
\return Null when uncached.
*/
TextureCache::CachedTexture *precacheLayer(UD3D10RenderDevice &renderDevice, TextureCache &cache,
	FTextureInfo &Info, DWORD PolyFlags, bool lightmap) {
	TextureCache::CachedTexture *entry = cache.findTexture(Info.CacheID);

	if (!textureNeedsPrecache(cache, entry, Info, PolyFlags))
		return entry;

	if (lightmap)
		renderDevice.precacheLightmap(Info);
	else
		renderDevice.PrecacheTexture(Info, PolyFlags);

	return cache.findTexture(Info.CacheID);
}

/**
Only called when the game precaches.
\param Info Includes the CacheID.
\param PolyFlags Texture flags.
\note Skipped unless dynamic.
*/
void UD3D10RenderDevice::PrecacheTexture(FTextureInfo &Info, DWORD PolyFlags) {
	makeContextCurrent();

	TextureCache::CachedTexture *cached = textureCache->findTexture(Info.CacheID);
	if (!textureNeedsPrecache(*textureCache, cached, Info, PolyFlags))
		return; //Cached, uncacheable, or unchanged.

	if (cached) {
		if (TexInfoRealtimeChanged(Info)) {
			if (texConverter->update(Info, PolyFlags))
				return;

			debugs("Texture could not be updated in place; recreating it as dynamic.");
			textureCache->deleteTexture(Info.CacheID);
		} else //Mask bit changed. Static.
		{
			textureCache->deleteTexture(Info.CacheID);
		}
	}

	if (!texConverter->loadOverride(Info, PolyFlags)) {
		texConverter->convertAndCache(Info, PolyFlags);
	}
}

/**
Prefers an atlas page.
\note Skips the override lookup.
*/
void UD3D10RenderDevice::precacheLightmap(FTextureInfo &Info) {
	makeContextCurrent();

	const TextureCache::CachedTexture *entry = textureCache->findTexture(Info.CacheID);
	if (entry && entry->atlased) {
		//Moving out breaks batching.
		if (texConverter->updateAtlasedLightmap(Info)) {
			//Consumed on success.
			TexInfoSetRealtimeChanged(Info, 0);
			return;
		}

		textureCache->deleteTexture(Info.CacheID);
	} else if (entry) {
		PrecacheTexture(Info, 0);
		return;
	}

	if (options.lightmapAtlas && texConverter->convertAndCacheLightmap(Info, 0)) {
		TexInfoSetRealtimeChanged(Info, 0);
		return;
	}

	PrecacheTexture(Info, 0);
}
