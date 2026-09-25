/*=============================================================================
	TextureCache.cpp: residency and eviction.
=============================================================================*/

#include "../OpenGLDrv.h"
#include "../OpenGL.h"

void UOpenGLRenderDevice::PrecacheTexture(FTextureInfo &Info, DWORD PolyFlags) {
	guard(UOpenGLRenderDevice::PrecacheTexture);

	//Unit 0 like every other bind.
	EndBuffering();
	SetDefaultTextureState();

	SetTextureNoPanBias(0, Info, PolyFlags);

	unguard;
}


void UOpenGLRenderDevice::InitNoTextureSafe(void) {
	guard(UOpenGLRenderDevice::InitNoTexture);
	unsigned int u;
	DWORD Data[4 * 4];

	if (m_noTextureId != 0) {
		return;
	}

	for (u = 0; u < (4 * 4); u++) {
		Data[u] = 0xFFFFFFFF;
	}

	glGenTextures(1, &m_noTextureId);

	SetNoTexture(0);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, 4, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, Data);

	return;
	unguard;
}

void UOpenGLRenderDevice::InitAlphaTextureSafe(void) {
	guard(UOpenGLRenderDevice::InitAlphaTexture);
	unsigned int u;
	BYTE AlphaData[256];

	if (m_alphaTextureId != 0) {
		return;
	}

	for (u = 0; u < 256; u++) {
		AlphaData[u] = 255 - u;
	}

	glGenTextures(1, &m_alphaTextureId);

	SetAlphaTexture(0);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, 256, 1, 0, GL_ALPHA, GL_UNSIGNED_BYTE, AlphaData);

	return;
	unguard;
}

void UOpenGLRenderDevice::ScanForDeletedTextures(void) {
	guard(UOpenGLRenderDevice::ScanForDeletedTextures);

	unsigned int u;

	//Another context may have deleted this one.
	for (u = 0; u < MAX_TMUNITS; u++) {
		FCachedTexture *pBind = TexInfo[u].pBind;
		if (pBind != NULL) {
			//Compare pointers, because an id can be re-cached.
			if (FindCachedTexture(TexInfo[u].CurrentCacheID) != pBind) {
				//Only the cache entry.
				TexInfo[u].CurrentCacheID = TEX_CACHE_ID_UNUSED;
				TexInfo[u].pBind = NULL;
			}
		}
	}

	unguard;
}

DWORD UOpenGLRenderDevice::CalcTexBytes(const FCachedTexture *pBind, INT levelCount) {
	const GLenum texInternalFormat = pBind->texInternalFormat;
	DWORD bytes = 0;
	DWORD width = 1U << pBind->UBits;
	DWORD height = 1U << pBind->VBits;
	INT level;

	for (level = 0; level < levelCount; level++) {
		switch (texInternalFormat) {
			case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
				//Compressed levels are stored as 4x4 blocks.
				bytes += ((width + 3) / 4) * ((height + 3) / 4) * 8;
				break;

			case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
			case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
				bytes += ((width + 3) / 4) * ((height + 3) / 4) * 16;
				break;

			case GL_COLOR_INDEX8_EXT:
				bytes += width * height;
				break;

			case GL_RGB5:
			case GL_RGB5_A1:
				bytes += width * height * 2;
				break;

			default:
				bytes += width * height * 4;
		}

		//Both are halved to a floor of one.
		width = (width & 0x1) | (width >> 1);
		height = (height & 0x1) | (height >> 1);
	}

	return bytes;
}

/*
Zero prefix binds are the game's own textures and they are kept until Flush, so the cache
is otherwise unbounded in a 32 bit process and only those are evictable here while the
budget counts more than that, which makes the loop's condition unsatisfiable and is why
the per-frame cap below exists at all.
*/
void UOpenGLRenderDevice::EvictOverBudgetTextures(void) {
	guard(UOpenGLRenderDevice::EvictOverBudgetTextures);

	unsigned int u;
	FCachedTexture *pCT;
	INT budgetMegs;
	DWORD budgetBytes;

	if (TexCacheBudgetMegs <= 0) {
		return;
	}
	//Beyond 2048 the byte conversion would wrap.
	budgetMegs = (TexCacheBudgetMegs > 2048) ? 2048 : TexCacheBudgetMegs;
	budgetBytes = (DWORD)budgetMegs * 1024 * 1024;
	if (*m_texCacheBytes <= budgetBytes) {
		//Reports again next time.
		m_texCacheBudgetUnreachable = false;
		return;
	}

	//A large set falling out of use drains over a few frames.
	//The list is ordered by use.
	enum { MAX_EVICTIONS_PER_FRAME = 16 };
	DWORD evictionBudget = MAX_EVICTIONS_PER_FRAME;

	//A still bound texture skips the cache hit path.
	for (u = 0; u < MAX_TMUNITS; u++) {
		FCachedTexture *pBind = TexInfo[u].pBind;
		if ((pBind != NULL) && (pBind->bindType == BIND_TYPE_ZERO_PREFIX)) {
			pBind->LastUsedFrameCount = m_currentFrameCount;
			m_zeroPrefixBindChain->unlink(pBind);
			m_zeroPrefixBindChain->link_to_tail(pBind);
		}
	}

	TArray<GLuint> Binds;

	pCT = m_zeroPrefixBindChain->begin();
	while ((pCT != m_zeroPrefixBindChain->end()) && (*m_texCacheBytes > budgetBytes)) {
		//In use order, so the rest belongs to this frame.
		if (pCT->LastUsedFrameCount == m_currentFrameCount) {
			break;
		}

		if (evictionBudget-- == 0) {
			break;
		}

		m_zeroPrefixBindChain->unlink(pCT);

		//32-bit only.
		DWORD_CTTree_t::node_t *pNode = (DWORD_CTTree_t::node_t *)((BYTE *)pCT - (uintptr_t)&(((DWORD_CTTree_t::node_t *)0)->data));
		BYTE treeIndex = pCT->treeIndex;
		pCT = pCT->pNext;

		m_zeroPrefixBindTrees[treeIndex].remove(pNode);

		Binds.AddItem(pNode->data.Id);
		(*m_allocatedTextures)--;
		*m_texCacheBytes -= pNode->data.texBytes;

		//Ownership comes back here.
		m_DWORD_CTTree_Allocator.free_node(pNode);
	}

	if (Binds.Num()) {
		glDeleteTextures(Binds.Num(), (GLuint *)&Binds(0));
	}

	//Reported once the evictable set runs out.
	//Sticky until back under budget.
	if ((pCT == m_zeroPrefixBindChain->end()) && (*m_texCacheBytes > budgetBytes) && !m_texCacheBudgetUnreachable) {
		m_texCacheBudgetUnreachable = true;
		debugf(NAME_Warning, TEXT("TexCacheBudgetMegs of %i cannot be met: %i MB is in textures this path cannot evict"),
			(INT)budgetMegs, (INT)(*m_texCacheBytes / (1024 * 1024)));
	}

	unguard;
}

