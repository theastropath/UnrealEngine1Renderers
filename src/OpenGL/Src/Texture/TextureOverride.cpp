/*=============================================================================
	TextureOverride.cpp: replacement textures read from disk.

	A pack drops a .dds beside the game and it stands in for the package texture
	of the same name. The layout and the naming are the D3D10 renderer's, which
	shipped first, so one pack serves all three; Shared\textureoverride.h holds
	the rules both ends follow and Shared\ddsfile.h does the parsing.

	What an override does not change is how the surface is mapped. The
	multipliers in CacheTextureInfo stay derived from the engine's own size and
	scale, so a replacement at four times the resolution lands on exactly the
	same part of the world as the texture it stands in for, and nothing outside
	this file needs to know a substitution happened.
=============================================================================*/

#include "../OpenGLDrv.h"
#include "../OpenGL.h"

#include <new>
#include <stdio.h>


/*-----------------------------------------------------------------------------
	Reaching the files.
-----------------------------------------------------------------------------*/

/*
Probed once. A game with no pack installed is the common case and it would
otherwise cost a failed directory lookup for every texture it ever binds.
*/
static bool OverrideDirectoryExists(void) {
	static bool checked = false;
	static bool exists = false;

	if (!checked) {
		checked = true;
		const DWORD attributes = GetFileAttributesA(TEXOVERRIDE_DIRECTORY);
		exists = (attributes != INVALID_FILE_ATTRIBUTES) &&
			((attributes & FILE_ATTRIBUTE_DIRECTORY) != 0);
		if (!exists) {
			debugf(NAME_Init, TEXT("No %s directory; texture overrides are off."), appFromAnsi(TEXOVERRIDE_DIRECTORY));
		}
	}

	return exists;
}

/**
Read a whole file.

\param ppData Receives a buffer the caller frees with delete[].
\return false when the file is absent, unreadable or implausibly large, none of
	which is an error worth reporting: a pack simply does not cover this texture.
*/
static bool ReadWholeFile(const char *pPath, BYTE **ppData, DWORD *pBytes) {
	*ppData = NULL;
	*pBytes = 0;

	//The checked variants throughout, so this file compiles without the deprecation warnings
	//the rest of the tree does not suppress.
	FILE *pFile = NULL;
	if ((fopen_s(&pFile, pPath, "rb") != 0) || (pFile == NULL)) {
		return false;
	}

	bool ok = false;
	do {
		if (fseek(pFile, 0, SEEK_END) != 0) {
			break;
		}
		const long size = ftell(pFile);
		//Bounded before anything is allocated for it, the length being the file's own claim.
		if ((size <= 0) || (size > (64 * 1024 * 1024))) {
			break;
		}
		if (fseek(pFile, 0, SEEK_SET) != 0) {
			break;
		}

		BYTE *pData = new (std::nothrow) BYTE[(size_t)size];
		if (pData == NULL) {
			break;
		}
		if (fread(pData, 1, (size_t)size, pFile) != (size_t)size) {
			delete[] pData;
			break;
		}

		*ppData = pData;
		*pBytes = (DWORD)size;
		ok = true;
	} while (0);

	fclose(pFile);
	return ok;
}

static bool BuildOverridePath(const FTextureInfo &Info, ETexOverrideKind kind, char *pPath, size_t pathBytes) {
	const UTexture *pTexture = TexInfoTexture(Info);
	if (pTexture == NULL) {
		return false;
	}

	const TCHAR *pFullName = pTexture->GetFullName();
	if (pFullName == NULL) {
		return false;
	}

	//An object name is ASCII, so the whole path is, and nothing here has to care which
	//of the three TCHAR conventions this build was compiled with.
	return TexOverrideBuildPath(appToAnsi(pFullName), kind, pPath, pathBytes);
}

/** The kind of override a cache id is asking for. */
static ETexOverrideKind OverrideKindOf(const FTextureInfo &Info) {
	return (((QWORD)Info.CacheID & TEXOVERRIDE_DETAIL_CACHEID_BIT) != 0)
		? TEXOVERRIDE_DETAIL
		: TEXOVERRIDE_DIFFUSE;
}

static bool FileExists(const char *pPath) {
	const DWORD attributes = GetFileAttributesA(pPath);
	return (attributes != INVALID_FILE_ATTRIBUTES) && ((attributes & FILE_ATTRIBUTE_DIRECTORY) == 0);
}

