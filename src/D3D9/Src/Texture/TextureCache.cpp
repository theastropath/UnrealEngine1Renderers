/*=============================================================================
	TextureCache.cpp: what is resident, and what gets thrown out.

	The cache is shared between render devices, so eviction has to account for every
	device's use of it.
=============================================================================*/

#include "../D3D9Drv.h"
#include "../D3D9.h"
#include "TextureHelpers.h"

void UD3D9RenderDevice::PrecacheTexture(FTextureInfo &Info, DWORD PolyFlags) {
	guard(UD3D9RenderDevice::PrecacheTexture);

	if (m_frameSkipped) {
		return;
	}

	FlushDeferred();
	EndBuffering();
	SetDefaultTextureState();

	SetTextureNoPanBias(0, Info, PolyFlags);
	unguard;
}


void UD3D9RenderDevice::InitNoTextureSafe(void) {
	guard(UD3D9RenderDevice::InitNoTexture);
	unsigned int u, v;
	HRESULT hResult;
	D3DLOCKED_RECT lockRect;
	DWORD *pTex;

	if (m_pNoTexObj != 0) {
		return;
	}

	hResult = m_d3dDevice->CreateTexture(4, 4, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &m_pNoTexObj, NULL);
	if (FAILED(hResult)) {
		appErrorf(TEXT("CreateTexture (basic RGBA8) failed"));
	}

	if (FAILED(m_pNoTexObj->LockRect(0, &lockRect, NULL, D3DLOCK_NOSYSLOCK))) {
		appErrorf(TEXT("Texture lock failed"));
	}

	pTex = (DWORD *)lockRect.pBits;
	for (u = 0; u < 4; u++) {
		for (v = 0; v < 4; v++) {
			pTex[v] = 0xFFFFFFFF;
		}
		pTex = (DWORD *)((BYTE *)pTex + lockRect.Pitch);
	}

	if (FAILED(m_pNoTexObj->UnlockRect(0))) {
		appErrorf(TEXT("Texture unlock failed"));
	}

	return;
	unguard;
}

void UD3D9RenderDevice::InitAlphaTextureSafe(void) {
	guard(UD3D9RenderDevice::InitAlphaTexture);
	unsigned int u;
	HRESULT hResult;
	D3DLOCKED_RECT lockRect;
	BYTE *pTex;

	if (m_pAlphaTexObj != 0) {
		return;
	}

	hResult = m_d3dDevice->CreateTexture(256, 1, 1, 0, D3DFMT_A8, D3DPOOL_MANAGED, &m_pAlphaTexObj, NULL);
	if (FAILED(hResult)) {
		appErrorf(TEXT("CreateTexture (alpha) failed"));
	}

	if (FAILED(m_pAlphaTexObj->LockRect(0, &lockRect, NULL, D3DLOCK_NOSYSLOCK))) {
		appErrorf(TEXT("Texture lock failed"));
	}

	pTex = (BYTE *)lockRect.pBits;
	for (u = 0; u < 256; u++) {
		pTex[u] = 255 - u;
	}

	if (FAILED(m_pAlphaTexObj->UnlockRect(0))) {
		appErrorf(TEXT("Texture unlock failed"));
	}

	return;
	unguard;
}

