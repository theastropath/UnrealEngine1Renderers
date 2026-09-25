/*=============================================================================
	TextureBind.cpp: getting the right texture onto the right unit.

	The cache lookup and the filter state a binding carries.
=============================================================================*/

#include "../OpenGLDrv.h"
#include "../OpenGL.h"

void UOpenGLRenderDevice::SetNoTextureNoCheck(INT Multi) {
	guard(UOpenGLRenderDevice::SetNoTexture);

	clock(BindCycles);

	glBindTexture(GL_TEXTURE_2D, m_noTextureId);

	TexInfo[Multi].CurrentCacheID = TEX_CACHE_ID_NO_TEX;
	TexInfo[Multi].pBind = NULL;

	unclock(BindCycles);

	unguard;
}

void UOpenGLRenderDevice::SetAlphaTextureNoCheck(INT Multi) {
	guard(UOpenGLRenderDevice::SetAlphaTexture);

	clock(BindCycles);

	glBindTexture(GL_TEXTURE_2D, m_alphaTextureId);

	TexInfo[Multi].CurrentCacheID = TEX_CACHE_ID_ALPHA_TEX;
	TexInfo[Multi].pBind = NULL;

	unclock(BindCycles);

	unguard;
}

FCachedTexture *UOpenGLRenderDevice::FindCachedTexture(QWORD CacheID) {
	bool isZeroPrefixCacheID = ((CacheID & 0xFFFFFFFF00000000ULL) == 0) ? true : false;
	FCachedTexture *pBind = NULL;

	if (isZeroPrefixCacheID) {
		DWORD CacheIDSuffix = (CacheID & 0x00000000FFFFFFFFULL);

		DWORD_CTTree_t *zeroPrefixBindTree = &m_zeroPrefixBindTrees[CTZeroPrefixCacheIDSuffixToTreeIndex(CacheIDSuffix)];
		DWORD_CTTree_t::node_t *bindTreePtr = zeroPrefixBindTree->find(CacheIDSuffix);
		if (bindTreePtr != 0) {
			pBind = &bindTreePtr->data;
		}
	} else {
		DWORD CacheIDSuffix = (CacheID & 0x00000000FFFFFFFF);
		DWORD treeIndex = CTNonZeroPrefixCacheIDSuffixToTreeIndex(CacheIDSuffix);

		QWORD_CTTree_t *nonZeroPrefixBindTree = &m_nonZeroPrefixBindTrees[treeIndex];
		QWORD_CTTree_t::node_t *bindTreePtr = nonZeroPrefixBindTree->find(CacheID);
		if (bindTreePtr != 0) {
			pBind = &bindTreePtr->data;
		}
	}

	return pBind;
}

UOpenGLRenderDevice::QWORD_CTTree_NodePool_t::node_t *UOpenGLRenderDevice::TryAllocFromTexPool(TexPoolMapKey_t texPoolKey) {
	TexPoolMap_t::node_t *texPoolPtr;

	texPoolPtr = m_RGBA8TexPool->find(texPoolKey);
	if (texPoolPtr != 0) {
		QWORD_CTTree_NodePool_t::node_t *texPoolNodePtr;

		QWORD_CTTree_NodePool_t &texPool = texPoolPtr->data;

		if ((texPoolNodePtr = texPool.try_remove()) != 0) {
#if 0
{
	static unsigned int s_c;
	dbgPrintf("utglr: TexPool retrieve = %u, Id = %u, u = %u, v = %u\n",
		s_c++, texPoolNodePtr->data.Id, texPoolNodePtr->data.UBits, texPoolNodePtr->data.VBits);
}
#endif

			return texPoolNodePtr;
		}
	}

	return NULL;
}