/** Reads the hex poly flags a sidecar carries, if it has one. */
static DWORD ReadOverridePolyFlags(const char *pDDSPath) {
	char flagsPath[TEXOVERRIDE_MAX_PATH];
	if (!TexOverrideBuildFlagsPath(pDDSPath, flagsPath, sizeof(flagsPath))) {
		return 0;
	}

	FILE *pFile = NULL;
	if ((fopen_s(&pFile, flagsPath, "r") != 0) || (pFile == NULL)) {
		return 0;
	}

	unsigned long flags = 0;
	if (fscanf_s(pFile, "%lx", &flags) != 1) {
		flags = 0;
	}
	fclose(pFile);

	return (DWORD)flags;
}


/*-----------------------------------------------------------------------------
	Deciding the bind.
-----------------------------------------------------------------------------*/

static GLenum OverrideInternalFormat(EDDSFormat format) {
	switch (format) {
		case DDS_FORMAT_DXT1:
			return GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
		case DDS_FORMAT_DXT3:
			return GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
		case DDS_FORMAT_DXT5:
			return GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
		default:
			return GL_RGBA8;
	}
}

/**
Whether this driver can take the format as it stands.

\note A DDS holds its colour in the order D3D wants, so an uncompressed one needs
	the BGRA extension rather than a swizzle pass of its own. Without it the
	replacement is declined and the package texture is drawn, which is a great
	deal better than drawing one with its red and blue exchanged.
*/
bool UOpenGLRenderDevice::OverrideFormatSupported(EDDSFormat format) const {
	switch (format) {
		case DDS_FORMAT_DXT1:
		case DDS_FORMAT_DXT3:
		case DDS_FORMAT_DXT5:
			/*
			The driver's capability and not the S3TC setting, which is about whether the
			game's own compressed textures are uploaded compressed. A replacement is a file
			the player chose to install in this format, so refusing it over that preference
			would only mean the texture they asked for does not appear. The D3D9 renderer
			decides this the same way, from its format caps.
			*/
			return SUPPORTS_GL_EXT_texture_compression_s3tc && SUPPORTS_GL_ARB_texture_compression;
		case DDS_FORMAT_BGRA8:
			return SUPPORTS_GL_EXT_bgra ? true : false;
		default:
			return false;
	}
}

/**
Note what a replacement asked for, keeping the list sorted by cache id.

\note Insertion is linear, and deliberately so: it happens once per replaced texture, on
	the miss that loads it, where the file read it follows costs orders of magnitude more.
	The lookup that runs per polygon is the one that had to be cheap.
*/
void FASTCALL UOpenGLRenderDevice::RecordOverride(QWORD CacheID, DWORD PolyFlags, bool hasDetail) {
	size_t lo = 0;
	size_t hi = m_overrideRecords.size();
	while (lo < hi) {
		const size_t mid = lo + ((hi - lo) / 2);
		if (m_overrideRecords[mid].CacheID < CacheID) {
			lo = mid + 1;
		} else {
			hi = mid;
		}
	}

	if ((lo < m_overrideRecords.size()) && (m_overrideRecords[lo].CacheID == CacheID)) {
		m_overrideRecords[lo].PolyFlags = PolyFlags;
		m_overrideRecords[lo].HasDetail = hasDetail ? 1 : 0;
		return;
	}

	FTextureOverrideRecord record;
	record.CacheID = CacheID;
	record.PolyFlags = PolyFlags;
	record.HasDetail = hasDetail ? 1 : 0;
	m_overrideRecords.insert(m_overrideRecords.begin() + lo, record);
}

const UOpenGLRenderDevice::FTextureOverrideRecord *FASTCALL UOpenGLRenderDevice::FindOverrideRecord(const FTextureInfo *pInfo) const {
	/*
	The usual answer, and the one a game with no pack installed gives every time.

	The setting is tested here and not only where a file is loaded because it can be turned
	off while the game runs, and records already taken would otherwise go on claiming a
	detail map that nothing will load any more, leaving the surface path to detail itself
	with the diffuse texture's own mips.
	*/
	if (!UseTextureOverrides || m_overrideRecords.empty() || (pInfo == NULL)) {
		return NULL;
	}

	const QWORD key = (QWORD)pInfo->CacheID;
	size_t lo = 0;
	size_t hi = m_overrideRecords.size();
	while (lo < hi) {
		const size_t mid = lo + ((hi - lo) / 2);
		if (m_overrideRecords[mid].CacheID < key) {
			lo = mid + 1;
		} else {
			hi = mid;
		}
	}

	if ((lo < m_overrideRecords.size()) && (m_overrideRecords[lo].CacheID == key)) {
		return &m_overrideRecords[lo];
	}
	return NULL;
}

