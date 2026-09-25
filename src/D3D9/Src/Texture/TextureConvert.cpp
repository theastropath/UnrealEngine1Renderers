/*=============================================================================
	TextureConvert.cpp: turning the engine's texture formats into Direct3D's.

	Palettised, DXT and the 7-bit-per-channel form the engine uses for lightmaps.
=============================================================================*/

#include "../D3D9Drv.h"
#include "../D3D9.h"
#include "TextureHelpers.h"
//DecodeDXTTexel returns the four components.
//The A8R8G8B8 packing is this file's business.
#include "dxtdecode.h"

//Honors stepBits.
void UD3D9RenderDevice::ConvertDXT_RGBA8888(const FMipmapBase *Mip, INT Level, DWORD dxtType) {
	DWORD *pTex = (DWORD *)m_texConvertCtx.lockRect.pBits;
	INT StepBits = m_texConvertCtx.stepBits;
	INT ij_inc = 1 << StepBits;
	INT i_stop = 1 << Max(0, (INT)m_texConvertCtx.pBind->VBits - Level + StepBits);
	INT j_stop = 1 << Max(0, (INT)m_texConvertCtx.pBind->UBits - Level + StepBits);

	const BYTE *pSrcBase = (const BYTE *)Mip->DataPtr;
	DWORD UMask = Mip->USize - 1;
	DWORD VMask = Mip->VSize - 1;
	//Still a whole block.
	DWORD blocksPerRow = (Mip->USize + 3) >> 2;
	DWORD bytesPerBlock = (dxtType == 1) ? 8 : 16;
	DWORD bytesPerBlockRow = blocksPerRow * bytesPerBlock;

	INT i = 0;
	do {
		DWORD srcY = (DWORD)i & VMask;
		const BYTE *pBlockRow = pSrcBase + ((srcY >> 2) * bytesPerBlockRow);
		INT j = 0;
		do {
			DWORD srcX = (DWORD)j & UMask;
			const BYTE *pBlock = pBlockRow + ((srcX >> 2) * bytesPerBlock);
			const dxt_texel_t texel = DecodeDXTTexel(pBlock, dxtType, srcX & 3, srcY & 3);
			pTex[j >> StepBits] = (texel.a << 24) | (texel.r << 16) | (texel.g << 8) | texel.b;
		} while ((j += ij_inc) < j_stop);
		pTex = (DWORD *)((BYTE *)pTex + m_texConvertCtx.lockRect.Pitch);
	} while ((i += ij_inc) < i_stop);

#ifdef UTGLR_DEBUG_SHOW_TEX_CONVERT_COUNTS
	{
		static int si;
		dout << L"utd3d9r: ConvertDXT_RGBA8888 = " << si++ << std::endl;
	}
#endif
}

void UD3D9RenderDevice::ConvertDXT1_DXT1(const FMipmapBase *Mip, INT Level) {
	const DWORD *pSrc = (DWORD *)Mip->DataPtr;
	DWORD *pTex = (DWORD *)m_texConvertCtx.lockRect.pBits;
	DWORD UBlocks = 1U << Max(0, (INT)m_texConvertCtx.pBind->UBits - Level - 2);
	DWORD VBlocks = 1U << Max(0, (INT)m_texConvertCtx.pBind->VBits - Level - 2);
	BoundBlocksToMip(Mip, UBlocks, VBlocks);

	for (DWORD v = 0; v < VBlocks; v++) {
		DWORD *pDest = pTex;
		for (DWORD u = 0; u < UBlocks; u++) {
			pDest[0] = pSrc[0];
			pDest[1] = pSrc[1];
			pSrc += 2;
			pDest += 2;
		}
		pTex = (DWORD *)((BYTE *)pTex + m_texConvertCtx.lockRect.Pitch);
	}

#ifdef UTGLR_DEBUG_SHOW_TEX_CONVERT_COUNTS
	{
		static int si;
		dout << L"utd3d9r: ConvertDXT1_DXT1 = " << si++ << std::endl;
	}
#endif
}

