/*=============================================================================
	TextureMips.cpp: mip levels the engine did not supply.

	Several games ship textures with one level only. Where the format allows it, the
	missing levels are generated here.
=============================================================================*/

#include "../D3D9Drv.h"
#include "../D3D9.h"
#include "TextureHelpers.h"

//Formats the mip filter can process. Block compressed levels are not texel addressable
bool UD3D9RenderDevice::CanDownsampleTexFormat(D3DFORMAT texFormat) {
	switch (texFormat) {
		case D3DFMT_A8R8G8B8:
		case D3DFMT_R5G6B5:
		case D3DFMT_A1R5G5B5:
		case D3DFMT_X1R5G5B5:
			return true;

		default:
			return false;
	}
}

//Whether the levels a single mip source lacks should be filtered down from level 0
bool UD3D9RenderDevice::CanGenerateMipmaps(const FCachedTexture *pBind, const FTextureInfo &Info) {
	if (!GenerateMipMaps) {
		return false;
	}

	if (pBind->MaxLevel < 1) {
		return false;
	}

	//Re-uploaded constantly, already smooth.
	if (pBind->texType == TEX_TYPE_NORMAL) {
		return false;
	}

	//Pays on every change.
	if (TexInfoRealtime(Info)) {
		return false;
	}

	return CanDownsampleTexFormat(pBind->texFormat);
}

INT UD3D9RenderDevice::CalcTexLevelCount(const FCachedTexture *pBind, const FTextureInfo &Info) {
	if ((Info.NumMips != 1) || CanGenerateMipmaps(pBind, Info)) {
		return pBind->MaxLevel + 1;
	}

	return 1;
}

//Approximate, for the budget.
DWORD UD3D9RenderDevice::CalcTexBytes(const FCachedTexture *pBind, INT levelCount) {
	DWORD bytes = 0;
	DWORD width = 1U << pBind->UBits;
	DWORD height = 1U << pBind->VBits;
	INT level;

	for (level = 0; level < levelCount; level++) {
		switch (pBind->texFormat) {
			case D3DFMT_DXT1:
				//Compressed levels are always stored as whole 4x4 blocks
				bytes += ((width + 3) / 4) * ((height + 3) / 4) * 8;
				break;

			case D3DFMT_DXT3:
			case D3DFMT_DXT5:
				bytes += ((width + 3) / 4) * ((height + 3) / 4) * 16;
				break;

			case D3DFMT_R5G6B5:
			case D3DFMT_A1R5G5B5:
			case D3DFMT_X1R5G5B5:
				bytes += width * height * 2;
				break;

			default:
				bytes += width * height * 4;
		}

		//Both are halved down to a floor of one
		width = (width & 0x1) | (width >> 1);
		height = (height & 0x1) | (height >> 1);
	}

	return bytes;
}