void UOpenGLRenderDevice::ReleasePendingOverride(void) {
	delete[] m_pOverrideFileData;
	m_pOverrideFileData = NULL;
	m_overrideFileBytes = 0;
	m_overrideImageValid = false;
}

/**
Point a bind at a replacement file instead of the engine's mip chain.

\param UCopyBits,VCopyBits The padding the engine's own layout needed. Non-zero
	means the package texture does not fill the texture object it is uploaded
	into, and the multipliers were scaled for that, so a replacement sized to
	the object would be mapped across the padding as well.
\return true when the bind now describes the file, in which case the caller is
	finished: nothing about the engine's texture applies any more.
*/
bool FASTCALL UOpenGLRenderDevice::TryCacheOverrideTextureInfo(FCachedTexture *pBind, const FTextureInfo &Info,
	INT UCopyBits, INT VCopyBits) {
	//Whatever a previous decision left behind, in case its upload never ran.
	ReleasePendingOverride();

	if (!UseTextureOverrides) {
		return false;
	}
	if ((UCopyBits | VCopyBits) != 0) {
		return false;
	}
	//The engine rewrites these under us; a file cannot stand in for one.
	if (TexInfoVolatile(Info)) {
		return false;
	}
	if (!OverrideDirectoryExists()) {
		return false;
	}

	char path[TEXOVERRIDE_MAX_PATH];
	if (!BuildOverridePath(Info, OverrideKindOf(Info), path, sizeof(path))) {
		return false;
	}

	BYTE *pData;
	DWORD dataBytes;
	if (!ReadWholeFile(path, &pData, &dataBytes)) {
		return false;
	}

	FDDSImage image;
	if (!DDSParse(pData, dataBytes, image)) {
		debugf(NAME_Warning, TEXT("Texture override %s is not a DDS this renderer can upload; ignoring it."), appFromAnsi(path));
		delete[] pData;
		return false;
	}

	if (!OverrideFormatSupported(image.Format)) {
		debugf(NAME_Warning, TEXT("Texture override %s uses a format this driver does not support; ignoring it."), appFromAnsi(path));
		delete[] pData;
		return false;
	}

	/*
	MaxLogTextureSize is a budget the player set, so a replacement obeys it the
	same way the engine's own texture does, by starting further down the chain
	rather than by being refused.
	*/
	unsigned int startLevel = 0;
	while (startLevel < image.NumMips) {
		const FDDSLevel &level = image.Levels[startLevel];
		INT uBits = 0;
		INT vBits = 0;
		while ((1U << uBits) < level.USize) {
			uBits++;
		}
		while ((1U << vBits) < level.VSize) {
			vBits++;
		}
		if ((uBits <= MaxLogTextureSize) && (vBits <= MaxLogTextureSize)) {
			break;
		}
		startLevel++;
	}
	if (startLevel >= image.NumMips) {
		//Every level is larger than the player allows, and there is nothing below to fall back to.
		delete[] pData;
		return false;
	}

	const FDDSLevel &base = image.Levels[startLevel];
	INT UBits = 0;
	INT VBits = 0;
	while ((1U << UBits) < base.USize) {
		UBits++;
	}
	while ((1U << VBits) < base.VSize) {
		VBits++;
	}

	//As the engine's own path does, so a small texture keeps the levels the player asked for.
	INT MaxLevel = Min(UBits, VBits) - MinLogTextureSize;
	if (MaxLevel < 0) {
		MaxLevel = 0;
	}
	const INT availLevels = (INT)(image.NumMips - startLevel) - 1;
	if (MaxLevel > availLevels) {
		MaxLevel = availLevels;
	}

	pBind->texType = TEX_TYPE_OVERRIDE_DDS;
	pBind->texInternalFormat = OverrideInternalFormat(image.Format);
	//Unused for a compressed level, and the order a DDS stores for an uncompressed one.
	pBind->texSourceFormat = GL_BGRA_EXT;
	//Reused as the file's own starting level; the engine's mips are not read at all.
	pBind->BaseMip = (BYTE)startLevel;
	pBind->MaxLevel = (BYTE)MaxLevel;
	pBind->UBits = (BYTE)UBits;
	pBind->VBits = (BYTE)VBits;

	/*
	Only the diffuse carries flags. A detail override is reached through a cache
	id the engine never sees, so a sidecar on it would key an entry nothing could
	ever look up.
	*/
	if (OverrideKindOf(Info) == TEXOVERRIDE_DIFFUSE) {
		const DWORD customPolyFlags = ReadOverridePolyFlags(path);

		//Settled here, once, for the same reason the flags are.
		char detailPath[TEXOVERRIDE_MAX_PATH];
		const bool hasDetail = BuildOverridePath(Info, TEXOVERRIDE_DETAIL, detailPath, sizeof(detailPath)) &&
			FileExists(detailPath);

		if ((customPolyFlags != 0) || hasDetail) {
			RecordOverride((QWORD)Info.CacheID, customPolyFlags, hasDetail);
		}
	}

	m_pOverrideFileData = pData;
	m_overrideFileBytes = dataBytes;
	m_overrideImage = image;
	m_overrideImageValid = true;
	return true;
}


