/*=============================================================================
	D3D9.h: Unreal Direct3D 9 support header.
	Portions copyright 1999 Epic Games, Inc. All Rights Reserved.

	UD3D9RenderDevice and nothing else, with the build configuration, compiler
	feature switches, debug switches, fixed sizes, texture cache types, lightmap
	atlas and vertex layouts all living in the headers included below, each one in
	the directory that owns the code using it, so that the class is still a single
	declaration whose length is mostly inline bodies on per-triangle paths, several
	of which exist twice with different signatures depending on whether the
	assembly version is compiled.
=============================================================================*/

#pragma once

#include "buildconfig.h"


#include <math.h>
#include <stdio.h>


//Core's clock(Timer) macro collides with <ctime>'s clock().
#pragma push_macro("clock")
#undef clock
#pragma warning(push)
#pragma warning(disable : 4663) //obsolete VC6 template syntax
#pragma warning(disable : 4018) //signed/unsigned mismatch
#pragma warning(disable : 4244) //conversion, possible loss of data
#pragma warning(disable : 4245) //signed/unsigned conversion
#pragma warning(disable : 4146) //unary minus on unsigned

#include <map>
#include <vector>
#include <sstream>
#include <iostream>
#include <iomanip>

#pragma warning(pop)
#pragma pop_macro("clock")

#include "c_gclip.h"


typedef IDirect3D9 *(WINAPI *LPDIRECT3DCREATE9)(UINT SDKVersion);

#include "Core/Compiler.h"
#include "Core/Debug.h"


#include "c_rbtree.h"

#include "jobsystem.h"
#include "framearena.h"

#include "Core/Limits.h"


/*-----------------------------------------------------------------------------
	D3D9Drv.
-----------------------------------------------------------------------------*/

#include "Texture/TextureCacheTypes.h"
#include "Texture/LightmapAtlas.h"
#include "Geometry/VertexFormats.h"
#include "Core/ScopedBoolFlag.h"


#include "Geometry/Deferred.h"


class UD3D9RenderDevice : public URenderDevice {
#if defined UTGLR_DX_BUILD || defined UTGLR_OLD_URENDERDEVICE
	DECLARE_CLASS(UD3D9RenderDevice, URenderDevice, CLASS_Config)
#else
	DECLARE_CLASS(UD3D9RenderDevice, URenderDevice, CLASS_Config, D3D9Drv)
#endif

	class ods_buf : public std::basic_stringbuf<TCHAR, std::char_traits<TCHAR>> {
	public:
		virtual ~ods_buf() {
			sync();
		}

	protected:
		int sync() {
#ifdef WIN32
			TCHAR_CALL_OS(OutputDebugStringW(str().c_str()), OutputDebugStringA(appToAnsi(str().c_str())));
#else
			//Non-win32 debug output here.
#endif

			str(std::basic_string<TCHAR>());

			return 0;
		}
	};
	class ods_stream : public std::basic_ostream<TCHAR, std::char_traits<TCHAR>> {
	public:
		ods_stream() :
			std::basic_ostream<TCHAR, std::char_traits<TCHAR>>(new ods_buf()) {
		}
		~ods_stream() {
			delete rdbuf();
		}
	};

	std::basic_string<TCHAR> HexString(DWORD data, DWORD numBits = 4) {
		std::basic_ostringstream<TCHAR> strHexNum;

		strHexNum << std::hex;
		strHexNum.fill('0');
		strHexNum << std::uppercase;
		strHexNum << std::setw(((numBits + 3) & -4) / 4);
		strHexNum << data;

		return strHexNum.str();
	}

	//TODO: the disabled debug blocks elsewhere write wide string literals, wrong on the Klingon
	//build where TCHAR is char, and some touch fields that SDK lacks. Fix both before enabling any.
	ods_stream dout;

	DWORD m_debugBits;
	inline bool FASTCALL DebugBit(DWORD debugBit) {
		return ((m_debugBits & debugBit) != 0);
	}
	enum {
		DEBUG_BIT_BASIC = 0x00000001,
		DEBUG_BIT_GL_ERROR = 0x00000002,
		DEBUG_BIT_TILES = 0x00000004,
		DEBUG_BIT_ANY = 0xFFFFFFFF
	};

	enum { MAX_LOGGED_TILES = 64 };
	QWORD m_loggedTileIds[MAX_LOGGED_TILES];
	DWORD m_numLoggedTiles;
	void FASTCALL LogTileOnce(const FTextureInfo &Info, DWORD polyFlags, DWORD blendPolyFlags);

#define TEX_CACHE_ID_UNUSED 0xFFFFFFFFFFFFFFFFULL
#define TEX_CACHE_ID_NO_TEX 0xFFFFFFFF00000010ULL
#define TEX_CACHE_ID_ALPHA_TEX 0xFFFFFFFF00000020ULL
//Cache id namespace for lightmap atlas pages.
//0x30 upward is unused by real textures.
#define TEX_CACHE_ID_ATLAS_PAGE 0xFFFFFFFF00000030ULL

	enum {
		TEX_CACHE_ID_FLAG_MASKED = 0x1,
		TEX_CACHE_ID_FLAG_16BIT = 0x2
	};

//Affects texture object state.
#define TEX_DYNAMIC_POLY_FLAGS_MASK (PF_NoSmooth)

	enum tex_type_t {
		TEX_TYPE_NONE,
		TEX_TYPE_COMPRESSED_DXT1,
		TEX_TYPE_COMPRESSED_DXT1_TO_DXT3,
		TEX_TYPE_COMPRESSED_DXT3,
		TEX_TYPE_COMPRESSED_DXT5,
		//Decoded during upload.
		TEX_TYPE_DECOMPRESSED_DXT1,
		TEX_TYPE_DECOMPRESSED_DXT3,
		TEX_TYPE_DECOMPRESSED_DXT5,
		TEX_TYPE_HAS_PALETTE,
		TEX_TYPE_NORMAL
	};
#define TEX_FLAG_NO_CLAMP 0x00000001

	struct FTexConvertCtx {
		INT stepBits;
		DWORD texWidthPow2;
		DWORD texHeightPow2;
		const FCachedTexture *pBind;
		D3DLOCKED_RECT lockRect;
	} m_texConvertCtx;


	//Truncates on 64 bit.
	inline void *FASTCALL AlignMemPtr(void *ptr, size_t align) {
		return (void *)(((uintptr_t)ptr + (align - 1)) & ~(uintptr_t)(align - 1));
	}
	enum { VERTEX_ARRAY_ALIGN = 64 }; //Must be even multiple of 16B for SSE
	enum { VERTEX_ARRAY_TAIL_PADDING = 72 };

	IDirect3DVertexDeclaration9 *m_oneColorVertexDecl;
	IDirect3DVertexDeclaration9 *m_standardNTextureVertexDecl[MAX_TMUNITS];
	IDirect3DVertexDeclaration9 *m_twoColorSingleTextureVertexDecl;

	IDirect3DVertexDeclaration9 *m_curVertexDecl;
	IDirect3DVertexShader9 *m_curVertexShader;
	IDirect3DPixelShader9 *m_curPixelShader;

	FGLVertex m_csVertexArray[VERTEX_ARRAY_SIZE];
	IDirect3DVertexBuffer9 *m_d3dVertexColorBuffer;
	FGLVertexColor *m_pVertexColorArray;

	IDirect3DVertexBuffer9 *m_d3dSecondaryColorBuffer;
	FGLSecondaryColor *m_pSecondaryColorArray;

	IDirect3DVertexBuffer9 *m_d3dTexCoordBuffer[MAX_TMUNITS];
	FGLTexCoord *m_pTexCoordArray[MAX_TMUNITS];

	IDirect3DIndexBuffer9 *m_d3dIndexBuffer;
	INT m_curIndexBufferPos;
	bool m_indexBufferNeedsDiscard;
	WORD *m_pIndexArray;
	INT m_csIndexCount;
	INT m_csIndexBufferPos;

	FGLMapDot *MapDotArray;
	BYTE m_MapDotArrayMem[(sizeof(FGLMapDot) * VERTEX_ARRAY_SIZE) + VERTEX_ARRAY_ALIGN + VERTEX_ARRAY_TAIL_PADDING];

	INT m_curVertexBufferPos;
	bool m_vertexColorBufferNeedsDiscard;
	bool m_secondaryColorBufferNeedsDiscard;
	bool m_texCoordBufferNeedsDiscard[MAX_TMUNITS];

	//All streams are marked for discard because they share one write position.
	inline void FlushVertexBuffers(void) {
		unsigned int u;
		m_curVertexBufferPos = 0;
		m_vertexColorBufferNeedsDiscard = true;
		m_secondaryColorBufferNeedsDiscard = true;
		for (u = 0; u < MAX_TMUNITS; u++) {
			m_texCoordBufferNeedsDiscard[u] = true;
		}

		m_vbFlushCount++;
	}

	inline void FlushIndexBuffer(void) {
		m_curIndexBufferPos = 0;
		m_indexBufferNeedsDiscard = true;
	}

	inline void LockVertexColorBuffer(void) {
		BYTE *pData;
		DWORD lockFlags;

		if (m_frameSkipped) {
			m_pVertexColorArray = (FGLVertexColor *)m_skippedFrameScratch;
			return;
		}

		lockFlags = D3DLOCK_NOSYSLOCK;
		if (m_vertexColorBufferNeedsDiscard) {
			m_vertexColorBufferNeedsDiscard = false;
			lockFlags |= D3DLOCK_DISCARD;
		} else {
			lockFlags |= D3DLOCK_NOOVERWRITE;
		}
		if (FAILED(m_d3dVertexColorBuffer->Lock(0, 0, (VOID **)&pData, lockFlags))) {
			appErrorf(TEXT("Vertex buffer lock failed"));
		}

		m_pVertexColorArray = (FGLVertexColor *)(pData + (m_curVertexBufferPos * sizeof(FGLVertexColor)));
	}
	inline void UnlockVertexColorBuffer(void) {
		if (m_frameSkipped) {
			return;
		}
		if (FAILED(m_d3dVertexColorBuffer->Unlock())) {
			appErrorf(TEXT("Vertex buffer unlock failed"));
		}
	}

