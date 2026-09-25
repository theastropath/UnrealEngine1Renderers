
#include "TextureCache.h"
#include "../D3D10.h"

#include <algorithm>
#include <vector>


const TextureCache::ExternalTexture TextureCache::externalTextures[TextureCache::DUMMY_NUM_EXTERNAL_TEXTURES] = { { TEXT(".detail"), 1 }, { TEXT(".bump"), 0 }, { TEXT(".height"), 0 } };


TextureCache::TextureCache(ID3D10Device *device) :
	texturePasses(), //0 means nothing bound.
	device(device),
	lightmapAtlas(device),
	currentFrameCount(1), //Older than any frame.
	cacheBytes(0),
	budgetBytes(0),
	budgetUnreachable(false) {
}

/** \param pData One entry per level. */
ID3D10Texture2D *TextureCache::createTexture(const D3D10_TEXTURE2D_DESC &desc, const D3D10_SUBRESOURCE_DATA *pData) const {
	HRESULT hr;

	ID3D10Texture2D *texture;
	hr = device->CreateTexture2D(&desc, pData, &texture);
	if (FAILED(hr)) {
		UD3D10RenderDevice::debugs("Error creating texture resource.");
		return nullptr;
	}
	return texture;
}

bool TextureCache::updateMip(DWORD64 id, int mipNum, const D3D10_SUBRESOURCE_DATA &data, UINT rows, UINT rowBytes) const {
	std::unordered_map<DWORD64, CachedTexture>::const_iterator it = textureCache.find(id);
	if (it == textureCache.end())
		return false;

	const auto &entry = it->second;

	if (entry.atlased || entry.texture == nullptr)
		return false;

	D3D10_TEXTURE2D_DESC desc;
	entry.texture->GetDesc(&desc);
	if (mipNum < 0 || (UINT)mipNum >= desc.MipLevels)
		return false;

	for (int i = 0; i < TextureCache::DUMMY_NUM_TEXTURE_PASSES; i++) {
		if (texturePasses.boundTextureID[i] == id) {
			D3D::render();
			break;
		}
	}

	//device->UpdateSubresource(entry.texture,mipNum,nullptr,(void*) data.pSysMem,data.SysMemPitch,data.SysMemSlicePitch);

	//UpdateSubResource leads to flickering on nvidia
	D3D10_MAPPED_TEXTURE2D Mapping;
	if (FAILED(entry.texture->Map(mipNum, D3D10_MAP_WRITE_DISCARD, 0, &Mapping))) {
		//Unlogged: an animating immutable texture fails every frame.
		return false;
	}

	unsigned char *pDst = static_cast<unsigned char *>(Mapping.pData);
	const unsigned char *pSrc = static_cast<const unsigned char *>(data.pSysMem);

	const UINT mipRows = max(desc.Height >> mipNum, 1u);
	if (rows > mipRows)
		rows = mipRows;
	if (rowBytes > Mapping.RowPitch)
		rowBytes = Mapping.RowPitch;

	for (UINT y = 0; y < rows; y++) {
		memcpy(pDst, pSrc, rowBytes);
		pSrc += data.SysMemPitch;
		pDst += Mapping.RowPitch;
	}

	entry.texture->Unmap(mipNum);
	return true;
}

bool TextureCache::loadFileTexture(const TCHAR *fileName, ID3D10Texture2D **tex, D3DX10_IMAGE_LOAD_INFO *loadInfo) const {
	HRESULT hr;

	hr = D3DX10CreateTextureFromFile(device, fileName, loadInfo, nullptr, (ID3D10Resource **)tex, nullptr);
	if (FAILED(hr))
		return false;

	return true;
}

