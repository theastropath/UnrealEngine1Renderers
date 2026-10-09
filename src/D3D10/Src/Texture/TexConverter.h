
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

	/**
	Load one override file.

	Shared\ddsfile.h is tried first so that a DDS is read by exactly the same code
	the D3D9 and OpenGL renderers read it with, and a pack therefore behaves the
	same on all three. D3DX10 is kept behind it for everything that parser declines,
	which is how packs holding PNG or uncompressed DDS keep working here.

	\param mipLevels Levels to take. 0 and D3DX10_DEFAULT both mean all of them,
		and the value reaches D3DX10 untouched when the fallback runs.
	\return false when neither route produced a texture.
	*/
	bool loadOverrideFile(const TCHAR *fileName, UINT mipLevels, ID3D10Texture2D **ppTexture) const;

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