	inline void LockSecondaryColorBuffer(void) {
		BYTE *pData;
		DWORD lockFlags;

		if (m_frameSkipped) {
			m_pSecondaryColorArray = (FGLSecondaryColor *)m_skippedFrameScratch;
			return;
		}

		lockFlags = D3DLOCK_NOSYSLOCK;
		if (m_secondaryColorBufferNeedsDiscard) {
			m_secondaryColorBufferNeedsDiscard = false;
			lockFlags |= D3DLOCK_DISCARD;
		} else {
			lockFlags |= D3DLOCK_NOOVERWRITE;
		}
		if (FAILED(m_d3dSecondaryColorBuffer->Lock(0, 0, (VOID **)&pData, lockFlags))) {
			appErrorf(TEXT("Vertex buffer lock failed"));
		}

		m_pSecondaryColorArray = (FGLSecondaryColor *)(pData + (m_curVertexBufferPos * sizeof(FGLSecondaryColor)));
	}
	inline void UnlockSecondaryColorBuffer(void) {
		if (m_frameSkipped) {
			return;
		}
		if (FAILED(m_d3dSecondaryColorBuffer->Unlock())) {
			appErrorf(TEXT("Vertex buffer unlock failed"));
		}
	}

	inline void FASTCALL LockTexCoordBuffer(DWORD texUnit) {
		BYTE *pData;
		DWORD lockFlags;

		if (m_frameSkipped) {
			m_pTexCoordArray[texUnit] = (FGLTexCoord *)m_skippedFrameScratch;
			return;
		}

		lockFlags = D3DLOCK_NOSYSLOCK;
		if (m_texCoordBufferNeedsDiscard[texUnit]) {
			m_texCoordBufferNeedsDiscard[texUnit] = false;
			lockFlags |= D3DLOCK_DISCARD;
		} else {
			lockFlags |= D3DLOCK_NOOVERWRITE;
		}
		if (FAILED(m_d3dTexCoordBuffer[texUnit]->Lock(0, 0, (VOID **)&pData, lockFlags))) {
			appErrorf(TEXT("Vertex buffer lock failed"));
		}

		m_pTexCoordArray[texUnit] = (FGLTexCoord *)(pData + (m_curVertexBufferPos * sizeof(FGLTexCoord)));
	}
	inline void FASTCALL UnlockTexCoordBuffer(DWORD texUnit) {
		if (m_frameSkipped) {
			return;
		}
		if (FAILED(m_d3dTexCoordBuffer[texUnit]->Unlock())) {
			appErrorf(TEXT("Vertex buffer unlock failed"));
		}
	}

	inline void LockIndexBuffer(void) {
		BYTE *pData;
		DWORD lockFlags;

		lockFlags = D3DLOCK_NOSYSLOCK;
		if (m_indexBufferNeedsDiscard) {
			m_indexBufferNeedsDiscard = false;
			lockFlags |= D3DLOCK_DISCARD;
		} else {
			lockFlags |= D3DLOCK_NOOVERWRITE;
		}
		//Whole buffer lock only.
		if (FAILED(m_d3dIndexBuffer->Lock(0, 0, (VOID **)&pData, lockFlags))) {
			appErrorf(TEXT("Index buffer lock failed"));
		}

		m_pIndexArray = (WORD *)pData + m_curIndexBufferPos;
	}
	inline void UnlockIndexBuffer(void) {
		if (FAILED(m_d3dIndexBuffer->Unlock())) {
			appErrorf(TEXT("Index buffer unlock failed"));
		}
	}

	DWORD DetailTextureIsNearArray[VERTEX_ARRAY_SIZE / 3];

	INT MultiDrawFirstArray[VERTEX_ARRAY_SIZE / 3];
	INT MultiDrawCountArray[VERTEX_ARRAY_SIZE / 3];

	DWORD m_csPolyCount;
	INT m_csPtCount;

	FLOAT m_csUDot;
	FLOAT m_csVDot;


	struct FGLRenderPass {
		struct FGLSinglePass {
			FTextureInfo *Info;
			DWORD PolyFlags;
			FLOAT PanBias;
			//NULL when not atlased.
			const FLightmapAtlas::FPlacement *pPlacement;
			//The TEXF_RGBA7 layers that ReduceBanding reconstructs with a cubic filter, tracked
			//as a flag of their own because poly flags alone cannot pick them out with a macro
			//texture being PF_Modulated too, and because the reconstruction samples mip zero and
			//would alias on a minified macro texture, the one case it is wrong for, so the flag is
			//decided where the layer is staged and carried from there into both the shader choice
			//and the deferred run key.
			bool bSevenBitMap;
		} TMU[MAX_TMUNITS];
	} MultiPass; // vogel: MULTIPASS!!! ;)

	FLightmapAtlas m_lightmapAtlas;

	bool m_csBatchOpen;
	//Legal only because every lock but the first uses D3DLOCK_NOOVERWRITE.
	//Only while nothing draws from these buffers.
	bool m_csBatchLocked;
	DWORD m_csBatchLockedLayers;
	INT m_csBatchLockVertexPos;
	INT m_csBatchLockIndexPos;
	INT m_csBatchVertexBase;
	INT m_csBatchIndexBase;
	INT m_csBatchVertexCount;
	INT m_csBatchIndexCount;
	DWORD m_csBatchPassCount;
	QWORD m_csBatchTexKey[MAX_TMUNITS];
	DWORD m_csBatchLayerFlags[MAX_TMUNITS];
	DWORD m_csBatchSevenBitMask;
	bool m_csBatchDepthBias;

	DWORD m_csBatchSurfaceCount;
	DWORD m_csBatchDrawCount;

	DWORD m_texEnableBits;

	IDirect3DVertexShader9 *m_vpDefaultRenderingState;
	IDirect3DVertexShader9 *m_vpDefaultRenderingStateWithFog;
	IDirect3DVertexShader9 *m_vpDefaultRenderingStateWithLinearFog;
	IDirect3DVertexShader9 *m_vpComplexSurface[MAX_TMUNITS];
	IDirect3DVertexShader9 *m_vpComplexSurfaceCT[MAX_TMUNITS];
	IDirect3DVertexShader9 *m_vpDetailTexture;
	IDirect3DVertexShader9 *m_vpComplexSurfaceSingleTextureAndDetailTexture;
	IDirect3DVertexShader9 *m_vpComplexSurfaceDualTextureAndDetailTexture;
	IDirect3DVertexShader9 *m_vpGammaCorrection;

	IDirect3DPixelShader9 *m_fpDefaultRenderingState;
	IDirect3DPixelShader9 *m_fpDefaultRenderingStateWithFog;
	IDirect3DPixelShader9 *m_fpDefaultRenderingStateWithLinearFog;
	IDirect3DPixelShader9 *m_fpComplexSurfaceSingleTexture;
	IDirect3DPixelShader9 *m_fpComplexSurfaceDualTextureModulated;
	IDirect3DPixelShader9 *m_fpComplexSurfaceTripleTextureModulated;
	IDirect3DPixelShader9 *m_fpComplexSurfaceSingleTextureWithFog;
	IDirect3DPixelShader9 *m_fpComplexSurfaceDualTextureModulatedWithFog;
	IDirect3DPixelShader9 *m_fpComplexSurfaceTripleTextureModulatedWithFog;
	IDirect3DPixelShader9 *m_fpDetailTexture;
	IDirect3DPixelShader9 *m_fpDetailTextureTwoLayer;
	IDirect3DPixelShader9 *m_fpSingleTextureAndDetailTexture;
	IDirect3DPixelShader9 *m_fpSingleTextureAndDetailTextureTwoLayer;
	IDirect3DPixelShader9 *m_fpDualTextureAndDetailTexture;
	IDirect3DPixelShader9 *m_fpDualTextureAndDetailTextureTwoLayer;
	IDirect3DPixelShader9 *m_fpGammaCorrection;

	//ReduceBanding twins of the combiners above, a single modulated layer needing two.
	IDirect3DPixelShader9 *m_fpComplexSurfaceDualTextureLightRecon;
	IDirect3DPixelShader9 *m_fpComplexSurfaceTripleTextureLightRecon;
	IDirect3DPixelShader9 *m_fpComplexSurfaceSingleTextureFogRecon;
	IDirect3DPixelShader9 *m_fpComplexSurfaceDualTextureLightFogRecon;
	IDirect3DPixelShader9 *m_fpComplexSurfaceDualTextureMacroFogRecon;
	IDirect3DPixelShader9 *m_fpComplexSurfaceTripleTextureLightFogRecon;
	IDirect3DPixelShader9 *m_fpComplexSurfaceDualTextureLightDetailRecon;
	IDirect3DPixelShader9 *m_fpComplexSurfaceDualTextureLightDetailTwoLayerRecon;

	bool m_sevenBitMapReconLoaded;
	bool m_sevenBitMapRecon;

	//c7-c9 hold the texel dimensions of layers 1 to 3 and c10-c14 the filter, dither and detail
	//constants, in two adjacent blocks named by number, so raising the texture unit count slides
	//the second block upward and drags every shader side binding along with it, which is what the
	//static_assert below is here to catch, the two blocks only ever fitting together at the unit
	//count they were written for.
	enum { SEVEN_BIT_MAP_SIZE_REG = 7 };
	enum { SEVEN_BIT_MAP_SIZE_REG_COUNT = MAX_TMUNITS - 1 };
	enum { SEVEN_BIT_MAP_CONST_REG = 10 };

	static_assert(SEVEN_BIT_MAP_SIZE_REG + SEVEN_BIT_MAP_SIZE_REG_COUNT <= SEVEN_BIT_MAP_CONST_REG,
		"The seven bit map size registers have grown into the constants above them");

	FLOAT m_sevenBitMapSizes[SEVEN_BIT_MAP_SIZE_REG_COUNT * 4];
	bool m_sevenBitMapSizesValid;

#ifdef WIN32
	HWND m_hWnd;
	HDC m_hDC;
#endif

	UBOOL WasFullscreen;

	bool m_frameRateLimitTimerInitialized;

	bool m_prevSwapBuffersStatus;

	bool m_frameSkipped;
	static BYTE m_skippedFrameScratch[VERTEX_RING_SIZE * 16];

	typedef rbtree<DWORD, FCachedTexture> DWORD_CTTree_t;
	typedef rbtree_allocator<DWORD_CTTree_t> DWORD_CTTree_Allocator_t;
	typedef rbtree<QWORD, FCachedTexture> QWORD_CTTree_t;
	typedef rbtree_allocator<QWORD_CTTree_t> QWORD_CTTree_Allocator_t;
	typedef rbtree_node_pool<QWORD_CTTree_t> QWORD_CTTree_NodePool_t;
	typedef DWORD TexPoolMapKey_t;
	typedef rbtree<TexPoolMapKey_t, QWORD_CTTree_NodePool_t> TexPoolMap_t;
	typedef rbtree_allocator<TexPoolMap_t> TexPoolMap_Allocator_t;

