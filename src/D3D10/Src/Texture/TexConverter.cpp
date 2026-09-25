/**
\brief Converts Unreal textures to Direct3D 10 initial data.
\note Extent and stride follow the clamp, garbage lying outside it.
*/
#include <stdio.h>
#include <new>
#include <D3dx10.h>
#include "TexConverter.h"
#include "../Shaders/PolyFlags.h"

/**
Indexed by texture format. Rows stay contiguous and in enum order.
\note Row 1 keeps its channels; the shader expands them. See BGRA7_EXPAND.
*/
TexConverter::TextureFormat TexConverter::formats[] = {
	{ true, 0, 0, false, DXGI_FORMAT_R8G8B8A8_UNORM, &TexConverter::fromPaletted }, /**< TEXF_P8 = 0x00 */
	{ true, 0, 0, true, DXGI_FORMAT_R8G8B8A8_UNORM, nullptr }, /**< TEXF_RGBA7	= 0x01. See the note above */
	{ false, 0, 0, true, DXGI_FORMAT_R8G8B8A8_UNORM, nullptr }, /**< TEXF_RGB16	= 0x02 */
	{ true, 4, 8, true, DXGI_FORMAT_BC1_UNORM, nullptr }, /**< TEXF_DXT1 = 0x03 */
	{ false, 0, 0, true, DXGI_FORMAT_UNKNOWN, nullptr }, /**< TEXF_RGB8 = 0x04 */
	{ true, 0, 0, true, DXGI_FORMAT_R8G8B8A8_UNORM, nullptr }, /**< TEXF_RGBA8	= 0x05 */
	{ false, 0, 0, true, DXGI_FORMAT_UNKNOWN, nullptr }, /**< TEXF_NODATA = 0x06. Nothing to upload */
	{ true, 4, 16, true, DXGI_FORMAT_BC2_UNORM, nullptr }, /**< TEXF_DXT3 = 0x07. 16 bytes a block */
	{ true, 4, 16, true, DXGI_FORMAT_BC3_UNORM, nullptr }, /**< TEXF_DXT5 = 0x08 */
};

/** \param Info The first mip must be present. */
static bool usableDimensions(const FTextureInfo &Info) {
	return Info.USize > 0 && Info.VSize > 0 && Info.UClamp > 0 && Info.VClamp > 0 && Info.Mips[0]->USize > 0 && Info.Mips[0]->VSize > 0;
}

/** A disagreement with the fill is a heap overflow. */
static unsigned int mipRows(const FTextureInfo &Info, int mipLevel) {
	INT rows = max(Info.VClamp >> mipLevel, 1);
	if (rows > Info.Mips[mipLevel]->VSize)
		rows = Info.Mips[mipLevel]->VSize;
	return (unsigned int)max(rows, 1);
}

TextureCache::TextureMetaData TexConverter::buildMetaData(const FTextureInfo &Info, DWORD PolyFlags, DWORD customPolyFlags,
	UINT extentU, UINT extentV) {
	TextureCache::TextureMetaData metadata;
	const UINT normalizeU = (extentU != 0) ? extentU : (UINT)Info.UClamp;
	const UINT normalizeV = (extentV != 0) ? extentV : (UINT)Info.VClamp;
	const FLOAT uDenominator = Info.UScale * (FLOAT)normalizeU;
	const FLOAT vDenominator = Info.VScale * (FLOAT)normalizeV;
	metadata.multU = (uDenominator != 0.0f) ? (1.0f / uDenominator) : 0.0f;
	metadata.multV = (vDenominator != 0.0f) ? (1.0f / vDenominator) : 0.0f;
	metadata.offsetU = 0.0f;
	metadata.offsetV = 0.0f;
	metadata.masked = (PolyFlags & PF_Masked) != 0;
	metadata.customPolyFlags = customPolyFlags;
	for (int i = 0; i < TextureCache::DUMMY_NUM_EXTERNAL_TEXTURES; i++) {
		metadata.externalTextures[i] = false;
	}
	return metadata;
}
//Relative to System.
static const TCHAR *OVERRIDE_DIRECTORY = TEXT("..\\textures\\");