BYTE UOpenGLRenderDevice::GenerateTexFilterParams(DWORD PolyFlags, FCachedTexture *pBind) {
	BYTE texFilter;

	texFilter = 0;
	if (NoFiltering) {
		texFilter |= CT_MIN_FILTER_NEAREST;
	} else if (PolyFlags & PF_NoSmooth) {
		texFilter |= (!pBind->texParams.hasMipmaps) ? CT_MIN_FILTER_NEAREST : CT_MIN_FILTER_NEAREST_MIPMAP_NEAREST;
	} else {
		texFilter |= (!pBind->texParams.hasMipmaps) ? CT_MIN_FILTER_LINEAR : (UseTrilinear ? CT_MIN_FILTER_LINEAR_MIPMAP_LINEAR : CT_MIN_FILTER_LINEAR_MIPMAP_NEAREST);
		texFilter |= CT_MAG_FILTER_NEAREST_OR_LINEAR_BIT;
		if (MaxAnisotropy) {
			texFilter |= CT_ANISOTROPIC_FILTER_BIT;
		}
	}
	texFilter |= (pBind->texParams.texObjFilter & (CT_ADDRESS_U_CLAMP | CT_ADDRESS_V_CLAMP));

	return texFilter;
}

void UOpenGLRenderDevice::SetTexFilterNoCheck(FCachedTexture *pBind, BYTE texFilter) {
	BYTE texFilterXor;

	texFilterXor = pBind->texParams.filter ^ texFilter;

	pBind->texParams.filter = texFilter;

	if (texFilterXor & CT_MIN_FILTER_MASK) {
		GLint intParam = GL_NEAREST;

		switch (texFilter & CT_MIN_FILTER_MASK) {
			case CT_MIN_FILTER_NEAREST: intParam = GL_NEAREST; break;
			case CT_MIN_FILTER_LINEAR: intParam = GL_LINEAR; break;
			case CT_MIN_FILTER_NEAREST_MIPMAP_NEAREST: intParam = GL_NEAREST_MIPMAP_NEAREST; break;
			case CT_MIN_FILTER_LINEAR_MIPMAP_NEAREST: intParam = GL_LINEAR_MIPMAP_NEAREST; break;
			case CT_MIN_FILTER_NEAREST_MIPMAP_LINEAR: intParam = GL_NEAREST_MIPMAP_LINEAR; break;
			case CT_MIN_FILTER_LINEAR_MIPMAP_LINEAR: intParam = GL_LINEAR_MIPMAP_LINEAR; break;
			default:;
		}

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, intParam);
	}
	if (texFilterXor & CT_MAG_FILTER_NEAREST_OR_LINEAR_BIT) {
		GLint intParam;

		intParam = (texFilter & CT_MAG_FILTER_NEAREST_OR_LINEAR_BIT) ? GL_LINEAR : GL_NEAREST;

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, intParam);
	}
	if (texFilterXor & CT_ANISOTROPIC_FILTER_BIT) {
		GLfloat floatParam;

		floatParam = (texFilter & CT_ANISOTROPIC_FILTER_BIT) ? (GLfloat)MaxAnisotropy : 1.0f;

		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, floatParam);
	}
	if (texFilterXor & CT_ADDRESS_U_CLAMP) {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, (texFilter & CT_ADDRESS_U_CLAMP) ? GL_CLAMP_TO_EDGE : GL_REPEAT);
	}
	if (texFilterXor & CT_ADDRESS_V_CLAMP) {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, (texFilter & CT_ADDRESS_V_CLAMP) ? GL_CLAMP_TO_EDGE : GL_REPEAT);
	}

	return;
}

