/*=============================================================================
	TextureInfo.cpp: choosing a storage format and a conversion.
=============================================================================*/

#include "../OpenGLDrv.h"
#include "../OpenGL.h"

void UOpenGLRenderDevice::CacheTextureInfo(FCachedTexture *pBind, const FTextureInfo &Info, DWORD PolyFlags) {
//Klingon has no Texture member.
#if 0
{
	dbgPrintf("utglr: CacheId = %08X:%08X\n",
		(DWORD)((QWORD)Info.CacheID >> 32), (DWORD)((QWORD)Info.CacheID & 0xFFFFFFFF));
}
#ifndef UTGLR_KLINGON_BUILD
{
	const UTexture *pTexture = Info.Texture;
	const TCHAR *pName = pTexture->GetFullName();
	if (pName) dbgPrintf("utglr: TexName = %s\n", appToAnsi(pName));
}
#endif
{
	dbgPrintf("utglr: NumMips = %d\n", Info.NumMips);
}
{
	unsigned int u;
	TCHAR dbgStr[1024];
	TCHAR numStr[32];

	dbgStr[0] = _T('\0');
	appStrcat(dbgStr, TEXT("utglr: ZPBindTree Size = "));
	for (u = 0; u < NUM_CTTree_TREES; u++) {
		appSprintf(numStr, TEXT("%u"), m_zeroPrefixBindTrees[u].calc_size());
		appStrcat(dbgStr, numStr);
		if (u != (NUM_CTTree_TREES - 1)) appStrcat(dbgStr, TEXT(", "));
	}
	dbgPrintf("%s\n", appToAnsi(dbgStr));

	dbgStr[0] = _T('\0');
	appStrcat(dbgStr, TEXT("utglr: NZPBindTree Size = "));
	for (u = 0; u < NUM_CTTree_TREES; u++) {
		appSprintf(numStr, TEXT("%u"), m_nonZeroPrefixBindTrees[u].calc_size());
		appStrcat(dbgStr, numStr);
		if (u != (NUM_CTTree_TREES - 1)) appStrcat(dbgStr, TEXT(", "));
	}
	dbgPrintf("%s\n", appToAnsi(dbgStr));
}
#endif

	//Recorded first, always.
	pBind->srcUSize = Info.USize;
	pBind->srcVSize = Info.VSize;
	pBind->srcNumMips = Info.NumMips;
	pBind->srcFormat = (BYTE)Info.Format;
	pBind->srcHasPalette = (Info.Palette != NULL) ? 1 : 0;

	//A truncated texture can have no base mip.
	//Refused below, so the decision is not repeated.
	if ((Info.NumMips < 1) || (Info.Mips[0] == NULL)) {
		pBind->texType = TEX_TYPE_NONE;
		pBind->texSourceFormat = GL_RGBA;
		pBind->texInternalFormat = GL_RGBA8;
		pBind->BaseMip = 0;
		pBind->MaxLevel = 0;
		pBind->UBits = 0;
		pBind->VBits = 0;
		pBind->UMult = 0.0f;
		pBind->VMult = 0.0f;
		pBind->UClampVal = 0;
		pBind->VClampVal = 0;
		debugf(NAME_Warning, TEXT("Texture has no base mipmap; texture not uploaded"));
		return;
	}

	DWORD texFlags = 0;
	INT BaseMip = 0;
	INT MaxLevel;
	INT UBits = Info.Mips[0]->UBits;
	INT VBits = Info.Mips[0]->VBits;
	INT UCopyBits = 0;
	INT VCopyBits = 0;
	if ((UBits - VBits) > MaxLogUOverV) {
		VCopyBits += (UBits - VBits) - MaxLogUOverV;
		VBits = UBits - MaxLogUOverV;
	}
	if ((VBits - UBits) > MaxLogVOverU) {
		UCopyBits += (VBits - UBits) - MaxLogVOverU;
		UBits = VBits - MaxLogVOverU;
	}
	if (UBits < MinLogTextureSize) {
		UCopyBits += MinLogTextureSize - UBits;
		UBits += MinLogTextureSize - UBits;
	}
	if (VBits < MinLogTextureSize) {
		VCopyBits += MinLogTextureSize - VBits;
		VBits += MinLogTextureSize - VBits;
	}
	if (UBits > MaxLogTextureSize) {
		BaseMip += UBits - MaxLogTextureSize;
		VBits -= UBits - MaxLogTextureSize;
		UBits = MaxLogTextureSize;
		if (VBits < 0) {
			//Accumulated like the clamps above.
			VCopyBits += -VBits;
			VBits = 0;
		}
	}
	if (VBits > MaxLogTextureSize) {
		BaseMip += VBits - MaxLogTextureSize;
		UBits -= VBits - MaxLogTextureSize;
		VBits = MaxLogTextureSize;
		if (UBits < 0) {
			UCopyBits += -UBits;
			UBits = 0;
		}
	}

	pBind->BaseMip = BaseMip;
	MaxLevel = Min(UBits, VBits) - MinLogTextureSize;
	if (MaxLevel < 0) {
		MaxLevel = 0;
	}
	pBind->MaxLevel = MaxLevel;
	pBind->UBits = UBits;
	pBind->VBits = VBits;

	pBind->UMult = 1.0f / (Info.UScale * (Info.USize << UCopyBits));
	pBind->VMult = 1.0f / (Info.VScale * (Info.VSize << VCopyBits));

	pBind->UClampVal = Info.UClamp - 1;
	pBind->VClampVal = Info.VClamp - 1;

	if (((Info.UClamp ^ Info.USize) | (Info.VClamp ^ Info.VSize)) == 0) {
		texFlags |= TEX_FLAG_NO_CLAMP;
	}


	//PF_Masked cannot change once a texture is updated.
	bool paletted = false;
	if (UsePalette && Info.Palette) {
		paletted = true;
		if (!UseAlphaPalette) {
			if ((PolyFlags & PF_Masked) || (Info.Palette[0].A != 255)) {
				paletted = false;
			}
		}
	}

	pBind->texType = TEX_TYPE_NONE;

	/*
	Dispatch is by source format because the mip layout follows the format and the doubling
	arithmetic these use is only carry free up to a 7 bit channel, so an 8 bit format such as
	TEXF_RGBA8 decodes wrong and only the formats this renderer reads correctly are accepted,
	a merely unrecognised format being the safer failure because it draws the no-texture
	object where a wrongly recognised one reads past the end of the mip.
	*/
	DWORD srcDXTType = 0;
	bool srcFormatSupported = false;
	switch (Info.Format) {
		case TEXF_P8:
		case TEXF_RGBA7:
			srcFormatSupported = true;
			break;

		case TEXF_DXT1:
			srcDXTType = 1;
			srcFormatSupported = true;
			break;

#ifdef UTGLR_HAS_DXT3_DXT5
		case TEXF_DXT3:
			srcDXTType = 3;
			srcFormatSupported = true;
			break;

		case TEXF_DXT5:
			srcDXTType = 5;
			srcFormatSupported = true;
			break;
#endif

		default:;
	}

	if (!srcFormatSupported) {
		/*
		Left marked unsupported, so later use costs one lookup, and the formats are still filled
		in because a texture object is generated and bound regardless while the recycling paths
		read them whether the upload ever happens or not, a zeroed pair being enough to confuse
		them, and the entry is cheaper to keep than to re-derive every time the surface using it
		comes back into view.
		*/
		pBind->texSourceFormat = GL_RGBA;
		pBind->texInternalFormat = GL_RGBA8;
		debugf(NAME_Warning, TEXT("Unsupported texture format %i; texture not uploaded"), (INT)Info.Format);
		return;
	}

	if (srcDXTType != 0) {
		//Compressed blocks cannot be resampled.
		//Every level must exist in the source.
		INT availLevels = (INT)Info.NumMips - 1 - BaseMip;
		if (availLevels < 0) {
			availLevels = 0;
		}
		if (MaxLevel > availLevels) {
			MaxLevel = availLevels;
			pBind->MaxLevel = MaxLevel;
		}

		//The base mip must exist and the driver must take the format.
		//Earlier clamps can inflate the bit counts.
		//Declining here routes to the decoder below.
		if (SupportsTC && (BaseMip < (INT)Info.NumMips) && ((UCopyBits | VCopyBits) == 0)) {
			if (srcDXTType == 1) {
				if (TexDXT1ToDXT3 && (!(PolyFlags & PF_Masked))) {
					pBind->texType = TEX_TYPE_COMPRESSED_DXT1_TO_DXT3;
					//texSourceFormat is unused for compressed textures.
					pBind->texInternalFormat = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
				} else {
					pBind->texType = TEX_TYPE_COMPRESSED_DXT1;
					pBind->texInternalFormat = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
				}
			} else if (srcDXTType == 3) {
				pBind->texType = TEX_TYPE_COMPRESSED_DXT3;
				pBind->texInternalFormat = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
			} else {
				pBind->texType = TEX_TYPE_COMPRESSED_DXT5;
				pBind->texInternalFormat = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
			}
		}

		//Otherwise decode during upload.
		if (pBind->texType == TEX_TYPE_NONE) {
			pBind->texType = (srcDXTType == 1) ? TEX_TYPE_DECOMPRESSED_DXT1 : ((srcDXTType == 3) ? TEX_TYPE_DECOMPRESSED_DXT3 : TEX_TYPE_DECOMPRESSED_DXT5);
			pBind->texSourceFormat = GL_RGBA;
			pBind->texInternalFormat = GL_RGBA8;

			MaxLevel = Min(UBits, VBits) - MinLogTextureSize;
			if (MaxLevel < 0) {
				MaxLevel = 0;
			}
			pBind->MaxLevel = MaxLevel;
		}
	}

	if (pBind->texType != TEX_TYPE_NONE) {
		//Using compressed texture, or decoding on upload.
	} else if (paletted) {
		pBind->texType = TEX_TYPE_PALETTED;
		pBind->texSourceFormat = GL_COLOR_INDEX;
		pBind->texInternalFormat = GL_COLOR_INDEX8_EXT;
	} else if (Info.Palette) {
		pBind->texType = TEX_TYPE_HAS_PALETTE;
		pBind->texSourceFormat = GL_RGBA;
		pBind->texInternalFormat = GL_RGBA8;
		if (PolyFlags & PF_Memorized) {
			pBind->texInternalFormat = (PolyFlags & PF_Masked) ? GL_RGB5_A1 : GL_RGB5;
		}
	} else {
		pBind->texType = TEX_TYPE_NORMAL;
		if (UseBGRATextures) {
			pBind->texSourceFormat = GL_BGRA_EXT;
			if (texFlags & TEX_FLAG_NO_CLAMP) {
				pBind->pConvertBGRA7777 = &UOpenGLRenderDevice::ConvertBGRA7777_BGRA8888_NoClamp;
			} else {
				pBind->pConvertBGRA7777 = &UOpenGLRenderDevice::ConvertBGRA7777_BGRA8888;
			}
		} else {
			pBind->texSourceFormat = GL_RGBA;
			pBind->pConvertBGRA7777 = &UOpenGLRenderDevice::ConvertBGRA7777_RGBA8888;
		}
		pBind->texInternalFormat = GL_RGBA8;
	}

	return;
}
