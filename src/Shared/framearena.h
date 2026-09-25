/*=============================================================================
	framearena.h: a bump allocator for one frame of geometry.
=============================================================================*/

#ifndef UTGLR_FRAMEARENA_H
#define UTGLR_FRAMEARENA_H

#include <malloc.h>
#include <new>
#include <stddef.h>
#include <string.h>


#define FA_DEFAULT_BLOCK_BYTES (2 * 1024 * 1024)

#define FA_DEFAULT_ALIGN 16


#pragma pack(push, 8)

/**
Bump allocated blocks, rewound once per frame and kept across frames. Single threaded;
workers only read.
*/
class FFrameArena {
public:
	FFrameArena() :
		m_pHead(NULL),
		m_pCurrent(NULL),
		m_blockBytes(FA_DEFAULT_BLOCK_BYTES),
		m_bytesUsed(0),
		m_bytesReserved(0),
		m_outOfMemory(false) {
	}

	~FFrameArena() { Free(); }

	/** \param blockBytes Size of each block, or 0 for the default. */
	void Init(size_t blockBytes) {
		Free();
		m_blockBytes = blockBytes ? blockBytes : FA_DEFAULT_BLOCK_BYTES;
	}

	void Reset() {
		for (FBlock *pBlock = m_pHead; pBlock != NULL; pBlock = pBlock->pNext) {
			pBlock->Used = 0;
		}
		m_pCurrent = m_pHead;
		m_bytesUsed = 0;
		m_outOfMemory = false;
	}

	void Free() {
		FBlock *pBlock = m_pHead;
		while (pBlock != NULL) {
			FBlock *pNext = pBlock->pNext;
			_aligned_free(pBlock->pData);
			delete pBlock;
			pBlock = pNext;
		}
		m_pHead = NULL;
		m_pCurrent = NULL;
		m_bytesUsed = 0;
		m_bytesReserved = 0;
		m_outOfMemory = false;
	}

	/**
	\param align Must be a power of two.
	\return NULL only when out of memory; the flag stays set until the next rewind.
	*/
	void *Alloc(size_t bytes, size_t align) {
		if (bytes == 0) {
			bytes = 1;
		}
		if (align < 1) {
			align = 1;
		}

		for (;;) {
			if (m_pCurrent != NULL) {
				//Aligned by address: the block base carries only the default alignment.
				const size_t base = (size_t)m_pCurrent->pData;
				const size_t alignedAddr = (base + m_pCurrent->Used + (align - 1)) & ~(align - 1);
				const size_t start = alignedAddr - base;
				if ((start + bytes) <= m_pCurrent->Capacity) {
					void *pResult = m_pCurrent->pData + start;
					m_bytesUsed += (start + bytes) - m_pCurrent->Used;
					m_pCurrent->Used = start + bytes;
					return pResult;
				}
				if (m_pCurrent->pNext != NULL) {
					m_pCurrent = m_pCurrent->pNext;
					continue;
				}
			}

			if (!AppendBlock(bytes + align)) {
				m_outOfMemory = true;
				return NULL;
			}
		}
	}

	inline void *Alloc(size_t bytes) { return Alloc(bytes, FA_DEFAULT_ALIGN); }

	/** Checked: a wrapped product would be a small block. */
	template <typename T>
	inline T *AllocArray(size_t count) {
		if (count && (sizeof(T) > ((size_t)-1 / count))) {
			m_outOfMemory = true;
			return NULL;
		}
		return (T *)Alloc(sizeof(T) * count, FA_DEFAULT_ALIGN);
	}

	void *AllocCopy(const void *pSrc, size_t bytes, size_t align) {
		void *pDst = Alloc(bytes, align);
		if (pDst != NULL) {
			memcpy(pDst, pSrc, bytes);
		}
		return pDst;
	}

	inline bool IsExhausted() const { return m_outOfMemory; }
	inline size_t BytesUsed() const { return m_bytesUsed; }
	inline size_t BytesReserved() const { return m_bytesReserved; }

private:
	FFrameArena(const FFrameArena &);
	FFrameArena &operator=(const FFrameArena &);

	struct FBlock {
		unsigned char *pData;
		size_t Capacity;
		size_t Used;
		FBlock *pNext;
	};

	bool AppendBlock(size_t minBytes) {
		size_t capacity = m_blockBytes;
		if (capacity < minBytes) {
			capacity = minBytes;
		}

		unsigned char *pData = (unsigned char *)_aligned_malloc(capacity, FA_DEFAULT_ALIGN);
		if (pData == NULL) {
			return false;
		}

		FBlock *pBlock = new (std::nothrow) FBlock;
		if (pBlock == NULL) {
			_aligned_free(pData);
			return false;
		}
		pBlock->pData = pData;
		pBlock->Capacity = capacity;
		pBlock->Used = 0;
		pBlock->pNext = NULL;

		if (m_pCurrent != NULL) {
			m_pCurrent->pNext = pBlock;
		} else {
			m_pHead = pBlock;
		}
		m_pCurrent = pBlock;
		m_bytesReserved += capacity;
		return true;
	}

	FBlock *m_pHead;
	FBlock *m_pCurrent;
	size_t m_blockBytes;
	size_t m_bytesUsed;
	size_t m_bytesReserved;
	bool m_outOfMemory;
};

#pragma pack(pop)

#endif //UTGLR_FRAMEARENA_H