void UD3D9RenderDevice::ConvertDXT1_DXT3(const FMipmapBase *Mip, INT Level) {
	const DWORD *pSrc = (DWORD *)Mip->DataPtr;
	DWORD *pTex = (DWORD *)m_texConvertCtx.lockRect.pBits;
	DWORD UBlocks = 1U << Max(0, (INT)m_texConvertCtx.pBind->UBits - Level - 2);
	DWORD VBlocks = 1U << Max(0, (INT)m_texConvertCtx.pBind->VBits - Level - 2);
	BoundBlocksToMip(Mip, UBlocks, VBlocks);

	for (DWORD v = 0; v < VBlocks; v++) {
		DWORD *pDest = pTex;
		for (DWORD u = 0; u < UBlocks; u++) {
			pDest[0] = 0xFFFFFFFF;
			pDest[1] = 0xFFFFFFFF;
			pDest[2] = pSrc[0];
			pDest[3] = pSrc[1];
			pSrc += 2;
			pDest += 4;
		}
		pTex = (DWORD *)((BYTE *)pTex + m_texConvertCtx.lockRect.Pitch);
	}

#ifdef UTGLR_DEBUG_SHOW_TEX_CONVERT_COUNTS
	{
		static int si;
		dout << L"utd3d9r: ConvertDXT1_DXT3 = " << si++ << std::endl;
	}
#endif
}

void UD3D9RenderDevice::ConvertDXT35_DXT35(const FMipmapBase *Mip, INT Level) {
	const DWORD *pSrc = (DWORD *)Mip->DataPtr;
	DWORD *pTex = (DWORD *)m_texConvertCtx.lockRect.pBits;
	DWORD UBlocks = 1U << Max(0, (INT)m_texConvertCtx.pBind->UBits - Level - 2);
	DWORD VBlocks = 1U << Max(0, (INT)m_texConvertCtx.pBind->VBits - Level - 2);
	BoundBlocksToMip(Mip, UBlocks, VBlocks);

	for (DWORD v = 0; v < VBlocks; v++) {
		DWORD *pDest = pTex;
		for (DWORD u = 0; u < UBlocks; u++) {
			pDest[0] = pSrc[0];
			pDest[1] = pSrc[1];
			pDest[2] = pSrc[2];
			pDest[3] = pSrc[3];
			pSrc += 4;
			pDest += 4;
		}
		pTex = (DWORD *)((BYTE *)pTex + m_texConvertCtx.lockRect.Pitch);
	}

#ifdef UTGLR_DEBUG_SHOW_TEX_CONVERT_COUNTS
	{
		static int si;
		dout << L"utd3d9r: ConvertDXT35_DXT35 = " << si++ << std::endl;
	}
#endif
}

void UD3D9RenderDevice::ConvertP8_RGBA8888(const FMipmapBase *Mip, const FColor *Palette, INT Level) {
	DWORD *pTex = (DWORD *)m_texConvertCtx.lockRect.pBits;
	INT StepBits = m_texConvertCtx.stepBits;
	DWORD UMask = Mip->USize - 1;
	DWORD VMask = Mip->VSize - 1;
	INT ij_inc = 1 << StepBits;
	INT i_stop = 1 << Max(0, (INT)m_texConvertCtx.pBind->VBits - Level + StepBits);
	INT j_stop = 1 << Max(0, (INT)m_texConvertCtx.pBind->UBits - Level + StepBits);
	INT i = 0;
	do { //i_stop always >= 1
		BYTE *Base = (BYTE *)Mip->DataPtr + (i & VMask) * Mip->USize;
		INT j = 0;
		do { //j_stop always >= 1
			DWORD dwColor = GET_COLOR_DWORD(Palette[Base[j & UMask]]);
			pTex[j >> StepBits] = (dwColor & 0xFF00FF00) | ((dwColor >> 16) & 0xFF) | ((dwColor << 16) & 0xFF0000);
		} while ((j += ij_inc) < j_stop);
		pTex = (DWORD *)((BYTE *)pTex + m_texConvertCtx.lockRect.Pitch);
	} while ((i += ij_inc) < i_stop);

#ifdef UTGLR_DEBUG_SHOW_TEX_CONVERT_COUNTS
	{
		static int si;
		dout << L"utd3d9r: ConvertP8_RGBA8888 = " << si++ << std::endl;
	}
#endif
}