/**
\param extraIndex Optional external slot.
\return false when not cached.
*/
bool TextureCache::cacheTexture(unsigned __int64 id, const TextureMetaData &metadata, ID3D10Texture2D *tex, int extraIndex) {
	HRESULT hr;

	D3D10_TEXTURE2D_DESC desc;
	tex->GetDesc(&desc);
	ID3D10ShaderResourceView *r;
	D3D10_SHADER_RESOURCE_VIEW_DESC srDesc;
	srDesc.Format = desc.Format;
	srDesc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2D;
	srDesc.Texture2D.MostDetailedMip = 0;
	srDesc.Texture2D.MipLevels = desc.MipLevels;

	hr = device->CreateShaderResourceView(tex, &srDesc, &r);
	if (FAILED(hr)) {
		UD3D10RenderDevice::debugs("Error creating texture shader resource view.");
		return false;
	}

	if (extraIndex == -1) {
		std::unordered_map<DWORD64, CachedTexture>::iterator existing = textureCache.find(id);
		if (existing != textureCache.end()) {
			dropBinding(id);
			cacheBytes -= existing->second.bytes;
			releaseEntry(existing->second);
		}

		CachedTexture c;
		c.metadata = metadata;
		tex->AddRef();
		c.texture = tex;
		c.resourceView = r;
		for (int i = 0; i < DUMMY_NUM_EXTERNAL_TEXTURES; i++) {
			c.externalTextures[i] = nullptr;
		}
		c.lastUsedFrameCount = currentFrameCount;
		c.bytes = textureBytes(tex);
		c.atlased = false;
		cacheBytes += c.bytes;
		textureCache[id] = c;

		failedConversions.erase(id);
	} else {
		std::unordered_map<DWORD64, CachedTexture>::iterator parent = textureCache.find(id);
		if (parent == textureCache.end()) {
			UD3D10RenderDevice::debugs("Extra texture layer without a cached parent texture; dropping it.");
			SAFE_RELEASE(r);
			return false;
		}

		CachedTexture *c = &parent->second;
		SAFE_RELEASE(c->externalTextures[extraIndex]);
		c->externalTextures[extraIndex] = r;
		c->metadata.externalTextures[extraIndex] = true;

		DWORD layerBytes = textureBytes(tex);
		c->bytes += layerBytes;
		cacheBytes += layerBytes;
	}

	return true;
}

/** \return false when it did not fit. */
bool TextureCache::cacheAtlasedLightmap(DWORD64 id, TextureMetaData metadata, FLOAT uScale, FLOAT vScale,
	const void *rows, UINT pitchBytes, UINT w, UINT h) {
	LightmapAtlas::Placement placement;
	if (!lightmapAtlas.add(w, h, rows, pitchBytes, placement))
		return false;

	std::unordered_map<DWORD64, CachedTexture>::iterator existing = textureCache.find(id);
	if (existing != textureCache.end()) {
		dropBinding(id);
		cacheBytes -= existing->second.bytes;
		//The old tile goes back.
		if (existing->second.atlased)
			lightmapAtlas.release(existing->second.atlasPlacement);
		releaseEntry(existing->second);
		textureCache.erase(existing);
	}

	//Page size stands in for UClamp.
	const FLOAT invPage = 1.0f / (FLOAT)placement.pageSize;
	metadata.multU = (uScale != 0.0f) ? (1.0f / (uScale * (FLOAT)placement.pageSize)) : 0.0f;
	metadata.multV = (vScale != 0.0f) ? (1.0f / (vScale * (FLOAT)placement.pageSize)) : 0.0f;
	metadata.offsetU = (FLOAT)placement.x * invPage;
	metadata.offsetV = (FLOAT)placement.y * invPage;

	CachedTexture c;
	c.metadata = metadata;
	placement.view->AddRef();
	c.resourceView = placement.view;
	c.texture = nullptr; //Nothing to map.
	for (int i = 0; i < DUMMY_NUM_EXTERNAL_TEXTURES; i++) {
		c.externalTextures[i] = nullptr;
	}
	c.lastUsedFrameCount = currentFrameCount;
	c.bytes = 0;
	c.atlased = true;
	c.atlasPlacement = placement;
	textureCache[id] = c;

	return true;
}

bool TextureCache::updateAtlasedLightmap(DWORD64 id, const void *rows, UINT pitchBytes, UINT w, UINT h) {
	std::unordered_map<DWORD64, CachedTexture>::iterator i = textureCache.find(id);
	if (i == textureCache.end() || !i->second.atlased)
		return false;

	const LightmapAtlas::Placement &placement = i->second.atlasPlacement;
	if (placement.w != w || placement.h != h)
		return false;

	if (!lightmapAtlas.write(placement, rows, pitchBytes))
		return false;

	i->second.lastUsedFrameCount = currentFrameCount;
	return true;
}

bool TextureCache::textureIsAtlased(DWORD64 id) const {
	std::unordered_map<DWORD64, CachedTexture>::const_iterator i = textureCache.find(id);
	return (i != textureCache.end()) && i->second.atlased;
}