	enum { NUM_CTTree_TREES = 16 }; //Power of 2.
	inline DWORD FASTCALL CTZeroPrefixCacheIDSuffixToTreeIndex(DWORD CacheIDSuffix) {
		return ((CacheIDSuffix >> 12) & (NUM_CTTree_TREES - 1));
	}
	inline DWORD FASTCALL CTNonZeroPrefixCacheIDSuffixToTreeIndex(DWORD CacheIDSuffix) {
		return ((CacheIDSuffix >> 20) & (NUM_CTTree_TREES - 1));
	}
	inline DWORD FASTCALL MakeTexPoolMapKey(DWORD UBits, DWORD VBits) {
		return ((UBits << 16) | VBits);
	}

	DWORD_CTTree_t m_localZeroPrefixBindTrees[NUM_CTTree_TREES], *m_zeroPrefixBindTrees;
	QWORD_CTTree_t m_localNonZeroPrefixBindTrees[NUM_CTTree_TREES], *m_nonZeroPrefixBindTrees;
	CCachedTextureChain m_localZeroPrefixBindChain, *m_zeroPrefixBindChain;
	CCachedTextureChain m_localNonZeroPrefixBindChain, *m_nonZeroPrefixBindChain;
	TexPoolMap_t m_localRGBA8TexPool, *m_RGBA8TexPool;

	DWORD m_texCacheBytes;
	//Warns once.
	bool m_texCacheBudgetUnreachable;

	DWORD_CTTree_Allocator_t m_DWORD_CTTree_Allocator;
	QWORD_CTTree_Allocator_t m_QWORD_CTTree_Allocator;
	TexPoolMap_Allocator_t m_TexPoolMap_Allocator;

	QWORD_CTTree_NodePool_t m_nonZeroPrefixNodePool;

	TArray<FPlane> Modes;

	//Use UViewport* in URenderDevice
	//UViewport* Viewport;


	//Timing.
	DWORD BindCycles, ImageCycles, ComplexCycles, GouraudCycles, TileCycles;

	DWORD m_vpEnableCount;
	DWORD m_vpSwitchCount;
	DWORD m_fpEnableCount;
	DWORD m_fpSwitchCount;
	DWORD m_AASwitchCount;
	DWORD m_sceneNodeCount;
	DWORD m_sceneNodeRefreshCount;
	DWORD m_vbFlushCount;
	DWORD m_stat0Count;
	DWORD m_stat1Count;

	//Reset every frame.
	//@{
	DWORD m_drawCallCount; //Calls issued.
	DWORD m_recordedCmdCount; //Primitives handed over.
	DWORD m_runCount; //Draws they collapsed into.
	DWORD m_buildJobCount; //Jobs the build spanned.
	DWORD m_buildMicroseconds; //Wall time in the build phase.
	//@}


	struct {
		UBOOL SinglePassDetail;
		UBOOL UseFragmentProgram;
		//Clamped in place, so a shadow of the configured value is needed here.
		INT DetailMax;
	} DCV;

	FLOAT LODBias;
	FLOAT GammaOffset;
	FLOAT GammaOffsetRed;
	FLOAT GammaOffsetGreen;
	FLOAT GammaOffsetBlue;
	INT Brightness;
	UBOOL UseHardwareGamma;
	UBOOL ReduceBanding;
	UBOOL OneXBlending;
	INT MaxLogUOverV;
	INT MaxLogVOverU;
	INT MinLogTextureSize;
	INT MaxLogTextureSize;
	INT MaxAnisotropy;
	INT TMUnits;
	INT MaxTMUnits;
	INT RefreshRate;
	UBOOL UseMultiTexture;
	UBOOL UsePrecache;
	UBOOL UseTrilinear;
	UBOOL UseVertexSpecular;
	UBOOL UseS3TC;
	UBOOL Use16BitTextures;
	UBOOL Use565Textures;
	UBOOL NoFiltering;
	INT DetailMax;
	UBOOL UseDetailAlpha;
	UBOOL DetailClipping;
	UBOOL ColorizeDetailTextures;
	//A base-class feature flag on other SDKs, the 226 generation's base class predating it.
#ifdef UTGLR_OLD_URENDERDEVICE
	BITFIELD DetailTextures;
#endif
#ifdef UTGLR_KLINGON_BUILD
	BITFIELD SupportsTC;
#endif
	UBOOL SinglePassFog;
	UBOOL SinglePassDetail;
	UBOOL UseSSE;
	UBOOL UseSSE2;
	UBOOL LightmapAtlas;
	UBOOL UseSurfaceBatching;
	//Off by default.
	UBOOL DeferredRecording;
	//0 asks for one per logical core capped at 16, and 1 runs with no workers at all; read once at startup.
	INT RenderThreads;
	UBOOL UseTexIdPool;
	UBOOL UseTexPool;
	UBOOL CacheStaticMaps;
	UBOOL GenerateMipMaps;
	INT TexCacheBudgetMegs;
	INT DynamicTexIdRecycleLevel;
	UBOOL TexDXT1ToDXT3;
	UBOOL UseFragmentProgram;
	INT SwapInterval;
	INT FrameRateLimit;
	/*
	Some SDKs return a fixed-point time where magnitude never matters, while others return an
	absolute value off a 64-bit cycle counter that grows without bound until a FLOAT quantises
	the timing math away within days of uptime and the frame limiter starts stalling or
	overshooting.
	*/
#if defined UTGLR_DX_BUILD || defined UTGLR_RUNE_BUILD || defined UTGLR_OLD_URENDERDEVICE || defined UTGLR_UNREAL_227_BUILD
	DOUBLE m_prevFrameTimestamp;
#else
	FTime m_prevFrameTimestamp;
#endif
	UBOOL SmoothMaskedTextures;

	UBOOL UseTripleBuffering;
	UBOOL UsePureDevice;
	UBOOL UseSoftwareVertexProcessing;
	UBOOL UseAA;
	INT NumAASamples;
	UBOOL NoAATiles;

#ifndef UTGLR_OLD_URENDERDEVICE
	UBOOL ZRangeHack;
#endif
	bool m_useZRangeHack;
	bool m_nearZRangeHackProjectionActive;
	bool m_requestNearZRangeHackProjection;

	UBOOL BufferActorTris;
	UBOOL BufferClippedActorTris;

	enum {
		BV_TYPE_NONE = 0x00,
		BV_TYPE_GOURAUD_POLYS = 0x01,
		BV_TYPE_TILES = 0x02,
		BV_TYPE_LINES = 0x03,
		BV_TYPE_POINTS = 0x04,
		BV_TYPE_COMPLEX_SURFACE = 0x05
	};
	BYTE m_bufferedVertsType;
	DWORD m_bufferedVerts;

	/*
	Only the safety net, but it has to stay: opening a batch locks the vertex buffer and captures
	the write position, and a later flush would nest that lock and move the position it was about
	to draw from, which is the one failure in this area that does not announce itself, the batch
	going on to draw from wherever the flush left the cursor and painting somebody else's
	triangles in its place.
	*/
	inline void FASTCALL StartBuffering(DWORD bvType) {
		if (!m_drawCmds.empty()) {
			FlushDeferred();
			if ((m_curVertexBufferPos + (3 * (MAX_BUFFERED_GP_PTS - 2))) >= VERTEX_RING_SIZE) {
				FlushVertexBuffers();
			}
		}
		m_bufferedVertsType = bvType;
	}
	inline void FASTCALL EndBufferingExcept(DWORD bvExceptType) {
		if (m_bufferedVertsType != bvExceptType) {
			EndBuffering();
		}
	}
	inline void EndBuffering(void) {
		if ((m_bufferedVerts > 0) || m_csBatchOpen) {
			EndBufferingNoCheck();
		}
	}
	void EndBufferingNoCheck(void);
	void EndGouraudPolygonBufferingNoCheck(void);
	void EndTileBufferingNoCheck(void);
	void EndLineBufferingNoCheck(void);
	void EndPointBufferingNoCheck(void);

	void DrawBatchedComplexSurface(void);
	void EndComplexSurfaceBufferingNoCheck(void);
	inline void EndComplexSurfaceBuffering(void) {
		if (m_csBatchOpen) {
			EndComplexSurfaceBufferingNoCheck();
		}
	}


	BITFIELD PL_DetailTextures;
	UBOOL PL_OneXBlending;
	UBOOL PL_ReduceBanding;
	//While this is stale the two gamma paths disagree.
	UBOOL PL_UseHardwareGamma;
	FLOAT PL_ClientBrightness;
	FLOAT PL_GammaOffset;
	FLOAT PL_GammaOffsetRed;
	FLOAT PL_GammaOffsetGreen;
	FLOAT PL_GammaOffsetBlue;
	INT PL_Brightness;
	INT PL_MaxLogUOverV;
	INT PL_MaxLogVOverU;
	INT PL_MinLogTextureSize;
	INT PL_MaxLogTextureSize;
	UBOOL PL_NoFiltering;
	UBOOL PL_UseTrilinear;
	UBOOL PL_Use16BitTextures;
	UBOOL PL_Use565Textures;
	UBOOL PL_TexDXT1ToDXT3;
	UBOOL PL_GenerateMipMaps;
	INT PL_MaxAnisotropy;
	UBOOL PL_SmoothMaskedTextures;
	FLOAT PL_LODBias;
	UBOOL PL_UseDetailAlpha;
	UBOOL PL_SinglePassDetail;
	UBOOL PL_UseFragmentProgram;
	UBOOL PL_UseSSE;
	UBOOL PL_UseSSE2;

	bool m_useSSSE3;

	DWORD m_numDepthBits;

	INT AllocatedTextures;

	INT m_rpPassCount;
	INT m_rpTMUnits;
	bool m_rpForceSingle;
	bool m_rpMasked;
	bool m_rpSetDepthEqual;
	DWORD m_rpColor;

	void (UD3D9RenderDevice::*m_pRenderPassesNoCheckSetupProc)(void);
	void (FASTCALL UD3D9RenderDevice::*m_pRenderPassesNoCheckSetup_SingleOrDualTextureAndDetailTextureProc)(FTextureInfo &);

	DWORD (FASTCALL UD3D9RenderDevice::*m_pBufferDetailTextureDataProc)(FLOAT);

	//Hit info.
	BYTE *m_HitData;
	INT *m_HitSize;
	INT m_HitBufSize;
	INT m_HitCount;
	CGClip m_gclip;


	DWORD m_currentFrameCount;

	//Lock variables.
	FPlane FlashScale, FlashFog;
	FLOAT m_RProjZ, m_Aspect;
	FLOAT m_RFX2, m_RFY2;
	INT m_sceneNodeX, m_sceneNodeY;

