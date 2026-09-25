#pragma once

class TextureCache;

#include <d3d10.h>
#include <d3dx10.h>
#include <unordered_map>
#include <unordered_set>
#include "../Shaders/Shader_Unreal.h"
#include "LightmapAtlas.h"


class TextureCache {

public:
	/** \note Trailing entry is a count. */
	enum TexturePass { PASS_DIFFUSE,
		PASS_LIGHT,
		PASS_DETAIL,
		PASS_FOG,
		PASS_MACRO,
		PASS_BUMP,
		PASS_HEIGHT,
		DUMMY_NUM_TEXTURE_PASSES };

	enum ExternalTextures { EXTRA_TEX_DETAIL,
		EXTRA_TEX_BUMP,
		EXTRA_TEX_HEIGHT,
		DUMMY_NUM_EXTERNAL_TEXTURES };

	struct ExternalTexture {
		const TCHAR *suffix;
		UINT mipLevels;
	};
	static const ExternalTexture externalTextures[DUMMY_NUM_EXTERNAL_TEXTURES];

	struct TextureMetaData {
		//UINT height;
		//UINT width;

		FLOAT multU;
		FLOAT multV;
		FLOAT offsetU;
		FLOAT offsetV;
		bool masked;
		bool externalTextures[DUMMY_NUM_EXTERNAL_TEXTURES]; /**< Extra slots in use. */
		DWORD customPolyFlags; /**< From the override file. */
	};

	struct CachedTexture {
		TextureMetaData metadata;
		ID3D10ShaderResourceView *resourceView;
		ID3D10Texture2D *texture;
		ID3D10ShaderResourceView *externalTextures[DUMMY_NUM_EXTERNAL_TEXTURES]; /**< Optional extras. */

		DWORD lastUsedFrameCount;
		DWORD bytes; /**< Approximate. */

		/** Eviction skips it. */
		bool atlased;

		LightmapAtlas::Placement atlasPlacement;
	};


private:
	struct
	{
		DWORD64 boundTextureID[DUMMY_NUM_TEXTURE_PASSES]; /**< CPU side. */
		const TextureMetaData *boundMetaData[DUMMY_NUM_TEXTURE_PASSES]; /**< Returned without rebinding. */
		ID3D10ShaderResourceView *boundView[DUMMY_NUM_TEXTURE_PASSES];
	} texturePasses;


	std::unordered_map<unsigned __int64, CachedTexture> textureCache;

	/** \note Its own set, so eviction sees real textures. */
	std::unordered_set<DWORD64> failedConversions;

	ID3D10Device *device;

	LightmapAtlas lightmapAtlas;

	/**@name Eviction state
	Without these the cache only shrinks on an engine flush, so a long session that streams through
	enough textures grows until the process runs out of address space and takes the game down with it.
	*/
	//@{
	DWORD currentFrameCount; /**< Stamped on bind. */
	size_t cacheBytes;
	size_t budgetBytes; /**< Zero disables eviction. */
	bool budgetUnreachable;
	//@}

	static void releaseEntry(CachedTexture &entry);
	static DWORD textureBytes(ID3D10Texture2D *tex);
	void dropBinding(DWORD64 id);
	bool isBound(DWORD64 id) const;

public:
	/**@name Texture cache */
	//@{

	TextureCache(ID3D10Device *device);
	~TextureCache() { flush(); }
	/** \param pData One entry per mip level. */
	ID3D10Texture2D *createTexture(const D3D10_TEXTURE2D_DESC &desc, const D3D10_SUBRESOURCE_DATA *pData) const;
	/** \return false on a failed write. */
	bool updateMip(DWORD64 id, int mipNum, const D3D10_SUBRESOURCE_DATA &data, UINT rows, UINT rowBytes) const;
	bool loadFileTexture(const TCHAR *fileName, ID3D10Texture2D **tex, D3DX10_IMAGE_LOAD_INFO *loadInfo) const;
	bool cacheTexture(DWORD64 id, const TextureMetaData &metadata, ID3D10Texture2D *tex, int extraIndex = -1);
	/**
	\param metadata Rewritten for the tile.
	\param uScale,vScale The lightmap's UV scale.
	\return false when it did not fit.
	*/
	bool cacheAtlasedLightmap(DWORD64 id, TextureMetaData metadata, FLOAT uScale, FLOAT vScale,
		const void *rows, UINT pitchBytes, UINT w, UINT h);
	/**
	\return false on a size change.
	*/
	bool updateAtlasedLightmap(DWORD64 id, const void *rows, UINT pitchBytes, UINT w, UINT h);
	bool textureIsCached(DWORD64 id) const;
	bool textureIsAtlased(DWORD64 id) const;
	/** For the statistics overlay. */
	const LightmapAtlas::Stats &lightmapAtlasStats() const { return lightmapAtlas.stats(); }
	UINT lightmapAtlasPages() const { return lightmapAtlas.pageCount(); }
	UINT lightmapAtlasFreeTiles() const { return lightmapAtlas.freeTileCount(); }
	/**
	\return true only the first time.
	*/
	bool markConversionFailed(DWORD64 id);
	bool conversionFailed(DWORD64 id) const;
	size_t size() const;
	const TextureMetaData &getTextureMetaData(DWORD64 id) const;
	/**
	A single lookup.
	\return null when the id is not cached.
	*/
	CachedTexture *findTexture(DWORD64 id);
	const TextureMetaData *setTexture(const Shader_Unreal *shader, TexturePass pass, DWORD64 id, int extraIndex = -1);
	/** \param entry Null if up to date. */
	const TextureMetaData *setTexture(const Shader_Unreal *shader, TexturePass pass, DWORD64 id, CachedTexture *entry, int extraIndex = -1);
	void deleteTexture(DWORD64 id);
	void flush();
	//@}

	/**@name Eviction */
	//@{
	void setBudget(int megs);
	void newFrame();
	void evictOverBudget();
	size_t bytes() const;
	size_t evictableBytes() const;
	//@}
};