void UD3D9RenderDevice::ConvertP8_RGBA8888_NoStep(const FMipmapBase *Mip, const FColor *Palette, INT Level) {
	DWORD *pTex = (DWORD *)m_texConvertCtx.lockRect.pBits;
	DWORD UMask = Mip->USize - 1;
	DWORD VMask = Mip->VSize - 1;
	INT i_stop = m_texConvertCtx.texHeightPow2;
	INT j_stop = m_texConvertCtx.texWidthPow2;
	INT i = 0;
	do { //i_stop always >= 1
		BYTE *Base = (BYTE *)Mip->DataPtr + (i & VMask) * Mip->USize;
		INT j = 0;
		do { //j_stop always >= 1
			DWORD dwColor = GET_COLOR_DWORD(Palette[Base[j & UMask]]);
			pTex[j] = (dwColor & 0xFF00FF00) | ((dwColor >> 16) & 0xFF) | ((dwColor << 16) & 0xFF0000);
		} while ((j += 1) < j_stop);
		pTex = (DWORD *)((BYTE *)pTex + m_texConvertCtx.lockRect.Pitch);
	} while ((i += 1) < i_stop);

#ifdef UTGLR_DEBUG_SHOW_TEX_CONVERT_COUNTS
	{
		static int si;
		dout << L"utd3d9r: ConvertP8_RGBA8888_NoStep = " << si++ << std::endl;
	}
#endif
}

#ifdef UTGLR_INCLUDE_SSE_CODE
//The red/blue swap has only 256 possible outcomes, so it is done once over the palette here.
void UD3D9RenderDevice::ConvertP8_RGBA8888_NoStep_SSSE3(const FMipmapBase *Mip, const FColor *Palette, INT Level) {
	DWORD *pTex = (DWORD *)m_texConvertCtx.lockRect.pBits;
	DWORD UMask = Mip->USize - 1;
	DWORD VMask = Mip->VSize - 1;
	INT i_stop = m_texConvertCtx.texHeightPow2;
	INT j_stop = m_texConvertCtx.texWidthPow2;

	//Within each dword: byte 0 and byte 2 swap,
	//bytes 1 and 3 stay put - the same swap the scalar path applies per texel via (c & 0xFF00FF00) | (c >> 16 &
	//0xFF) | (c << 16 & 0xFF0000)
	const __m128i swizzle = _mm_set_epi8(15, 12, 13, 14, 11, 8, 9, 10, 7, 4, 5, 6, 3, 0, 1, 2);

	//A palette is always 256 FColor entries.
	__declspec(align(16)) DWORD swizzledPalette[256];
	for (INT k = 0; k < 256; k += 4) {
		_mm_store_si128((__m128i *)&swizzledPalette[k],
			_mm_shuffle_epi8(_mm_loadu_si128((const __m128i *)&Palette[k]), swizzle));
	}

	//Only a texture padded past its own width needs j folded back with the mask.
	const bool wraps = ((DWORD)j_stop > (DWORD)Mip->USize);

	INT i = 0;
	do { //i_stop always >= 1
		const BYTE *Base = (const BYTE *)Mip->DataPtr + (i & VMask) * Mip->USize;
		INT j = 0;

		if (!wraps) {
			for (; (j + 4) <= j_stop; j += 4) {
				//Gathers stay scalar.
				_mm_storeu_si128((__m128i *)&pTex[j], _mm_set_epi32((int)swizzledPalette[Base[j + 3]], (int)swizzledPalette[Base[j + 2]], (int)swizzledPalette[Base[j + 1]], (int)swizzledPalette[Base[j + 0]]));
			}
			for (; j < j_stop; j++) {
				pTex[j] = swizzledPalette[Base[j]];
			}
		} else {
			do { //j_stop always >= 1
				pTex[j] = swizzledPalette[Base[j & UMask]];
			} while ((j += 1) < j_stop);
		}

		pTex = (DWORD *)((BYTE *)pTex + m_texConvertCtx.lockRect.Pitch);
	} while ((i += 1) < i_stop);

#ifdef UTGLR_DEBUG_SHOW_TEX_CONVERT_COUNTS
	{
		static int si;
		dout << L"utd3d9r: ConvertP8_RGBA8888_NoStep_SSSE3 = " << si++ << std::endl;
	}
#endif
}
#endif //UTGLR_INCLUDE_SSE_CODE

void UD3D9RenderDevice::ConvertP8_RGB565(const FMipmapBase *Mip, const FColor *Palette, INT Level) {
	_WORD *pTex = (_WORD *)m_texConvertCtx.lockRect.pBits;
	INT StepBits = m_texConvertCtx.stepBits;
	DWORD UMask = Mip->USize - 1;
	DWORD VMask = Mip->VSize - 1;
	INT ij_inc = 1 << StepBits;
	INT i_stop = 1 << Max(0, (INT)m_texConvertCtx.pBind->VBits - Level + StepBits);
	INT j_stop = 1 << Max(0, (INT)m_texConvertCtx.pBind->UBits - Level + StepBits);
	INT i = 0;
	do { //i_stop always >= 1
		BYTE *Base = (BYTE *)Mip->DataPtr + (i & VMask) * Mip->USize;
		INT j = 0;
		do { //j_stop always >= 1
			DWORD dwColor = GET_COLOR_DWORD(Palette[Base[j & UMask]]);
			pTex[j >> StepBits] = ((dwColor >> 19) & 0x001F) | ((dwColor >> 5) & 0x07E0) | ((dwColor << 8) & 0xF800);
		} while ((j += ij_inc) < j_stop);
		pTex = (WORD *)((BYTE *)pTex + m_texConvertCtx.lockRect.Pitch);
	} while ((i += ij_inc) < i_stop);

#ifdef UTGLR_DEBUG_SHOW_TEX_CONVERT_COUNTS
	{
		static int si;
		dout << L"utd3d9r: ConvertP8_RGBA565 = " << si++ << std::endl;
	}
#endif
}