	//Child nodes - mirrors, letterboxing, weapon overlays - can share a parent's position.
	struct FSceneNodeKey {
		INT X, Y, XB, YB;
		FLOAT FX, FY;
		FLOAT FX2, FY2;
		FLOAT RProjZ;
		FLOAT FovAngle;
		//Not derivable from the other fields.
		INT IsOrtho;
		INT HitX, HitXL, HitY, HitYL;
	};
	//Compared bytewise, so -0.0 and +0.0 compare unequal. One redundant re-set at worst.
	static_assert(sizeof(FSceneNodeKey) == 15 * 4, "FSceneNodeKey must be padding free to be compared as bytes");
	FSceneNodeKey m_sceneNodeKey;

	inline void FASTCALL BuildSceneNodeKey(const FSceneNode *Frame, FSceneNodeKey &key) const {
		key.X = Frame->X;
		key.Y = Frame->Y;
		key.XB = Frame->XB;
		key.YB = Frame->YB;
		key.FX = Frame->FX;
		key.FY = Frame->FY;
		key.FX2 = Frame->FX2;
		key.FY2 = Frame->FY2;
		key.RProjZ = Frame->RProj.Z;
		key.FovAngle = Viewport->Actor ? Viewport->Actor->FovAngle : 0.0f;
		key.IsOrtho = Viewport->IsOrtho() ? 1 : 0;
		if (m_HitData) {
			key.HitX = Viewport->HitX;
			key.HitXL = Viewport->HitXL;
			key.HitY = Viewport->HitY;
			key.HitYL = Viewport->HitYL;
		} else {
			key.HitX = 0;
			key.HitXL = 0;
			key.HitY = 0;
			key.HitYL = 0;
		}
	}

	inline bool FASTCALL SceneNodeChanged(const FSceneNode *Frame) const {
		FSceneNodeKey key;
		BuildSceneNodeKey(Frame, key);
		return appMemcmp(&key, &m_sceneNodeKey, sizeof(key)) != 0;
	}

	bool m_usingAA;
	bool m_curAAEnable;
	//Survives a device reset.
	bool m_defAAEnable;
	INT m_initNumAASamples;

	bool m_isATI;
	bool m_isNVIDIA;

	enum {
		PF2_NEAR_Z_RANGE_HACK = 0x01
	};
	//In the normalized units D3DRS_DEPTHBIAS takes.
	//Zero on hardware without the raster cap.
	FLOAT m_moverDepthBias;
	FLOAT m_moverSlopeScaleDepthBias;
	FLOAT m_curDepthBias;
	FLOAT m_curSlopeScaleDepthBias;

	DWORD m_curBlendFlags;

	//A separate flag, since group changes come from the XOR of old and new values.
	bool m_blendStateInvalid;

	DWORD m_smoothMaskedTexturesBit;
	bool m_useAlphaToCoverageForMasked;
	bool m_alphaToCoverageEnabled;
	bool m_alphaTestEnabled;
	DWORD m_curPolyFlags;
	DWORD m_curPolyFlags2;
	INT m_bufferActorTrisCutoff;

	//Blended geometry writes no depth.
	//@{
	struct FDeferredGouraudPoly {
		FLOAT Depth; //Sort key.
		DWORD PolyFlags; //The engine's own.
		DWORD PolyFlags2; //Decided at submission.
		INT TexIndex; //Into the texture list.
		INT FirstPt; //Into the point list.
		INT NumPts;
#ifdef UTGLR_RUNE_BUILD
		BYTE Alpha; //Rune's per object alpha.
#endif
	};
	FDeferredGouraudPoly m_deferredGouraudPolys[MAX_DEFERRED_GP_POLYS];
	//Draw order and scratch.
	INT m_deferredGouraudOrder[MAX_DEFERRED_GP_POLYS];
	INT m_deferredGouraudOrderScratch[MAX_DEFERRED_GP_POLYS];
	FTransTexture m_deferredGouraudPts[MAX_DEFERRED_GP_PTS];
	FTransTexture *m_deferredGouraudPtPtrs[MAX_DEFERRED_GP_PTS];
	//The source is local to the engine's mesh drawing code and does not outlive the call, so the
	//texture is bound to a cache entry at defer time, with the poly flags part of that cache key
	//because masking is applied per texel on upload, and the 16 bit decision is captured in the
	//same breath, the palette it was read from being free to belong to some other object by the
	//time the run draws.
	struct FDeferredGouraudTexture {
		FTextureInfo Info;
		DWORD PolyFlags;
		bool Is16Bit;
	};
	FDeferredGouraudTexture m_deferredGouraudTextures[MAX_DEFERRED_GP_TEXTURES];
	INT m_numDeferredGouraudPolys;
	INT m_numDeferredGouraudPts;
	INT m_numDeferredGouraudTextures;
	//Only ever compared, never dereferenced.
	FSceneNode *m_deferredGouraudFrame;
	bool m_replayingDeferredGouraudPolys;
	DWORD m_replayPolyFlags2;
	bool m_replayIs16Bit;
#ifdef UTGLR_RUNE_BUILD
	BYTE m_replayAlpha;
#endif

	/*
	Whether a polygon takes the near-clip projection reserved for the weapon.
	*/
	inline DWORD FASTCALL NearZRangeHackFlag(DWORD PolyFlags) const {
#ifdef UTGLR_OLD_URENDERDEVICE
		return 0;
#else
		if (!m_useZRangeHack || !(GUglyHackFlags & 0x1)) {
			return 0;
		}
		return PF2_NEAR_Z_RANGE_HACK;
#endif
	}

	//False when too wide.
	bool DeferGouraudPolygon(FSceneNode *Frame, FTextureInfo &Info, FTransTexture **Pts, INT NumPts, DWORD PolyFlags);
	void SortDeferredGouraudPolys(INT numPolys);
	void EndDeferredGouraudPolysNoCheck(void);
	inline void EndDeferredGouraudPolys(void) {
		if (m_numDeferredGouraudPolys > 0) {
			EndDeferredGouraudPolysNoCheck();
		}
	}
	//A plain dimension test misses child nodes like mirrors.
	inline void FASTCALL EndDeferredGouraudPolysForFrame(FSceneNode *Frame) {
		if ((m_numDeferredGouraudPolys > 0) && (Frame != m_deferredGouraudFrame)) {
			EndDeferredGouraudPolysNoCheck();
		}
	}
	inline void DiscardDeferredGouraudPolys(void) {
		m_numDeferredGouraudPolys = 0;
		m_numDeferredGouraudPts = 0;
		m_numDeferredGouraudTextures = 0;
		m_deferredGouraudFrame = NULL;
		m_replayingDeferredGouraudPolys = false;
		m_replayIs16Bit = false;
	}

	//Whether this leaves depth writes off.
	static inline bool FASTCALL IsBlendedGeometry(DWORD PolyFlags) {
		if (PolyFlags & PF_Occlude) {
			return false;
		}
#ifdef UTGLR_RUNE_BUILD
		if (PolyFlags & PF_AlphaBlend) {
			//Occluded unless masked.
			return (PolyFlags & PF_Masked) != 0;
		}
#endif
		return (PolyFlags & (PF_Translucent | PF_Modulated | PF_Highlighted)) != 0;
	}
	//@}

	DWORD m_detailTextureColor4ub;

	enum {
		CF_COLOR_ARRAY = 0x01,
		CF_FOG_MODE = 0x02
	};
	BYTE m_requestedColorFlags;
#ifdef UTGLR_RUNE_BUILD
	BYTE m_gpAlpha;
	bool m_gpFogEnabled;
#endif

	FLOAT m_fsBlendInfo[4];

	DWORD m_curTexEnvFlags[MAX_TMUNITS];
	tex_params_t m_curTexStageParams[MAX_TMUNITS];
	FTexInfo TexInfo[MAX_TMUNITS];

	void(FASTCALL *m_pBuffer3BasicVertsProc)(UD3D9RenderDevice *, FTransTexture **);
	void(FASTCALL *m_pBuffer3ColoredVertsProc)(UD3D9RenderDevice *, FTransTexture **);
	void(FASTCALL *m_pBuffer3FoggedVertsProc)(UD3D9RenderDevice *, FTransTexture **);

	void(FASTCALL *m_pBuffer3VertsProc)(UD3D9RenderDevice *, FTransTexture **);

	IDirect3DTexture9 *m_pNoTexObj;
	IDirect3DTexture9 *m_pAlphaTexObj;

	IDirect3DTexture9 *m_pGammaTexObj;
	IDirect3DSurface9 *m_pGammaTexSurface;
	IDirect3DSurface9 *m_pBackBufferSurface;
	bool m_gammaPassAvailable;
	INT m_gammaPassFailCount;
	bool m_gammaPassGaveUp;

	static INT NumDevices;
	static INT LockCount;

	static HMODULE hModuleD3d9;
	static LPDIRECT3DCREATE9 pDirect3DCreate9;

	static bool g_haveOriginalGammaRamp;
	static D3DGAMMARAMP g_originalGammaRamp;


	IDirect3D9 *m_d3d9;
	IDirect3DDevice9 *m_d3dDevice;

	INT m_SetRes_NewX;
	INT m_SetRes_NewY;
	INT m_SetRes_NewColorBytes;
	UBOOL m_SetRes_Fullscreen;
	bool m_SetRes_isDeviceReset;
	//Set when a windowed resize released the permanent resources.
	bool m_deferredDeviceReset;

	D3DCAPS9 m_d3dCaps;
	bool m_dxt1TextureCap;
	bool m_dxt3TextureCap;
	bool m_dxt5TextureCap;
	bool m_16BitTextureCap;
	bool m_565TextureCap;
	bool m_alphaTextureCap;
	bool m_supportsAlphaToCoverage;
	bool m_anisotropicMagFilterCap;

