/*=============================================================================
	TextureConvert.cpp: the engine's texture formats, turned into GL's.
=============================================================================*/

#include "../OpenGLDrv.h"
#include "../OpenGL.h"
//DecodeDXTTexel returns four components.
#include "dxtdecode.h"

void UOpenGLRenderDevice::ConvertDXT1_DXT3(const FMipmapBase *Mip, INT Level) {
	DWORD *pSrc = (DWORD *)Mip->DataPtr;
	DWORD *pDest = (DWORD *)m_texConvertCtx.pCompose;
	/*
	Bind dimensions can be inflated past the source mip's real size when copy bits are in play
	and the uncompressed converters mask their source index, while compressed blocks cannot be
	masked, so this stops short at the real block count and leaves the rest as it was, which
	is why the bind path declines a compressed upload whenever the dimensions disagree.
	*/
	DWORD srcBlocks = ((DWORD)Max(0, Mip->USize + 3) >> 2) * ((DWORD)Max(0, Mip->VSize + 3) >> 2);
	DWORD destBlocks = 1 << (Max(0, (INT)m_texConvertCtx.pBind->UBits - Level - 2) + Max(0, (INT)m_texConvertCtx.pBind->VBits - Level - 2));
	DWORD numBlocks = Min(srcBlocks, destBlocks);
	for (DWORD block = 0; block < numBlocks; block++) {
		*pDest = 0xFFFFFFFF;
		*(pDest + 1) = 0xFFFFFFFF;
		*(pDest + 2) = *pSrc;
		*(pDest + 3) = *(pSrc + 1);
		pSrc += 2;
		pDest += 4;
	}

	UTGLR_DEBUG_TEX_CONVERT_COUNT(ConvertDXT1_DXT3);
}

//Honors stepBits, so it can resample.
//Used when the driver refuses the compressed format.
//The destination is GL_RGBA, matching FColor's byte layout.
void UOpenGLRenderDevice::ConvertDXT_RGBA8888(const FMipmapBase *Mip, INT Level, DWORD dxtType) {
	FColor *pTex = (FColor *)m_texConvertCtx.pCompose;
	INT StepBits = m_texConvertCtx.stepBits;
	INT ij_inc = 1 << StepBits;
	INT i_stop = 1 << Max(0, (INT)m_texConvertCtx.pBind->VBits - Level + StepBits);
	INT j_stop = 1 << Max(0, (INT)m_texConvertCtx.pBind->UBits - Level + StepBits);
	//pCompose has no row padding, so one destination row is exactly the mip's width
	DWORD destRowPixels = 1U << Max(0, (INT)m_texConvertCtx.pBind->UBits - Level);

	const BYTE *pSrcBase = (const BYTE *)Mip->DataPtr;
	DWORD UMask = Mip->USize - 1;
	DWORD VMask = Mip->VSize - 1;
	//Partial blocks at the edge still occupy a whole 4x4 block
	DWORD blocksPerRow = (Mip->USize + 3) >> 2;
	DWORD bytesPerBlock = (dxtType == 1) ? 8 : 16;
	DWORD bytesPerBlockRow = blocksPerRow * bytesPerBlock;

	INT i = 0;
	do { //i_stop always >= 1
		DWORD srcY = (DWORD)i & VMask;
		const BYTE *pBlockRow = pSrcBase + ((srcY >> 2) * bytesPerBlockRow);
		INT j = 0;
		do { //j_stop always >= 1
			DWORD srcX = (DWORD)j & UMask;
			const BYTE *pBlock = pBlockRow + ((srcX >> 2) * bytesPerBlock);
			const dxt_texel_t texel = DecodeDXTTexel(pBlock, dxtType, srcX & 3, srcY & 3);
			pTex[j >> StepBits] = FColor((BYTE)texel.r, (BYTE)texel.g, (BYTE)texel.b, (BYTE)texel.a);
		} while ((j += ij_inc) < j_stop);
		pTex += destRowPixels;
	} while ((i += ij_inc) < i_stop);

	UTGLR_DEBUG_TEX_CONVERT_COUNT(ConvertDXT_RGBA8888);
}

void UOpenGLRenderDevice::ConvertP8_P8(const FMipmapBase *Mip, INT Level) {
	BYTE *Ptr = (BYTE *)m_texConvertCtx.pCompose;
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
			*Ptr++ = Base[j & UMask];
		} while ((j += ij_inc) < j_stop);
	} while ((i += ij_inc) < i_stop);

	UTGLR_DEBUG_TEX_CONVERT_COUNT(ConvertP8_P8);
}