void TextureCache::releaseEntry(CachedTexture &entry) {
	SAFE_RELEASE(entry.texture);
	SAFE_RELEASE(entry.resourceView);
	for (int i = 0; i < DUMMY_NUM_EXTERNAL_TEXTURES; i++) {
		SAFE_RELEASE(entry.externalTextures[i]);
	}
}

/** Over the mip chain. */
DWORD TextureCache::textureBytes(ID3D10Texture2D *tex) {
	if (tex == nullptr)
		return 0;

	D3D10_TEXTURE2D_DESC desc;
	tex->GetDesc(&desc);

	UINT bitsPerPixel;
	bool blockCompressed = false;
	switch (desc.Format) {
		case DXGI_FORMAT_BC1_UNORM:
		case DXGI_FORMAT_BC1_UNORM_SRGB:
			bitsPerPixel = 4;
			blockCompressed = true;
			break;
		case DXGI_FORMAT_BC2_UNORM:
		case DXGI_FORMAT_BC2_UNORM_SRGB:
		case DXGI_FORMAT_BC3_UNORM:
		case DXGI_FORMAT_BC3_UNORM_SRGB:
			bitsPerPixel = 8;
			blockCompressed = true;
			break;
		case DXGI_FORMAT_B5G6R5_UNORM:
		case DXGI_FORMAT_B5G5R5A1_UNORM:
			bitsPerPixel = 16;
			break;
		default:
			bitsPerPixel = 32;
			break;
	}

	DWORD total = 0;
	for (UINT mip = 0; mip < desc.MipLevels; mip++) {
		UINT w = max(desc.Width >> mip, 1u);
		UINT h = max(desc.Height >> mip, 1u);
		if (blockCompressed) {
			w = max((w + 3) / 4, 1u) * 4;
			h = max((h + 3) / 4, 1u) * 4;
		}
		total += (w * h * bitsPerPixel) / 8;
	}

	return total;
}

bool TextureCache::isBound(DWORD64 id) const {
	for (int pass = 0; pass < DUMMY_NUM_TEXTURE_PASSES; pass++) {
		if (texturePasses.boundTextureID[pass] == id)
			return true;
	}
	return false;
}

/**
\param megs Zero disables eviction.
*/
void TextureCache::setBudget(int megs) {
	if (megs <= 0) {
		budgetBytes = 0;
		return;
	}

	//Past what this process can hold.
	if (megs > 2048)
		megs = 2048;
	budgetBytes = (size_t)megs * 1024 * 1024;
}

void TextureCache::newFrame() {
	currentFrameCount++;
	lightmapAtlas.newFrame();
}

size_t TextureCache::bytes() const {
	return cacheBytes + lightmapAtlas.bytes();
}

size_t TextureCache::evictableBytes() const {
	return cacheBytes;
}

/** Down to the budget. */
void TextureCache::evictOverBudget() {
	if (budgetBytes == 0)
		return;
	if (cacheBytes <= budgetBytes) {
		budgetUnreachable = false;
		return;
	}

	//Otherwise it spikes below the working set.
	enum { MAX_EVICTIONS_PER_FRAME = 16 };

	std::vector<std::pair<DWORD, DWORD64>> candidates;
	candidates.reserve(textureCache.size());
	for (std::unordered_map<DWORD64, CachedTexture>::const_iterator i = textureCache.begin(); i != textureCache.end(); ++i) {
		if (i->second.lastUsedFrameCount == currentFrameCount)
			continue;
		if (isBound(i->first))
			continue;
		if (i->second.atlased)
			continue;
		candidates.push_back(std::make_pair(i->second.lastUsedFrameCount, i->first));
	}

	//Only the oldest few.
	const size_t wanted = (candidates.size() < MAX_EVICTIONS_PER_FRAME) ? candidates.size() : MAX_EVICTIONS_PER_FRAME;
	std::partial_sort(candidates.begin(), candidates.begin() + wanted, candidates.end());

	size_t evicted = 0;
	for (size_t c = 0; c < wanted && cacheBytes > budgetBytes; c++) {
		deleteTexture(candidates[c].second);
		evicted++;
	}

	if ((cacheBytes > budgetBytes) && (evicted == candidates.size()) && !budgetUnreachable) {
		budgetUnreachable = true;
		char msg[192];
		_snprintf_s(msg, sizeof(msg), _TRUNCATE,
			"TextureCacheBudgetMegs cannot be met: %u MB is bound or in use this frame, against a budget of %u MB.",
			(unsigned int)(cacheBytes / (1024 * 1024)), (unsigned int)(budgetBytes / (1024 * 1024)));
		UD3D10RenderDevice::debugs(msg);
	}
}

