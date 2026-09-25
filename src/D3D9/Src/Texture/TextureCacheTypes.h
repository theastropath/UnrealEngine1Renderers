//Include after c_rbtree.h.

#pragma once

#include "cachedtexture.h"

class UD3D9RenderDevice;


enum bind_type_t {
	BIND_TYPE_ZERO_PREFIX,
	BIND_TYPE_NON_ZERO_PREFIX,
	BIND_TYPE_NON_ZERO_PREFIX_LRU_LIST
};

#define CT_MIN_FILTER_POINT 0x00
#define CT_MIN_FILTER_LINEAR 0x01
#define CT_MIN_FILTER_ANISOTROPIC 0x02
#define CT_MIN_FILTER_MASK 0x03

#define CT_MIP_FILTER_NONE 0x00
#define CT_MIP_FILTER_POINT 0x04
#define CT_MIP_FILTER_LINEAR 0x08
#define CT_MIP_FILTER_MASK 0x0C

#define CT_MAG_FILTER_LINEAR_NOT_POINT_BIT 0x10

#define CT_HAS_MIPMAPS_BIT 0x20

#define CT_ADDRESS_CLAMP_NOT_WRAP_BIT 0x40

const BYTE CT_DEFAULT_TEX_FILTER_PARAMS = CT_MIN_FILTER_POINT | CT_MIP_FILTER_NONE;

struct tex_params_t {
	BYTE filter;
	BYTE reserved1;
	BYTE reserved2;
	BYTE reserved3;
};

const tex_params_t CT_DEFAULT_TEX_PARAMS = { CT_DEFAULT_TEX_FILTER_PARAMS, 0, 0, 0 };

#define DT_NO_SMOOTH_BIT 0x01

struct FCachedTexture {
	IDirect3DTexture9 *pTexObj;
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
	D3DFORMAT texFormat;
	INT srcUSize, srcVSize;
	INT srcNumMips;
	BYTE srcFormat;
	BYTE srcHasPalette;
	BYTE bContentStale;
	DWORD texBytes;
	union {
		void (FASTCALL UD3D9RenderDevice::*pConvertBGRA7777)(const FMipmapBase *, INT);
	};
	FCachedTexture *pPrev;
	FCachedTexture *pNext;
};

typedef TCachedTextureChain<FCachedTexture> CCachedTextureChain;

struct FTexInfo {
	QWORD CurrentCacheID;
	DWORD CurrentDynamicPolyFlags;
	FCachedTexture *pBind;
	FLOAT UMult;
	FLOAT VMult;
	FLOAT UPan;
	FLOAT VPan;
	FLOAT UOffset;
	FLOAT VOffset;
};
