#ifndef UTGLR_DXTDECODE_H
#define UTGLR_DXTDECODE_H


/** One decoded texel, alpha resolved for the block type. */
struct dxt_texel_t {
	unsigned int r;
	unsigned int g;
	unsigned int b;
	unsigned int a;
};

//dxtType is 1, 3 or 5 for BC1/BC2/BC3; tx and ty are 0 to 3.
inline dxt_texel_t DecodeDXTTexel(const unsigned char *pBlock, unsigned int dxtType, unsigned int tx, unsigned int ty) {
	//DXT3/DXT5 put an 8 byte alpha block before the color block
	const unsigned char *pColorBlock = (dxtType == 1) ? pBlock : (pBlock + 8);

	unsigned int c0 = (unsigned int)pColorBlock[0] | ((unsigned int)pColorBlock[1] << 8);
	unsigned int c1 = (unsigned int)pColorBlock[2] | ((unsigned int)pColorBlock[3] << 8);
	unsigned int colorBits = (unsigned int)pColorBlock[4] | ((unsigned int)pColorBlock[5] << 8) |
		((unsigned int)pColorBlock[6] << 16) | ((unsigned int)pColorBlock[7] << 24);
	unsigned int texelIndex = (ty * 4) + tx;
	unsigned int sel = (colorBits >> (2 * texelIndex)) & 3;

	//Endpoints are RGB565
	unsigned int r0 = (c0 >> 11) & 0x1F, g0 = (c0 >> 5) & 0x3F, b0 = c0 & 0x1F;
	unsigned int r1 = (c1 >> 11) & 0x1F, g1 = (c1 >> 5) & 0x3F, b1 = c1 & 0x1F;
	r0 = (r0 << 3) | (r0 >> 2);
	g0 = (g0 << 2) | (g0 >> 4);
	b0 = (b0 << 3) | (b0 >> 2);
	r1 = (r1 << 3) | (r1 >> 2);
	g1 = (g1 << 2) | (g1 >> 4);
	b1 = (b1 << 3) | (b1 >> 2);

	unsigned int r, g, b;
	unsigned int a = 255;
	if (sel == 0) {
		r = r0;
		g = g0;
		b = b0;
	} else if (sel == 1) {
		r = r1;
		g = g1;
		b = b1;
	}
	//DXT1 only: three colors plus transparent, when c0 <= c1
	else if ((dxtType != 1) || (c0 > c1)) {
		if (sel == 2) {
			r = ((2 * r0) + r1) / 3;
			g = ((2 * g0) + g1) / 3;
			b = ((2 * b0) + b1) / 3;
		} else {
			r = (r0 + (2 * r1)) / 3;
			g = (g0 + (2 * g1)) / 3;
			b = (b0 + (2 * b1)) / 3;
		}
	} else {
		if (sel == 2) {
			r = (r0 + r1) / 2;
			g = (g0 + g1) / 2;
			b = (b0 + b1) / 2;
		} else {
			r = 0;
			g = 0;
			b = 0;
			a = 0;
		}
	}

	if (dxtType == 3) {
		unsigned char alphaByte = pBlock[texelIndex >> 1];
		a = (texelIndex & 1) ? (unsigned int)(alphaByte >> 4) : (unsigned int)(alphaByte & 0xF);
		a = (a << 4) | a;
	} else if (dxtType == 5) {
		unsigned int a0 = pBlock[0];
		unsigned int a1 = pBlock[1];
		unsigned int bitPos = texelIndex * 3;
		unsigned int byteIdx = 2 + (bitPos >> 3);
		unsigned int alphaBits = (unsigned int)pBlock[byteIdx] | ((unsigned int)pBlock[byteIdx + 1] << 8);
		unsigned int aSel = (alphaBits >> (bitPos & 7)) & 7;

		if (aSel == 0) {
			a = a0;
		} else if (aSel == 1) {
			a = a1;
		} else if (a0 > a1) {
			a = (((8 - aSel) * a0) + ((aSel - 1) * a1)) / 7;
		} else if (aSel == 6) {
			a = 0;
		} else if (aSel == 7) {
			a = 255;
		} else {
			a = (((6 - aSel) * a0) + ((aSel - 1) * a1)) / 5;
		}
	}

	dxt_texel_t out;
	out.r = r;
	out.g = g;
	out.b = b;
	out.a = a;
	return out;
}

#endif
