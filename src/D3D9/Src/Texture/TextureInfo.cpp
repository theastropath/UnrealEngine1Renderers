/*=============================================================================
	TextureInfo.cpp: deciding how a texture will be stored and converted.
=============================================================================*/

#include "../D3D9Drv.h"
#include "../D3D9.h"
#include "TextureHelpers.h"

void UD3D9RenderDevice::CacheTextureInfo(FCachedTexture *pBind, const FTextureInfo &Info, DWORD PolyFlags) {
#if 0
{
	dout << L"utd3d9r: CacheId = "
		<< HexString((DWORD)((QWORD)Info.CacheID >> 32), 32) << L":"
		<< HexString((DWORD)((QWORD)Info.CacheID & 0xFFFFFFFF), 32) << std::endl;
}
{
	const UTexture *pTexture = Info.Texture;
	const TCHAR *pName = pTexture->GetFullName();
	if (pName) dout << L"utd3d9r: TexName = " << pName << std::endl;
}
{
	dout << L"utd3d9r: NumMips = " << Info.NumMips << std::endl;
}
{
	unsigned int u;

	dout << L"utd3d9r: ZPBindTree Size = ";
	for (u = 0; u < NUM_CTTree_TREES; u++) {
		dout << m_zeroPrefixBindTrees[u].calc_size();
		if (u != (NUM_CTTree_TREES - 1)) dout << L", ";
	}
	dout << std::endl;

	dout << L"utd3d9r: NZPBindTree Size = ";
	for (u = 0; u < NUM_CTTree_TREES; u++) {
		dout << m_nonZeroPrefixBindTrees[u].calc_size();
		if (u != (NUM_CTTree_TREES - 1)) dout << L", ";
	}
	dout << std::endl;
}
#endif

	//Recorded before any decision is made, because every decision below derives from it.
	pBind->srcUSize = Info.USize;
	pBind->srcVSize = Info.VSize;
	pBind->srcNumMips = Info.NumMips;
	pBind->srcFormat = (BYTE)Info.Format;
	pBind->srcHasPalette = (Info.Palette != NULL) ? 1 : 0;

	//Nothing on the way here has established that a base mip exists. Refused as an unsupported format.
	if ((Info.NumMips < 1) || (Info.Mips[0] == NULL)) {
		pBind->texType = TEX_TYPE_NONE;
		pBind->texFormat = D3DFMT_A8R8G8B8;
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
			//Accumulated like the four clamps above. Assigning outright would discard what those clamps put in.
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


	//PF_Masked cannot change once an existing texture is updated.
	//Texture type is cached here.
	pBind->texType = TEX_TYPE_NONE;

	//Mip layout follows the source format, so dispatch is by format.
	//Anything this renderer cannot read correctly is refused.
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
		//Left untextured, but a texture object is still created and bound.
		pBind->texFormat = D3DFMT_A8R8G8B8;
		MaxLevel = 0;
		pBind->MaxLevel = 0;
		debugf(NAME_Warning, TEXT("Unsupported texture format %i; texture not uploaded"), (INT)Info.Format);
		return;
	}

	if (srcDXTType != 0) {
		//Compressed blocks cannot be resampled, so every level must exist in the source.
		INT availLevels = (INT)Info.NumMips - 1 - BaseMip;
		if (availLevels < 0) {
			availLevels = 0;
		}
		if (MaxLevel > availLevels) {
			MaxLevel = availLevels;
			pBind->MaxLevel = MaxLevel;
		}

		//The base mip must exist, the hardware must take the format,
		//and the bind's dimensions must match the mip's own.
		//Declining routes to the decoder below, which resamples.
		if ((BaseMip < (INT)Info.NumMips) && ((UCopyBits | VCopyBits) == 0)) {
			if (srcDXTType == 1) {
				bool asDXT3 = (TexDXT1ToDXT3 && (!(PolyFlags & PF_Masked)) && m_dxt3TextureCap) ? true : false;
				//The relabel is only equivalent for blocks in the four colour mode.
				if (asDXT3 && DXT1HasThreeColorBlocks(Info, BaseMip, MaxLevel, UBits, VBits)) {
					asDXT3 = false;
				}
				if (asDXT3) {
					pBind->texType = TEX_TYPE_COMPRESSED_DXT1_TO_DXT3;
					pBind->texFormat = D3DFMT_DXT3;
				} else if (m_dxt1TextureCap) {
					pBind->texType = TEX_TYPE_COMPRESSED_DXT1;
					pBind->texFormat = D3DFMT_DXT1;
				}
			} else if (srcDXTType == 3) {
				if (m_dxt3TextureCap) {
					pBind->texType = TEX_TYPE_COMPRESSED_DXT3;
					pBind->texFormat = D3DFMT_DXT3;
				}
			} else {
				if (m_dxt5TextureCap) {
					pBind->texType = TEX_TYPE_COMPRESSED_DXT5;
					pBind->texFormat = D3DFMT_DXT5;
				}
			}
		}

		//Full chain again.
		if (pBind->texType == TEX_TYPE_NONE) {
			pBind->texType = (srcDXTType == 1) ? TEX_TYPE_DECOMPRESSED_DXT1 : ((srcDXTType == 3) ? TEX_TYPE_DECOMPRESSED_DXT3 : TEX_TYPE_DECOMPRESSED_DXT5);
			pBind->texFormat = D3DFMT_A8R8G8B8;

			MaxLevel = Min(UBits, VBits) - MinLogTextureSize;
			if (MaxLevel < 0) {
				MaxLevel = 0;
			}
			pBind->MaxLevel = MaxLevel;
		}
	}

	if (pBind->texType != TEX_TYPE_NONE) {
		//Using compressed texture, or decoding on upload.
	} else if (Info.Palette) {
		pBind->texType = TEX_TYPE_HAS_PALETTE;
		pBind->texFormat = D3DFMT_A8R8G8B8;
		if (PolyFlags & PF_Memorized) {
			pBind->texFormat = (PolyFlags & PF_Masked) ? D3DFMT_A1R5G5B5 : ((Use565Textures) ? D3DFMT_R5G6B5 : D3DFMT_X1R5G5B5);
		}
	} else {
		pBind->texType = TEX_TYPE_NORMAL;
		if (texFlags & TEX_FLAG_NO_CLAMP) {
			pBind->pConvertBGRA7777 = &UD3D9RenderDevice::ConvertBGRA7777_BGRA8888_NoClamp;
		} else {
			pBind->pConvertBGRA7777 = &UD3D9RenderDevice::ConvertBGRA7777_BGRA8888;
		}
		pBind->texFormat = D3DFMT_A8R8G8B8;
	}

	return;
}
