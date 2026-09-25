/*=============================================================================
	TextureUpload.cpp: handing texture data to the driver.

	The scratch buffer is reused across uploads.
=============================================================================*/

#include "../OpenGLDrv.h"
#include "../OpenGL.h"

//Grown to fit an upload too large for the compose buffer.
//The allocator errors out on failure.
BYTE *FASTCALL UOpenGLRenderDevice::GetTexComposeBuffer(DWORD sizeBytes) {
	if (sizeBytes <= m_texComposeBufferSize) {
		return m_pTexComposeBuffer;
	}

	//Released first.
	FreeTexComposeBuffer();

	m_pTexComposeBuffer = (BYTE *)appMalloc(sizeBytes + TEX_COMPOSE_BUFFER_SLACK, TEXT("OpenGL1xDrv texture compose"));
	m_texComposeBufferSize = sizeBytes;

	return m_pTexComposeBuffer;
}

void UOpenGLRenderDevice::FreeTexComposeBuffer(void) {
	if (m_pTexComposeBuffer != NULL) {
		appFree(m_pTexComposeBuffer);
		m_pTexComposeBuffer = NULL;
	}
	m_texComposeBufferSize = 0;

	return;
}

void UOpenGLRenderDevice::UploadTextureExec(FTextureInfo &Info, DWORD PolyFlags, FCachedTexture *pBind, bool existingBind, bool needTexAllocate) {
	FColor paletteIndex0;

	//Nothing to run.
	if (pBind->texType == TEX_TYPE_NONE) {
		return;
	}

	//This SDK variant has no lazy texture support.
#ifndef UTGLR_KLINGON_BUILD
	if (SupportsLazyTextures) {
		Info.Load();
	}
#endif
	TexInfoClearRealtimeChanged(Info);

	//Palette index 0 goes black for masked textures.
	if (Info.Palette && (PolyFlags & PF_Masked)) {
		paletteIndex0 = Info.Palette[0];
		Info.Palette[0] = FColor(0, 0, 0, 0);
	}

	clock(ImageCycles);

	if (pBind->texType == TEX_TYPE_PALETTED) {
		glColorTableEXT(GL_TEXTURE_2D, GL_RGBA, 256, GL_RGBA, GL_UNSIGNED_BYTE, Info.Palette);
	}

	DWORD memAllocSize = 1 << (pBind->UBits + pBind->VBits + 2);
	if (memAllocSize > LOCAL_TEX_COMPOSE_BUFFER_SIZE) {
		m_texConvertCtx.pCompose = GetTexComposeBuffer(memAllocSize);
	} else {
		m_texConvertCtx.pCompose = m_localTexComposeBuffer;
	}

	m_texConvertCtx.pBind = pBind;

	UBOOL SkipMipmaps = (Info.NumMips == 1) && !AlwaysMipmap;
	INT MaxLevel = pBind->MaxLevel;

	//Only allocating passes change the size.
	if (needTexAllocate) {
		*m_texCacheBytes -= pBind->texBytes;
		pBind->texBytes = CalcTexBytes(pBind, SkipMipmaps ? 1 : (MaxLevel + 1));
		*m_texCacheBytes += pBind->texBytes;
	}

	//Only update texture state for new textures.
	if (!existingBind) {
		DWORD texMaxLevel;
		BYTE texFilter;

		texMaxLevel = 1000;
		if (!SkipMipmaps) {
			texMaxLevel = MaxLevel;
		} else {
			texMaxLevel = 0;
		}
		if (texMaxLevel != 1000) {
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, texMaxLevel);
		}

		pBind->texParams.hasMipmaps = (!SkipMipmaps) ? true : false;

		pBind->texParams.texObjFilter = 0;
#ifdef UTGLR_UNREAL_227_BUILD
		if (Info.UClampMode) {
			pBind->texParams.texObjFilter |= CT_ADDRESS_U_CLAMP;
		}
		if (Info.VClampMode) {
			pBind->texParams.texObjFilter |= CT_ADDRESS_V_CLAMP;
		}
#endif


		texFilter = GenerateTexFilterParams(PolyFlags, pBind);

		SetTexFilter(pBind, texFilter);
	}


	//Some textures only upload the base level.
	INT MaxUploadLevel = MaxLevel;
	if (SkipMipmaps) {
		MaxUploadLevel = 0;
	}


	//Both bit counts must be >= 0 here.
	m_texConvertCtx.texWidthPow2 = 1 << pBind->UBits;
	m_texConvertCtx.texHeightPow2 = 1 << pBind->VBits;

	INT Level;
	for (Level = 0; Level <= MaxUploadLevel; Level++) {
		INT MipIndex = pBind->BaseMip + Level;
		INT stepBits = 0;
		if (MipIndex >= Info.NumMips) {
			stepBits = MipIndex - (Info.NumMips - 1);
			MipIndex = Info.NumMips - 1;
		}
		m_texConvertCtx.stepBits = stepBits;

		//Converters cannot resample.
		if (stepBits != 0) {
			if ((pBind->texType == TEX_TYPE_COMPRESSED_DXT1) ||
				(pBind->texType == TEX_TYPE_COMPRESSED_DXT1_TO_DXT3) ||
				(pBind->texType == TEX_TYPE_COMPRESSED_DXT3) ||
				(pBind->texType == TEX_TYPE_COMPRESSED_DXT5)) {
				break;
			}
		}

		FMipmapBase *Mip = Info.Mips[MipIndex];
		if (!Mip || !Mip->DataPtr) {
			break;
		} else {
			switch (pBind->texType) {
				case TEX_TYPE_COMPRESSED_DXT1:
					break;

				case TEX_TYPE_COMPRESSED_DXT3:
					break;

				case TEX_TYPE_COMPRESSED_DXT5:
					break;

				case TEX_TYPE_COMPRESSED_DXT1_TO_DXT3:
					guard(ConvertDXT1_DXT3);
					ConvertDXT1_DXT3(Mip, Level);
					unguard;
					break;

				case TEX_TYPE_DECOMPRESSED_DXT1:
					guard(ConvertDXT1_RGBA8888);
					ConvertDXT_RGBA8888(Mip, Level, 1);
					unguard;
					break;

				case TEX_TYPE_DECOMPRESSED_DXT3:
					guard(ConvertDXT3_RGBA8888);
					ConvertDXT_RGBA8888(Mip, Level, 3);
					unguard;
					break;

				case TEX_TYPE_DECOMPRESSED_DXT5:
					guard(ConvertDXT5_RGBA8888);
					ConvertDXT_RGBA8888(Mip, Level, 5);
					unguard;
					break;

				case TEX_TYPE_PALETTED:
					guard(ConvertP8_P8);
					if (stepBits == 0) {
						ConvertP8_P8_NoStep(Mip, Level);
					} else {
						ConvertP8_P8(Mip, Level);
					}
					unguard;
					break;

				case TEX_TYPE_HAS_PALETTE:
					guard(ConvertP8_RGBA8888);
					if (stepBits == 0) {
						ConvertP8_RGBA8888_NoStep(Mip, Info.Palette, Level);
					} else {
						ConvertP8_RGBA8888(Mip, Info.Palette, Level);
					}
					unguard;
					break;

				default:
					guard(ConvertBGRA7777);
					(this->*pBind->pConvertBGRA7777)(Mip, Level);
					unguard;
			}

			BYTE *Src = (BYTE *)m_texConvertCtx.pCompose;
			DWORD texWidth, texHeight;

			texWidth = m_texConvertCtx.texWidthPow2;
			texHeight = m_texConvertCtx.texHeightPow2;

			//Halved down to a floor of one.
			m_texConvertCtx.texWidthPow2 = (texWidth & 0x1) | (texWidth >> 1);
			m_texConvertCtx.texHeightPow2 = (texHeight & 0x1) | (texHeight >> 1);

			//Compressed levels are stored as 4x4 blocks.
			const DWORD compressedBytes = (((texWidth + 3) >> 2) * ((texHeight + 3) >> 2)) *
				((pBind->texType == TEX_TYPE_COMPRESSED_DXT1) ? 8 : 16);

			if (!needTexAllocate) {
				switch (pBind->texType) {
					case TEX_TYPE_COMPRESSED_DXT1:
					case TEX_TYPE_COMPRESSED_DXT1_TO_DXT3:
					case TEX_TYPE_COMPRESSED_DXT3:
					case TEX_TYPE_COMPRESSED_DXT5:
						guard(glCompressedTexSubImage2D);
#if 0
{
	static unsigned int s_c;
	dbgPrintf("utglr: glCompressedTexSubImage2DARB = %u\n", s_c++);
}
#endif
						glCompressedTexSubImage2DARB(
							GL_TEXTURE_2D,
							Level,
							0,
							0,
							texWidth,
							texHeight,
							pBind->texInternalFormat,
							compressedBytes,
							(pBind->texType == TEX_TYPE_COMPRESSED_DXT1_TO_DXT3) ? Src : Mip->DataPtr);
						unguard;

						break;

					default:
						guard(glTexSubImage2D);
#if 0
{
	static unsigned int s_c;
	dbgPrintf("utglr: glTexSubImage2D = %u\n", s_c++);
}
#endif
						glTexSubImage2D(
							GL_TEXTURE_2D,
							Level,
							0,
							0,
							texWidth,
							texHeight,
							pBind->texSourceFormat,
							GL_UNSIGNED_BYTE,
							Src);
						unguard;
				}
			} else {
				switch (pBind->texType) {
					case TEX_TYPE_COMPRESSED_DXT1:
					case TEX_TYPE_COMPRESSED_DXT1_TO_DXT3:
					case TEX_TYPE_COMPRESSED_DXT3:
					case TEX_TYPE_COMPRESSED_DXT5:
						guard(glCompressedTexImage2D);
#if 0
{
	static unsigned int s_c;
	dbgPrintf("utglr: glCompressedTexImage2DARB = %u\n", s_c++);
}
#endif
						glCompressedTexImage2DARB(
							GL_TEXTURE_2D,
							Level,
							pBind->texInternalFormat,
							texWidth,
							texHeight,
							0,
							compressedBytes,
							(pBind->texType == TEX_TYPE_COMPRESSED_DXT1_TO_DXT3) ? Src : Mip->DataPtr);
						unguard;

						break;

					default:
						guard(glTexImage2D);
#if 0
{
	static unsigned int s_c;
	dbgPrintf("utglr: glTexImage2D = %u\n", s_c++);
}
#endif
						glTexImage2D(
							GL_TEXTURE_2D,
							Level,
							pBind->texInternalFormat,
							texWidth,
							texHeight,
							0,
							pBind->texSourceFormat,
							GL_UNSIGNED_BYTE,
							Src);
						unguard;
				}
			}
		}
	}

	//Nothing to release.

	unclock(ImageCycles);

	//Restore palette index 0 for masked textures.
	if (Info.Palette && (PolyFlags & PF_Masked)) {
		Info.Palette[0] = paletteIndex0;
	}

#ifndef UTGLR_KLINGON_BUILD
	if (SupportsLazyTextures) {
		Info.Unload();
	}
#endif

	return;
}