void UD3D9RenderDevice::ConvertP8_RGB565_NoStep(const FMipmapBase *Mip, const FColor *Palette, INT Level) {
	_WORD *pTex = (_WORD *)m_texConvertCtx.lockRect.pBits;
	DWORD UMask = Mip->USize - 1;
	DWORD VMask = Mip->VSize - 1;
	INT i_stop = m_texConvertCtx.texHeightPow2;
	INT j_stop = m_texConvertCtx.texWidthPow2;
	INT i = 0;
	do { //i_stop always >= 1
		BYTE *Base = (BYTE *)Mip->DataPtr + (i & VMask) * Mip->USize;
		INT j = 0;
		do { //j_stop always >= 1
			DWORD dwColor = GET_COLOR_DWORD(Palette[Base[j & UMask]]);
			pTex[j] = ((dwColor >> 19) & 0x001F) | ((dwColor >> 5) & 0x07E0) | ((dwColor << 8) & 0xF800);
		} while ((j += 1) < j_stop);
		pTex = (_WORD *)((BYTE *)pTex + m_texConvertCtx.lockRect.Pitch);
	} while ((i += 1) < i_stop);

#ifdef UTGLR_DEBUG_SHOW_TEX_CONVERT_COUNTS
	{
		static int si;
		dout << L"utd3d9r: ConvertP8_RGBA565_NoStep = " << si++ << std::endl;
	}
#endif
}

void UD3D9RenderDevice::ConvertP8_RGBA5551(const FMipmapBase *Mip, const FColor *Palette, INT Level) {
	_WORD *pTex = (_WORD *)m_texConvertCtx.lockRect.pBits;
	INT StepBits = m_texConvertCtx.stepBits;
	DWORD UMask = Mip->USize - 1;
	DWORD VMask = Mip->VSize - 1;
	INT ij_inc = 1 << StepBits;
	INT i_stop = 1 << Max(0, (INT)m_texConvertCtx.pBind->VBits - Level + StepBits);
	INT j_stop = 1 << Max(0, (INT)m_texConvertCtx.pBind->UBits - Level + StepBits);
	INT i = 0;
	do { //i_stop always >= 1
		BYTE *Base = (BYTE *)Mip->DataPtr + (i & VMask) * Mip->USize;
		INT j = 0;
		do { //j_stop always >= 1
			DWORD dwColor = GET_COLOR_DWORD(Palette[Base[j & UMask]]);
			pTex[j >> StepBits] = ((dwColor >> 19) & 0x001F) | ((dwColor >> 6) & 0x03E0) | ((dwColor << 7) & 0x7C00) | ((dwColor >> 16) & 0x8000);
		} while ((j += ij_inc) < j_stop);
		pTex = (WORD *)((BYTE *)pTex + m_texConvertCtx.lockRect.Pitch);
	} while ((i += ij_inc) < i_stop);

#ifdef UTGLR_DEBUG_SHOW_TEX_CONVERT_COUNTS
	{
		static int si;
		dout << L"utd3d9r: ConvertP8_RGBA5551 = " << si++ << std::endl;
	}
#endif
}