void TextureCache::dropBinding(DWORD64 id) {
	for (int pass = 0; pass < DUMMY_NUM_TEXTURE_PASSES; pass++) {
		if (texturePasses.boundTextureID[pass] == id) {
			texturePasses.boundTextureID[pass] = 0;
			texturePasses.boundMetaData[pass] = nullptr;
			texturePasses.boundView[pass] = nullptr;
		}
	}
}

bool TextureCache::textureIsCached(DWORD64 id) const {
	return textureCache.find(id) != textureCache.end();
}

bool TextureCache::markConversionFailed(DWORD64 id) {
	return failedConversions.insert(id).second;
}

bool TextureCache::conversionFailed(DWORD64 id) const {
	return failedConversions.find(id) != failedConversions.end();
}

size_t TextureCache::size() const {
	return textureCache.size();
}

const TextureCache::TextureMetaData &TextureCache::getTextureMetaData(DWORD64 id) const {
	std::unordered_map<DWORD64, CachedTexture>::const_iterator i = textureCache.find(id);
	if (i == textureCache.end()) {
		//Wrong but finite.
		static const TextureMetaData missing = { 1.0f, 1.0f, 0.0f, 0.0f, false, { false, false, false }, 0 };
		return missing;
	}
	return i->second.metadata;
}

TextureCache::CachedTexture *TextureCache::findTexture(DWORD64 id) {
	std::unordered_map<DWORD64, CachedTexture>::iterator i = textureCache.find(id);
	if (i == textureCache.end())
		return nullptr;
	return &i->second;
}


/**
\note The early-out keys on pass and id alone, so a pass has to be asked for the same extra-texture
	slot every time, and switching slots under one id would go on drawing whatever that pass already had
	bound without ever noticing the difference.
*/
const TextureCache::TextureMetaData *TextureCache::setTexture(const Shader_Unreal *shader, TexturePass pass, DWORD64 id, int extraIndex) {
	CachedTexture *entry = (id != texturePasses.boundTextureID[pass]) ? findTexture(id) : nullptr;
	return setTexture(shader, pass, id, entry, extraIndex);
}

const TextureCache::TextureMetaData *TextureCache::setTexture(const Shader_Unreal *shader, TexturePass pass, DWORD64 id, CachedTexture *entry, int extraIndex) {
	if (id != texturePasses.boundTextureID[pass]) {
		if (entry == nullptr)
			return nullptr;

		texturePasses.boundTextureID[pass] = id;

		CachedTexture *tex = entry;
		ID3D10ShaderResourceView *view = (extraIndex == -1)
			? tex->resourceView
			: tex->externalTextures[extraIndex];

		if (view != texturePasses.boundView[pass]) {
			D3D::render();

			shader->setTexture(pass, view);
			texturePasses.boundView[pass] = view;
		}

		texturePasses.boundMetaData[pass] = &tex->metadata;

		tex->lastUsedFrameCount = currentFrameCount;
	}

	return texturePasses.boundMetaData[pass];
}

void TextureCache::deleteTexture(DWORD64 id) {
	failedConversions.erase(id);

	std::unordered_map<DWORD64, CachedTexture>::iterator i = textureCache.find(id);
	if (i == textureCache.end())
		return;

	dropBinding(id);
	cacheBytes -= i->second.bytes;
	//No texture, but a tile.
	if (i->second.atlased)
		lightmapAtlas.release(i->second.atlasPlacement);
	releaseEntry(i->second);
	textureCache.erase(i);
}

void TextureCache::flush() {
	for (int i = 0; i < DUMMY_NUM_TEXTURE_PASSES; i++) {
		texturePasses.boundTextureID[i] = 0;
		texturePasses.boundMetaData[i] = nullptr;
		texturePasses.boundView[i] = nullptr;
	}

	for (std::unordered_map<DWORD64, CachedTexture>::iterator i = textureCache.begin(); i != textureCache.end(); i++) {
		releaseEntry(i->second);
	}
	textureCache.clear();
	cacheBytes = 0;

	//Fresh try per package.
	failedConversions.clear();

	lightmapAtlas.reset();
}