/** Four misses per texture. */
static bool overrideDirectoryExists() {
	static bool checked = false;
	static bool exists = false;

	if (!checked) {
		checked = true;
		const DWORD attributes = GetFileAttributes(OVERRIDE_DIRECTORY);
		exists = (attributes != INVALID_FILE_ATTRIBUTES) && ((attributes & FILE_ATTRIBUTE_DIRECTORY) != 0);
		if (!exists)
			UD3D10RenderDevice::debugs("No override texture directory; not looking for override textures.");
	}
	return exists;
}

/** \return true if one was present. */
bool TexConverter::loadOverride(const FTextureInfo &Info, DWORD PolyFlags) const {
	if (!overrideDirectoryExists())
		return false;

	if (TexInfoTexture(Info) == nullptr)
		return false;

	if (TexInfoVolatile(Info) != 0)
		return false;

	const TCHAR *texName = TexInfoTexture(Info)->GetFullName();

	if (_tcsstr(texName, TEXT("Texture ")) != texName) //No prefix on lightmaps.
		return false;

	TCHAR baseName[MAX_PATH];
	if (_sntprintf_s(baseName, MAX_PATH, _TRUNCATE, TEXT("%s%s"), OVERRIDE_DIRECTORY, texName + 8) < 0)
		return false;

	TCHAR *loc;
	while (loc = _tcschr(baseName + 2, '.')) {
		*loc = '\\';
	}

	TCHAR overrideFile[TextureCache::DUMMY_NUM_EXTERNAL_TEXTURES + 1][MAX_PATH];

	if (_sntprintf_s(overrideFile[0], MAX_PATH, _TRUNCATE, TEXT("%s.dds"), baseName) < 0)
		return false;

	for (int i = 0; i < TextureCache::DUMMY_NUM_EXTERNAL_TEXTURES; i++) {
		if (_sntprintf_s(overrideFile[i + 1], MAX_PATH, _TRUNCATE, TEXT("%s%s.dds"), baseName, TextureCache::externalTextures[i].suffix) < 0)
			return false;
	}

	ID3D10Texture2D *texture = nullptr;
	D3DX10_IMAGE_LOAD_INFO loadInfo;
	loadInfo.Width = D3DX10_DEFAULT;
	loadInfo.Height = D3DX10_DEFAULT;
	loadInfo.Depth = D3DX10_DEFAULT;
	loadInfo.Filter = D3DX10_DEFAULT;
	loadInfo.MipFilter = D3DX10_DEFAULT;
	loadInfo.FirstMipLevel = D3DX10_DEFAULT;
	loadInfo.Format = (DXGI_FORMAT)D3DX10_DEFAULT;
	loadInfo.MipLevels = D3DX10_DEFAULT;
	loadInfo.MiscFlags = D3DX10_DEFAULT;
	loadInfo.pSrcInfo = nullptr;
	loadInfo.Usage = D3D10_USAGE_IMMUTABLE;
	loadInfo.BindFlags = D3D10_BIND_SHADER_RESOURCE;
	loadInfo.CpuAccessFlags = 0;

	if (!textureCache->loadFileTexture(overrideFile[0], &texture, &loadInfo))
		return false;


	DWORD customPolyFlags = 0;

	TCHAR flagsFile[MAX_PATH];
	FILE *flagFile;
	if ((_sntprintf_s(flagsFile, MAX_PATH, _TRUNCATE, TEXT("%s.flags"), overrideFile[0]) >= 0) && (_tfopen_s(&flagFile, flagsFile, TEXT("r")) == 0)) {
		if (fscanf_s(flagFile, "%lx", &customPolyFlags) != 1)
			customPolyFlags = 0;
		fclose(flagFile);
	}

	TextureCache::TextureMetaData metadata = buildMetaData(Info, PolyFlags, customPolyFlags);
	const bool cached = textureCache->cacheTexture(Info.CacheID, metadata, texture);
	SAFE_RELEASE(texture);
	if (!cached)
		return false;

	for (int i = 0; i < TextureCache::DUMMY_NUM_EXTERNAL_TEXTURES; i++) {
		loadInfo.MipLevels = TextureCache::externalTextures[i].mipLevels;
		if (textureCache->loadFileTexture(overrideFile[1 + i], &texture, &loadInfo)) {
			textureCache->cacheTexture(Info.CacheID, metadata, texture, i);
			SAFE_RELEASE(texture);
		}
	}

	return true;
}

