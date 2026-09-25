/*=============================================================================
	OpenGL.h: Unreal OpenGL support header.
	Portions copyright 1999 Epic Games, Inc. All Rights Reserved.

	UOpenGLRenderDevice and nothing else.
=============================================================================*/

#pragma once

//From ..\..\Shared, with the D3D9 renderer.
#include "buildconfig.h"


#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <tchar.h>


#pragma warning(push)
#pragma warning(disable : 4663) //obsolete VC6 template syntax.
#pragma warning(disable : 4018) //signed/unsigned mismatch.
#pragma warning(disable : 4244) //conversion, possible loss of data.
#pragma warning(disable : 4245) //signed/unsigned conversion.
#pragma warning(disable : 4146) //unary minus on unsigned.

#include <map>
#include <vector>

#pragma warning(pop)

#include "c_gclip.h"

#include "Core/Compiler.h"

#include "Core/Debug.h"


//Churns tree nodes from inside draw calls.
#define UTGLR_RBTREE_REUSE_NODES
#include "c_rbtree.h"


/*-----------------------------------------------------------------------------
	Globals.
-----------------------------------------------------------------------------*/

#include "Core/Limits.h"


/*-----------------------------------------------------------------------------
	OpenGL1xDrv.
-----------------------------------------------------------------------------*/

#include "Texture/TextureCacheTypes.h"

#include "Geometry/VertexFormats.h"
#include "Device/PixelFormatTypes.h"


//An OpenGL rendering device attached to a viewport.
class UOpenGLRenderDevice : public URenderDevice {
//Deus Ex and the 226 generation take three arguments.
#if defined UTGLR_DX_BUILD || defined UTGLR_OLD_URENDERDEVICE
	DECLARE_CLASS(UOpenGLRenderDevice, URenderDevice, CLASS_Config)
#else
	DECLARE_CLASS(UOpenGLRenderDevice, URenderDevice, CLASS_Config, OpenGL1xDrv)
#endif

	int dbgPrintf(const char *format, ...);

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

	//Bounded log buffer.
	enum { MAX_LOGGED_TILES = 64 };
	QWORD m_loggedTileIds[MAX_LOGGED_TILES];
	DWORD m_numLoggedTiles;
	void FASTCALL LogTileOnce(const FTextureInfo &Info, DWORD polyFlags, DWORD blendPolyFlags);

//Fixed texture cache ids.
#define TEX_CACHE_ID_UNUSED 0xFFFFFFFFFFFFFFFFULL
#define TEX_CACHE_ID_NO_TEX 0xFFFFFFFF00000010ULL
#define TEX_CACHE_ID_ALPHA_TEX 0xFFFFFFFF00000020ULL

	//Texture cache id flags.
	enum {
		TEX_CACHE_ID_FLAG_MASKED = 0x1,
		TEX_CACHE_ID_FLAG_16BIT = 0x2
	};

//Mask for poly flags that affect texture state.
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
		TEX_TYPE_PALETTED,
		TEX_TYPE_HAS_PALETTE,
		TEX_TYPE_NORMAL
	};
#define TEX_FLAG_NO_CLAMP 0x00000001

	struct FTexConvertCtx {
		BYTE *pCompose;
		INT stepBits;
		DWORD texWidthPow2;
		DWORD texHeightPow2;
		const FCachedTexture *pBind;
	} m_texConvertCtx;

	enum { LOCAL_TEX_COMPOSE_BUFFER_SIZE = 16384 };
	BYTE m_localTexComposeBuffer[LOCAL_TEX_COMPOSE_BUFFER_SIZE + 16];

	//Grown once and kept.
	//@{
	enum { TEX_COMPOSE_BUFFER_SLACK = 16 };
	BYTE *m_pTexComposeBuffer;
	DWORD m_texComposeBufferSize;
	//@}


	//As wide as the pointer.
	inline void *FASTCALL AlignMemPtr(void *ptr, size_t align) {
		return (void *)(((uintptr_t)ptr + (align - 1)) & ~(uintptr_t)(align - 1));
	}
	enum { VERTEX_ARRAY_ALIGN = 64 }; //Must be even multiple of 16B for SSE
	//Slack for the SSE loop.
	enum { VERTEX_ARRAY_TAIL_PADDING = 72 };

	FGLVertex *VertexArray;
	BYTE m_VertexArrayMem[(sizeof(FGLVertex) * VERTEX_ARRAY_SIZE) + VERTEX_ARRAY_ALIGN + VERTEX_ARRAY_TAIL_PADDING];

	//Tex coord streams.
	FGLTexCoord *TexCoordArray[MAX_TMUNITS];
	BYTE m_TexCoordArrayMem[MAX_TMUNITS][(sizeof(FGLTexCoord) * VERTEX_ARRAY_SIZE) + VERTEX_ARRAY_ALIGN + VERTEX_ARRAY_TAIL_PADDING];

	//Primary and secondary color.
	FGLSingleColor *SingleColorArray;
	FGLDoubleColor *DoubleColorArray;
	BYTE m_ColorArrayMem[(sizeof(FGLColorAlloc) * VERTEX_ARRAY_SIZE) + VERTEX_ARRAY_ALIGN + VERTEX_ARRAY_TAIL_PADDING];

	FGLMapDot *MapDotArray;
	BYTE m_MapDotArrayMem[(sizeof(FGLMapDot) * VERTEX_ARRAY_SIZE) + VERTEX_ARRAY_ALIGN + VERTEX_ARRAY_TAIL_PADDING];

	enum { DETAIL_TEXTURE_MAX_POLY_PTS = 32 };

	//Texture coordinates are regenerated every pass.
	enum {
		VERTEX_STREAM_VERTEX,
		VERTEX_STREAM_COLOR,
		VERTEX_STREAM_TEXCOORD0,
		NUM_VERTEX_STREAMS = VERTEX_STREAM_TEXCOORD0 + MAX_TMUNITS
	};

	//Larger than one batch.
	enum { VERTEX_STREAM_RING_VERTS = VERTEX_ARRAY_SIZE * 8 };

	//A whole vertex.
	enum { VERTEX_STREAM_POS_ALIGN = 48 };
	static_assert((VERTEX_STREAM_POS_ALIGN % sizeof(FGLVertex)) == 0,
		"the position ring offset must divide into whole vertices to be used as a first vertex");
	static_assert((VERTEX_STREAM_POS_ALIGN % 16) == 0,
		"the position ring must stay 16 byte aligned like the other streams");

	//In bytes, never elements.
	struct FGLVertexStream {
		GLuint BufferId;
		DWORD CapacityBytes;
		DWORD RingPosBytes;
		DWORD BaseByteOffset;
		const void *pCpuData;
		DWORD PendingStride;
		DWORD PendingCount;
	} m_vertexStreams[NUM_VERTEX_STREAMS];

	bool m_vboActive;
	GLuint m_curArrayBuffer;

	bool m_colorStreamIsDouble;

	//Complex surfaces fold the ring offset into the draw.
	DWORD m_vertexArrayByteOffset;

	DWORD DetailTextureIsNearArray[VERTEX_ARRAY_SIZE / 3];

	GLint MultiDrawFirstArray[VERTEX_ARRAY_SIZE / 3];
	GLsizei MultiDrawCountArray[VERTEX_ARRAY_SIZE / 3];

	DWORD m_csPolyCount;
	INT m_csPtCount;

	INT m_csBaseVertex;

	FLOAT m_csUDot;
	FLOAT m_csVDot;


	//Multi-pass rendering information.
	struct FGLRenderPass {
		struct FGLSinglePass {
			FTextureInfo *Info;
			DWORD PolyFlags;
			FLOAT PanBias;
			/*
			The TEXF_RGBA7 layers are the lightmap and the fog map, which ReduceBanding reconstructs
			with a cubic filter, and a macro texture is PF_Modulated too while the reconstruction
			reads the base level only, so the two cannot be told apart by their flags alone and the
			layer that wants reconstructing is marked where it is added and carried through the
			pass split.
			*/
			bool bSevenBitMap;
		} TMU[MAX_TMUNITS];
	} MultiPass; // vogel: MULTIPASS!!! ;)

	//Texture state cache information.
	BYTE m_texEnableBits;
	BYTE m_clientTexEnableBits;

	//Skips a redundant reselect.
	//@{
	DWORD m_curActiveTexUnit;
	DWORD m_curClientActiveTexUnit;
	//@}

	//A draw with the color array leaves this undefined.
	//@{
	FLOAT m_curColor[4];
	bool m_curColorValid;
	//@}

	//Compared by value, because panning moves these.
	//@{
	FLOAT m_curTexAttrib[MAX_TMUNITS][4];
	DWORD m_curTexAttribValidBits;
	//@}

	//The gamma pass restores it.
	GLint m_curViewport[4];

	GLuint m_vpCurrent;
	GLuint m_fpCurrent;

	bool m_allocatedShaderNames;

	//Vertex program ids.
	GLuint m_vpDefaultRenderingState;
	GLuint m_vpDefaultRenderingStateWithFog;
	GLuint m_vpDefaultRenderingStateWithLinearFog;
	GLuint m_vpComplexSurface[MAX_TMUNITS];
	GLuint m_vpComplexSurfaceSingleTextureWithPos;
	GLuint m_vpComplexSurfaceDualTextureWithPos;
	GLuint m_vpComplexSurfaceTripleTextureWithPos;

	//Fragment program ids.
	GLuint m_fpDefaultRenderingState;
	GLuint m_fpDefaultRenderingStateWithFog;
	GLuint m_fpDefaultRenderingStateWithLinearFog;
	GLuint m_fpComplexSurfaceSingleTexture;
	GLuint m_fpComplexSurfaceDualTextureModulated;
	GLuint m_fpComplexSurfaceTripleTextureModulated;
	GLuint m_fpComplexSurfaceSingleTextureWithFog;
	GLuint m_fpComplexSurfaceDualTextureModulatedWithFog;
	GLuint m_fpComplexSurfaceTripleTextureModulatedWithFog;
	GLuint m_fpDetailTexture;
	GLuint m_fpDetailTextureTwoLayer;
	GLuint m_fpSingleTextureAndDetailTexture;
	GLuint m_fpSingleTextureAndDetailTextureTwoLayer;
	GLuint m_fpDualTextureAndDetailTexture;
	GLuint m_fpDualTextureAndDetailTextureTwoLayer;

	//ReduceBanding twins of the programs above.
	//A modulated layer needs two.
	GLuint m_fpComplexSurfaceDualTextureLightRecon;
	GLuint m_fpComplexSurfaceTripleTextureLightRecon;
	GLuint m_fpComplexSurfaceSingleTextureFogRecon;
	GLuint m_fpComplexSurfaceDualTextureLightFogRecon;
	GLuint m_fpComplexSurfaceDualTextureMacroFogRecon;
	GLuint m_fpComplexSurfaceTripleTextureLightFogRecon;
	GLuint m_fpDualTextureAndDetailTextureLightRecon;
	GLuint m_fpDualTextureAndDetailTextureTwoLayerLightRecon;

	/*
	Whether all of those loaded and fit the hardware, and whether they are this frame's
	choice, with NoFiltering wanting the software and Glide look so it turns reconstruction
	off, and fit mattering because a cubic filter as four bilinear taps runs past what
	ARB_fragment_program guarantees and a driver will take such a program and emulate it, two
	flags keeping the per-frame decision free to flip without reloading anything.
	*/
	bool m_sevenBitMapReconLoaded;
	bool m_sevenBitMapRecon;
	//Handed to the reconstruction per unit.
	//The filter works in texel space.
	FLOAT m_curSevenBitMapSize[MAX_TMUNITS][4];
	DWORD m_curSevenBitMapSizeValidBits;

	//Its own program, left alone by the mode toggle.
	GLuint m_fpGammaRamp;
	GLuint m_gammaSceneTexId;
	GLuint m_gammaRampTexId;
	INT m_gammaSceneTexWidth;
	INT m_gammaSceneTexHeight;
	bool m_gammaPostProcessSupported;
	bool m_gammaRampTexOutOfDate;
	bool m_gammaRampIsIdentity;


	struct FGammaRamp {
		_WORD red[256];
		_WORD green[256];
		_WORD blue[256];
	};
	struct FByteGammaRamp {
		BYTE red[256];
		BYTE green[256];
		BYTE blue[256];
	};

