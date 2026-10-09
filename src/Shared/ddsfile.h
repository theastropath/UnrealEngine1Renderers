/*=============================================================================
	ddsfile.h: a DDS container, described as mip levels ready for upload.

	No engine and no renderer dependency, so the same parse runs in all three
	renderers and can be exercised on the host without a game SDK. Reading the
	file is left to the caller: the three have different path and I/O
	conventions and nothing here needs to know about them.

	Every field read from the file is treated as hostile. A texture pack is
	content a player downloads, so a header claiming more data than the file
	holds has to be rejected rather than trusted into a read past the end.
=============================================================================*/

#ifndef UTGLR_DDSFILE_H
#define UTGLR_DDSFILE_H

#include <stddef.h>


/*
A full chain from the largest dimension accepted below is fourteen levels,
8192 down to 1, so a file declaring more than that is malformed whatever it
says in its header.
*/
#define DDS_MAX_MIP_LEVELS 14

//Past this a file is refused rather than merely unusual.
#define DDS_MAX_DIMENSION 8192

//Magic, then the 124 byte header.
#define DDS_HEADER_BYTES 128


enum EDDSFormat {
	DDS_FORMAT_NONE = 0,
	DDS_FORMAT_DXT1,
	DDS_FORMAT_DXT3,
	DDS_FORMAT_DXT5,
	//Eight bits a channel, stored BGRA, which is what both D3D and GL want back.
	DDS_FORMAT_BGRA8
};

struct FDDSLevel {
	//Into the caller's buffer. Valid only while that buffer lives.
	const unsigned char *pData;
	unsigned int Bytes;
	unsigned int USize;
	unsigned int VSize;
};

struct FDDSImage {
	EDDSFormat Format;
	unsigned int USize;
	unsigned int VSize;
	unsigned int NumMips;
	FDDSLevel Levels[DDS_MAX_MIP_LEVELS];
};


/*
Read a little endian DWORD a byte at a time: the header is a file format rather
than a struct this compiler laid out, so neither its alignment nor the host's
byte order can be assumed.
*/
static inline unsigned int DDSReadDword(const unsigned char *pSrc) {
	return (unsigned int)pSrc[0] |
		((unsigned int)pSrc[1] << 8) |
		((unsigned int)pSrc[2] << 16) |
		((unsigned int)pSrc[3] << 24);
}

static inline bool DDSIsPowerOfTwo(unsigned int value) {
	return (value != 0) && ((value & (value - 1)) == 0);
}

/** Bytes one mip level of this format occupies. Never zero for a level that exists. */
static inline unsigned int DDSLevelBytes(EDDSFormat format, unsigned int uSize, unsigned int vSize) {
	if (format == DDS_FORMAT_BGRA8) {
		//Bounded by DDS_MAX_DIMENSION, so the product cannot wrap.
		return uSize * vSize * 4;
	}

	const unsigned int blockBytes = (format == DDS_FORMAT_DXT1) ? 8 : 16;
	const unsigned int uBlocks = (uSize + 3) >> 2;
	const unsigned int vBlocks = (vSize + 3) >> 2;
	return uBlocks * vBlocks * blockBytes;
}

