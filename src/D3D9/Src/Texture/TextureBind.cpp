/*=============================================================================
	TextureBind.cpp: getting the right texture onto the right sampler.
=============================================================================*/

#include "../D3D9Drv.h"
#include "../D3D9.h"
#include "TextureHelpers.h"

//Carries the folded flags.
void UD3D9RenderDevice::SetTextureNoCheck(DWORD texNum, FTexInfo &Tex, FTextureInfo &Info, DWORD PolyFlags) {
	guard(UD3D9RenderDevice::SetTexture);

	clock(BindCycles);

	bool isZeroPrefixCacheID = ((Tex.CurrentCacheID & 0xFFFFFFFF00000000ULL) == 0) ? true : false;

	FCachedTexture *pBind = NULL;
	bool existingBind = false;
	HRESULT hResult;

	if (isZeroPrefixCacheID) {
		DWORD CacheIDSuffix = (Tex.CurrentCacheID & 0x00000000FFFFFFFFULL);

		DWORD_CTTree_t *zeroPrefixBindTree = &m_zeroPrefixBindTrees[CTZeroPrefixCacheIDSuffixToTreeIndex(CacheIDSuffix)];
		DWORD_CTTree_t::node_t *bindTreePtr = zeroPrefixBindTree->find(CacheIDSuffix);
		if (bindTreePtr == 0) {
			DWORD_CTTree_t::node_t *pNewNode;

			pNewNode = m_DWORD_CTTree_Allocator.alloc_node();
			pNewNode->key = CacheIDSuffix;
			if (!zeroPrefixBindTree->insert(pNewNode)) {
				//A refusal means the entry the lookup said was missing now exists, and keeping the
				//new node would leave a second texture object under the same cache id that nothing
				//could ever find again to release, so this recovers onto the existing entry, which
				//holds the same texture anyway, the id being everything the cache distinguishes on
				//and the masked and 16 bit variants having already been folded into it by the time
				//the lookup ran.
				m_DWORD_CTTree_Allocator.free_node(pNewNode);
				bindTreePtr = zeroPrefixBindTree->find(CacheIDSuffix);
				check(bindTreePtr != 0);
			} else {
				pBind = &pNewNode->data;

				pBind->pTexObj = NULL;
				pBind->texBytes = 0;

				pBind->bindType = BIND_TYPE_ZERO_PREFIX;
				pBind->treeIndex = (BYTE)CTZeroPrefixCacheIDSuffixToTreeIndex(CacheIDSuffix);
				pBind->bContentStale = 0;

				pBind->LastUsedFrameCount = m_currentFrameCount;
				m_zeroPrefixBindChain->link_to_tail(pBind);

				pBind->texParams = CT_DEFAULT_TEX_PARAMS;
				pBind->dynamicTexBits = (PolyFlags & PF_NoSmooth) ? DT_NO_SMOOTH_BIT : 0;

				CacheTextureInfo(pBind, Info, PolyFlags);

#if 0
{
	static int si;
	dout << L"utd3d9r: Create texture zp = " << si++ << std::endl;
}
#endif
				INT levelCount = CalcTexLevelCount(pBind, Info);
				hResult = m_d3dDevice->CreateTexture(
					1U << pBind->UBits, 1U << pBind->VBits, levelCount,
					0, pBind->texFormat, D3DPOOL_MANAGED, &pBind->pTexObj, NULL);
				if (FAILED(hResult)) {
					appErrorf(TEXT("CreateTexture failed"));
				}

				AllocatedTextures++;
				pBind->texBytes = CalcTexBytes(pBind, levelCount);
				m_texCacheBytes += pBind->texBytes;
			}
		}
		//Already there, or recovered.
		if (bindTreePtr != 0) {
			pBind = &bindTreePtr->data;
			pBind->LastUsedFrameCount = m_currentFrameCount;

			m_zeroPrefixBindChain->unlink(pBind);
			m_zeroPrefixBindChain->link_to_tail(pBind);

			existingBind = true;
		}
	} else {
		DWORD CacheIDSuffix = (Tex.CurrentCacheID & 0x00000000FFFFFFFF);
		DWORD treeIndex = CTNonZeroPrefixCacheIDSuffixToTreeIndex(CacheIDSuffix);

		QWORD_CTTree_t *nonZeroPrefixBindTree = &m_nonZeroPrefixBindTrees[treeIndex];
		QWORD_CTTree_t::node_t *bindTreePtr = nonZeroPrefixBindTree->find(Tex.CurrentCacheID);
		if (bindTreePtr == 0) {
			QWORD_CTTree_t::node_t *pNewNode;

			pNewNode = m_nonZeroPrefixNodePool.try_remove();
			if (!pNewNode) {
				pNewNode = m_QWORD_CTTree_Allocator.alloc_node();
			}

			pNewNode->key = Tex.CurrentCacheID;
			if (!nonZeroPrefixBindTree->insert(pNewNode)) {
				m_QWORD_CTTree_Allocator.free_node(pNewNode);
				bindTreePtr = nonZeroPrefixBindTree->find(Tex.CurrentCacheID);
				check(bindTreePtr != 0);
			} else {
				pBind = &pNewNode->data;

				pBind->pTexObj = NULL;
				pBind->texBytes = 0;

				pBind->LastUsedFrameCount = m_currentFrameCount;

				pBind->bindType = BIND_TYPE_NON_ZERO_PREFIX_LRU_LIST;
				if (CacheStaticMaps && ((Tex.CurrentCacheID & 0xFF) == 0x18)) {
					pBind->bindType = BIND_TYPE_NON_ZERO_PREFIX;
				}

				pBind->treeIndex = (BYTE)treeIndex;
				pBind->bContentStale = 0;

				pBind->texParams = CT_DEFAULT_TEX_PARAMS;
				pBind->dynamicTexBits = (PolyFlags & PF_NoSmooth) ? DT_NO_SMOOTH_BIT : 0;

				if (pBind->bindType == BIND_TYPE_NON_ZERO_PREFIX_LRU_LIST) {
					m_nonZeroPrefixBindChain->link_to_tail(pBind);
				}

				CacheTextureInfo(pBind, Info, PolyFlags);

				bool needTexIdAllocate = true;
				if (UseTexPool) {
					if ((pBind->texType == TEX_TYPE_NORMAL) && (Info.NumMips == 1)) {
						TexPoolMap_t::node_t *texPoolPtr;

						TexPoolMapKey_t texPoolKey = MakeTexPoolMapKey(pBind->UBits, pBind->VBits);

						texPoolPtr = m_RGBA8TexPool->find(texPoolKey);
						if (texPoolPtr != 0) {
							QWORD_CTTree_NodePool_t::node_t *texPoolNodePtr;

							QWORD_CTTree_NodePool_t &texPool = texPoolPtr->data;

							if ((texPoolNodePtr = texPool.try_remove()) != 0) {
								pBind->pTexObj = texPoolNodePtr->data.pTexObj;

								pBind->texParams = texPoolNodePtr->data.texParams;
								pBind->dynamicTexBits = texPoolNodePtr->data.dynamicTexBits;

								pBind->texBytes = texPoolNodePtr->data.texBytes;
								m_texCacheBytes += pBind->texBytes;

								m_nonZeroPrefixNodePool.add(texPoolNodePtr);

#if 0
{
	static int si;
	dout << L"utd3d9r: TexPool retrieve = " << si++ << L", Id = 0x" << HexString((DWORD)pBind->pTexObj, 32)
		<< L", u = " << pBind->UBits << L", v = " << pBind->VBits << std::endl;
}
#endif

								needTexIdAllocate = false;
							}
						}
					}
				}
				if (needTexIdAllocate) {
#if 0
{
	static int si;
	dout << L"utd3d9r: Create texture nzp = " << si++ << std::endl;
}
#endif
					INT levelCount = CalcTexLevelCount(pBind, Info);
					hResult = m_d3dDevice->CreateTexture(
						1U << pBind->UBits, 1U << pBind->VBits, levelCount,
						0, pBind->texFormat, D3DPOOL_MANAGED, &pBind->pTexObj, NULL);
					if (FAILED(hResult)) {
						appErrorf(TEXT("CreateTexture failed"));
					}

					AllocatedTextures++;
					pBind->texBytes = CalcTexBytes(pBind, levelCount);
					m_texCacheBytes += pBind->texBytes;
				}
			}
		}
		//Already there, or recovered.
		if (bindTreePtr != 0) {
			pBind = &bindTreePtr->data;
			pBind->LastUsedFrameCount = m_currentFrameCount;

			if (pBind->bindType == BIND_TYPE_NON_ZERO_PREFIX_LRU_LIST) {
				m_nonZeroPrefixBindChain->unlink(pBind);
				m_nonZeroPrefixBindChain->link_to_tail(pBind);
			}

			existingBind = true;
		}
	}

	//The source can have changed size, mip count, format or palette since it was first seen.
	if (existingBind && !TexInfoMatchesBind(pBind, Info)) {
		CacheTextureInfo(pBind, Info, PolyFlags);

		if (pBind->pTexObj != NULL) {
			pBind->pTexObj->Release();
			pBind->pTexObj = NULL;
			AllocatedTextures--;
			m_texCacheBytes -= pBind->texBytes;
			pBind->texBytes = 0;
		}

		existingBind = false;
	}

	//A failed create leaves the node with no texture object.
	if (pBind->pTexObj == NULL) {
		INT levelCount = CalcTexLevelCount(pBind, Info);
		hResult = m_d3dDevice->CreateTexture(
			1U << pBind->UBits, 1U << pBind->VBits, levelCount,
			0, pBind->texFormat, D3DPOOL_MANAGED, &pBind->pTexObj, NULL);
		if (FAILED(hResult)) {
			pBind->pTexObj = NULL;
			appErrorf(TEXT("CreateTexture failed"));
		}

		AllocatedTextures++;
		pBind->texBytes = CalcTexBytes(pBind, levelCount);
		m_texCacheBytes += pBind->texBytes;

		existingBind = false;
	}

	Tex.pBind = pBind;

	/*
	Substitutes the no-texture object for an unsupported format that was never uploaded.
	A managed texture's system memory copy is not zero filled, so an untouched surface would
	sample whatever the allocator last left there, usually another texture's pixels, and the
	cache entry itself is left exactly as it was because its existing is the only thing stopping
	the texture from being processed again under the same id on the very next surface.
	*/
	m_d3dDevice->SetTexture(texNum, (pBind->texType == TEX_TYPE_NONE) ? m_pNoTexObj : pBind->pTexObj);

	unclock(BindCycles);

	Tex.UMult = pBind->UMult;
	Tex.VMult = pBind->VMult;

	{
		BYTE desiredDynamicTexBits;

		desiredDynamicTexBits = (PolyFlags & PF_NoSmooth) ? DT_NO_SMOOTH_BIT : 0;
		if (desiredDynamicTexBits != pBind->dynamicTexBits) {
			BYTE dynamicTexBitsXor;

			dynamicTexBitsXor = desiredDynamicTexBits ^ pBind->dynamicTexBits;

			pBind->dynamicTexBits = desiredDynamicTexBits;

			if (dynamicTexBitsXor & DT_NO_SMOOTH_BIT) {
				BYTE desiredTexParamsFilter;

				desiredTexParamsFilter = 0;
				if (NoFiltering) {
					desiredTexParamsFilter |= CT_MIN_FILTER_POINT | CT_MIP_FILTER_NONE;
				} else if (PolyFlags & PF_NoSmooth) {
					desiredTexParamsFilter |= CT_MIN_FILTER_POINT;
					desiredTexParamsFilter |= ((pBind->texParams.filter & CT_HAS_MIPMAPS_BIT) == 0) ? CT_MIP_FILTER_NONE : CT_MIP_FILTER_POINT;
				} else {
					desiredTexParamsFilter |= (MaxAnisotropy) ? CT_MIN_FILTER_ANISOTROPIC : CT_MIN_FILTER_LINEAR;
					desiredTexParamsFilter |= ((pBind->texParams.filter & CT_HAS_MIPMAPS_BIT) == 0) ? CT_MIP_FILTER_NONE : (UseTrilinear ? CT_MIP_FILTER_LINEAR : CT_MIP_FILTER_POINT);
					desiredTexParamsFilter |= CT_MAG_FILTER_LINEAR_NOT_POINT_BIT;
				}

				const BYTE MODIFIED_TEX_PARAMS_FILTER_BITS = CT_MIN_FILTER_MASK | CT_MIP_FILTER_MASK | CT_MAG_FILTER_LINEAR_NOT_POINT_BIT;
				pBind->texParams.filter = (pBind->texParams.filter & ~MODIFIED_TEX_PARAMS_FILTER_BITS) | desiredTexParamsFilter;
			}
		}
	}

	//An unsupported source format has no converter and no layout to trust,
	//so it gets the no-texture object. The realtime flag is cleared too.
	if (pBind->texType == TEX_TYPE_NONE) {
		TexInfoClearRealtimeChanged(Info);
		pBind->bContentStale = 0;
	}
	//bContentStale stands in for the flag on a lightmap whose change NoteLightmapChanged took.
	else if (!existingBind || TexInfoRealtimeChanged(Info) || pBind->bContentStale) {
		FColor paletteIndex0;

#ifndef UTGLR_KLINGON_BUILD
		if (SupportsLazyTextures) {
			Info.Load();
		}
#endif
		TexInfoClearRealtimeChanged(Info);
		pBind->bContentStale = 0;

		if (Info.Palette && (PolyFlags & PF_Masked)) {
			paletteIndex0 = Info.Palette[0];
			Info.Palette[0] = FColor(0, 0, 0, 0);
		}

		clock(ImageCycles);

		m_texConvertCtx.pBind = pBind;

		UBOOL SkipMipmaps = (Info.NumMips == 1);
		INT MaxLevel = pBind->MaxLevel;

		//A single mip source still gets a full chain where the texture object already has the levels.
		UBOOL BuildMipmaps = SkipMipmaps && (pBind->pTexObj->GetLevelCount() > 1) &&
			CanDownsampleTexFormat(pBind->texFormat);
		UBOOL HasMipmaps = !SkipMipmaps || BuildMipmaps;

		if (!existingBind) {
			tex_params_t desiredTexParams;

			desiredTexParams.filter = 0;
			if (NoFiltering) {
				desiredTexParams.filter |= CT_MIN_FILTER_POINT | CT_MIP_FILTER_NONE;
			} else if (PolyFlags & PF_NoSmooth) {
				desiredTexParams.filter |= CT_MIN_FILTER_POINT;
				desiredTexParams.filter |= HasMipmaps ? CT_MIP_FILTER_POINT : CT_MIP_FILTER_NONE;
			} else {
				desiredTexParams.filter |= (MaxAnisotropy) ? CT_MIN_FILTER_ANISOTROPIC : CT_MIN_FILTER_LINEAR;
				desiredTexParams.filter |= HasMipmaps ? (UseTrilinear ? CT_MIP_FILTER_LINEAR : CT_MIP_FILTER_POINT) : CT_MIP_FILTER_NONE;
				desiredTexParams.filter |= CT_MAG_FILTER_LINEAR_NOT_POINT_BIT;
			}

			if (HasMipmaps) {
				desiredTexParams.filter |= CT_HAS_MIPMAPS_BIT;
			}

			pBind->texParams = desiredTexParams;
		}


		INT MaxUploadLevel = MaxLevel;
		if (SkipMipmaps) {
			MaxUploadLevel = 0;
		}

		{
			INT texLevelCount = (INT)pBind->pTexObj->GetLevelCount();
			if (MaxUploadLevel > (texLevelCount - 1)) {
				MaxUploadLevel = texLevelCount - 1;
			}
		}


		m_texConvertCtx.texWidthPow2 = 1 << pBind->UBits;
		m_texConvertCtx.texHeightPow2 = 1 << pBind->VBits;

		guard(WriteTexture);
		INT Level;
		bool level0Uploaded = false;
		for (Level = 0; Level <= MaxUploadLevel; Level++) {
			INT MipIndex = pBind->BaseMip + Level;
			INT stepBits = 0;
			if (MipIndex >= Info.NumMips) {
				stepBits = MipIndex - (Info.NumMips - 1);
				MipIndex = Info.NumMips - 1;
			}
			m_texConvertCtx.stepBits = stepBits;

			//The block copy converters cannot resample, so stop here; the wrong source mip would be read.
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
				if (FAILED(pBind->pTexObj->LockRect(Level, &m_texConvertCtx.lockRect, NULL, D3DLOCK_NOSYSLOCK))) {
					appErrorf(TEXT("Texture lock failed"));
				}

				switch (pBind->texType) {
					case TEX_TYPE_COMPRESSED_DXT1:
						guard(ConvertDXT1_DXT1);
						ConvertDXT1_DXT1(Mip, Level);
						unguard;
						break;

					case TEX_TYPE_COMPRESSED_DXT1_TO_DXT3:
						guard(ConvertDXT1_DXT3);
						ConvertDXT1_DXT3(Mip, Level);
						unguard;
						break;

					case TEX_TYPE_COMPRESSED_DXT3:
						guard(ConvertDXT3);
						ConvertDXT35_DXT35(Mip, Level);
						unguard;
						break;

					case TEX_TYPE_COMPRESSED_DXT5:
						guard(ConvertDXT5);
						ConvertDXT35_DXT35(Mip, Level);
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

					case TEX_TYPE_HAS_PALETTE:
						switch (pBind->texFormat) {
							case D3DFMT_R5G6B5:
								guard(ConvertP8_RGB565);
								if (stepBits == 0) {
									ConvertP8_RGB565_NoStep(Mip, Info.Palette, Level);
								} else {
									ConvertP8_RGB565(Mip, Info.Palette, Level);
								}
								unguard;
								break;

							case D3DFMT_X1R5G5B5:
							case D3DFMT_A1R5G5B5:
								guard(ConvertP8_RGBA5551);
								if (stepBits == 0) {
									ConvertP8_RGBA5551_NoStep(Mip, Info.Palette, Level);
								} else {
									ConvertP8_RGBA5551(Mip, Info.Palette, Level);
								}
								unguard;
								break;

							default:
								guard(ConvertP8_RGBA8888);
								if (stepBits == 0) {
#ifdef UTGLR_INCLUDE_SSE_CODE
									//Gated on the level being big enough to pay for it.
									if (m_useSSSE3 &&
										((m_texConvertCtx.texWidthPow2 * m_texConvertCtx.texHeightPow2) >= 4096)) {
										ConvertP8_RGBA8888_NoStep_SSSE3(Mip, Info.Palette, Level);
									} else
#endif
										ConvertP8_RGBA8888_NoStep(Mip, Info.Palette, Level);
								} else {
									ConvertP8_RGBA8888(Mip, Info.Palette, Level);
								}
								unguard;
						}
						break;

					default:
						guard(ConvertBGRA7777);
						(this->*pBind->pConvertBGRA7777)(Mip, Level);
						unguard;
				}

				DWORD texWidth, texHeight;

				texWidth = m_texConvertCtx.texWidthPow2;
				texHeight = m_texConvertCtx.texHeightPow2;

				m_texConvertCtx.texWidthPow2 = (texWidth & 0x1) | (texWidth >> 1);
				m_texConvertCtx.texHeightPow2 = (texHeight & 0x1) | (texHeight >> 1);

				if (FAILED(pBind->pTexObj->UnlockRect(Level))) {
					appErrorf(TEXT("Texture unlock failed"));
				}

				if (Level == 0) {
					level0Uploaded = true;
				}
			}
		}

		//Only level 0 came from the source, so the rest is built from it.
		if (BuildMipmaps && level0Uploaded) {
			guard(GenerateMipmapLevels);
			bool chainComplete = GenerateMipmapLevels(pBind);

			//The mipmap bits are otherwise only set on the first-upload path above and a failed
			//rebuild clears them, so an entry that ever hit one would never sample a mip again
			//however many times the chain was rebuilt afterwards, which is why they are restored
			//here, ahead of the filter setup at the end of the function.
			if (chainComplete) {
				BYTE mipFilter;
				if (NoFiltering) {
					mipFilter = CT_MIP_FILTER_NONE;
				} else if (PolyFlags & PF_NoSmooth) {
					mipFilter = CT_MIP_FILTER_POINT;
				} else {
					mipFilter = UseTrilinear ? CT_MIP_FILTER_LINEAR : CT_MIP_FILTER_POINT;
				}
				pBind->texParams.filter = (pBind->texParams.filter & ~CT_MIP_FILTER_MASK) | mipFilter | CT_HAS_MIPMAPS_BIT;
			}
			unguard;
		}
		unguard;

		unclock(ImageCycles);

		if (Info.Palette && (PolyFlags & PF_Masked)) {
			Info.Palette[0] = paletteIndex0;
		}

#ifndef UTGLR_KLINGON_BUILD
		if (SupportsLazyTextures) {
			Info.Unload();
		}
#endif
	}

	SetTexFilter(texNum, pBind->texParams.filter);

	unguard;
}