TexConverter::TexConverter(TextureCache *textureCache) {
	this->textureCache = textureCache;
}

void TexConverter::conversionFailed(const FTextureInfo &Info, const char *reason) const {
	if (textureCache->markConversionFailed(Info.CacheID))
		UD3D10RenderDevice::debugs(reason);
}

/**
Fill texture info and run the conversion for its pixel data.
\param Info Cache id, size, pixel data.
\param PolyFlags See polyflags.h.
*/
void TexConverter::convertAndCache(FTextureInfo &Info, DWORD PolyFlags) const {


	if (Info.Format > HIGHEST_SUPPORTED_TEXF) {
		conversionFailed(Info, "Unknown texture type.");
		return;
	}

	TextureFormat &format = formats[Info.Format];
	if (format.supported == false) {
		conversionFailed(Info, "Unsupported texture type.");
		return;
	}

	CLAMP(Info.NumMips, 0, MAX_MIPS); //Some report more than fits.

	if (Info.NumMips < 1 || Info.Mips[0] == nullptr) {
		conversionFailed(Info, "Texture has no mipmaps.");
		return;
	}

	if (!usableDimensions(Info)) {
		conversionFailed(Info, "Texture has an unusable size.");
		return;
	}

	if (Info.USize != Info.Mips[0]->USize) {
		float scale = (float)Info.Mips[0]->USize / Info.USize;
		Info.USize = Info.Mips[0]->USize;
		Info.UClamp *= scale;
		Info.UScale /= scale;
	}
	if (Info.VSize != Info.Mips[0]->VSize) {
		float scale = (float)Info.Mips[0]->VSize / Info.VSize;
		Info.VSize = Info.Mips[0]->VSize;
		Info.VClamp *= scale;
		Info.VScale /= scale;
	}

	CLAMP(Info.UClamp, 1, Info.Mips[0]->USize);
	CLAMP(Info.VClamp, 1, Info.Mips[0]->VSize);
	UINT texWidth = (UINT)Info.UClamp;
	UINT texHeight = (UINT)Info.VClamp;
	if (format.blocksize > 0) {
		const UINT block = (UINT)format.blocksize;
		texWidth = (texWidth + block - 1) / block * block;
		texHeight = (texHeight + block - 1) / block * block;
	}

	TextureCache::TextureMetaData metadata = buildMetaData(Info, PolyFlags, 0, texWidth, texHeight);
	//Mult normalizes texture coordinates, dividing once here.
	//metadata.width = Info.USize;
	//metadata.height = Info.VSize;
	//metadata.multU = 1.0 / (Info.UScale * Info.USize);
	//metadata.multV = 1.0 / (Info.VScale * Info.VSize);


	const bool dynamic = (TexInfoVolatile(Info) != 0);
	const int numLevels = dynamic ? 1 : Info.NumMips;

	D3D10_SUBRESOURCE_DATA *data = new (std::nothrow) D3D10_SUBRESOURCE_DATA[numLevels];
	if (data == nullptr) {
		return;
	}
	int builtLevels = 0;
	for (int i = 0; i < numLevels; i++) {
		if (Info.Mips[i] == nullptr || Info.Mips[i]->USize < 1 || Info.Mips[i]->VSize < 1)
			break;

		if (Info.Mips[i]->DataPtr == nullptr)
			break;

		if (((UINT)Info.Mips[i]->USize < max(texWidth >> i, 1u)) || ((UINT)Info.Mips[i]->VSize < max(texHeight >> i, 1u)))
			break;
		data[i].pSysMem = nullptr;
		convertMip(Info, format, PolyFlags, i, data[i]);
		if (data[i].pSysMem == nullptr)
			break;
		builtLevels++;
	}
	if (builtLevels < 1) {
		conversionFailed(Info, "Texture's first mip level could not be converted.");
		delete[] data;
		return;
	}

	D3D10_TEXTURE2D_DESC desc;
	desc.BindFlags = D3D10_BIND_SHADER_RESOURCE;
	desc.ArraySize = 1;
	desc.Height = texHeight;
	desc.Width = texWidth;
	desc.MipLevels = builtLevels;
	desc.MiscFlags = 0;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Format = format.d3dFormat;

	if (dynamic) {
		desc.Usage = D3D10_USAGE_DYNAMIC;
		desc.CPUAccessFlags = D3D10_CPU_ACCESS_WRITE;
	} else {
		desc.Usage = D3D10_USAGE_IMMUTABLE;
		desc.CPUAccessFlags = 0;
	}
	ID3D10Texture2D *texture = textureCache->createTexture(desc, data);
	if (texture != nullptr) {
		//Marking stops the retry.
		if (!textureCache->cacheTexture(Info.CacheID, metadata, texture))
			conversionFailed(Info, "Could not cache a converted texture; its surfaces will not be drawn.");

		/*	if( wcswcs(Info.Texture->GetFullName(),L"seal")  )
		{
			char buf[256];
			sprintf(buf,"d:\\%d.dds",Info.CacheID);
			D3DX10SaveTextureToFileA(texture,D3DX10_IFF_DDS,buf);
		}*/

		SAFE_RELEASE(texture);
	} else {
		//Marked first, for the name.
		const bool firstReport = textureCache->markConversionFailed(Info.CacheID);
		if (firstReport)
			UD3D10RenderDevice::debugs("Could not create a texture for:");
		if (firstReport && TexInfoTexture(Info) != nullptr) {
#if KLINGON
			UD3D10RenderDevice::debugs(TexInfoTexture(Info)->GetFullName());
#else
			char name[256];
			size_t converted;
			if (wcstombs_s(&converted, name, TexInfoTexture(Info)->GetFullName(), _TRUNCATE) == 0)
				UD3D10RenderDevice::debugs(name);
#endif
		}
	}

	if (!format.directAssign) {
		for (int i = 0; i < builtLevels; i++) {
			delete[] data[i].pSysMem;
		}
	}
	delete[] data;
}

