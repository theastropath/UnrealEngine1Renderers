
#pragma once
#include "TextureCache.h"
#include "../D3D10.h"

class TexConverter {
private:
	TextureCache *textureCache;

	/** Per engine format. */
	struct TextureFormat {
		bool supported;
		char blocksize; /**< Compressed formats only. */
		char pixelsPerBlock;
		bool directAssign; /**< No conversion, no scratch. */
		DXGI_FORMAT d3dFormat;
		void (*conversionFunc)(const FTextureInfo &, DWORD, void *, int);
	};
	static TexConverter::TextureFormat formats[];

	/**@name Format conversion functions */
	//@{
	static void fromPaletted(const FTextureInfo &Info, DWORD PolyFlags, void *target, int mipLevel);
	//@}

	static void convertMip(const FTextureInfo &Info, const TextureFormat &format, DWORD PolyFlags, int mipLevel, D3D10_SUBRESOURCE_DATA &data);
	/** \param extentU,extentV Zero means the clamp. */
	static TextureCache::TextureMetaData buildMetaData(const FTextureInfo &Info, DWORD PolyFlags, DWORD customPolyFlags = 0,
		UINT extentU = 0, UINT extentV = 0);
	//Can a tile hold it.
	bool lightmapSource(FTextureInfo &Info, const void *&rows, UINT &pitchBytes, UINT &w, UINT &h) const;
	/**
	Marks the id against retries.
	\param reason Narrow string.
	*/
	void conversionFailed(const FTextureInfo &Info, const char *reason) const;

public:
	TexConverter(TextureCache *textureCache);
	bool loadOverride(const FTextureInfo &Info, DWORD PolyFlags) const;
	void convertAndCache(FTextureInfo &Info, DWORD PolyFlags) const;
	/**
	Onto a shared atlas page.
	\return false if unpacked.
	*/
	bool convertAndCacheLightmap(FTextureInfo &Info, DWORD PolyFlags) const;
	/**
	\return false when it no longer fits.
	*/
	bool updateAtlasedLightmap(FTextureInfo &Info) const;
	/**
	Dynamic textures, in place.
	\return false to recreate.
	*/
	bool update(FTextureInfo &Info, DWORD PolyFlags) const;
};