/**
Describe a DDS held in memory.

\param pFile The whole file.
\param fileBytes Its length.
\param out Filled in only on success; untouched otherwise.
\return false for anything this cannot upload as it stands, which includes a
	file whose declared levels do not fit inside it.
\note Nothing is copied. The levels point into pFile, which has to outlive them.
*/
static inline bool DDSParse(const unsigned char *pFile, size_t fileBytes, FDDSImage &out) {
	if ((pFile == NULL) || (fileBytes < DDS_HEADER_BYTES)) {
		return false;
	}

	//'DDS ', little endian.
	if (DDSReadDword(pFile + 0) != 0x20534444) {
		return false;
	}
	//Both sizes are fixed by the format and are the cheapest way to catch a file that is not one.
	if (DDSReadDword(pFile + 4) != 124) {
		return false;
	}
	if (DDSReadDword(pFile + 76) != 32) {
		return false;
	}

	const unsigned int headerFlags = DDSReadDword(pFile + 8);
	const unsigned int vSize = DDSReadDword(pFile + 12);
	const unsigned int uSize = DDSReadDword(pFile + 16);
	const unsigned int declaredMips = DDSReadDword(pFile + 28);
	const unsigned int formatFlags = DDSReadDword(pFile + 80);
	const unsigned int fourCC = DDSReadDword(pFile + 84);
	const unsigned int bitCount = DDSReadDword(pFile + 88);
	const unsigned int maskR = DDSReadDword(pFile + 92);
	const unsigned int maskG = DDSReadDword(pFile + 96);
	const unsigned int maskB = DDSReadDword(pFile + 100);
	const unsigned int maskA = DDSReadDword(pFile + 104);
	const unsigned int caps2 = DDSReadDword(pFile + 112);

	/*
	Power of two because the OpenGL renderer targets 1.x, where a non power of two
	texture needs an extension that is not required anywhere else in that tree, and
	a pack that loads on one renderer and not another is worse than one that is
	refused everywhere.
	*/
	if (!DDSIsPowerOfTwo(uSize) || !DDSIsPowerOfTwo(vSize)) {
		return false;
	}
	if ((uSize > DDS_MAX_DIMENSION) || (vSize > DDS_MAX_DIMENSION)) {
		return false;
	}

	//Cube maps and volumes have more surfaces than the one walked below.
	const unsigned int DDSCAPS2_CUBEMAP = 0x00000200;
	const unsigned int DDSCAPS2_VOLUME = 0x00200000;
	if ((caps2 & (DDSCAPS2_CUBEMAP | DDSCAPS2_VOLUME)) != 0) {
		return false;
	}

	const unsigned int DDPF_ALPHAPIXELS = 0x00000001;
	const unsigned int DDPF_FOURCC = 0x00000004;
	const unsigned int DDPF_RGB = 0x00000040;

	EDDSFormat format = DDS_FORMAT_NONE;
	if ((formatFlags & DDPF_FOURCC) != 0) {
		switch (fourCC) {
			case 0x31545844: //'DXT1'
				format = DDS_FORMAT_DXT1;
				break;
			case 0x33545844: //'DXT3'
				format = DDS_FORMAT_DXT3;
				break;
			case 0x35545844: //'DXT5'
				format = DDS_FORMAT_DXT5;
				break;
			default:
				//'DX10' and the rest carry a second header this does not read.
				return false;
		}
	} else if ((formatFlags & DDPF_RGB) != 0) {
		/*
		Only the one uncompressed layout, the eight bit BGRA every exporter writes
		as A8R8G8B8. Accepting more would mean a swizzle per variant in three
		renderers for a case a pack has no reason to be in.
		*/
		if ((bitCount != 32) ||
			(maskR != 0x00FF0000) || (maskG != 0x0000FF00) || (maskB != 0x000000FF)) {
			return false;
		}
		//An opaque file still uploads: the alpha channel simply reads as whatever is stored.
		if (((formatFlags & DDPF_ALPHAPIXELS) != 0) && (maskA != 0xFF000000)) {
			return false;
		}
		format = DDS_FORMAT_BGRA8;
	} else {
		return false;
	}

	const unsigned int DDSD_MIPMAPCOUNT = 0x00020000;
	unsigned int numMips = 1;
	if (((headerFlags & DDSD_MIPMAPCOUNT) != 0) && (declaredMips > 1)) {
		numMips = declaredMips;
	}
	if (numMips > DDS_MAX_MIP_LEVELS) {
		return false;
	}

	//Walked before anything is written out, so a refusal leaves the caller's image alone.
	FDDSImage parsed;
	parsed.Format = format;
	parsed.USize = uSize;
	parsed.VSize = vSize;
	parsed.NumMips = numMips;

	size_t offset = DDS_HEADER_BYTES;
	unsigned int levelU = uSize;
	unsigned int levelV = vSize;
	for (unsigned int level = 0; level < numMips; level++) {
		const unsigned int levelBytes = DDSLevelBytes(format, levelU, levelV);

		//Subtracted rather than added, so a large level cannot wrap past the end and look small.
		if (levelBytes > (fileBytes - offset)) {
			return false;
		}

		parsed.Levels[level].pData = pFile + offset;
		parsed.Levels[level].Bytes = levelBytes;
		parsed.Levels[level].USize = levelU;
		parsed.Levels[level].VSize = levelV;

		offset += levelBytes;

		//A chain that reaches 1x1 before its declared count is short, not merely unusual.
		if ((level + 1) < numMips) {
			if ((levelU == 1) && (levelV == 1)) {
				return false;
			}
			levelU = (levelU > 1) ? (levelU >> 1) : 1;
			levelV = (levelV > 1) ? (levelV >> 1) : 1;
		}
	}

	for (unsigned int level = numMips; level < DDS_MAX_MIP_LEVELS; level++) {
		parsed.Levels[level].pData = NULL;
		parsed.Levels[level].Bytes = 0;
		parsed.Levels[level].USize = 0;
		parsed.Levels[level].VSize = 0;
	}

	out = parsed;
	return true;
}

#endif //UTGLR_DDSFILE_H