/**
Whether this lightmap fits a shared tile, and where.
\param rows,pitchBytes The pitch is not width by four.
*/
bool TexConverter::lightmapSource(FTextureInfo &Info, const void *&rows, UINT &pitchBytes, UINT &w, UINT &h) const {
	if (Info.Format > HIGHEST_SUPPORTED_TEXF)
		return false;

	const TextureFormat &format = formats[Info.Format];

	if (!format.supported || !format.directAssign || format.blocksize > 0 || format.d3dFormat != DXGI_FORMAT_R8G8B8A8_UNORM)
		return false;

	if (Info.NumMips < 1 || Info.Mips[0] == nullptr || Info.Mips[0]->DataPtr == nullptr)
		return false;

	if (!usableDimensions(Info))
		return false;

	if (Info.USize != Info.Mips[0]->USize) {
		float scale = (float)Info.Mips[0]->USize / Info.USize;
		Info.USize = Info.Mips[0]->USize;
		Info.UClamp *= scale;
		Info.UScale /= scale;
	}
	if (Info.VSize != Info.Mips[0]->VSize) {
		float scale = (float)Info.Mips[0]->VSize / Info.VSize;
		Info.VSize = Info.Mips[0]->VSize;
		Info.VClamp *= scale;
		Info.VScale /= scale;
	}

	w = (UINT)max(Info.UClamp, 1);
	h = (UINT)max(Info.VClamp, 1);

	if (w > (UINT)Info.Mips[0]->USize || h > (UINT)Info.Mips[0]->VSize)
		return false;

	rows = Info.Mips[0]->DataPtr;
	pitchBytes = (UINT)Info.Mips[0]->USize * sizeof(DWORD);
	return true;
}

/** Cache a lightmap onto a shared atlas page. */
bool TexConverter::convertAndCacheLightmap(FTextureInfo &Info, DWORD PolyFlags) const {
	const void *rows = nullptr;
	UINT pitchBytes = 0, w = 0, h = 0;
	if (!lightmapSource(Info, rows, pitchBytes, w, h))
		return false;

	const TextureCache::TextureMetaData metadata = buildMetaData(Info, PolyFlags);

	return textureCache->cacheAtlasedLightmap(Info.CacheID, metadata, Info.UScale, Info.VScale,
		rows, pitchBytes, w, h);
}

/** \return false when it no longer fits. */
bool TexConverter::updateAtlasedLightmap(FTextureInfo &Info) const {
	const void *rows = nullptr;
	UINT pitchBytes = 0, w = 0, h = 0;
	if (!lightmapSource(Info, rows, pitchBytes, w, h))
		return false;

	return textureCache->updateAtlasedLightmap(Info.CacheID, rows, pitchBytes, w, h);
}

