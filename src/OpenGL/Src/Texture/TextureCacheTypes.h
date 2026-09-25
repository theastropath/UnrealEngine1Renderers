#pragma once

#include "cachedtexture.h"

class UOpenGLRenderDevice;


enum bind_type_t {
	BIND_TYPE_ZERO_PREFIX,
	BIND_TYPE_NON_ZERO_PREFIX,
	BIND_TYPE_NON_ZERO_PREFIX_LRU_LIST
};

#define CT_MIN_FILTER_NEAREST 0x00
#define CT_MIN_FILTER_LINEAR 0x01
#define CT_MIN_FILTER_NEAREST_MIPMAP_NEAREST 0x02
#define CT_MIN_FILTER_LINEAR_MIPMAP_NEAREST 0x03
#define CT_MIN_FILTER_NEAREST_MIPMAP_LINEAR 0x04
#define CT_MIN_FILTER_LINEAR_MIPMAP_LINEAR 0x05
#define CT_MIN_FILTER_MASK 0x07

#define CT_MAG_FILTER_NEAREST_OR_LINEAR_BIT 0x08

#define CT_ANISOTROPIC_FILTER_BIT 0x10

#define CT_ADDRESS_U_CLAMP 0x20
#define CT_ADDRESS_V_CLAMP 0x40

struct tex_params_t {
	BYTE filter;
	bool hasMipmaps;
	BYTE texObjFilter;
	BYTE reserved3;
};

const tex_params_t CT_DEFAULT_TEX_PARAMS = { CT_MIN_FILTER_NEAREST_MIPMAP_LINEAR | CT_MAG_FILTER_NEAREST_OR_LINEAR_BIT, true, 0, 0 };

#define DT_NO_SMOOTH_BIT 0x01

struct FCachedTexture {
	GLuint Id;
	DWORD LastUsedFrameCount;
	BYTE BaseMip;
	BYTE MaxLevel;
	BYTE UBits, VBits;
	DWORD UClampVal, VClampVal;
	FLOAT UMult, VMult;
	BYTE texType;
	BYTE bindType;
	BYTE treeIndex;
	BYTE dynamicTexBits;
	tex_params_t texParams;
	GLenum texSourceFormat;
	GLenum texInternalFormat;
	INT srcUSize, srcVSize;
	INT srcNumMips;
	BYTE srcFormat;
	BYTE srcHasPalette;
	DWORD texBytes;
	union {
		void (FASTCALL UOpenGLRenderDevice::*pConvertBGRA7777)(const FMipmapBase *, INT);
	};
	FCachedTexture *pPrev;
	FCachedTexture *pNext;
};

//Both this and TexInfoMatchesBind live in Shared/cachedtexture.h.
typedef TCachedTextureChain<FCachedTexture> CCachedTextureChain;

struct FTexInfo {
	QWORD CurrentCacheID;
	DWORD CurrentDynamicPolyFlags;
	FCachedTexture *pBind;
	FLOAT UMult;
	FLOAT VMult;
	FLOAT UPan;
	FLOAT VPan;
};