	D3DPRESENT_PARAMETERS m_d3dpp;
	bool m_doSoftwareVertexInit;


#ifdef BGRA_MAKE
#undef BGRA_MAKE
#endif
	static inline DWORD BGRA_MAKE(BYTE b, BYTE g, BYTE r, BYTE a) {
		return (a << 24) | (r << 16) | (g << 8) | b;
	}


#if defined WIN32 && defined UTGLR_USE_ASM_CODE
	static inline DWORD FPlaneTo_BGR_A255(const FPlane *pPlane) {
		static FLOAT f255 = 255.0f;
		INT iR, iG, iB;
		__asm {
			mov eax, pPlane
			fld [eax]FVector.X
			fmul [f255]
			fistp [iR]
			fld [eax]FVector.Y
			fmul [f255]
			fistp [iG]
			fld [eax]FVector.Z
			fmul [f255]
			fistp [iB]
		}
		return BGRA_MAKE(iB, iG, iR, 255);
	}
#else
	static inline DWORD FASTCALL FPlaneTo_BGR_A255(const FPlane *pPlane) {
		return BGRA_MAKE(
			appRound(pPlane->Z * 255.0f),
			appRound(pPlane->Y * 255.0f),
			appRound(pPlane->X * 255.0f),
			255);
	}
#endif

#if defined WIN32 && defined UTGLR_USE_ASM_CODE
	static inline DWORD FPlaneTo_BGRClamped_A255(const FPlane *pPlane) {
		static FLOAT f255 = 255.0f;
		INT iR, iG, iB;
		__asm {
			mov eax, pPlane
			fld [eax]FVector.X
			fmul [f255]
			fistp [iR]
			fld [eax]FVector.Y
			fmul [f255]
			fistp [iG]
			fld [eax]FVector.Z
			fmul [f255]
			fistp [iB]
		}
		return BGRA_MAKE(Clamp(iB, 0, 255), Clamp(iG, 0, 255), Clamp(iR, 0, 255), 255);
	}
#else
	static inline DWORD FASTCALL FPlaneTo_BGRClamped_A255(const FPlane *pPlane) {
		return BGRA_MAKE(
			Clamp(appRound(pPlane->Z * 255.0f), 0, 255),
			Clamp(appRound(pPlane->Y * 255.0f), 0, 255),
			Clamp(appRound(pPlane->X * 255.0f), 0, 255),
			255);
	}
#endif

#if defined WIN32 && defined UTGLR_USE_ASM_CODE
	static inline DWORD FPlaneTo_BGR_A0(const FPlane *pPlane) {
		static FLOAT f255 = 255.0f;
		INT iR, iG, iB;
		__asm {
			mov eax, pPlane
			fld [eax]FVector.X
			fmul [f255]
			fistp [iR]
			fld [eax]FVector.Y
			fmul [f255]
			fistp [iG]
			fld [eax]FVector.Z
			fmul [f255]
			fistp [iB]
		}
		return BGRA_MAKE(iB, iG, iR, 0);
	}
#else
	static inline DWORD FASTCALL FPlaneTo_BGR_A0(const FPlane *pPlane) {
		return BGRA_MAKE(
			appRound(pPlane->Z * 255.0f),
			appRound(pPlane->Y * 255.0f),
			appRound(pPlane->X * 255.0f),
			0);
	}
#endif

#if defined WIN32 && defined UTGLR_USE_ASM_CODE
	static inline DWORD FPlaneTo_BGR_Aub(const FPlane *pPlane, BYTE alpha) {
		static FLOAT f255 = 255.0f;
		INT iR, iG, iB;
		__asm {
			mov eax, pPlane
			fld [eax]FVector.X
			fmul [f255]
			fistp [iR]
			fld [eax]FVector.Y
			fmul [f255]
			fistp [iG]
			fld [eax]FVector.Z
			fmul [f255]
			fistp [iB]
		}
		return BGRA_MAKE(iB, iG, iR, alpha);
	}
#else
	static inline DWORD FASTCALL FPlaneTo_BGR_Aub(const FPlane *pPlane, BYTE alpha) {
		return BGRA_MAKE(
			appRound(pPlane->Z * 255.0f),
			appRound(pPlane->Y * 255.0f),
			appRound(pPlane->X * 255.0f),
			alpha);
	}
#endif

	//One function, so no two draw paths round differently.
	inline DWORD FASTCALL ComputeTileColor(FPlane &Color, FTextureInfo &Info, DWORD PolyFlags) {
		if (PolyFlags & PF_Modulated) {
			return 0xFFFFFFFF;
		}

		if (UseSSE2) {
#ifdef UTGLR_INCLUDE_SSE_CODE
			static __m128 fColorMul = { 255.0f, 255.0f, 255.0f, 0.0f };
			static const union {
				DWORD u[4];
				__m128 f;
			} fRGBLaneMask = { { 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0x00000000u } };
			__m128 fColorMulReg;
			__m128 fColor;
			__m128 fAlpha;
			__m128i iColor;

			fColorMulReg = fColorMul;
			fColor = _mm_loadu_ps(&Color.X);
			fColor = _mm_mul_ps(fColor, fColorMulReg);
			fColor = _mm_and_ps(fColor, fRGBLaneMask.f);

			fColor = _mm_shuffle_ps(fColor, fColor, _MM_SHUFFLE(3, 0, 1, 2));

			fAlpha = _mm_setzero_ps();
			fAlpha = _mm_move_ss(fAlpha, fColorMulReg);
#ifdef UTGLR_RUNE_BUILD
			if (PolyFlags & PF_AlphaBlend) {
				fAlpha = _mm_mul_ss(fAlpha, _mm_load_ss(&Info.Texture->Alpha));
			}
#endif
			fAlpha = _mm_shuffle_ps(fAlpha, fAlpha, _MM_SHUFFLE(0, 1, 1, 1));

			fColor = _mm_or_ps(fColor, fAlpha);

			iColor = _mm_cvtps_epi32(fColor);
			iColor = _mm_packs_epi32(iColor, iColor);
			iColor = _mm_packus_epi16(iColor, iColor);

			return (DWORD)_mm_cvtsi128_si32(iColor);
#endif
		}

#ifdef UTGLR_RUNE_BUILD
		if (PolyFlags & PF_AlphaBlend) {
			Color.W = Info.Texture->Alpha;
			return FPlaneTo_BGRAClamped(&Color);
		}
#endif
		return FPlaneTo_BGRClamped_A255(&Color);
	}

#if defined WIN32 && defined UTGLR_USE_ASM_CODE
	static inline DWORD FPlaneTo_BGRA(const FPlane *pPlane) {
		static FLOAT f255 = 255.0f;
		INT iR, iG, iB, iA;
		__asm {
			mov eax, pPlane
			fld [eax]FVector.X
			fmul [f255]
			fistp [iR]
			fld [eax]FVector.Y
			fmul [f255]
			fistp [iG]
			fld [eax]FVector.Z
			fmul [f255]
			fistp [iB]
			fld [eax]FPlane.W
			fmul [f255]
			fistp [iA]
		}
		return BGRA_MAKE(iB, iG, iR, iA);
	}
#else
	static inline DWORD FASTCALL FPlaneTo_BGRA(const FPlane *pPlane) {
		return BGRA_MAKE(
			appRound(pPlane->Z * 255.0f),
			appRound(pPlane->Y * 255.0f),
			appRound(pPlane->X * 255.0f),
			appRound(pPlane->W * 255.0f));
	}
#endif

#if defined WIN32 && defined UTGLR_USE_ASM_CODE
	static inline DWORD FPlaneTo_BGRAClamped(const FPlane *pPlane) {
		static FLOAT f255 = 255.0f;
		INT iR, iG, iB, iA;
		__asm {
			mov eax, pPlane
			fld [eax]FVector.X
			fmul [f255]
			fistp [iR]
			fld [eax]FVector.Y
			fmul [f255]
			fistp [iG]
			fld [eax]FVector.Z
			fmul [f255]
			fistp [iB]
			fld [eax]FPlane.W
			fmul [f255]
			fistp [iA]
		}
		return BGRA_MAKE(Clamp(iB, 0, 255), Clamp(iG, 0, 255), Clamp(iR, 0, 255), Clamp(iA, 0, 255));
	}
#else
	static inline DWORD FASTCALL FPlaneTo_BGRAClamped(const FPlane *pPlane) {
		return BGRA_MAKE(
			Clamp(appRound(pPlane->Z * 255.0f), 0, 255),
			Clamp(appRound(pPlane->Y * 255.0f), 0, 255),
			Clamp(appRound(pPlane->X * 255.0f), 0, 255),
			Clamp(appRound(pPlane->W * 255.0f), 0, 255));
	}
#endif

#if defined WIN32 && defined UTGLR_USE_ASM_CODE
	static inline DWORD FPlaneTo_BGRScaled_A255(const FPlane *pPlane, FLOAT rgbScale) {
		INT iR, iG, iB;
		__asm {
			mov eax, pPlane
			fld [eax]FVector.X
			fmul [rgbScale]
			fistp [iR]
			fld [eax]FVector.Y
			fmul [rgbScale]
			fistp [iG]
			fld [eax]FVector.Z
			fmul [rgbScale]
			fistp [iB]
		}
		return BGRA_MAKE(iB, iG, iR, 255);
	}
#else
	static inline DWORD FASTCALL FPlaneTo_BGRScaled_A255(const FPlane *pPlane, FLOAT rgbScale) {
		return BGRA_MAKE(
			appRound(pPlane->Z * rgbScale),
			appRound(pPlane->Y * rgbScale),
			appRound(pPlane->X * rgbScale),
			255);
	}
#endif

	//An out-of-range component converted to a byte turns overbright light dark; the SSE path saturates.
	static inline DWORD FASTCALL FPlaneTo_BGRScaledClamped_A255(const FPlane *pPlane, FLOAT rgbScale) {
		return BGRA_MAKE(
			Clamp(appRound(pPlane->Z * rgbScale), 0, 255),
			Clamp(appRound(pPlane->Y * rgbScale), 0, 255),
			Clamp(appRound(pPlane->X * rgbScale), 0, 255),
			255);
	}

	static inline DWORD FASTCALL FPlaneTo_BGRClamped_A0(const FPlane *pPlane) {
		return BGRA_MAKE(
			Clamp(appRound(pPlane->Z * 255.0f), 0, 255),
			Clamp(appRound(pPlane->Y * 255.0f), 0, 255),
			Clamp(appRound(pPlane->X * 255.0f), 0, 255),
			0);
	}

	static inline DWORD FASTCALL FPlaneTo_BGRClamped_Aub(const FPlane *pPlane, BYTE alpha) {
		return BGRA_MAKE(
			Clamp(appRound(pPlane->Z * 255.0f), 0, 255),
			Clamp(appRound(pPlane->Y * 255.0f), 0, 255),
			Clamp(appRound(pPlane->X * 255.0f), 0, 255),
			alpha);
	}


	//UObject interface.
	/*
	Must stay user provided, because a non-user-provided default constructor value-initializes the
	object first and wipes the header the engine has already set up.
	*/
	UD3D9RenderDevice() :
		m_defAAEnable(true),
		m_blendStateInvalid(false),
		m_isUncountedDevice(false),
		m_exited(false),
		m_deferredReady(false),
		m_deferredBuildFailCount(0),
		m_deferredGaveUp(false),
		m_pJobSystem(NULL) {}
#ifdef UTGLR_KLINGON_BUILD
	static void StaticConstructor(UClass *Class);
#else
	void StaticConstructor();
#endif
	void StaticConstructorBody();