/*-----------------------------------------------------------------------------
	Uploading.
-----------------------------------------------------------------------------*/

/**
Hand the file's levels to the texture object the bind just had bound.

\param needTexAllocate Whether this name has no storage yet, which decides
	between defining a level and replacing one, exactly as the engine path does.
\note Called instead of the engine mip walk, not alongside it. The file was read
	while this bind was being decided and is released here either way.
*/
void FASTCALL UOpenGLRenderDevice::UploadOverrideLevels(FCachedTexture *pBind, INT MaxUploadLevel, bool needTexAllocate) {
	if (!m_overrideImageValid) {
		return;
	}

	/*
	The bind was described from what the decision above read, so a disagreement now
	would mean the two disagree about the layout being written into, and uploading
	anyway is how a texture upload becomes a read past the end of the file.
	*/
	const unsigned int startLevel = pBind->BaseMip;
	bool consistent = (startLevel < m_overrideImage.NumMips) &&
		(OverrideInternalFormat(m_overrideImage.Format) == pBind->texInternalFormat);
	if (consistent) {
		const FDDSLevel &base = m_overrideImage.Levels[startLevel];
		consistent = (base.USize == (1U << pBind->UBits)) && (base.VSize == (1U << pBind->VBits));
	}
	if (!consistent) {
		ReleasePendingOverride();
		return;
	}

	const bool blockCompressed = (m_overrideImage.Format != DDS_FORMAT_BGRA8);

	for (INT Level = 0; Level <= MaxUploadLevel; Level++) {
		const unsigned int sourceLevel = startLevel + (unsigned int)Level;
		if (sourceLevel >= m_overrideImage.NumMips) {
			break;
		}

		const FDDSLevel &source = m_overrideImage.Levels[sourceLevel];
		if (source.pData == NULL) {
			break;
		}

		if (blockCompressed) {
			if (needTexAllocate) {
				guard(glCompressedTexImage2D);
				glCompressedTexImage2DARB(
					GL_TEXTURE_2D,
					Level,
					pBind->texInternalFormat,
					source.USize,
					source.VSize,
					0,
					source.Bytes,
					source.pData);
				unguard;
			} else {
				guard(glCompressedTexSubImage2D);
				glCompressedTexSubImage2DARB(
					GL_TEXTURE_2D,
					Level,
					0,
					0,
					source.USize,
					source.VSize,
					pBind->texInternalFormat,
					source.Bytes,
					source.pData);
				unguard;
			}
		} else {
			if (needTexAllocate) {
				guard(glTexImage2D);
				glTexImage2D(
					GL_TEXTURE_2D,
					Level,
					pBind->texInternalFormat,
					source.USize,
					source.VSize,
					0,
					pBind->texSourceFormat,
					GL_UNSIGNED_BYTE,
					source.pData);
				unguard;
			} else {
				guard(glTexSubImage2D);
				glTexSubImage2D(
					GL_TEXTURE_2D,
					Level,
					0,
					0,
					source.USize,
					source.VSize,
					pBind->texSourceFormat,
					GL_UNSIGNED_BYTE,
					source.pData);
				unguard;
			}
		}
	}

	ReleasePendingOverride();
}