void UOpenGLRenderDevice::ScanForOldTextures(void) {
	guard(UOpenGLRenderDevice::ScanForOldTextures);

	unsigned int u;
	FCachedTexture *pCT;

	//Prevent bound textures from being recycled.
	for (u = 0; u < MAX_TMUNITS; u++) {
		FCachedTexture *pBind = TexInfo[u].pBind;
		if (pBind != NULL) {
			pBind->LastUsedFrameCount = m_currentFrameCount;

			if (pBind->bindType == BIND_TYPE_NON_ZERO_PREFIX_LRU_LIST) {
				m_nonZeroPrefixBindChain->unlink(pBind);
				m_nonZeroPrefixBindChain->link_to_tail(pBind);
			}
			//Same, for use order.
			else if (pBind->bindType == BIND_TYPE_ZERO_PREFIX) {
				m_zeroPrefixBindChain->unlink(pBind);
				m_zeroPrefixBindChain->link_to_tail(pBind);
			}
		}
	}

	/*
	An Immediate config property, so the options page or a console set can change it live
	with nothing else re-clamping it, and a negative would cast to about 4.29 billion and
	stop recycling altogether, which is why the clamp sits here at the point of use and why
	anything reachable from the console gets read as hostile input.
	*/
	const DWORD recycleLevel = (DynamicTexIdRecycleLevel < 10) ? 10 : (DWORD)DynamicTexIdRecycleLevel;

	//Bounded per frame.
	DWORD recyclesLeft = MAX_TEX_ID_RECYCLES_PER_FRAME;

	pCT = m_nonZeroPrefixBindChain->begin();
	while ((pCT != m_nonZeroPrefixBindChain->end()) && (recyclesLeft != 0)) {
		DWORD numFramesSinceUsed = m_currentFrameCount - pCT->LastUsedFrameCount;
		if (numFramesSinceUsed > recycleLevel) {
			recyclesLeft--;

			if (!UseTexPool || (pCT->texInternalFormat != GL_RGBA8) || (pCT->texParams.hasMipmaps)) {
				m_nonZeroPrefixBindChain->unlink(pCT);

				QWORD_CTTree_t::node_t *pNode = (QWORD_CTTree_t::node_t *)((BYTE *)pCT - (uintptr_t)&(((QWORD_CTTree_t::node_t *)0)->data));
				BYTE treeIndex = pCT->treeIndex;
				pCT = pCT->pNext;

				m_nonZeroPrefixBindTrees[treeIndex].remove(pNode);

				m_nonZeroPrefixTexIdPool->add(pNode);

				continue;
			} else {
				TexPoolMap_t::node_t *texPoolPtr;

#if 0
{
	static unsigned int s_c;
	dbgPrintf("utglr: TexPool free = %u, Id = %u, u = %u, v = %u\n",
		s_c++, pCT->Id, pCT->UBits, pCT->VBits);
}
#endif

				m_nonZeroPrefixBindChain->unlink(pCT);

				TexPoolMapKey_t texPoolKey = MakeTexPoolMapKey(pCT->UBits, pCT->VBits);

				QWORD_CTTree_t::node_t *pNode = (QWORD_CTTree_t::node_t *)((BYTE *)pCT - (uintptr_t)&(((QWORD_CTTree_t::node_t *)0)->data));
				BYTE treeIndex = pCT->treeIndex;
				pCT = pCT->pNext;

				m_nonZeroPrefixBindTrees[treeIndex].remove(pNode);

				texPoolPtr = m_RGBA8TexPool->find(texPoolKey);
				if (texPoolPtr == 0) {
					texPoolPtr = m_TexPoolMap_Allocator.alloc_node();
					texPoolPtr->key = texPoolKey;
					texPoolPtr->data = QWORD_CTTree_NodePool_t();
					m_RGBA8TexPool->insert(texPoolPtr);
				}

				texPoolPtr->data.add(pNode);

				continue;
			}
		}

		//The list is sorted by use.
		//The first entry not due means none after it are.
		break;
	}

	unguard;
}