	//Implementation.
	void FASTCALL SC_AddBoolConfigParam(DWORD BitMaskOffset, const TCHAR *pName, UBOOL &param, ECppProperty EC_CppProperty, INT InOffset, UBOOL defaultValue);
	void FASTCALL SC_AddBoolConfigParam(DWORD BitMaskOffset, const TCHAR *pName, BITFIELD &param, ECppProperty EC_CppProperty, INT InOffset, UBOOL defaultValue);
	void FASTCALL SC_AddIntConfigParam(const TCHAR *pName, INT &param, ECppProperty EC_CppProperty, INT InOffset, INT defaultValue);
	void FASTCALL SC_AddFloatConfigParam(const TCHAR *pName, FLOAT &param, ECppProperty EC_CppProperty, INT InOffset, FLOAT defaultValue);

	struct FBoolConfigParam {
		const TCHAR *pName;
		DWORD BitMaskOffset;
	};
	enum { MAX_BOOL_CONFIG_PARAMS = 64 };
	static FBoolConfigParam m_boolConfigParams[MAX_BOOL_CONFIG_PARAMS];
	static DWORD m_numBoolConfigParams;
	static DWORD m_numDroppedBoolConfigParams;

	static UClass *m_scClass;

	void FASTCALL SC_RecordBoolConfigParam(const TCHAR *pName, DWORD BitMaskOffset);
	void ValidateBoolConfigParamOffsets(void);

	void FASTCALL DbgPrintInitParam(const TCHAR *pName, INT value);
	void FASTCALL DbgPrintInitParam(const TCHAR *pName, FLOAT value);
	void FASTCALL DbgPrintInitParam(const TCHAR *pName, BITFIELD value);

#ifdef UTGLR_INCLUDE_SSE_CODE
	static bool CPU_DetectCPUID(void);
	static bool CPU_DetectSSE(void);
	static bool CPU_DetectSSE2(void);
	static bool CPU_DetectSSSE3(void);
#endif //UTGLR_INCLUDE_SSE_CODE

	void FASTCALL InitEngineFeatureFlagSafe(const TCHAR *pName, BITFIELD &flag);
	void InitEngineFeatureFlags(void);

	void InitFrameRateLimitTimerSafe(void);
	void ShutdownFrameRateLimitTimer(void);

	void BuildGammaRamp(float redGamma, float greenGamma, float blueGamma, int brightness, D3DGAMMARAMP &ramp);
	void SetGamma(FLOAT GammaCorrection);
	void ResetGamma(void);

	void InitGammaResourcesSafe(void);
	void FreeGammaResources(void);
	bool CalcGammaPassParams(FLOAT GammaCorrection, FLOAT *pExponents, FLOAT *pOffsets);
	void ApplyGammaPass(void);
	void GammaPassFailed(void);

	UBOOL FailedInitf(const TCHAR *Fmt, ...);
	void Exit();
	void ShutdownAfterError();

	UBOOL SetRes(INT NewX, INT NewY, INT NewColorBytes, UBOOL Fullscreen);
	void UnsetRes();

	bool IsFullscreenModeSupported(INT NewX, INT NewY, INT NewColorBytes);

	bool FASTCALL CheckDepthFormat(D3DFORMAT adapterFormat, D3DFORMAT backBufferFormat, D3DFORMAT depthBufferFormat);

	void ConfigValidate_RefreshDCV(void);
	void ConfigValidate_RequiredExtensions(void);
	void ConfigValidate_Main(void);

	void InitPermanentResourcesAndRenderingState(void);
	void FreePermanentResources(void);


	UBOOL Init(UViewport *InViewport, INT NewX, INT NewY, INT NewColorBytes, UBOOL Fullscreen);

	static QSORT_RETURN CDECL CompareRes(const FPlane *A, const FPlane *B) {
		return (QSORT_RETURN)(((A->X - B->X) != 0.0f) ? (A->X - B->X) : (A->Y - B->Y));
	}

	UBOOL Exec(const TCHAR *Cmd, FOutputDevice &Ar);
	void Lock(FPlane InFlashScale, FPlane InFlashFog, FPlane ScreenClear, DWORD RenderLockFlags, BYTE *InHitData, INT *InHitSize);
	void SetSceneNode(FSceneNode *Frame);
	void Unlock(UBOOL Blit);
#ifdef UTGLR_OLD_URENDERDEVICE
	void Flush();
#else
	void Flush(UBOOL AllowPrecache);
#endif
	inline void InternalFlush() {
#ifdef UTGLR_OLD_URENDERDEVICE
		Flush();
#else
		Flush(1);
#endif
	}

	void DrawComplexSurface(FSceneNode *Frame, FSurfaceInfo &Surface, FSurfaceFacet &Facet);
#ifdef UTGLR_RUNE_BUILD
	void PreDrawFogSurface();
	void PostDrawFogSurface();
	void DrawFogSurface(FSceneNode *Frame, FFogSurf &FogSurf);
	void PreDrawGouraud(FSceneNode *Frame, FLOAT FogDistance, FPlane FogColor);
	void PostDrawGouraud(FLOAT FogDistance);
#endif
	void DrawGouraudPolygonOld(FSceneNode *Frame, FTextureInfo &Info, FTransTexture **Pts, INT NumPts, DWORD PolyFlags, FSpanBuffer *Span);
	void DrawGouraudPolygon(FSceneNode *Frame, FTextureInfo &Info, FTransTexture **Pts, INT NumPts, DWORD PolyFlags, FSpanBuffer *Span);
#ifdef UTGLR_HP_BUILD
	INT MaxVertices();
	void DrawTriangles(FSceneNode *Frame, FTextureInfo &Info, FTransTexture **Pts, INT NumPts, _WORD *Indices, INT NumIndices, DWORD PolyFlags, FSpanBuffer *Span);

	/*
	Indices come from game package mesh data, so they are range checked first, the comparison signed
	deliberately.
	*/
	static inline bool FASTCALL IndexedTriangleInRange(const _WORD *Indices, INT first, INT NumPts) {
		if (!Indices) {
			return true;
		}
		return (Indices[first + 0] < NumPts) && (Indices[first + 1] < NumPts) && (Indices[first + 2] < NumPts);
	}
#endif
	void DrawTile(FSceneNode *Frame, FTextureInfo &Info, FLOAT X, FLOAT Y, FLOAT XL, FLOAT YL, FLOAT U, FLOAT V, FLOAT UL, FLOAT VL, class FSpanBuffer *Span, FLOAT Z, FPlane Color, FPlane Fog, DWORD PolyFlags);
	void Draw3DLine(FSceneNode *Frame, FPlane Color, DWORD LineFlags, FVector P1, FVector P2);
	void Draw2DLine(FSceneNode *Frame, FPlane Color, DWORD LineFlags, FVector P1, FVector P2);
	void Draw2DPoint(FSceneNode *Frame, FPlane Color, DWORD LineFlags, FLOAT X1, FLOAT Y1, FLOAT X2, FLOAT Y2, FLOAT Z);

	void ClearZ(FSceneNode *Frame);
	void PushHit(const BYTE *Data, INT Count);
	void PopHit(INT Count, UBOOL bForce);
	void GetStats(TCHAR *Result);
	void ReadPixels(FColor *Pixels);
	void EndFlash();
	void PrecacheTexture(FTextureInfo &Info, DWORD PolyFlags);


	void InitNoTextureSafe(void);
	void InitAlphaTextureSafe(void);

	void ScanForOldTextures(void);

	inline void FASTCALL SetNoTexture(INT Multi) {
		if (TexInfo[Multi].CurrentCacheID != TEX_CACHE_ID_NO_TEX) {
			SetNoTextureNoCheck(Multi);
		}
	}
	inline void SetAlphaTexture(INT Multi) {
		if (TexInfo[Multi].CurrentCacheID != TEX_CACHE_ID_ALPHA_TEX) {
			SetAlphaTextureNoCheck(Multi);
		}
	}

	void FASTCALL SetNoTextureNoCheck(INT Multi);
	void FASTCALL SetAlphaTextureNoCheck(INT Multi);

	//The optional 16-bit flag is only wanted by callers that bind a texture.
	inline QWORD FASTCALL ComposeTexCacheID(const FTextureInfo &Info, DWORD PolyFlags, bool *pIs16Bit = NULL) const {
		QWORD CacheID = Info.CacheID;

		if (pIs16Bit) {
			*pIs16Bit = false;
		}

		if ((CacheID & 0xFF) == 0xE0) {
			//A masked texture needs its own cache entry: transparency is applied per texel on upload.
			CacheID |= (PolyFlags & PF_Masked) ? TEX_CACHE_ID_FLAG_MASKED : 0;

			//On a deferred replay use the value captured at defer time.
			if (Use16BitTextures) {
				const bool is16Bit = m_replayingDeferredGouraudPolys
					? m_replayIs16Bit
					: (Info.Palette && (Info.Palette[128].A == 255));
				if (is16Bit) {
					CacheID |= TEX_CACHE_ID_FLAG_16BIT;
					if (pIs16Bit) {
						*pIs16Bit = true;
					}
				}
			}
		}

		return CacheID;
	}

	inline void FASTCALL SetTexture(INT Multi, FTextureInfo &Info, DWORD PolyFlags, FLOAT PanBias) {
		FTexInfo &Tex = TexInfo[Multi];
		QWORD CacheID;
		DWORD DynamicPolyFlags;

		Tex.UPan = Info.Pan.X + (PanBias * Info.UScale);
		Tex.VPan = Info.Pan.Y + (PanBias * Info.VScale);

		//Internally means 16-bit.
		PolyFlags &= ~PF_Memorized;

		bool is16Bit = false;
		CacheID = ComposeTexCacheID(Info, PolyFlags, &is16Bit);
		if (is16Bit) {
			PolyFlags |= PF_Memorized;
		}

		DynamicPolyFlags = PolyFlags & TEX_DYNAMIC_POLY_FLAGS_MASK;

		if ((CacheID == Tex.CurrentCacheID) && (DynamicPolyFlags == Tex.CurrentDynamicPolyFlags) && !TexInfoRealtimeChanged(Info)) {
			return;
		}

		Tex.CurrentCacheID = CacheID;
		Tex.CurrentDynamicPolyFlags = DynamicPolyFlags;

		SetTextureNoCheck(Multi, Tex, Info, PolyFlags);

		return;
	}