void UD3D9RenderDevice::ScanForOldTextures(void) {
	guard(UD3D9RenderDevice::ScanForOldTextures);

	//Recorded bindings name a cache entry that must still exist at submission.
	//Asserted, so moving it earlier fails loudly.
	check(m_drawCmds.empty());

	unsigned int u;
	FCachedTexture *pCT;

	for (u = 0; u < MAX_TMUNITS; u++) {
		FCachedTexture *pBind = TexInfo[u].pBind;
		if (pBind != NULL) {
			pBind->LastUsedFrameCount = m_currentFrameCount;

			if (pBind->bindType == BIND_TYPE_NON_ZERO_PREFIX_LRU_LIST) {
				m_nonZeroPrefixBindChain->unlink(pBind);
				m_nonZeroPrefixBindChain->link_to_tail(pBind);
			} else if (pBind->bindType == BIND_TYPE_ZERO_PREFIX) {
				m_zeroPrefixBindChain->unlink(pBind);
				m_zeroPrefixBindChain->link_to_tail(pBind);
			}
		}
	}

	//Capped so a whole set going out of view together cannot be released in one spike.
	enum { MAX_RECYCLES_PER_FRAME = 16 };
	DWORD recycleBudget = MAX_RECYCLES_PER_FRAME;

	pCT = m_nonZeroPrefixBindChain->begin();
	while (pCT != m_nonZeroPrefixBindChain->end()) {
		DWORD numFramesSinceUsed = m_currentFrameCount - pCT->LastUsedFrameCount;
		if (numFramesSinceUsed > (DWORD)DynamicTexIdRecycleLevel) {
			if (recycleBudget-- == 0) {
				break;
			}
			if (!UseTexPool || (pCT->pTexObj == NULL) || (pCT->texFormat != D3DFMT_A8R8G8B8) || (pCT->texParams.filter & CT_HAS_MIPMAPS_BIT)) {
				m_nonZeroPrefixBindChain->unlink(pCT);

				QWORD_CTTree_t::node_t *pNode = (QWORD_CTTree_t::node_t *)((BYTE *)pCT - (DWORD) & (((QWORD_CTTree_t::node_t *)0)->data));
				BYTE treeIndex = pCT->treeIndex;
				pCT = pCT->pNext;

				m_nonZeroPrefixBindTrees[treeIndex].remove(pNode);

				if (pNode->data.pTexObj) {
					pNode->data.pTexObj->Release();
					AllocatedTextures--;
					m_texCacheBytes -= pNode->data.texBytes;
				}

				//Ownership comes back here.
				m_QWORD_CTTree_Allocator.free_node(pNode);
#if 0
{
	static int si;
	dout << L"utd3d9r: Texture delete = " << si++ << std::endl;
}
#endif

				continue;
			} else {
				TexPoolMap_t::node_t *texPoolPtr;

#if 0
{
	static int si;
	dout << L"utd3d9r: TexPool free = " << si++ << L", Id = 0x" << HexString((DWORD)pCT->pTexObj, 32)
		<< L", u = " << pCT->UBits << L", v = " << pCT->VBits << std::endl;
}
#endif

				m_nonZeroPrefixBindChain->unlink(pCT);

				TexPoolMapKey_t texPoolKey = MakeTexPoolMapKey(pCT->UBits, pCT->VBits);

				QWORD_CTTree_t::node_t *pNode = (QWORD_CTTree_t::node_t *)((BYTE *)pCT - (DWORD) & (((QWORD_CTTree_t::node_t *)0)->data));
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

				//The texture object stays alive, so the allocation count is untouched,
				//but its bytes leave the budget: eviction can never reach a pooled texture.
				//Added back on retrieval.
				m_texCacheBytes -= pNode->data.texBytes;

				texPoolPtr->data.add(pNode);

				continue;
			}
		}

		break;
	}

	unguard;
}

/*
Zero prefix binds are the game's own textures, kept until Flush,
so the cache is otherwise unbounded.
What the budget counts and what this can evict differ,
so the loop's condition can be unsatisfiable:
left uncapped it would destroy every game texture not drawn last frame.
*/
void UD3D9RenderDevice::EvictOverBudgetTextures(void) {
	guard(UD3D9RenderDevice::EvictOverBudgetTextures);

	check(m_drawCmds.empty());

	unsigned int u;
	FCachedTexture *pCT;
	INT budgetMegs;
	DWORD budgetBytes;

	if (TexCacheBudgetMegs <= 0) {
		return;
	}
	budgetMegs = (TexCacheBudgetMegs > 2048) ? 2048 : TexCacheBudgetMegs;
	budgetBytes = (DWORD)budgetMegs * 1024 * 1024;
	if (m_texCacheBytes <= budgetBytes) {
		m_texCacheBudgetUnreachable = false;
		return;
	}

	enum { MAX_EVICTIONS_PER_FRAME = 16 };
	DWORD evictionBudget = MAX_EVICTIONS_PER_FRAME;

	for (u = 0; u < MAX_TMUNITS; u++) {
		FCachedTexture *pBind = TexInfo[u].pBind;
		if ((pBind != NULL) && (pBind->bindType == BIND_TYPE_ZERO_PREFIX)) {
			pBind->LastUsedFrameCount = m_currentFrameCount;
			m_zeroPrefixBindChain->unlink(pBind);
			m_zeroPrefixBindChain->link_to_tail(pBind);
		}
	}

	pCT = m_zeroPrefixBindChain->begin();
	while ((pCT != m_zeroPrefixBindChain->end()) && (m_texCacheBytes > budgetBytes)) {
		if (pCT->LastUsedFrameCount == m_currentFrameCount) {
			break;
		}

		if (evictionBudget-- == 0) {
			break;
		}

		m_zeroPrefixBindChain->unlink(pCT);

		DWORD_CTTree_t::node_t *pNode = (DWORD_CTTree_t::node_t *)((BYTE *)pCT - (DWORD) & (((DWORD_CTTree_t::node_t *)0)->data));
		BYTE treeIndex = pCT->treeIndex;
		pCT = pCT->pNext;

		m_zeroPrefixBindTrees[treeIndex].remove(pNode);

		if (pNode->data.pTexObj) {
			pNode->data.pTexObj->Release();
			AllocatedTextures--;
			m_texCacheBytes -= pNode->data.texBytes;
		}

		//Ownership comes back here.
		m_DWORD_CTTree_Allocator.free_node(pNode);
	}

	//Reported once, when the evictable set runs out with the total still over budget.
	//Sticky until back under budget.
	if ((pCT == m_zeroPrefixBindChain->end()) && (m_texCacheBytes > budgetBytes) && !m_texCacheBudgetUnreachable) {
		m_texCacheBudgetUnreachable = true;
		debugf(NAME_Warning, TEXT("TexCacheBudgetMegs of %i cannot be met: %i MB is in textures this path cannot evict"),
			(INT)budgetMegs, (INT)(m_texCacheBytes / (1024 * 1024)));
	}

	unguard;
}