void UOpenGLRenderDevice::ConvertP8_P8_NoStep(const FMipmapBase *Mip, INT Level) {
	BYTE *Ptr = (BYTE *)m_texConvertCtx.pCompose;
	DWORD UMask = Mip->USize - 1;
	DWORD VMask = Mip->VSize - 1;
	INT i_stop = m_texConvertCtx.texHeightPow2;
	INT j_stop = m_texConvertCtx.texWidthPow2;
	INT i = 0;
	do { //i_stop always >= 1
		BYTE *Base = (BYTE *)Mip->DataPtr + (i & VMask) * Mip->USize;
		INT j = 0;
		do { //j_stop always >= 1
			*Ptr++ = Base[j & UMask];
		} while ((j += 1) < j_stop);
	} while ((i += 1) < i_stop);

	UTGLR_DEBUG_TEX_CONVERT_COUNT(ConvertP8_P8_NoStep);
}

void UOpenGLRenderDevice::ConvertP8_RGBA8888(const FMipmapBase *Mip, const FColor *Palette, INT Level) {
	FColor *Ptr = (FColor *)m_texConvertCtx.pCompose;
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
			*Ptr++ = Palette[Base[j & UMask]];
		} while ((j += ij_inc) < j_stop);
	} while ((i += ij_inc) < i_stop);

	UTGLR_DEBUG_TEX_CONVERT_COUNT(ConvertP8_RGBA8888);
}

void UOpenGLRenderDevice::ConvertP8_RGBA8888_NoStep(const FMipmapBase *Mip, const FColor *Palette, INT Level) {
	FColor *Ptr = (FColor *)m_texConvertCtx.pCompose;
	DWORD UMask = Mip->USize - 1;
	DWORD VMask = Mip->VSize - 1;
	INT i_stop = m_texConvertCtx.texHeightPow2;
	INT j_stop = m_texConvertCtx.texWidthPow2;
	INT i = 0;
	do { //i_stop always >= 1
		BYTE *Base = (BYTE *)Mip->DataPtr + (i & VMask) * Mip->USize;
		INT j = 0;
		do { //j_stop always >= 1
			*Ptr++ = Palette[Base[j & UMask]];
		} while ((j += 1) < j_stop);
	} while ((i += 1) < i_stop);

	UTGLR_DEBUG_TEX_CONVERT_COUNT(ConvertP8_RGBA8888_NoStep);
}

void UOpenGLRenderDevice::ConvertBGRA7777_BGRA8888(const FMipmapBase *Mip, INT Level) {
	FColor *Ptr = (FColor *)m_texConvertCtx.pCompose;
	INT StepBits = m_texConvertCtx.stepBits;
	//UClampVal and VClampVal are texel counts in the base mip.
	//They shift down for the level being read.
	//Capped at 31.
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
			const FColor &Src = Base[Min<DWORD>(j & UMask, UClampVal)];
			*(DWORD *)Ptr = *((DWORD *)&Src) * UTGLR_RGBA7_UPLOAD_MUL; // because of 7777
			Ptr++;
		} while ((j += ij_inc) < j_stop);
	} while ((i += ij_inc) < i_stop);

	UTGLR_DEBUG_TEX_CONVERT_COUNT(ConvertBGRA7777_BGRA8888);
}

void UOpenGLRenderDevice::ConvertBGRA7777_BGRA8888_NoClamp(const FMipmapBase *Mip, INT Level) {
	FColor *Ptr = (FColor *)m_texConvertCtx.pCompose;
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
			const FColor &Src = Base[(DWORD)(j & UMask)];
			*(DWORD *)Ptr = *((DWORD *)&Src) * UTGLR_RGBA7_UPLOAD_MUL; // because of 7777
			Ptr++;
		} while ((j += ij_inc) < j_stop);
	} while ((i += ij_inc) < i_stop);

	UTGLR_DEBUG_TEX_CONVERT_COUNT(ConvertBGRA7777_BGRA8888_NoClamp);
}

void UOpenGLRenderDevice::ConvertBGRA7777_RGBA8888(const FMipmapBase *Mip, INT Level) {
	FColor *Ptr = (FColor *)m_texConvertCtx.pCompose;
	INT StepBits = m_texConvertCtx.stepBits;
	//Capped at 31.
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
			const FColor &Src = Base[Min<DWORD>(j & UMask, UClampVal)];
			Ptr->R = (BYTE)(UTGLR_RGBA7_UPLOAD_MUL * Src.B);
			Ptr->G = (BYTE)(UTGLR_RGBA7_UPLOAD_MUL * Src.G);
			Ptr->B = (BYTE)(UTGLR_RGBA7_UPLOAD_MUL * Src.R);
			Ptr->A = (BYTE)(UTGLR_RGBA7_UPLOAD_MUL * Src.A); // because of 7777

			Ptr++;
		} while ((j += ij_inc) < j_stop);
	} while ((i += ij_inc) < i_stop);

	UTGLR_DEBUG_TEX_CONVERT_COUNT(ConvertBGRA7777_RGBA8888);
}