	inline void FASTCALL SetTextureNoPanBias(INT Multi, FTextureInfo &Info, DWORD PolyFlags) {
		FTexInfo &Tex = TexInfo[Multi];
		QWORD CacheID;
		DWORD DynamicPolyFlags;

		//Sets panning.
		Tex.UPan = Info.Pan.X;
		Tex.VPan = Info.Pan.Y;

		//Internally means 16-bit.
		PolyFlags &= ~PF_Memorized;

		bool is16Bit = false;
		CacheID = ComposeTexCacheID(Info, PolyFlags, &is16Bit);
		if (is16Bit) {
			PolyFlags |= PF_Memorized;
		}

		DynamicPolyFlags = PolyFlags & TEX_DYNAMIC_POLY_FLAGS_MASK;

		if ((CacheID == Tex.CurrentCacheID) && (DynamicPolyFlags == Tex.CurrentDynamicPolyFlags) && !TexInfoRealtimeChanged(Info)) {
			return;
		}

		Tex.CurrentCacheID = CacheID;
		Tex.CurrentDynamicPolyFlags = DynamicPolyFlags;

		SetTextureNoCheck(Multi, Tex, Info, PolyFlags);

		return;
	}

	void FASTCALL SetTextureNoCheck(DWORD texNum, FTexInfo &Tex, FTextureInfo &Info, DWORD PolyFlags);
	void FASTCALL CacheTextureInfo(FCachedTexture *pBind, const FTextureInfo &Info, DWORD PolyFlags);

	FCachedTexture *FASTCALL FindCachedTextureBind(QWORD CacheID);
	//Recorded on both places a lightmap is cached.
	void FASTCALL NoteLightmapChanged(FTextureInfo &Info);

	static bool FASTCALL CanDownsampleTexFormat(D3DFORMAT texFormat);
	bool FASTCALL CanGenerateMipmaps(const FCachedTexture *pBind, const FTextureInfo &Info);
	INT FASTCALL CalcTexLevelCount(const FCachedTexture *pBind, const FTextureInfo &Info);
	DWORD FASTCALL CalcTexBytes(const FCachedTexture *pBind, INT levelCount);
	bool FASTCALL GenerateMipmapLevels(FCachedTexture *pBind);
	void EvictOverBudgetTextures(void);

	void FASTCALL ConvertDXT1_DXT1(const FMipmapBase *Mip, INT Level);
	void FASTCALL ConvertDXT1_DXT3(const FMipmapBase *Mip, INT Level);
	void FASTCALL ConvertDXT35_DXT35(const FMipmapBase *Mip, INT Level);
	void FASTCALL ConvertDXT_RGBA8888(const FMipmapBase *Mip, INT Level, DWORD dxtType);
	void FASTCALL ConvertP8_RGBA8888(const FMipmapBase *Mip, const FColor *Palette, INT Level);
	void FASTCALL ConvertP8_RGBA8888_NoStep(const FMipmapBase *Mip, const FColor *Palette, INT Level);
#ifdef UTGLR_INCLUDE_SSE_CODE
	void FASTCALL ConvertP8_RGBA8888_NoStep_SSSE3(const FMipmapBase *Mip, const FColor *Palette, INT Level);
#endif //UTGLR_INCLUDE_SSE_CODE
	void FASTCALL ConvertP8_RGB565(const FMipmapBase *Mip, const FColor *Palette, INT Level);
	void FASTCALL ConvertP8_RGB565_NoStep(const FMipmapBase *Mip, const FColor *Palette, INT Level);
	void FASTCALL ConvertP8_RGBA5551(const FMipmapBase *Mip, const FColor *Palette, INT Level);
	void FASTCALL ConvertP8_RGBA5551_NoStep(const FMipmapBase *Mip, const FColor *Palette, INT Level);
	void FASTCALL ConvertBGRA7777_BGRA8888(const FMipmapBase *Mip, INT Level);
	void FASTCALL ConvertBGRA7777_BGRA8888_NoClamp(const FMipmapBase *Mip, INT Level);

	inline void FASTCALL SetBlend(DWORD PolyFlags) {
#ifdef UTGLR_RUNE_BUILD
		if (PolyFlags & PF_AlphaBlend) {
			if (!(PolyFlags & PF_Masked)) {
				PolyFlags |= PF_Occlude;
			} else {
				PolyFlags &= ~PF_Masked;
			}
		} else
#endif
			if (!(PolyFlags & (PF_Translucent | PF_Modulated | PF_Highlighted))) {
			PolyFlags |= PF_Occlude;
		} else if (PolyFlags & PF_Translucent) {
			PolyFlags &= ~PF_Masked;
		}

#ifdef UTGLR_RUNE_BUILD
		DWORD blendFlags = PolyFlags & (PF_Translucent | PF_Modulated | PF_Invisible | PF_Occlude | PF_Masked | PF_Highlighted | PF_RenderFog | PF_AlphaBlend);
#else
		DWORD blendFlags = PolyFlags & (PF_Translucent | PF_Modulated | PF_Invisible | PF_Occlude | PF_Masked | PF_Highlighted | PF_RenderFog);
#endif
		if ((m_curBlendFlags != blendFlags) || m_blendStateInvalid) {
			SetBlendNoCheck(blendFlags);
		}
	}
	void EnableAlphaToCoverageNoCheck(void);
	void DisableAlphaToCoverageNoCheck(void);
	void FASTCALL SetBlendNoCheck(DWORD blendFlags);

	inline void FASTCALL SetTexEnv(INT texUnit, DWORD PolyFlags) {
		DWORD texEnvFlags = PolyFlags & (PF_Modulated | PF_Highlighted | PF_Memorized | PF_FlatShaded);
		if ((texEnvFlags & (PF_Modulated | PF_Highlighted | PF_Memorized)) == 0) {
			texEnvFlags |= PF_Modulated;
		}
		if (m_curTexEnvFlags[texUnit] != texEnvFlags) {
			SetTexEnvNoCheck(texUnit, texEnvFlags);
		}
	}
	void InitOrInvalidateTexEnvState(void);
	void FASTCALL SetTexLODBiasState(INT TMUnits);
	void FASTCALL SetTexMaxAnisotropyState(INT TMUnits);
	void FASTCALL SetTexEnvNoCheck(DWORD texUnit, DWORD texEnvFlags);

	inline void FASTCALL SetTexFilter(DWORD texNum, BYTE texFilterParams) {
		if (m_curTexStageParams[texNum].filter != texFilterParams) {
			SetTexFilterNoCheck(texNum, texFilterParams);
		}
	}
	void FASTCALL SetTexFilterNoCheck(DWORD texNum, BYTE texFilterParams);

	inline void FASTCALL SetDepthBias(FLOAT depthBias) {
		if (m_curDepthBias != depthBias) {
			m_curDepthBias = depthBias;
			m_d3dDevice->SetRenderState(D3DRS_DEPTHBIAS, *(DWORD *)&depthBias);
		}
	}

	inline void FASTCALL SetSlopeScaleDepthBias(FLOAT slopeScaleDepthBias) {
		if (m_curSlopeScaleDepthBias != slopeScaleDepthBias) {
			m_curSlopeScaleDepthBias = slopeScaleDepthBias;
			m_d3dDevice->SetRenderState(D3DRS_SLOPESCALEDEPTHBIAS, *(DWORD *)&slopeScaleDepthBias);
		}
	}

	/*
	In practice a mover, and since movers aren't CSG'd against the world the geometry buried
	beneath one still competes for the same pixels, so the owner is recovered from bits packed
	into the lightmap's cache id and anything that cannot be classified counts as world, which is
	the safe way round, a world surface wrongly called a mover being biased away from the camera
	and taking the z-fight with it.
	*/
	inline bool FASTCALL IsNonWorldModelSurface(const FSurfaceInfo &Surface) {
		if ((Surface.LightMap == NULL) || (Surface.Level == NULL) || (Surface.Level->Model == NULL)) {
			return false;
		}
		DWORD surfaceModelIndex = (DWORD)((QWORD)Surface.LightMap->CacheID >> 32);
		if (surfaceModelIndex == 0) {
			return false;
		}
		if (surfaceModelIndex == (DWORD)Surface.Level->Model->GetIndex()) {
			return false;
		}
		return (surfaceModelIndex != (DWORD)Surface.Level->GetIndex());
	}


	inline void SetDefaultTextureState(void) {
		if (m_texEnableBits != 0x1) {
			DisableSubsequentTextures(1);
		}

		return;
	}