void UD3D9RenderDevice::SetNoTextureNoCheck(INT Multi) {
	guard(UD3D9RenderDevice::SetNoTexture);

	clock(BindCycles);

	m_d3dDevice->SetTexture(Multi, m_pNoTexObj);

	SetTexFilter(Multi, CT_MIN_FILTER_POINT | CT_MIP_FILTER_NONE);

	TexInfo[Multi].CurrentCacheID = TEX_CACHE_ID_NO_TEX;
	TexInfo[Multi].pBind = NULL;

	unclock(BindCycles);

	unguard;
}

void UD3D9RenderDevice::SetAlphaTextureNoCheck(INT Multi) {
	guard(UD3D9RenderDevice::SetAlphaTexture);

	clock(BindCycles);

	m_d3dDevice->SetTexture(Multi, m_pAlphaTexObj);

	SetTexFilter(Multi, CT_MIN_FILTER_LINEAR | CT_MIP_FILTER_NONE | CT_MAG_FILTER_LINEAR_NOT_POINT_BIT | CT_ADDRESS_CLAMP_NOT_WRAP_BIT);

	TexInfo[Multi].CurrentCacheID = TEX_CACHE_ID_ALPHA_TEX;
	TexInfo[Multi].pBind = NULL;

	unclock(BindCycles);

	unguard;
}


/*
Takes an id already in the form the trees are keyed by, which is what ComposeTexCacheID produces,
so a caller holding a raw FTextureInfo::CacheID has to compose it first: composition only folds the
masked and 16 bit flags into suffix 0xE0.
*/
FCachedTexture *FASTCALL UD3D9RenderDevice::FindCachedTextureBind(QWORD CacheID) {
	const DWORD CacheIDSuffix = (DWORD)(CacheID & 0x00000000FFFFFFFFULL);

	if ((CacheID & 0xFFFFFFFF00000000ULL) == 0) {
		DWORD_CTTree_t *pTree = &m_zeroPrefixBindTrees[CTZeroPrefixCacheIDSuffixToTreeIndex(CacheIDSuffix)];
		DWORD_CTTree_t::node_t *pNode = pTree->find(CacheIDSuffix);
		return (pNode != 0) ? &pNode->data : NULL;
	}

	QWORD_CTTree_t *pTree = &m_nonZeroPrefixBindTrees[CTNonZeroPrefixCacheIDSuffixToTreeIndex(CacheIDSuffix)];
	QWORD_CTTree_t::node_t *pNode = pTree->find(CacheID);
	return (pNode != 0) ? &pNode->data : NULL;
}

/*
A lightmap is cached in two unrelated places - on an atlas page,
and as a texture of its own for any frame its surface declines batching - and the engine reports a change through
the single bRealtimeChanged bit between them.
Whichever drew last consuming it would leave the other holding old lighting.
*/
void FASTCALL UD3D9RenderDevice::NoteLightmapChanged(FTextureInfo &Info) {
	m_lightmapAtlas.NoteChanged(Info.CacheID);

	FCachedTexture *pBind = FindCachedTextureBind(Info.CacheID);
	if (pBind != NULL) {
		pBind->bContentStale = 1;
	}

	//A texture unit holding this lightmap would otherwise skip the rebind on its cache id alone.
	for (DWORD u = 0; u < MAX_TMUNITS; u++) {
		if (TexInfo[u].CurrentCacheID == Info.CacheID) {
			TexInfo[u].CurrentCacheID = TEX_CACHE_ID_UNUSED;
			TexInfo[u].pBind = NULL;
		}
	}

	TexInfoClearRealtimeChanged(Info);
}