//Box filters one locked level into the next, alpha weighted so transparent texels do not bleed
static void FASTCALL DownsampleTexLevel(
	D3DFORMAT texFormat,
	const D3DLOCKED_RECT &srcRect, DWORD srcWidth, DWORD srcHeight,
	const D3DLOCKED_RECT &dstRect, DWORD dstWidth, DWORD dstHeight) {
	//A level only shrinks along an axis that has something left to shrink
	DWORD uStep = (srcWidth > dstWidth) ? 2 : 1;
	DWORD vStep = (srcHeight > dstHeight) ? 2 : 1;
	DWORD u, v, i;

	//Channel layout for the 16 bit formats
	DWORD aMask = 0, rMask, gMask, bMask;
	DWORD aShift = 0, rShift, gShift, bShift;
	switch (texFormat) {
		case D3DFMT_R5G6B5:
			rMask = 0x1F;
			rShift = 11;
			gMask = 0x3F;
			gShift = 5;
			bMask = 0x1F;
			bShift = 0;
			break;

		//Bit 15 is a don't care here, so it must not weight the filter:
		//some paletted converters write the palette's alpha bit into it regardless of destination format.
		case D3DFMT_X1R5G5B5:
			rMask = 0x1F;
			rShift = 10;
			gMask = 0x1F;
			gShift = 5;
			bMask = 0x1F;
			bShift = 0;
			break;

		default:
			aMask = 0x01;
			aShift = 15;
			rMask = 0x1F;
			rShift = 10;
			gMask = 0x1F;
			gShift = 5;
			bMask = 0x1F;
			bShift = 0;
	}

	const BYTE *pSrcBits = (const BYTE *)srcRect.pBits;
	BYTE *pDstBits = (BYTE *)dstRect.pBits;
	const INT srcPitch = srcRect.Pitch;
	const INT dstPitch = dstRect.Pitch;
	const INT srcRow1Offset = (vStep - 1) * srcPitch;

	for (v = 0; v < dstHeight; v++) {
		const BYTE *pSrcRow0 = pSrcBits + ((v * vStep) * srcPitch);
		const BYTE *pSrcRow1 = pSrcRow0 + srcRow1Offset;
		BYTE *pDstRow = pDstBits + (v * dstPitch);

		if (texFormat == D3DFMT_A8R8G8B8) {
			const DWORD *pSrc0 = (const DWORD *)pSrcRow0;
			const DWORD *pSrc1 = (const DWORD *)pSrcRow1;
			DWORD *pDst = (DWORD *)pDstRow;

			for (u = 0; u < dstWidth; u++) {
				DWORD su = u * uStep;
				DWORD texels[4];
				DWORD numSamples = 0;
				DWORD sumA = 0, sumR = 0, sumG = 0, sumB = 0;
				DWORD wSumR = 0, wSumG = 0, wSumB = 0;

				texels[numSamples++] = pSrc0[su];
				if (uStep == 2) {
					texels[numSamples++] = pSrc0[su + 1];
				}
				if (vStep == 2) {
					texels[numSamples++] = pSrc1[su];
					if (uStep == 2) {
						texels[numSamples++] = pSrc1[su + 1];
					}
				}

				for (i = 0; i < numSamples; i++) {
					DWORD texel = texels[i];
					DWORD a = (texel >> 24) & 0xFF;
					DWORD r = (texel >> 16) & 0xFF;
					DWORD g = (texel >> 8) & 0xFF;
					DWORD b = texel & 0xFF;

					sumA += a;
					sumR += r;
					sumG += g;
					sumB += b;
					wSumR += r * a;
					wSumG += g * a;
					wSumB += b * a;
				}

				DWORD outA = sumA / numSamples;
				DWORD outR, outG, outB;
				//A fully transparent block has no weights to divide by
				if (sumA != 0) {
					outR = wSumR / sumA;
					outG = wSumG / sumA;
					outB = wSumB / sumA;
				} else {
					outR = sumR / numSamples;
					outG = sumG / numSamples;
					outB = sumB / numSamples;
				}

				pDst[u] = (outA << 24) | (outR << 16) | (outG << 8) | outB;
			}
		} else {
			const WORD *pSrc0 = (const WORD *)pSrcRow0;
			const WORD *pSrc1 = (const WORD *)pSrcRow1;
			WORD *pDst = (WORD *)pDstRow;

			for (u = 0; u < dstWidth; u++) {
				DWORD su = u * uStep;
				WORD texels[4];
				DWORD numSamples = 0;
				DWORD sumA = 0, sumR = 0, sumG = 0, sumB = 0;
				DWORD wSumR = 0, wSumG = 0, wSumB = 0;

				texels[numSamples++] = pSrc0[su];
				if (uStep == 2) {
					texels[numSamples++] = pSrc0[su + 1];
				}
				if (vStep == 2) {
					texels[numSamples++] = pSrc1[su];
					if (uStep == 2) {
						texels[numSamples++] = pSrc1[su + 1];
					}
				}

				for (i = 0; i < numSamples; i++) {
					DWORD texel = texels[i];
					//Weight by the one bit alpha if there is one. Otherwise treat all as opaque.
					DWORD a = (aMask == 0) ? 1 : ((texel >> aShift) & aMask);
					DWORD r = (texel >> rShift) & rMask;
					DWORD g = (texel >> gShift) & gMask;
					DWORD b = (texel >> bShift) & bMask;

					sumA += a;
					sumR += r;
					sumG += g;
					sumB += b;
					wSumR += r * a;
					wSumG += g * a;
					wSumB += b * a;
				}

				//Half opaque stays opaque.
				DWORD outA = ((sumA * 2) + numSamples) / (numSamples * 2);
				DWORD outR, outG, outB;
				if (sumA != 0) {
					outR = wSumR / sumA;
					outG = wSumG / sumA;
					outB = wSumB / sumA;
				} else {
					outR = sumR / numSamples;
					outG = sumG / numSamples;
					outB = sumB / numSamples;
				}

				pDst[u] = (WORD)(((outA & aMask) << aShift) | (outR << rShift) | (outG << gShift) | (outB << bShift));
			}
		}
	}
}

//Fills every level past level 0 of a texture whose single source mip is already uploaded.
//Returns false if a lock failure stopped the walk short.
bool UD3D9RenderDevice::GenerateMipmapLevels(FCachedTexture *pBind) {
	DWORD width = 1U << pBind->UBits;
	DWORD height = 1U << pBind->VBits;
	INT levelCount = (INT)pBind->pTexObj->GetLevelCount();
	INT Level;

	for (Level = 1; Level < levelCount; Level++) {
		D3DLOCKED_RECT srcRect;
		D3DLOCKED_RECT dstRect;
		DWORD srcWidth = width;
		DWORD srcHeight = height;

		//Both are halved down to a floor of one
		width = (width & 0x1) | (width >> 1);
		height = (height & 0x1) | (height >> 1);

		//READONLY also keeps the source level from being marked dirty
		if (FAILED(pBind->pTexObj->LockRect(Level - 1, &srcRect, NULL, D3DLOCK_NOSYSLOCK | D3DLOCK_READONLY))) {
			break;
		}
		if (FAILED(pBind->pTexObj->LockRect(Level, &dstRect, NULL, D3DLOCK_NOSYSLOCK))) {
			pBind->pTexObj->UnlockRect(Level - 1);
			break;
		}

		DownsampleTexLevel(pBind->texFormat, srcRect, srcWidth, srcHeight, dstRect, width, height);

		pBind->pTexObj->UnlockRect(Level);
		pBind->pTexObj->UnlockRect(Level - 1);
	}

	//A lock failure leaves this level and every smaller one holding whatever the allocation held.
	//Clearing the mipmap bit stops later binds sampling them.
	if (Level < levelCount) {
		pBind->texParams.filter &= ~(CT_HAS_MIPMAPS_BIT | CT_MIP_FILTER_MASK);
		return false;
	}

	return true;
}