	inline void FASTCALL DisableSubsequentTextures(DWORD firstTexUnit) {
		DWORD texUnit;
		DWORD texBit = 1U << firstTexUnit;

		for (texUnit = firstTexUnit; texBit <= m_texEnableBits; texUnit++, texBit <<= 1) {
			if (texBit & m_texEnableBits) {
				m_texEnableBits -= texBit;

				m_curTexEnvFlags[texUnit] = 0;

				m_d3dDevice->SetTextureStageState(texUnit, D3DTSS_COLOROP, D3DTOP_DISABLE);
				m_d3dDevice->SetTextureStageState(texUnit, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
			}
		}

		return;
	}

	inline void SetDefaultStreamState(void) {
		if (m_curVertexDecl != m_standardNTextureVertexDecl[0]) {
			SetVertexDeclNoCheck(m_standardNTextureVertexDecl[0]);
		}

		IDirect3DVertexShader9 *vertexShader = (UseFragmentProgram) ? m_vpDefaultRenderingState : NULL;
		if (m_curVertexShader != vertexShader) {
			SetVertexShaderNoCheck(vertexShader);
		}

		IDirect3DPixelShader9 *pixelShader = (UseFragmentProgram) ? m_fpDefaultRenderingState : NULL;
		if (m_curPixelShader != pixelShader) {
			SetPixelShaderNoCheck(pixelShader);
		}
	}
	inline void FASTCALL SetStreamState(IDirect3DVertexDeclaration9 *vertexDecl, IDirect3DVertexShader9 *vertexShader, IDirect3DPixelShader9 *pixelShader) {
		if (m_curVertexDecl != vertexDecl) {
			SetVertexDeclNoCheck(vertexDecl);
		}
		if (m_curVertexShader != vertexShader) {
			SetVertexShaderNoCheck(vertexShader);
		}
		if (m_curPixelShader != pixelShader) {
			SetPixelShaderNoCheck(pixelShader);
		}
	}
	void FASTCALL SetVertexDeclNoCheck(IDirect3DVertexDeclaration9 *vertexDecl);
	void FASTCALL SetVertexShaderNoCheck(IDirect3DVertexShader9 *vertexShader);
	void FASTCALL SetPixelShaderNoCheck(IDirect3DPixelShader9 *pixelShader);

	inline void SetDefaultProjectionState(void) {
		if (m_nearZRangeHackProjectionActive) {
			SetProjectionStateNoCheck(false);
		}
	}

	inline void FASTCALL SetProjectionState(bool requestNearZRangeHackProjection) {
		if (requestNearZRangeHackProjection != m_nearZRangeHackProjectionActive) {
			SetProjectionStateNoCheck(requestNearZRangeHackProjection);
		}
	}

	inline void SetDefaultAAState(void) {
		if (m_defAAEnable != m_curAAEnable) {
			SetAAStateNoCheck(m_defAAEnable);
		}
	}
	inline void SetDisabledAAState(void) {
		if (m_curAAEnable && m_usingAA) {
			SetAAStateNoCheck(false);
		}
	}
	void FASTCALL SetAAStateNoCheck(bool AAEnable);

	bool FASTCALL LoadVertexProgram(IDirect3DVertexShader9 **, const DWORD *, const TCHAR *);
	bool FASTCALL LoadFragmentProgram(IDirect3DPixelShader9 **, const DWORD *, const TCHAR *);

	bool InitializeFragmentPrograms(void);
	void TryInitializeFragmentProgramMode(void);
	void ShutdownFragmentProgramMode(void);

	void FASTCALL SetProjectionStateNoCheck(bool);
	void SetOrthoProjection(void);

	inline void RenderPasses(void) {
		if (m_rpPassCount != 0) {
			RenderPassesExec();
		}
	}
	inline void FASTCALL RenderPasses_SingleOrDualTextureAndDetailTexture(FTextureInfo &DetailTextureInfo) {
		RenderPassesExec_SingleOrDualTextureAndDetailTexture(DetailTextureInfo);
	}

	//Both collection paths.
	inline void FASTCALL StageComplexSurfaceLayer(DWORD layer, FTextureInfo *Info, DWORD PolyFlags,
		FLOAT PanBias, bool bSevenBitMap) {
		FGLRenderPass::FGLSinglePass &pass = MultiPass.TMU[layer];

		pass.Info = Info;
		pass.PolyFlags = PolyFlags;
		pass.PanBias = PanBias;
		pass.bSevenBitMap = bSevenBitMap;
		pass.pPlacement = NULL;
	}

	inline void FASTCALL AddRenderPass(FTextureInfo *Info, DWORD PolyFlags, FLOAT PanBias, bool bSevenBitMap) {
		INT rpPassCount = m_rpPassCount;

		StageComplexSurfaceLayer((DWORD)rpPassCount, Info, PolyFlags, PanBias, bSevenBitMap);

		//Capped at one pass.
		rpPassCount++;
		m_rpPassCount = rpPassCount;
		if (rpPassCount >= m_rpTMUnits) {
			//Never zero here.
			RenderPassesExec();
		}
	}

	void RenderPassesExec(void);
	void FASTCALL RenderPassesExec_SingleOrDualTextureAndDetailTexture(FTextureInfo &DetailTextureInfo);

	void SetRenderPassBlend(void);
	void RenderPassesNoCheckSetup(void);
	void RenderPassesNoCheckSetup_FP(void);

	//False means no batch.
	bool FASTCALL TryBatchComplexSurface(FSurfaceInfo &Surface, const FSurfaceFacet &Facet, bool biasDepth);
	IDirect3DPixelShader9 *FASTCALL SelectComplexSurfacePixelShader(DWORD numLayers, const DWORD *pLayerFlags, DWORD sevenBitMask) const;
	//One bit per layer, built up front.
	inline DWORD FASTCALL ComposeSevenBitMask(DWORD numLayers) const {
		DWORD mask = 0;
		for (DWORD i = 0; i < numLayers; i++) {
			if (MultiPass.TMU[i].bSevenBitMap) {
				mask |= 1U << i;
			}
		}
		return mask;
	}
	void FASTCALL BindComplexSurfaceLayer(DWORD texUnit);
	void FASTCALL SetSevenBitMapSizes(DWORD numLayers);
	void FASTCALL BindAtlasPage(DWORD texUnit, IDirect3DTexture9 *pTexObj, QWORD pageID);
	void FASTCALL AppendBatchedComplexSurfaceChunk(DWORD numLayers);
	void FASTCALL LockComplexSurfaceBatchBuffers(DWORD numLayers);
	void UnlockComplexSurfaceBatchBuffers(void);
	INT FASTCALL BuildBatchedComplexSurfaceIndices(WORD *pIndex, DWORD vertexOffset);
	void FASTCALL RenderPassesNoCheckSetup_SingleOrDualTextureAndDetailTexture(FTextureInfo &);
	void FASTCALL RenderPassesNoCheckSetup_SingleOrDualTextureAndDetailTexture_FP(FTextureInfo &);

	INT FASTCALL BufferStaticComplexSurfaceGeometry(const FSurfaceFacet &, FSavedPoly *&pPoly);
	INT FASTCALL BufferStaticComplexSurfaceGeometry_VP(const FSurfaceFacet &, FSavedPoly *&pPoly);
	void DrawComplexSurfaceChunk(FSurfaceInfo &Surface, const FSurfaceFacet &Facet, INT numVerts);
	void DrawComplexSurfaceSelection(FSurfaceInfo &Surface, const FSurfaceFacet &Facet);
	void BuildComplexSurfaceIndices(void);
	void DrawComplexSurfacePolys(void);
	DWORD FASTCALL BufferDetailTextureData(FLOAT);
#ifdef UTGLR_INCLUDE_SSE_CODE
	DWORD FASTCALL BufferDetailTextureData_SSE2(FLOAT);
#endif //UTGLR_INCLUDE_SSE_CODE

	void FASTCALL DrawDetailTexture(FTextureInfo &, bool);
	void FASTCALL DrawDetailTexture_FP(FTextureInfo &);

	void FASTCALL BufferAdditionalClippedVerts(FTransTexture **Pts, INT NumPts);

	void OpenGouraudPolygonBatch(FTextureInfo &Info, DWORD PolyFlags, DWORD PolyFlags2, INT ringCost);


	//Deferred recording.
	//@{
public:
	inline bool DeferredActive(void) const {
		return m_deferredReady && !m_deferredGaveUp && (DeferredRecording != 0) && UseFragmentProgram && !m_frameSkipped;
	}

	void DeferredBuildFailed(void);

private:
	bool m_isUncountedDevice;
	//Set on first shutdown.
	bool m_exited;
	bool m_deferredReady;
	//Sticky across a reset.
	INT m_deferredBuildFailCount;
	bool m_deferredGaveUp;
	bool m_flushingDeferred;
	FJobSystem *m_pJobSystem;
	FFrameArena m_frameArena;

	std::vector<FDrawCmd> m_drawCmds;
	std::vector<FDrawRun> m_drawRuns;
	std::vector<FDrawRun> m_passRuns;
	std::vector<FBuildJob> m_buildJobs;
	std::vector<FRecordedTexture> m_recordedTextures;
	//A hard limit, since a vector reallocating here could throw where the recorders are
	//contracted to fail into the immediate path, and the cost is paid per device with the editor
	//running several at once, so both counts are reserved up front and a recorder that reaches
	//one declines the command and does not grow.
	enum {
		MAX_RECORDED_CMDS = 8192,
		MAX_RECORDED_TEXTURES = 2048
	};
	//Last binding recorded per texture unit.
	INT m_lastRecordedTexIndex[MAX_TMUNITS];
	void ClearRecordedTextures(void);

	DWORD m_csSelectedLayers;

	IDirect3DVertexBuffer9 *m_d3dQuadCornerBuffer; //4 corners, then 2 line ends
	IDirect3DVertexBuffer9 *m_d3dInstanceBuffer;
	IDirect3DVertexDeclaration9 *m_quadInstanceVertexDecl;
	IDirect3DVertexShader9 *m_vpQuadInstance;
	bool m_instancingAvailable;
	//Set when instance setup failed for a reason only a new device can change.
	bool m_instancingGaveUp;
	INT m_curInstanceBufferPos;
	bool m_instanceBufferNeedsDiscard;

	enum {
		//D3D9 needs the indexed geometry on stream 0 whenever another stream carries instance data.
		INSTANCE_CORNER_STREAM = 0,
		INSTANCE_DATA_STREAM = 2 + MAX_TMUNITS,
		//In one buffer.
		INSTANCE_CORNER_COUNT = 6,
		INSTANCE_QUAD_FIRST_CORNER = 0,
		INSTANCE_LINE_FIRST_CORNER = 4,
		INSTANCE_INDEX_RESERVE = 8,
		INSTANCE_RING_SIZE = 65536,
		//Shortest run worth instancing.
		INSTANCE_MIN_RUN = 16
	};
	//Must not land on a permanently bound stream, meaning the secondary colour buffer or the
	//texture coordinate buffers, and it is written as a derivation and asserted because the two
	//only agree at the current texture unit count, a literal here having to be revisited every
	//time that count moves and being silently wrong until somebody noticed.
	static_assert(INSTANCE_DATA_STREAM > (1 + MAX_TMUNITS), "INSTANCE_DATA_STREAM overlaps a permanently bound vertex stream");

	void InitDeferred(void);
	void ShutdownDeferred(void);
	void InitInstanceResources(void);
	void ReleaseInstanceResources(void);
	void StartJobSystem(void);

	bool FASTCALL RecordComplexSurface(FSurfaceInfo &Surface, const FSurfaceFacet &Facet, bool biasDepth);
	bool FASTCALL RecordGouraudPolygon(FTextureInfo &Info, FTransTexture **Pts, INT NumPts, DWORD PolyFlags, DWORD PolyFlags2);
	//Three variants, because three things read them.
	bool FASTCALL RecordQuad(EDrawCmdType type, FTextureInfo *pInfo, DWORD keyPolyFlags,
		DWORD bindPolyFlags, DWORD blendPolyFlags, DWORD PolyFlags2, const FQuadCmd &quad);

	//Returns -1 when full.
	INT FASTCALL RecordTexture(DWORD texUnit, FTextureInfo &Info, DWORD PolyFlags, FLOAT PanBias, const FLightmapAtlas::FPlacement *pPlacement);

	void FlushDeferred(void);
	void FASTCALL BuildRunOffsets(DWORD firstRun, DWORD numRuns, DWORD passVertexBase, DWORD passIndexBase);
	DWORD FASTCALL PartitionBuildJobs(DWORD firstCmd, DWORD numCmds);
	void FASTCALL SubmitRun(const FDrawRun &run, DWORD passVertexBase, DWORD unitIndexBase);

	static void BuildJobEntry(void *pContext, int jobIndex);
	void FASTCALL RunBuildJob(const FBuildContext &ctx, const FBuildJob &job);
	void FASTCALL FillComplexSurfaceCmd(const FBuildContext &ctx, const FDrawCmd &cmd, DWORD firstPoly, DWORD numPolys);
	void FASTCALL FillGouraudPolyCmd(const FBuildContext &ctx, const FDrawCmd &cmd);
	void FASTCALL FillQuadCmd(const FBuildContext &ctx, const FDrawCmd &cmd);
	//@}
};

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