#ifdef _WIN32
	//Permanent variables here.
	HGLRC m_hRC;
	HWND m_hWnd;
	HDC m_hDC;
#endif

	UBOOL WasFullscreen;

	bool m_frameRateLimitTimerInitialized;

	//A negative device count stops global cleanup.
	bool m_isUncountedDevice;

	//A second call does nothing.
	bool m_exited;

	bool m_prevSwapBuffersStatus;

	typedef rbtree<DWORD, FCachedTexture> DWORD_CTTree_t;
	typedef rbtree_allocator<DWORD_CTTree_t> DWORD_CTTree_Allocator_t;
	typedef rbtree<QWORD, FCachedTexture> QWORD_CTTree_t;
	typedef rbtree_allocator<QWORD_CTTree_t> QWORD_CTTree_Allocator_t;
	typedef rbtree_node_pool<QWORD_CTTree_t> QWORD_CTTree_NodePool_t;
	typedef _WORD TexPoolMapKey_t;
	typedef rbtree<TexPoolMapKey_t, QWORD_CTTree_NodePool_t> TexPoolMap_t;
	typedef rbtree_allocator<TexPoolMap_t> TexPoolMap_Allocator_t;

	enum { NUM_CTTree_TREES = 16 }; //Must be a power of 2
	inline DWORD FASTCALL CTZeroPrefixCacheIDSuffixToTreeIndex(DWORD CacheIDSuffix) {
		return ((CacheIDSuffix >> 12) & (NUM_CTTree_TREES - 1));
	}
	inline DWORD FASTCALL CTNonZeroPrefixCacheIDSuffixToTreeIndex(DWORD CacheIDSuffix) {
		return ((CacheIDSuffix >> 20) & (NUM_CTTree_TREES - 1));
	}
	inline _WORD FASTCALL MakeTexPoolMapKey(DWORD UBits, DWORD VBits) {
		return (((UBits << 8) | VBits) & 0xFFFF);
	}

	DWORD_CTTree_t m_localZeroPrefixBindTrees[NUM_CTTree_TREES], *m_zeroPrefixBindTrees;
	QWORD_CTTree_t m_localNonZeroPrefixBindTrees[NUM_CTTree_TREES], *m_nonZeroPrefixBindTrees;
	CCachedTextureChain m_localZeroPrefixBindChain, *m_zeroPrefixBindChain;
	CCachedTextureChain m_localNonZeroPrefixBindChain, *m_nonZeroPrefixBindChain;
	QWORD_CTTree_NodePool_t m_localNonZeroPrefixTexIdPool, *m_nonZeroPrefixTexIdPool;
	TexPoolMap_t m_localRGBA8TexPool, *m_RGBA8TexPool;

	//Aging into a pool leaves this alone.
	DWORD m_localTexCacheBytes, *m_texCacheBytes;
	bool m_texCacheBudgetUnreachable;

	INT m_localAllocatedTextures, *m_allocatedTextures;

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


	//Hardware constraints.
	struct {
		UBOOL SinglePassFog;
		UBOOL SinglePassDetail;
		UBOOL UseFragmentProgram;
		//Clamped against a limit that depends on fragment program use.
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
	UBOOL UseZTrick;
	UBOOL UseBGRATextures;
	UBOOL UseMultiTexture;
	UBOOL UsePalette;
	UBOOL ShareLists;
	UBOOL AlwaysMipmap;
	UBOOL UsePrecache;
	UBOOL UseTrilinear;
	UBOOL UseVertexSpecular;
	UBOOL UseAlphaPalette;
	UBOOL UseS3TC;
	UBOOL Use16BitTextures;
	UBOOL NoFiltering;
	INT DetailMax;
	UBOOL UseDetailAlpha;
	UBOOL DetailClipping;
	UBOOL ColorizeDetailTextures;
	//The 226 generation's base class has no such field.
#ifdef UTGLR_OLD_URENDERDEVICE
	BITFIELD DetailTextures;
#endif
	//Pinned to 0 on Klingon.
#ifdef UTGLR_KLINGON_BUILD
	BITFIELD SupportsTC;
#endif
	UBOOL SinglePassFog;
	UBOOL SinglePassDetail;
	UBOOL UseSSE;
	UBOOL UseSSE2;
	UBOOL UseTexIdPool;
	UBOOL UseTexPool;
	UBOOL CacheStaticMaps;
	INT TexCacheBudgetMegs;
	INT DynamicTexIdRecycleLevel;
	UBOOL TexDXT1ToDXT3;
	UBOOL UseMultiDrawArrays;
	//Applies on restart.
	UBOOL UseVBO;
	UBOOL UseFragmentProgram;
	INT SwapInterval;
	INT FrameRateLimit;
	//DOUBLE where it is needed.
	//Some SDKs hand back an absolute 64-bit cycle count.
#if defined UTGLR_DX_BUILD || defined UTGLR_RUNE_BUILD || defined UTGLR_OLD_URENDERDEVICE || defined UTGLR_UNREAL_227_BUILD
	DOUBLE m_prevFrameTimestamp;
#else
	FTime m_prevFrameTimestamp;
#endif
	UBOOL SmoothMaskedTextures;

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
	INT BufferedVerts;

	UBOOL BufferTileQuads;
	INT BufferedTileVerts;

	INT BufferedLineVerts;
	INT BufferedPointVerts;


	//Previous lock variables.
	BITFIELD PL_DetailTextures;
	UBOOL PL_OneXBlending;
	INT PL_MaxLogUOverV;
	INT PL_MaxLogVOverU;
	INT PL_MinLogTextureSize;
	INT PL_MaxLogTextureSize;
	UBOOL PL_NoFiltering;
	UBOOL PL_AlwaysMipmap;
	UBOOL PL_UseTrilinear;
	UBOOL PL_Use16BitTextures;
	UBOOL PL_TexDXT1ToDXT3;
	INT PL_MaxAnisotropy;
	UBOOL PL_SmoothMaskedTextures;
	FLOAT PL_LODBias;
	UBOOL PL_UsePalette;
	UBOOL PL_UseAlphaPalette;
	UBOOL PL_UseDetailAlpha;
	UBOOL PL_SinglePassDetail;
	UBOOL PL_UseFragmentProgram;
	UBOOL PL_UseSSE;
	UBOOL PL_UseSSE2;
	UBOOL PL_UseHardwareGamma;
	UBOOL PL_ReduceBanding;
	INT PL_SwapInterval;

	bool m_gammaRampInEffect;
	FLOAT SavedGammaCorrection;
	FLOAT PL_ClientBrightness;
	FLOAT PL_GammaOffset;
	FLOAT PL_GammaOffsetRed;
	FLOAT PL_GammaOffsetGreen;
	FLOAT PL_GammaOffsetBlue;
	INT PL_Brightness;

	DWORD m_numDepthBits;

	INT m_rpPassCount;
	INT m_rpTMUnits;
	bool m_rpForceSingle;
	bool m_rpMasked;
	bool m_rpSetDepthEqual;

	void (UOpenGLRenderDevice::*m_pRenderPassesNoCheckSetupProc)(void);
	void (FASTCALL UOpenGLRenderDevice::*m_pRenderPassesNoCheckSetup_SingleOrDualTextureAndDetailTextureProc)(FTextureInfo &);

	DWORD (FASTCALL UOpenGLRenderDevice::*m_pBufferDetailTextureDataProc)(FLOAT);

	//Hit info.
	BYTE *m_HitData;
	INT *m_HitSize;
	INT m_HitBufSize;
	INT m_HitCount;
	CGClip m_gclip;


	DWORD m_currentFrameCount;

	//Lock variables.
	UBOOL ZTrickToggle;
	INT ZTrickFunc;
	FPlane FlashScale, FlashFog;
	FLOAT m_RProjZ, m_Aspect;
	FLOAT m_RFX2, m_RFY2;

	//Dimensions alone will not do.
	//A mirror can sit at its parent's position.
	struct FSceneNodeKey {
		INT X, Y, XB, YB;
		FLOAT FX, FY;
		FLOAT FX2, FY2;
		FLOAT RProjZ;
		FLOAT FovAngle;
		//An unannounced switch would keep the previous matrix.
		INT IsOrtho;
		//Read while hit testing.
		INT HitX, HitXL, HitY, HitYL;
	};
	//Compared bytewise, so no padding holes.
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
		//Read by the ortho fallback.
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
	bool m_defAAEnable;
	INT m_initNumAASamples;

	enum {
		PF2_NEAR_Z_RANGE_HACK = 0x01
	};
	DWORD m_curBlendFlags;

	//A flag of its own, because group changes come from an XOR.
	bool m_blendStateInvalid;

	DWORD m_smoothMaskedTexturesBit;
	bool m_useAlphaToCoverageForMasked;
	bool m_alphaToCoverageEnabled;
	DWORD m_curPolyFlags;
	DWORD m_curPolyFlags2;

	//Detail texture clipping pulls toward the camera.
	//The mover depth bias pushes away.
	//Signed per frame.
	//@{
	FLOAT m_curPolyOffsetFactor;
	FLOAT m_curPolyOffsetUnits;
	bool m_polyOffsetEnabled;
	FLOAT m_detailPolyOffsetFactor, m_detailPolyOffsetUnits;
	FLOAT m_moverPolyOffsetFactor, m_moverPolyOffsetUnits;
	//@}
	INT m_bufferActorTrisCutoff;

	//Held back and replayed back to front once the run ends.
	//The engine orders models by one depth per actor.
	//@{
	struct FDeferredGouraudPoly {
		FLOAT Depth; //The sort key.
		DWORD PolyFlags;
		DWORD PolyFlags2; //Decided on submission.
		INT TexIndex;
		INT FirstPt;
		INT NumPts;
#ifdef UTGLR_RUNE_BUILD
		BYTE Alpha; //Rune's per object alpha.
#endif
	};
	FDeferredGouraudPoly m_deferredGouraudPolys[MAX_DEFERRED_GP_POLYS];
	INT m_deferredGouraudOrder[MAX_DEFERRED_GP_POLYS];
	INT m_deferredGouraudOrderScratch[MAX_DEFERRED_GP_POLYS];
	FTransTexture m_deferredGouraudPts[MAX_DEFERRED_GP_PTS];
	FTransTexture *m_deferredGouraudPtPtrs[MAX_DEFERRED_GP_PTS];
	//Copied, because the source does not outlive the call.
	struct FDeferredGouraudTexture {
		FTextureInfo Info;
		DWORD PolyFlags;
		//Deciding it again at replay would need lost image data.
		bool Is16Bit;
	};
	FDeferredGouraudTexture m_deferredGouraudTextures[MAX_DEFERRED_GP_TEXTURES];
	INT m_numDeferredGouraudPolys;
	INT m_numDeferredGouraudPts;
	INT m_numDeferredGouraudTextures;
	//Compared, never dereferenced.
	FSceneNode *m_deferredGouraudFrame;
	bool m_replayingDeferredGouraudPolys;
	DWORD m_replayPolyFlags2;
	bool m_replayIs16Bit;
#ifdef UTGLR_RUNE_BUILD
	BYTE m_replayAlpha;
#endif

	//Whether a polygon takes the near-clip projection.
	//Reserved for the weapon, so it cannot clip into walls.
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

	//Too wide for the store.
	bool DeferGouraudPolygon(FSceneNode *Frame, FTextureInfo &Info, FTransTexture **Pts, INT NumPts, DWORD PolyFlags);
	void SortDeferredGouraudPolys(INT numPolys);
	void EndDeferredGouraudPolysNoCheck(void);
	inline void EndDeferredGouraudPolys(void) {
		if (m_numDeferredGouraudPolys > 0) {
			EndDeferredGouraudPolysNoCheck();
		}
	}
	//Dimensions alone miss a child node.
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

	//Must mirror the blend setup's precedence exactly.
	static inline bool FASTCALL IsBlendedGeometry(DWORD PolyFlags) {
		if (PolyFlags & PF_Occlude) {
			return false;
		}
#ifdef UTGLR_RUNE_BUILD
		if (PolyFlags & PF_AlphaBlend) {
			//Occluded unless also masked, when the mask is dropped.
			return (PolyFlags & PF_Masked) != 0;
		}
#endif
		return (PolyFlags & (PF_Translucent | PF_Modulated | PF_Highlighted)) != 0;
	}
	//@}

	FLOAT m_complexSurfaceColor3f_1f[4];
	FLOAT m_detailTextureColor3f_1f[4];
	DWORD m_detailTextureColor4ub;

	enum {
		CF_COLOR_ARRAY = 0x01,
		CF_DUAL_COLOR_ARRAY = 0x02,
		CF_COLOR_SUM = 0x04
	};
	BYTE m_currentColorFlags;
	BYTE m_requestedColorFlags;

	BYTE m_gpAlpha;
	bool m_gpFogEnabled;

	DWORD m_curTexEnvFlags[MAX_TMUNITS];
	FTexInfo TexInfo[MAX_TMUNITS];

	void(FASTCALL *m_pBuffer3BasicVertsProc)(UOpenGLRenderDevice *, FTransTexture **);
	void(FASTCALL *m_pBuffer3ColoredVertsProc)(UOpenGLRenderDevice *, FTransTexture **);
	void(FASTCALL *m_pBuffer3FoggedVertsProc)(UOpenGLRenderDevice *, FTransTexture **);

	void(FASTCALL *m_pBuffer3VertsProc)(UOpenGLRenderDevice *, FTransTexture **);

	GLuint m_noTextureId;
	GLuint m_alphaTextureId;

	static DWORD_CTTree_t m_sharedZeroPrefixBindTrees[NUM_CTTree_TREES];
	static QWORD_CTTree_t m_sharedNonZeroPrefixBindTrees[NUM_CTTree_TREES];
	static CCachedTextureChain m_sharedZeroPrefixBindChain;
	static CCachedTextureChain m_sharedNonZeroPrefixBindChain;
	static QWORD_CTTree_NodePool_t m_sharedNonZeroPrefixTexIdPool;
	static TexPoolMap_t m_sharedRGBA8TexPool;
	static DWORD m_sharedTexCacheBytes;
	static INT m_sharedAllocatedTextures;
	static INT NumDevices;
	static INT LockCount;

#ifdef _WIN32
	static HGLRC hCurrentRC;
	static HMODULE hModuleGlMain;
	static TArray<HGLRC> AllContexts;
#else
	static UBOOL GLLoaded;
#endif

	static bool g_gammaFirstTime;
	static bool g_haveOriginalGammaRamp;
#ifdef _WIN32
	static FGammaRamp g_originalGammaRamp;
#endif

//OpenGL 1.x function pointers for the remaining subset.
#define GL1_PROC(ret, func, params) static ret(STDCALL *func) params;
#include "Device/OpenGL1Funcs.h"
#undef GL1_PROC

//OpenGL extension function pointers.
#define GL_EXT_NAME(name) static bool SUPPORTS##name;
#define GL_EXT_PROC(ext, ret, func, params) static ret(STDCALL *func) params;
#include "Device/OpenGLExtFuncs.h"
#undef GL_EXT_NAME
#undef GL_EXT_PROC


#ifdef RGBA_MAKE
#undef RGBA_MAKE
#endif
#if __INTEL_BYTE_ORDER__
	static inline DWORD RGBA_MAKE(BYTE r, BYTE g, BYTE b, BYTE a) { // vogel: I hate macros...
		return (a << 24) | (b << 16) | (g << 8) | r;
	} // vogel: ... and macros hate me
#else
	static inline DWORD RGBA_MAKE(BYTE r, BYTE g, BYTE b, BYTE a) {
		return (r << 24) | (g << 16) | (b << 8) | a;
	}
#endif

#if defined _WIN32 && defined UTGLR_USE_ASM_CODE
	static inline DWORD FPlaneTo_RGB_A255(const FPlane *pPlane) {
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
		return RGBA_MAKE(iR, iG, iB, 255);
	}
#else
	static inline DWORD FASTCALL FPlaneTo_RGB_A255(const FPlane *pPlane) {
		return RGBA_MAKE(
			appRound(pPlane->X * 255.0f),
			appRound(pPlane->Y * 255.0f),
			appRound(pPlane->Z * 255.0f),
			255);
	}
#endif

#if defined _WIN32 && defined UTGLR_USE_ASM_CODE
	static inline DWORD FPlaneTo_RGBClamped_A255(const FPlane *pPlane) {
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
		return RGBA_MAKE(Clamp(iR, 0, 255), Clamp(iG, 0, 255), Clamp(iB, 0, 255), 255);
	}
#else
	static inline DWORD FASTCALL FPlaneTo_RGBClamped_A255(const FPlane *pPlane) {
		return RGBA_MAKE(
			Clamp(appRound(pPlane->X * 255.0f), 0, 255),
			Clamp(appRound(pPlane->Y * 255.0f), 0, 255),
			Clamp(appRound(pPlane->Z * 255.0f), 0, 255),
			255);
	}
#endif

#if defined _WIN32 && defined UTGLR_USE_ASM_CODE
	static inline DWORD FPlaneTo_RGB_A0(const FPlane *pPlane) {
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
		return RGBA_MAKE(iR, iG, iB, 0);
	}
#else
	static inline DWORD FASTCALL FPlaneTo_RGB_A0(const FPlane *pPlane) {
		return RGBA_MAKE(
			appRound(pPlane->X * 255.0f),
			appRound(pPlane->Y * 255.0f),
			appRound(pPlane->Z * 255.0f),
			0);
	}
#endif

#if defined _WIN32 && defined UTGLR_USE_ASM_CODE
	static inline DWORD FPlaneTo_RGB_Aub(const FPlane *pPlane, BYTE alpha) {
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
		return RGBA_MAKE(iR, iG, iB, alpha);
	}
#else
	static inline DWORD FASTCALL FPlaneTo_RGB_Aub(const FPlane *pPlane, BYTE alpha) {
		return RGBA_MAKE(
			appRound(pPlane->X * 255.0f),
			appRound(pPlane->Y * 255.0f),
			appRound(pPlane->Z * 255.0f),
			alpha);
	}
#endif

#if defined _WIN32 && defined UTGLR_USE_ASM_CODE
	static inline DWORD FPlaneTo_RGBA(const FPlane *pPlane) {
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
		return RGBA_MAKE(iR, iG, iB, iA);
	}
#else
	static inline DWORD FASTCALL FPlaneTo_RGBA(const FPlane *pPlane) {
		return RGBA_MAKE(
			appRound(pPlane->X * 255.0f),
			appRound(pPlane->Y * 255.0f),
			appRound(pPlane->Z * 255.0f),
			appRound(pPlane->W * 255.0f));
	}
#endif

#if defined _WIN32 && defined UTGLR_USE_ASM_CODE
	static inline DWORD FPlaneTo_RGBAClamped(const FPlane *pPlane) {
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
		return RGBA_MAKE(Clamp(iR, 0, 255), Clamp(iG, 0, 255), Clamp(iB, 0, 255), Clamp(iA, 0, 255));
	}
#else
	static inline DWORD FASTCALL FPlaneTo_RGBAClamped(const FPlane *pPlane) {
		return RGBA_MAKE(
			Clamp(appRound(pPlane->X * 255.0f), 0, 255),
			Clamp(appRound(pPlane->Y * 255.0f), 0, 255),
			Clamp(appRound(pPlane->Z * 255.0f), 0, 255),
			Clamp(appRound(pPlane->W * 255.0f), 0, 255));
	}
#endif

#if defined _WIN32 && defined UTGLR_USE_ASM_CODE
	static inline DWORD FPlaneTo_RGBScaled_A255(const FPlane *pPlane, FLOAT rgbScale) {
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
		return RGBA_MAKE(iR, iG, iB, 255);
	}
#else
	static inline DWORD FASTCALL FPlaneTo_RGBScaled_A255(const FPlane *pPlane, FLOAT rgbScale) {
		return RGBA_MAKE(
			appRound(pPlane->X * rgbScale),
			appRound(pPlane->Y * rgbScale),
			appRound(pPlane->Z * rgbScale),
			255);
	}
#endif

//An out-of-range component wraps modulo 256.
//Clamped like the SSE path.
#if defined _WIN32 && defined UTGLR_USE_ASM_CODE
	static inline DWORD FPlaneTo_RGBClamped_A0(const FPlane *pPlane) {
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
		return RGBA_MAKE(Clamp(iR, 0, 255), Clamp(iG, 0, 255), Clamp(iB, 0, 255), 0);
	}
#else
	static inline DWORD FASTCALL FPlaneTo_RGBClamped_A0(const FPlane *pPlane) {
		return RGBA_MAKE(
			Clamp(appRound(pPlane->X * 255.0f), 0, 255),
			Clamp(appRound(pPlane->Y * 255.0f), 0, 255),
			Clamp(appRound(pPlane->Z * 255.0f), 0, 255),
			0);
	}
#endif

#if defined _WIN32 && defined UTGLR_USE_ASM_CODE
	static inline DWORD FPlaneTo_RGBClamped_Aub(const FPlane *pPlane, BYTE alpha) {
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
		return RGBA_MAKE(Clamp(iR, 0, 255), Clamp(iG, 0, 255), Clamp(iB, 0, 255), alpha);
	}
#else
	static inline DWORD FASTCALL FPlaneTo_RGBClamped_Aub(const FPlane *pPlane, BYTE alpha) {
		return RGBA_MAKE(
			Clamp(appRound(pPlane->X * 255.0f), 0, 255),
			Clamp(appRound(pPlane->Y * 255.0f), 0, 255),
			Clamp(appRound(pPlane->Z * 255.0f), 0, 255),
			alpha);
	}
#endif

#if defined _WIN32 && defined UTGLR_USE_ASM_CODE
	static inline DWORD FPlaneTo_RGBScaledClamped_A255(const FPlane *pPlane, FLOAT rgbScale) {
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
		return RGBA_MAKE(Clamp(iR, 0, 255), Clamp(iG, 0, 255), Clamp(iB, 0, 255), 255);
	}
#else
	static inline DWORD FASTCALL FPlaneTo_RGBScaledClamped_A255(const FPlane *pPlane, FLOAT rgbScale) {
		return RGBA_MAKE(
			Clamp(appRound(pPlane->X * rgbScale), 0, 255),
			Clamp(appRound(pPlane->Y * rgbScale), 0, 255),
			Clamp(appRound(pPlane->Z * rgbScale), 0, 255),
			255);
	}
#endif


	//UObject interface.
	//Must stay user provided.
	UOpenGLRenderDevice() :
		m_isUncountedDevice(false),
		m_exited(false),
		m_pTexComposeBuffer(NULL),
		m_texComposeBufferSize(0),
		m_blendStateInvalid(false),
		m_curArrayBuffer(0) {
		//Teardown reads these ids first.
		for (unsigned int u = 0; u < NUM_VERTEX_STREAMS; u++) {
			m_vertexStreams[u].BufferId = 0;
		}
	}
	//Klingon registers a class through a plain function.
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

	void FASTCALL DbgPrintInitParam(const char *pName, INT value);
	void FASTCALL DbgPrintInitParam(const char *pName, FLOAT value);
	void FASTCALL DbgPrintInitParam(const char *pName, BITFIELD value);

#ifdef UTGLR_INCLUDE_SSE_CODE
	static bool CPU_DetectCPUID(void);
	static bool CPU_DetectSSE(void);
	static bool CPU_DetectSSE2(void);
#endif //UTGLR_INCLUDE_SSE_CODE

	void FASTCALL InitEngineFeatureFlagSafe(const TCHAR *pName, BITFIELD &flag);
	void InitEngineFeatureFlags(void);

#ifndef __UNIX__
	bool TrySetDisplayMode(INT NewX, INT NewY, INT NewColorBytes);
#endif

	void SetSwapIntervalSafe(void);

	void InitFrameRateLimitTimerSafe(void);
	void ShutdownFrameRateLimitTimer(void);

	void BuildGammaRamp(float redGamma, float greenGamma, float blueGamma, int brightness, FGammaRamp &ramp);
	void BuildGammaRamp(float redGamma, float greenGamma, float blueGamma, int brightness, FByteGammaRamp &ramp);
	void SetGamma(FLOAT GammaCorrection);
	void ResetGamma(void);

	inline bool UsingPostProcessGamma(void) const {
		return m_gammaPostProcessSupported && (UseHardwareGamma == 0);
	}
	void TryInitializeGammaPostProcess(void);
	void ShutdownGammaPostProcess(void);
	bool FASTCALL ResizeGammaSceneTextureSafe(INT SizeX, INT SizeY);
	void UpdateGammaRampTexture(void);
	void DrawGammaPostProcess(void);

	static bool FASTCALL IsGLExtensionSupported(const char *pExtensionsString, const char *pExtensionName);
	bool FASTCALL GetGL1Proc(void *&ProcAddress, const char *pName);
	bool FASTCALL GetGL1Procs(void);
	bool FASTCALL FindGLExt(const char *pName);
	void FASTCALL GetGLExtProc(void *&ProcAddress, const char *pName, const char *pSupportName, bool &Supports);
	void FASTCALL GetGLExtProcs(void);

	UBOOL FailedInitf(const TCHAR *Fmt, ...);
	void Exit();
	void ShutdownAfterError();

	UBOOL SetRes(INT NewX, INT NewY, INT NewColorBytes, UBOOL Fullscreen);
	void UnsetRes();

	void MakeCurrent(void);
	void CheckGLErrorFlag(const TCHAR *pTag);

	void ConfigValidate_RefreshDCV(void);
	void ConfigValidate_RequiredExtensions(void);
	void ConfigValidate_Main(void);

#ifdef _WIN32
	void FASTCALL InitARBPixelFormat(INT NewColorBytes, InitARBPixelFormatRet_t *pRet);
	static LRESULT CALLBACK InitARBPixelFormatWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

	void FASTCALL SetBasicPixelFormat(INT NewColorBytes);
	bool FASTCALL SetARBPixelFormat(INT NewColorBytes);
	bool FASTCALL SetAAPixelFormat(INT NewColorBytes);
#endif


#ifndef __UNIX__
	void PrintFormat(HDC hDC, INT nPixelFormat) {
		guard(UOpenGlRenderDevice::PrintFormat);
		TCHAR Flags[1024] = TEXT("");
		PIXELFORMATDESCRIPTOR pfd;
		DescribePixelFormat(hDC, nPixelFormat, sizeof(pfd), &pfd);
		if (pfd.dwFlags & PFD_DRAW_TO_WINDOW) appStrcat(Flags, TEXT(" PFD_DRAW_TO_WINDOW"));
		if (pfd.dwFlags & PFD_DRAW_TO_BITMAP) appStrcat(Flags, TEXT(" PFD_DRAW_TO_BITMAP"));
		if (pfd.dwFlags & PFD_SUPPORT_GDI) appStrcat(Flags, TEXT(" PFD_SUPPORT_GDI"));
		if (pfd.dwFlags & PFD_SUPPORT_OPENGL) appStrcat(Flags, TEXT(" PFD_SUPPORT_OPENGL"));
		if (pfd.dwFlags & PFD_GENERIC_ACCELERATED) appStrcat(Flags, TEXT(" PFD_GENERIC_ACCELERATED"));
		if (pfd.dwFlags & PFD_GENERIC_FORMAT) appStrcat(Flags, TEXT(" PFD_GENERIC_FORMAT"));
		if (pfd.dwFlags & PFD_NEED_PALETTE) appStrcat(Flags, TEXT(" PFD_NEED_PALETTE"));
		if (pfd.dwFlags & PFD_NEED_SYSTEM_PALETTE) appStrcat(Flags, TEXT(" PFD_NEED_SYSTEM_PALETTE"));
		if (pfd.dwFlags & PFD_DOUBLEBUFFER) appStrcat(Flags, TEXT(" PFD_DOUBLEBUFFER"));
		if (pfd.dwFlags & PFD_STEREO) appStrcat(Flags, TEXT(" PFD_STEREO"));
		if (pfd.dwFlags & PFD_SWAP_LAYER_BUFFERS) appStrcat(Flags, TEXT("PFD_SWAP_LAYER_BUFFERS"));
		debugf(NAME_Init, TEXT("Pixel format %i:"), nPixelFormat);
		debugf(NAME_Init, TEXT("   Flags:%s"), Flags);
		debugf(NAME_Init, TEXT("   Pixel Type: %i"), pfd.iPixelType);
		debugf(NAME_Init, TEXT("   Bits: Color=%i R=%i G=%i B=%i A=%i"), pfd.cColorBits, pfd.cRedBits, pfd.cGreenBits, pfd.cBlueBits, pfd.cAlphaBits);
		debugf(NAME_Init, TEXT("   Bits: Accum=%i Depth=%i Stencil=%i"), pfd.cAccumBits, pfd.cDepthBits, pfd.cStencilBits);
		unguard;
	}
#endif

	UBOOL Init(UViewport *InViewport, INT NewX, INT NewY, INT NewColorBytes, UBOOL Fullscreen);

	static QSORT_RETURN CDECL CompareRes(const FPlane *A, const FPlane *B) {
		return (QSORT_RETURN)(((A->X - B->X) != 0.0f) ? (A->X - B->X) : (A->Y - B->Y));
	}

	UBOOL Exec(const TCHAR *Cmd, FOutputDevice &Ar);
	void Lock(FPlane InFlashScale, FPlane InFlashFog, FPlane ScreenClear, DWORD RenderLockFlags, BYTE *InHitData, INT *InHitSize);
	void SetSceneNode(FSceneNode *Frame);
	void Unlock(UBOOL Blit);
	//One older generation's flush takes no argument.
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
	//Harry Potter only.
	INT MaxVertices();
	void DrawTriangles(FSceneNode *Frame, FTextureInfo &Info, FTransTexture **Pts, INT NumPts, _WORD *Indices, INT NumIndices, DWORD PolyFlags, FSpanBuffer *Span);

	//Indices come from game package mesh data.
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

	void ScanForDeletedTextures(void);
	void ScanForOldTextures(void);
	DWORD FASTCALL CalcTexBytes(const FCachedTexture *pBind, INT levelCount);
	void EvictOverBudgetTextures(void);

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

	//Only a caller that binds wants the 16-bit flag.
	inline QWORD FASTCALL ComposeTexCacheID(const FTextureInfo &Info, DWORD PolyFlags, bool *pIs16Bit = NULL) const {
		QWORD CacheID = Info.CacheID;

		if (pIs16Bit) {
			*pIs16Bit = false;
		}

		if ((CacheID & 0xFF) == 0xE0) {
			//A masked texture needs its own entry.
			CacheID |= (PolyFlags & PF_Masked) ? TEX_CACHE_ID_FLAG_MASKED : 0;

			//From what defer time captured.
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

		//PF_Memorized marks a 16-bit texture.
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

		SetTextureNoCheck(Tex, Info, PolyFlags);

		return;
	}

	inline void FASTCALL SetTextureNoPanBias(INT Multi, FTextureInfo &Info, DWORD PolyFlags) {
		FTexInfo &Tex = TexInfo[Multi];
		QWORD CacheID;
		DWORD DynamicPolyFlags;

		//Panning is the one difference.
		Tex.UPan = Info.Pan.X;
		Tex.VPan = Info.Pan.Y;

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

		SetTextureNoCheck(Tex, Info, PolyFlags);

		return;
	}

	inline void FASTCALL SetTexFilter(FCachedTexture *pBind, BYTE texFilter) {
		if (pBind->texParams.filter != texFilter) {
			SetTexFilterNoCheck(pBind, texFilter);
		}
	}

	FCachedTexture *FindCachedTexture(QWORD CacheID);
	QWORD_CTTree_NodePool_t::node_t *FASTCALL TryAllocFromTexPool(TexPoolMapKey_t texPoolKey);
	BYTE FASTCALL GenerateTexFilterParams(DWORD PolyFlags, FCachedTexture *pBind);
	void FASTCALL SetTexFilterNoCheck(FCachedTexture *pBind, BYTE texFilter);
	void FASTCALL SetTextureNoCheck(FTexInfo &Tex, FTextureInfo &Info, DWORD PolyFlags);
	void FASTCALL UploadTextureExec(FTextureInfo &Info, DWORD PolyFlags, FCachedTexture *pBind, bool existingBind, bool needTexAllocate);
	void FASTCALL CacheTextureInfo(FCachedTexture *pBind, const FTextureInfo &Info, DWORD PolyFlags);

	BYTE *FASTCALL GetTexComposeBuffer(DWORD sizeBytes);
	void FreeTexComposeBuffer(void);

	void FASTCALL ConvertDXT1_DXT3(const FMipmapBase *Mip, INT Level);
	void FASTCALL ConvertDXT_RGBA8888(const FMipmapBase *Mip, INT Level, DWORD dxtType);
	void FASTCALL ConvertP8_P8(const FMipmapBase *Mip, INT Level);
	void FASTCALL ConvertP8_P8_NoStep(const FMipmapBase *Mip, INT Level);
	void FASTCALL ConvertP8_RGBA8888(const FMipmapBase *Mip, const FColor *Palette, INT Level);
	void FASTCALL ConvertP8_RGBA8888_NoStep(const FMipmapBase *Mip, const FColor *Palette, INT Level);
	void FASTCALL ConvertBGRA7777_BGRA8888(const FMipmapBase *Mip, INT Level);
	void FASTCALL ConvertBGRA7777_BGRA8888_NoClamp(const FMipmapBase *Mip, INT Level);
	void FASTCALL ConvertBGRA7777_RGBA8888(const FMipmapBase *Mip, INT Level);

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
		DWORD blendFlags = PolyFlags & (PF_Translucent | PF_Modulated | PF_Invisible | PF_Occlude | PF_Masked | PF_Highlighted | PF_AlphaBlend);
#elif defined(UTGLR_UNREAL_227_BUILD)
		DWORD blendFlags = PolyFlags & (PF_Translucent | PF_Modulated | PF_Invisible | PF_Occlude | PF_Masked | PF_Highlighted | PF_AlphaBlend);
#else
		DWORD blendFlags = PolyFlags & (PF_Translucent | PF_Modulated | PF_Invisible | PF_Occlude | PF_Masked | PF_Highlighted);
#endif
		if ((m_curBlendFlags != blendFlags) || m_blendStateInvalid) {
			SetBlendNoCheck(blendFlags);
		}
	}
	void FASTCALL SetBlendNoCheck(DWORD blendFlags);
	void SetPolygonOffsetsForDepthDirection(bool reversed);

	inline void FASTCALL SetPolygonOffset(FLOAT factor, FLOAT units) {
		if ((m_curPolyOffsetFactor != factor) || (m_curPolyOffsetUnits != units)) {
			m_curPolyOffsetFactor = factor;
			m_curPolyOffsetUnits = units;
			glPolygonOffset(factor, units);
		}
	}

	inline void FASTCALL SetPolygonOffsetEnabled(bool enabled) {
		if (m_polyOffsetEnabled != enabled) {
			m_polyOffsetEnabled = enabled;
			if (enabled) {
				glEnable(GL_POLYGON_OFFSET_FILL);
			} else {
				glDisable(GL_POLYGON_OFFSET_FILL);
			}
		}
	}

	//A mover, in practice.
	//Movers are not CSG'd against the world.
	//The owner is packed into the lightmap's cache id.
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

	inline void FASTCALL SetTexEnv(DWORD texUnit, DWORD PolyFlags) {
		DWORD texEnvFlags = PolyFlags & (PF_Modulated | PF_Highlighted | PF_Memorized | PF_FlatShaded);
		if ((texEnvFlags & (PF_Modulated | PF_Highlighted | PF_Memorized)) == 0) {
			texEnvFlags |= PF_Modulated;
		}
		if (m_curTexEnvFlags[texUnit] != texEnvFlags) {
			SetTexEnvNoCheck(texUnit, texEnvFlags);
		}
	}
	void InitOrInvalidateTexEnvState(void);
	void FASTCALL SetPermanentTexEnvState(INT TMUnits);
	void FASTCALL SetTexLODBiasState(INT TMUnits);
	void FASTCALL SetTexEnvNoCheck(DWORD texUnit, DWORD texEnvFlags);


	//Every unit selection goes through these two.
	//@{
	inline void FASTCALL SetActiveTexUnit(DWORD texUnit) {
		if (m_curActiveTexUnit == texUnit) {
			return;
		}
		m_curActiveTexUnit = texUnit;
		if (SUPPORTS_GL_ARB_multitexture) {
			glActiveTextureARB(GL_TEXTURE0_ARB + texUnit);
		}
	}
	inline void FASTCALL SetClientActiveTexUnit(DWORD texUnit) {
		if (m_curClientActiveTexUnit == texUnit) {
			return;
		}
		m_curClientActiveTexUnit = texUnit;
		if (SUPPORTS_GL_ARB_multitexture) {
			glClientActiveTextureARB(GL_TEXTURE0_ARB + texUnit);
		}
	}
	//@}

	//A bound fragment program samples whatever a unit holds.
	//The bits are still tracked.
	inline bool TexEnablesIgnored(void) const {
		return m_fpCurrent != 0;
	}
	void ResyncTextureEnables(void);

	//A 3 component color is issued as 4.
	//@{
	inline void FASTCALL SetColor4f(FLOAT r, FLOAT g, FLOAT b, FLOAT a) {
		if (m_curColorValid &&
			(m_curColor[0] == r) && (m_curColor[1] == g) && (m_curColor[2] == b) && (m_curColor[3] == a)) {
			return;
		}
		m_curColor[0] = r;
		m_curColor[1] = g;
		m_curColor[2] = b;
		m_curColor[3] = a;
		m_curColorValid = true;
		glColor4f(r, g, b, a);
	}
	inline void FASTCALL SetColor3f(FLOAT r, FLOAT g, FLOAT b) {
		SetColor4f(r, g, b, 1.0f);
	}
	inline void FASTCALL SetColor4fv(const FLOAT *pColor) {
		SetColor4f(pColor[0], pColor[1], pColor[2], pColor[3]);
	}
	inline void FASTCALL SetColor3fv(const FLOAT *pColor) {
		SetColor4f(pColor[0], pColor[1], pColor[2], 1.0f);
	}
	inline void InvalidateColorShadow(void) {
		m_curColorValid = false;
	}
	//@}

	//@{
	inline void FASTCALL SetTexAttrib(DWORD texUnit) {
		const FTexInfo &Tex = TexInfo[texUnit];
		const DWORD validBit = 1U << texUnit;
		FLOAT *pCur = m_curTexAttrib[texUnit];
		if ((m_curTexAttribValidBits & validBit) &&
			(pCur[0] == Tex.UPan) && (pCur[1] == Tex.VPan) && (pCur[2] == Tex.UMult) && (pCur[3] == Tex.VMult)) {
			return;
		}
		pCur[0] = Tex.UPan;
		pCur[1] = Tex.VPan;
		pCur[2] = Tex.UMult;
		pCur[3] = Tex.VMult;
		m_curTexAttribValidBits |= validBit;
		glVertexAttrib4fARB(texUnit + 8, Tex.UPan, Tex.VPan, Tex.UMult, Tex.VMult);
	}
	//For a draw that read an enabled coordinate array.
	inline void InvalidateTexAttribShadows(void) {
		m_curTexAttribValidBits = 0;
	}
	//Environment, so a program switch leaves no stale copy.
	inline void FASTCALL SetSevenBitMapSize(DWORD texUnit) {
		const FCachedTexture *pBind = TexInfo[texUnit].pBind;
		if (pBind == NULL) {
			return;
		}

		const FLOAT width = (FLOAT)(1U << pBind->UBits);
		const FLOAT height = (FLOAT)(1U << pBind->VBits);
		const DWORD validBit = 1U << texUnit;
		FLOAT *pCur = m_curSevenBitMapSize[texUnit];
		if ((m_curSevenBitMapSizeValidBits & validBit) && (pCur[0] == width) && (pCur[1] == height)) {
			return;
		}
		pCur[0] = width;
		pCur[1] = height;
		pCur[2] = 1.0f / width;
		pCur[3] = 1.0f / height;
		m_curSevenBitMapSizeValidBits |= validBit;
		glProgramEnvParameter4fARB(GL_FRAGMENT_PROGRAM_ARB, texUnit, width, height, pCur[2], pCur[3]);
	}
	//@}
	inline void FASTCALL SetViewport(GLint x, GLint y, GLsizei width, GLsizei height) {
		m_curViewport[0] = x;
		m_curViewport[1] = y;
		m_curViewport[2] = width;
		m_curViewport[3] = height;
		glViewport(x, y, width, height);
	}


	//No-op while texture enables are ignored.
	inline void FASTCALL EnableTexUnit(DWORD texUnit) {
		BYTE texBit = (BYTE)(1U << texUnit);
		if (m_texEnableBits & texBit) {
			return;
		}
		m_texEnableBits |= texBit;

		if (!TexEnablesIgnored()) {
			SetActiveTexUnit(texUnit);
			glEnable(GL_TEXTURE_2D);
		}
	}

	inline void SetDefaultTextureState(void) {
		if (m_texEnableBits != 0x1) {
			DWORD texUnit;
			DWORD texBit;

			//Unit zero first.
			if ((m_texEnableBits & 0x1) == 0) {
				m_texEnableBits |= 0x1;

				if (!TexEnablesIgnored()) {
					SetActiveTexUnit(0);
					glEnable(GL_TEXTURE_2D);
				}
			}

			for (texUnit = 1, texBit = 0x2; m_texEnableBits != 0x1; texUnit++, texBit <<= 1) {
				if (texBit & m_texEnableBits) {
					m_texEnableBits -= texBit;

					if (!TexEnablesIgnored()) {
						SetActiveTexUnit(texUnit);
						glDisable(GL_TEXTURE_2D);
					}
				}
			}

			SetActiveTexUnit(0);
		}

		if (m_clientTexEnableBits != 0x1) {
			DWORD texUnit;
			DWORD texBit;

			if ((m_clientTexEnableBits & 0x1) == 0) {
				m_clientTexEnableBits |= 0x1;

				SetClientActiveTexUnit(0);
				glEnableClientState(GL_TEXTURE_COORD_ARRAY);
			}

			for (texUnit = 1, texBit = 0x2; m_clientTexEnableBits != 0x1; texUnit++, texBit <<= 1) {
				if (texBit & m_clientTexEnableBits) {
					m_clientTexEnableBits -= texBit;

					SetClientActiveTexUnit(texUnit);
					glDisableClientState(GL_TEXTURE_COORD_ARRAY);
				}
			}
		}

		return;
	}

	inline void FASTCALL DisableSubsequentTextures(DWORD firstTexUnit) {
		DWORD texUnit;
		BYTE texBit = 1U << firstTexUnit;

		for (texUnit = firstTexUnit; texBit <= m_texEnableBits; texUnit++, texBit <<= 1) {
			if (texBit & m_texEnableBits) {
				m_texEnableBits -= texBit;

				if (!TexEnablesIgnored()) {
					SetActiveTexUnit(texUnit);
					glDisable(GL_TEXTURE_2D);
				}
			}
		}

		return;
	}

	inline void FASTCALL DisableSubsequentClientTextures(DWORD firstTexUnit) {
		DWORD texUnit;
		BYTE texBit = 1U << firstTexUnit;

		for (texUnit = firstTexUnit; texBit <= m_clientTexEnableBits; texUnit++, texBit <<= 1) {
			if (texBit & m_clientTexEnableBits) {
				m_clientTexEnableBits -= texBit;

				SetClientActiveTexUnit(texUnit);
				glDisableClientState(GL_TEXTURE_COORD_ARRAY);
			}
		}

		return;
	}

	inline void SetDefaultShaderState(void) {
		GLuint vpId = (UseFragmentProgram) ? m_vpDefaultRenderingState : 0;
		if (m_vpCurrent != vpId) {
			SetVertexProgramNoCheck(vpId);
		}

		GLuint fpId = (UseFragmentProgram) ? m_fpDefaultRenderingState : 0;
		if (m_fpCurrent != fpId) {
			SetFragmentProgramNoCheck(fpId);
		}
	}
	inline void FASTCALL SetShaderState(GLuint vpId, GLuint fpId) {
		if (m_vpCurrent != vpId) {
			SetVertexProgramNoCheck(vpId);
		}
		if (m_fpCurrent != fpId) {
			SetFragmentProgramNoCheck(fpId);
		}
	}

	void FASTCALL SetVertexProgramNoCheck(GLuint vpId);
	void FASTCALL SetFragmentProgramNoCheck(GLuint fpId);

	void InitVertexStreams(void);
	void ShutdownVertexStreams(void);
	void SetVertexStreamPointers(void);
	//The color array alternates between two strides.
	void SetColorArrayPointer(void);
	//The staged pointer doubles as the dirty marker.
	inline void FASTCALL StageVertexStream(DWORD stream, const void *pCpuData, DWORD stride, DWORD numElements) {
		m_vertexStreams[stream].pCpuData = pCpuData;
		m_vertexStreams[stream].PendingStride = stride;
		m_vertexStreams[stream].PendingCount = numElements;
	}
	//False only for complex surface positions.
	void FASTCALL UploadVertexStream(DWORD stream, bool repoint = true);
	void FASTCALL RepointVertexStream(DWORD stream);
	void SetVertexArrayBaseZero(void);
	inline void FASTCALL BindArrayBuffer(GLuint bufferId) {
		if (m_curArrayBuffer != bufferId) {
			m_curArrayBuffer = bufferId;
			glBindBufferARB(GL_ARRAY_BUFFER_ARB, bufferId);
		}
	}
	//Positions once per facet.
	void FASTCALL UploadComplexSurfaceStreams(DWORD numTexCoordStreams);
	void FASTCALL UploadGouraudStreams(DWORD numVerts);
	void FASTCALL UploadTileStreams(DWORD numVerts);

	inline void SetDefaultColorState(void) {
		if (m_currentColorFlags != 0) {
			SetDefaultColorStateNoCheck();
		}
	}
	void SetDefaultColorStateNoCheck(void);

	inline void SetColorState(void) {
		if (m_currentColorFlags != m_requestedColorFlags) {
			SetColorStateNoCheck();
		}
	}
	void SetColorStateNoCheck(void);

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

	bool FASTCALL LoadVertexProgram(GLuint, const char *, const char *);
	bool FASTCALL LoadFragmentProgram(GLuint, const char *, const char *);
	//As above.
	bool FASTCALL LoadFragmentProgramNative(GLuint, const char *, const char *);

	void AllocateFragmentProgramNamesSafe(void);
	void FreeFragmentProgramNamesSafe(void);
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

	inline void FASTCALL AddRenderPass(FTextureInfo *Info, DWORD PolyFlags, FLOAT PanBias, bool bSevenBitMap) {
		INT rpPassCount = m_rpPassCount;

		MultiPass.TMU[rpPassCount].Info = Info;
		MultiPass.TMU[rpPassCount].PolyFlags = PolyFlags;
		MultiPass.TMU[rpPassCount].PanBias = PanBias;
		MultiPass.TMU[rpPassCount].bSevenBitMap = bSevenBitMap;

		//Single texture is forced by capping the pass count.
		rpPassCount++;
		m_rpPassCount = rpPassCount;
		if (rpPassCount >= m_rpTMUnits) {
			//The pass count here is never zero.
			RenderPassesExec();
		}
	}

	void RenderPassesExec(void);
	void FASTCALL RenderPassesExec_SingleOrDualTextureAndDetailTexture(FTextureInfo &DetailTextureInfo);

	void SetRenderPassBlend(void);
	void RenderPassesNoCheckSetup(void);
	void RenderPassesNoCheckSetup_FP(void);
	void FASTCALL RenderPassesNoCheckSetup_SingleOrDualTextureAndDetailTexture(FTextureInfo &);
	void FASTCALL RenderPassesNoCheckSetup_SingleOrDualTextureAndDetailTexture_FP(FTextureInfo &);

	//Leaves the pointer on the first polygon that didn't fit.
	INT FASTCALL BufferStaticComplexSurfaceGeometry(const FSurfaceFacet &, FSavedPoly *&pPoly);
	INT FASTCALL BufferStaticComplexSurfaceGeometry_VP(const FSurfaceFacet &, FSavedPoly *&pPoly);
	void DrawComplexSurfaceChunk(const FSurfaceInfo &Surface, const FSurfaceFacet &Facet, INT numVerts);
	DWORD FASTCALL BufferDetailTextureData(FLOAT);
#ifdef UTGLR_INCLUDE_SSE_CODE
	DWORD FASTCALL BufferDetailTextureData_SSE2(FLOAT);
#endif //UTGLR_INCLUDE_SSE_CODE

	void FASTCALL DrawDetailTexture(FTextureInfo &, INT, bool);
	void FASTCALL DrawDetailTexture_FP(FTextureInfo &);

	inline void EndGouraudPolygonBuffering(void) {
		if (BufferedVerts > 0) {
			EndGouraudPolygonBufferingNoCheck();
		}
	}
	inline void EndTileBuffering(void) {
		if (BufferedTileVerts > 0) {
			EndTileBufferingNoCheck();
		}
	}
	inline void EndLineBuffering(void) {
		if (BufferedLineVerts > 0) {
			EndLineBufferingNoCheck();
		}
	}
	inline void EndPointBuffering(void) {
		if (BufferedPointVerts > 0) {
			EndPointBufferingNoCheck();
		}
	}
	//All four batch types share the staging arrays.
	//Only one may be open.
	inline void EndBuffering(void) {
		if (BufferedVerts > 0) {
			EndGouraudPolygonBufferingNoCheck();
		}
		if (BufferedTileVerts > 0) {
			EndTileBufferingNoCheck();
		}
		if (BufferedLineVerts > 0) {
			EndLineBufferingNoCheck();
		}
		if (BufferedPointVerts > 0) {
			EndPointBufferingNoCheck();
		}
	}
	inline void EndBufferingExceptGouraud(void) {
		EndTileBuffering();
		EndLineBuffering();
		EndPointBuffering();
	}
	inline void EndBufferingExceptTiles(void) {
		EndGouraudPolygonBuffering();
		EndLineBuffering();
		EndPointBuffering();
	}
	inline void EndBufferingExceptLines(void) {
		EndGouraudPolygonBuffering();
		EndTileBuffering();
		EndPointBuffering();
	}
	inline void EndBufferingExceptPoints(void) {
		EndGouraudPolygonBuffering();
		EndTileBuffering();
		EndLineBuffering();
	}
	void EndGouraudPolygonBufferingNoCheck(void);
	void EndTileBufferingNoCheck(void);
	void EndLineBufferingNoCheck(void);
	void EndPointBufferingNoCheck(void);

	void FASTCALL BufferAdditionalClippedVerts(FTransTexture **Pts, INT NumPts);
	void BufferLine(DWORD LineFlags, FPlane Color, FLOAT X1, FLOAT Y1, FLOAT Z1, FLOAT X2, FLOAT Y2, FLOAT Z2);

	void OpenGouraudPolygonBatch(FTextureInfo &Info, DWORD PolyFlags, DWORD PolyFlags2, INT arrayCost);
};

#ifdef UTGLR_UNREAL_227_BUILD

#if __STATIC_LINK

/* No native execs. */

#define AUTO_INITIALIZE_REGISTRANTS_OPENGLDRV \
	UOpenGLRenderDevice::StaticClass();
#endif

#endif

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
