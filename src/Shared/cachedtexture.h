/*=============================================================================
	cachedtexture.h: the cache list and the staleness test.
=============================================================================*/

#ifndef UTGLR_CACHEDTEXTURE_H
#define UTGLR_CACHEDTEXTURE_H

#include <stddef.h>


template <class CachedTextureT>
class TCachedTextureChain {
public:
	TCachedTextureChain() {
		mark_as_clear();
	}
	~TCachedTextureChain() {
	}

	inline void mark_as_clear(void) {
		m_head.pNext = &m_tail;
		m_tail.pPrev = &m_head;
	}

	inline void unlink(CachedTextureT *pCT) {
		pCT->pPrev->pNext = pCT->pNext;
		pCT->pNext->pPrev = pCT->pPrev;
	}
	inline void link_to_tail(CachedTextureT *pCT) {
		pCT->pPrev = m_tail.pPrev;
		pCT->pNext = &m_tail;
		m_tail.pPrev->pNext = pCT;
		m_tail.pPrev = pCT;
	}

	inline CachedTextureT *begin(void) {
		return m_head.pNext;
	}
	inline CachedTextureT *end(void) {
		return &m_tail;
	}

private:
	CachedTextureT m_head;
	CachedTextureT m_tail;
};

//A realtime texture can come back a different size, mip count or format. TextureInfoT is
//the engine's FTextureInfo.
template <class CachedTextureT, class TextureInfoT>
inline bool TexInfoMatchesBind(const CachedTextureT *pBind, const TextureInfoT &Info) {
	return (pBind->srcUSize == Info.USize) &&
		(pBind->srcVSize == Info.VSize) &&
		(pBind->srcNumMips == Info.NumMips) &&
		//srcFormat is a BYTE in every SDK here; the narrowing is deliberate
		(pBind->srcFormat == (unsigned char)Info.Format) &&
		(pBind->srcHasPalette == ((Info.Palette != NULL) ? 1 : 0));
}

#endif