/**
\return false only when the map failed.
*/
bool TexConverter::update(FTextureInfo &Info, DWORD PolyFlags) const {
	if (Info.Format > HIGHEST_SUPPORTED_TEXF) {
		UD3D10RenderDevice::debugs("Unknown texture type.");
		return true;
	}

	const TextureFormat &format = formats[Info.Format];
	if (format.supported == false) {
		UD3D10RenderDevice::debugs("Unsupported texture type.");
		return true;
	}

	TexInfoSetRealtimeChanged(Info, 0);

	if (format.blocksize > 0) {
		UD3D10RenderDevice::debugs("Cannot update a block compressed texture.");
		return true;
	}

	if (Info.NumMips < 1 || Info.Mips[0] == nullptr || Info.Mips[0]->USize < 1 || Info.Mips[0]->VSize < 1 || Info.Mips[0]->DataPtr == nullptr)
		return true;

	if (!usableDimensions(Info)) {
		UD3D10RenderDevice::debugs("Update: texture has an unusable size.");
		return true;
	}

	D3D10_SUBRESOURCE_DATA data;
	data.pSysMem = nullptr;
	convertMip(Info, format, PolyFlags, 0, data);
	if (data.pSysMem == nullptr)
		return true;
	const UINT rows = mipRows(Info, 0);
	const UINT rowBytes = min(max((UINT)Info.UClamp, 1u), (UINT)Info.Mips[0]->USize) * sizeof(DWORD);
	const bool updated = textureCache->updateMip(Info.CacheID, 0, data, rows, rowBytes);

	if (!format.directAssign)
		delete[] data.pSysMem;

	if (!updated) {
		TexInfoSetRealtimeChanged(Info, 1);
	}

	return updated;
}

/**
\param mipLevel Which mip to convert.
*/
void TexConverter::convertMip(const FTextureInfo &Info, const TextureFormat &format, DWORD PolyFlags, int mipLevel, D3D10_SUBRESOURCE_DATA &data) {
	if (format.blocksize > 0) {
		data.SysMemPitch = max(Info.Mips[mipLevel]->USize, format.blocksize) * format.pixelsPerBlock / format.blocksize; //At least block sized.
	} else {
		data.SysMemPitch = Info.Mips[mipLevel]->USize * sizeof(DWORD); //Skips garbage outside UClamp.
	}

	if (format.directAssign) {
		data.pSysMem = Info.Mips[mipLevel]->DataPtr;
	} else {
		const unsigned int rows = mipRows(Info, mipLevel);
		data.pSysMem = new (std::nothrow) DWORD[Info.Mips[mipLevel]->USize * rows];
		if (data.pSysMem == nullptr) {
			UD3D10RenderDevice::debugs("Convert: Error allocating texture initial data memory.");
			return;
		}
		format.conversionFunc(Info, PolyFlags, (void *)data.pSysMem, mipLevel);
	}
}

/**
Convert from paletted 8bpp to R8G8B8A8.
\note Masked textures write palette index 0 as transparent black, because clearing the palette entry
	itself would punch a hole through every other texture that happens to share that palette.
*/
void TexConverter::fromPaletted(const FTextureInfo &Info, DWORD PolyFlags, void *target, int mipLevel) {
	const bool masked = (PolyFlags & PF_Masked) != 0;

	DWORD *dest = (DWORD *)target;
	BYTE *source = (BYTE *)Info.Mips[mipLevel]->DataPtr;
	if (Info.Palette == nullptr)
		return;
	const DWORD *palette = (const DWORD *)&(Info.Palette->R);

	const unsigned int uSize = Info.Mips[mipLevel]->USize;
	const unsigned int rows = mipRows(Info, mipLevel);

	BYTE *sourceEnd = source + uSize * rows;
	if (masked) {
		while (source < sourceEnd) {
			*dest = (*source == 0) ? 0 : palette[*source];
			source++;
			dest++;
		}
	} else {
		while (source < sourceEnd) {
			*dest = palette[*source];
			source++;
			dest++;
		}
	}
}

//Swizzled in-shader, so no converter.