//Use the current cache id, the only one with the variant flags.
void UOpenGLRenderDevice::SetTextureNoCheck(FTexInfo &Tex, FTextureInfo &Info, DWORD PolyFlags) {
	guard(UOpenGLRenderDevice::SetTexture);

	clock(BindCycles);

	FCachedTexture *pBind;
	bool existingBind = false;
	bool needTexAllocate = true;
	pBind = FindCachedTexture(Tex.CurrentCacheID);

	if (pBind) {
		pBind->LastUsedFrameCount = m_currentFrameCount;

		if (pBind->bindType == BIND_TYPE_NON_ZERO_PREFIX_LRU_LIST) {
			m_nonZeroPrefixBindChain->unlink(pBind);
			m_nonZeroPrefixBindChain->link_to_tail(pBind);
		} else if (pBind->bindType == BIND_TYPE_ZERO_PREFIX) {
			m_zeroPrefixBindChain->unlink(pBind);
			m_zeroPrefixBindChain->link_to_tail(pBind);
		}

		existingBind = true;
		needTexAllocate = false;
	} else {
		bool isZeroPrefixCacheID = ((Tex.CurrentCacheID & 0xFFFFFFFF00000000ULL) == 0) ? true : false;
		if (isZeroPrefixCacheID) {
			DWORD CacheIDSuffix = (Tex.CurrentCacheID & 0x00000000FFFFFFFFULL);
			DWORD treeIndex = CTZeroPrefixCacheIDSuffixToTreeIndex(CacheIDSuffix);
			DWORD_CTTree_t *zeroPrefixBindTree = &m_zeroPrefixBindTrees[treeIndex];
			DWORD_CTTree_t::node_t *pNewNode;

			pNewNode = m_DWORD_CTTree_Allocator.alloc_node();
			pNewNode->key = CacheIDSuffix;
			if (!zeroPrefixBindTree->insert(pNewNode)) {
				/*
				A refusal means the entry the lookup above said was missing now exists and keeping the
				new node would leave a second texture object for the same cache id that nothing could
				find again to release, so recovery goes onto the existing entry, which holds the same
				texture and can only happen where two contexts share objects and both bind the same
				id in the same frame.
				*/
				m_DWORD_CTTree_Allocator.free_node(pNewNode);
				pNewNode = zeroPrefixBindTree->find(CacheIDSuffix);
				check(pNewNode != 0);
				pBind = &pNewNode->data;
				pBind->LastUsedFrameCount = m_currentFrameCount;
				m_zeroPrefixBindChain->unlink(pBind);
				m_zeroPrefixBindChain->link_to_tail(pBind);
				existingBind = true;
				needTexAllocate = false;
			} else {
				pBind = &pNewNode->data;
				pBind->LastUsedFrameCount = m_currentFrameCount;

				pBind->bindType = BIND_TYPE_ZERO_PREFIX;
				pBind->treeIndex = (BYTE)treeIndex;

				pBind->texBytes = 0;

				//Invariant: a tree node is always in the chain.
				m_zeroPrefixBindChain->link_to_tail(pBind);

				pBind->texParams = CT_DEFAULT_TEX_PARAMS;
				pBind->dynamicTexBits = (PolyFlags & PF_NoSmooth) ? DT_NO_SMOOTH_BIT : 0;

				CacheTextureInfo(pBind, Info, PolyFlags);

				glGenTextures(1, &pBind->Id);
				(*m_allocatedTextures)++;
			}
		} else {
			DWORD CacheIDSuffix = (Tex.CurrentCacheID & 0x00000000FFFFFFFF);
			DWORD treeIndex = CTNonZeroPrefixCacheIDSuffixToTreeIndex(CacheIDSuffix);
			QWORD_CTTree_t *nonZeroPrefixBindTree = &m_nonZeroPrefixBindTrees[treeIndex];
			QWORD_CTTree_t::node_t *pNewNode;

			pNewNode = m_nonZeroPrefixNodePool.try_remove();
			if (!pNewNode) {
				pNewNode = m_QWORD_CTTree_Allocator.alloc_node();
			}

			pNewNode->key = Tex.CurrentCacheID;
			if (!nonZeroPrefixBindTree->insert(pNewNode)) {
				//As above.
				m_QWORD_CTTree_Allocator.free_node(pNewNode);
				pNewNode = nonZeroPrefixBindTree->find(Tex.CurrentCacheID);
				check(pNewNode != 0);
				pBind = &pNewNode->data;
				pBind->LastUsedFrameCount = m_currentFrameCount;
				if (pBind->bindType == BIND_TYPE_NON_ZERO_PREFIX_LRU_LIST) {
					m_nonZeroPrefixBindChain->unlink(pBind);
					m_nonZeroPrefixBindChain->link_to_tail(pBind);
				}
				existingBind = true;
				needTexAllocate = false;
			} else {
				pBind = &pNewNode->data;
				pBind->LastUsedFrameCount = m_currentFrameCount;

				//Possibly overwritten below.
				pBind->texBytes = 0;

				pBind->bindType = BIND_TYPE_NON_ZERO_PREFIX_LRU_LIST;
				if (CacheStaticMaps && ((Tex.CurrentCacheID & 0xFF) == 0x18)) {
					pBind->bindType = BIND_TYPE_NON_ZERO_PREFIX;
				}

				pBind->treeIndex = (BYTE)treeIndex;

				pBind->texParams = CT_DEFAULT_TEX_PARAMS;
				pBind->dynamicTexBits = (PolyFlags & PF_NoSmooth) ? DT_NO_SMOOTH_BIT : 0;

				if (pBind->bindType == BIND_TYPE_NON_ZERO_PREFIX_LRU_LIST) {
					m_nonZeroPrefixBindChain->link_to_tail(pBind);
				}

				CacheTextureInfo(pBind, Info, PolyFlags);

				bool needTexIdAllocate = true;
				if (UseTexPool) {
					//Only textures without mipmaps are pooled here.
					if ((pBind->texType == TEX_TYPE_NORMAL) && (Info.NumMips == 1)) {
						QWORD_CTTree_NodePool_t::node_t *texPoolNodePtr;

						TexPoolMapKey_t texPoolKey = MakeTexPoolMapKey(pBind->UBits, pBind->VBits);

						texPoolNodePtr = TryAllocFromTexPool(texPoolKey);
						if (texPoolNodePtr) {
							pBind->Id = texPoolNodePtr->data.Id;

							pBind->texParams = texPoolNodePtr->data.texParams;
							pBind->dynamicTexBits = texPoolNodePtr->data.dynamicTexBits;

							//Already counted.
							pBind->texBytes = texPoolNodePtr->data.texBytes;

							m_nonZeroPrefixNodePool.add(texPoolNodePtr);

							needTexIdAllocate = false;
							needTexAllocate = false;
						}
					}
				}
				if (needTexIdAllocate) {
					QWORD_CTTree_NodePool_t::node_t *nzptipPtr;

					if (UseTexIdPool && ((nzptipPtr = m_nonZeroPrefixTexIdPool->try_remove()) != 0)) {
						pBind->Id = nzptipPtr->data.Id;

						pBind->texParams = nzptipPtr->data.texParams;
						pBind->dynamicTexBits = nzptipPtr->data.dynamicTexBits;

						//Storage is about to be replaced.
						pBind->texBytes = nzptipPtr->data.texBytes;

						m_nonZeroPrefixNodePool.add(nzptipPtr);
					} else {
						glGenTextures(1, &pBind->Id);
						(*m_allocatedTextures)++;
					}
				}
			}
		}
	}

	Tex.pBind = pBind;

	/*
	The source can have changed size, mip count, format or palette since it was first seen,
	so clearing the existing-bind flag is what turns the upload below into a full one and
	stops a sub-image update writing new data into storage laid out for the old.
	*/
	if (existingBind && !TexInfoMatchesBind(pBind, Info)) {
		CacheTextureInfo(pBind, Info, PolyFlags);
		existingBind = false;
		needTexAllocate = true;
	}

	//A refused format gets the no-texture object.
	//Not decided twice.
	glBindTexture(GL_TEXTURE_2D, (pBind->texType == TEX_TYPE_NONE) ? m_noTextureId : pBind->Id);

	unclock(BindCycles);

	Tex.UMult = pBind->UMult;
	Tex.VMult = pBind->VMult;

	{
		BYTE desiredDynamicTexBits;

		desiredDynamicTexBits = (PolyFlags & PF_NoSmooth) ? DT_NO_SMOOTH_BIT : 0;
		if (desiredDynamicTexBits != pBind->dynamicTexBits) {
			BYTE dynamicTexBitsXor;

			dynamicTexBitsXor = desiredDynamicTexBits ^ pBind->dynamicTexBits;

			//Updated early, since nothing later depends on it.
			pBind->dynamicTexBits = desiredDynamicTexBits;

			if (dynamicTexBitsXor & DT_NO_SMOOTH_BIT) {
				BYTE texFilter;

				texFilter = GenerateTexFilterParams(PolyFlags, pBind);

				SetTexFilter(pBind, texFilter);
			}
		}
	}

	if (!existingBind || TexInfoRealtimeChanged(Info)) {
		UploadTextureExec(Info, PolyFlags, pBind, existingBind, needTexAllocate);
	}

	unguard;
}