void UD3D9RenderDevice::ConvertP8_RGBA5551_NoStep(const FMipmapBase *Mip, const FColor *Palette, INT Level) {
	_WORD *pTex = (_WORD *)m_texConvertCtx.lockRect.pBits;
	DWORD UMask = Mip->USize - 1;
	DWORD VMask = Mip->VSize - 1;
	INT i_stop = m_texConvertCtx.texHeightPow2;
	INT j_stop = m_texConvertCtx.texWidthPow2;
	INT i = 0;
	do { //i_stop always >= 1
		BYTE *Base = (BYTE *)Mip->DataPtr + (i & VMask) * Mip->USize;
		INT j = 0;
		do { //j_stop always >= 1
			DWORD dwColor = GET_COLOR_DWORD(Palette[Base[j & UMask]]);
			pTex[j] = ((dwColor >> 19) & 0x001F) | ((dwColor >> 6) & 0x03E0) | ((dwColor << 7) & 0x7C00) | ((dwColor >> 16) & 0x8000);
		} while ((j += 1) < j_stop);
		pTex = (_WORD *)((BYTE *)pTex + m_texConvertCtx.lockRect.Pitch);
	} while ((i += 1) < i_stop);

#ifdef UTGLR_DEBUG_SHOW_TEX_CONVERT_COUNTS
	{
		static int si;
		dout << L"utd3d9r: ConvertP8_RGBA5551_NoStep = " << si++ << std::endl;
	}
#endif
}

void UD3D9RenderDevice::ConvertBGRA7777_BGRA8888(const FMipmapBase *Mip, INT Level) {
	DWORD *pTex = (DWORD *)m_texConvertCtx.lockRect.pBits;
	INT StepBits = m_texConvertCtx.stepBits;
	//UClampVal/VClampVal are texel counts in the base mip, so they shift down for the level being read.
	//Capped at 31: a larger shift is undefined.
	const INT clampShift = Min<INT>((INT)m_texConvertCtx.pBind->BaseMip + Level, 31);
	DWORD VMask = Mip->VSize - 1;
	DWORD VClampVal = m_texConvertCtx.pBind->VClampVal >> clampShift;
	DWORD UMask = Mip->USize - 1;
	DWORD UClampVal = m_texConvertCtx.pBind->UClampVal >> clampShift;
	INT ij_inc = 1 << StepBits;
	INT i_stop = 1 << Max(0, (INT)m_texConvertCtx.pBind->VBits - Level + StepBits);
	INT j_stop = 1 << Max(0, (INT)m_texConvertCtx.pBind->UBits - Level + StepBits);
	INT i = 0;
	do { //i_stop always >= 1
		FColor *Base = (FColor *)Mip->DataPtr + Min<DWORD>(i & VMask, VClampVal) * Mip->USize;
		INT j = 0;
		do { //j_stop always >= 1
			DWORD dwColor = GET_COLOR_DWORD(Base[Min<DWORD>(j & UMask, UClampVal)]);
			pTex[j >> StepBits] = dwColor * UTGLR_RGBA7_UPLOAD_MUL; // because of 7777
		} while ((j += ij_inc) < j_stop);
		pTex = (DWORD *)((BYTE *)pTex + m_texConvertCtx.lockRect.Pitch);
	} while ((i += ij_inc) < i_stop);

#ifdef UTGLR_DEBUG_SHOW_TEX_CONVERT_COUNTS
	{
		static int si;
		dout << L"utd3d9r: ConvertBGRA7777_BGRA8888 = " << si++ << std::endl;
	}
#endif
}

void UD3D9RenderDevice::ConvertBGRA7777_BGRA8888_NoClamp(const FMipmapBase *Mip, INT Level) {
	DWORD *pTex = (DWORD *)m_texConvertCtx.lockRect.pBits;
	INT StepBits = m_texConvertCtx.stepBits;
	DWORD VMask = Mip->VSize - 1;
	DWORD UMask = Mip->USize - 1;
	INT ij_inc = 1 << StepBits;
	INT i_stop = 1 << Max(0, (INT)m_texConvertCtx.pBind->VBits - Level + StepBits);
	INT j_stop = 1 << Max(0, (INT)m_texConvertCtx.pBind->UBits - Level + StepBits);
	INT i = 0;
	do { //i_stop always >= 1
		FColor *Base = (FColor *)Mip->DataPtr + (DWORD)(i & VMask) * Mip->USize;
		INT j = 0;
		do { //j_stop always >= 1
			DWORD dwColor = GET_COLOR_DWORD(Base[(DWORD)(j & UMask)]);
			pTex[j >> StepBits] = dwColor * UTGLR_RGBA7_UPLOAD_MUL; // because of 7777
		} while ((j += ij_inc) < j_stop);
		pTex = (DWORD *)((BYTE *)pTex + m_texConvertCtx.lockRect.Pitch);
	} while ((i += ij_inc) < i_stop);

#ifdef UTGLR_DEBUG_SHOW_TEX_CONVERT_COUNTS
	{
		static int si;
		dout << L"utd3d9r: ConvertBGRA7777_BGRA8888_NoClamp = " << si++ << std::endl;
	}
#endif
}
