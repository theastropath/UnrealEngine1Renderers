/*=============================================================================
	TextureHelpers.h: small questions more than one of the texture files asks.

	Include after D3D9.h.
=============================================================================*/

#pragma once

//The bind's dimensions can be inflated past the source mip's real size when non-zero copy bits are in play,
//so the source bounds them here as well.
static inline void FASTCALL BoundBlocksToMip(const FMipmapBase *Mip, DWORD &UBlocks, DWORD &VBlocks) {
	const DWORD srcUBlocks = (DWORD)Max(0, Mip->USize + 3) >> 2;
	const DWORD srcVBlocks = (DWORD)Max(0, Mip->VSize + 3) >> 2;
	if (UBlocks > srcUBlocks) {
		UBlocks = srcUBlocks;
	}
	if (VBlocks > srcVBlocks) {
		VBlocks = srcVBlocks;
	}
}

/*
Whether any block in a BC1 texture uses the three colour decode mode,
which BC2/BC3 cannot express: relabelled as BC2 without correction,
indices 2 and 3 land on the wrong interpolated colours.
*/
static inline bool FASTCALL DXT1HasThreeColorBlocks(const FTextureInfo &Info, INT BaseMip, INT MaxLevel, INT UBits, INT VBits) {
	for (INT Level = 0; Level <= MaxLevel; Level++) {
		const FMipmapBase *pMip = Info.Mips[BaseMip + Level];
		if (pMip == NULL) {
			return true; //Unreadable, so not a candidate for the relabel either
		}
		const WORD *pSrc = (const WORD *)pMip->DataPtr;
		if (pSrc == NULL) {
			return true;
		}

		DWORD uBlocks = 1U << Max(0, UBits - Level - 2);
		DWORD vBlocks = 1U << Max(0, VBits - Level - 2);
		BoundBlocksToMip(pMip, uBlocks, vBlocks);
		const DWORD numBlocks = uBlocks * vBlocks;
		for (DWORD b = 0; b < numBlocks; b++) {
			//Eight bytes a block.
			if (pSrc[0] <= pSrc[1]) {
				return true;
			}
			pSrc += 4;
		}
	}

	return false;
}
