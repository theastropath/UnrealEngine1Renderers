/*=============================================================================
	Deferred.h: the draw command log the worker pool builds geometry from.

	Texture and facet data are locals of the engine's own rendering code and do not
	outlive the call that hands them over, so recording resolves every piece of state
	at once and copies the values it needs into the frame arena, leaving only vertex
	generation to defer, which is the part worth moving off the engine's thread, and
	every structure below is therefore a value type sized at record time, holding no
	pointer back into anything the engine still owns.
=============================================================================*/

#ifndef UTGLR_D3D9DEFERRED_H
#define UTGLR_D3D9DEFERRED_H


enum EDrawCmdType {
	DCMD_COMPLEX_SURFACE = 0,
	DCMD_GOURAUD_POLY = 1,
	DCMD_TILE = 2,
	DCMD_LINE = 3,
	DCMD_POINT = 4
};


//Compared with memcmp, so Reset clears the whole object, padding included.
struct FDrawStateKey {
	DWORD Type; //EDrawCmdType
	DWORD PolyFlags;
	DWORD PolyFlags2;
	DWORD NumLayers;
	DWORD BiasDepth; //DWORD keeps the struct free of padding
	DWORD LayerFlags[MAX_TMUNITS];
	//One bit per layer. A macro texture and a lightmap both arrive as PF_Modulated,
	//yet take different pixel shaders under ReduceBanding.
	DWORD SevenBitMask;
	QWORD TexKey[MAX_TMUNITS]; //Atlas page or texture cache id, per layer
	DWORD Hash; //Cheap reject.

	void Reset() {
		memset(this, 0, sizeof(*this));
	}

	void ComputeHash() {
		Hash = 0;
		const DWORD *pWords = (const DWORD *)this;
		//Up to Hash.
		const DWORD numWords = (DWORD)(offsetof(FDrawStateKey, Hash) / sizeof(DWORD));
		for (DWORD i = 0; i < numWords; i++) {
			Hash = (Hash * 16777619u) ^ pWords[i];
		}
	}

	inline bool Matches(const FDrawStateKey &other) const {
		if (Hash != other.Hash) {
			return false;
		}
		return memcmp(this, &other, offsetof(FDrawStateKey, Hash)) == 0;
	}
};

//Compared as raw bytes, like FSceneNodeKey.
//A build without /Zp4 can leave a hole before the first QWORD member.
static_assert(sizeof(FDrawStateKey) == ((7 + MAX_TMUNITS) * sizeof(DWORD)) + (MAX_TMUNITS * sizeof(QWORD)),
	"FDrawStateKey must be padding free to be compared and hashed as bytes");


//The engine's own is a local of its rendering code, so binding at record time is what guarantees
//a cache entry still exists by draw time, and masked and unmasked count as two of them because
//transparency is applied per texel on upload, which is also why the flags travel with the index
//instead of being derived again later, the ones the engine submitted having gone out of scope
//long before the run draws.
struct FRecordedTexture {
	FTextureInfo Info;
	DWORD PolyFlags;
	FLOAT PanBias;
	FLOAT UPan, VPan;
	FLOAT UMult, VMult;
	FLOAT UOffset, VOffset;
	QWORD TexKey;
	IDirect3DTexture9 *pAtlasTexObj;
};


/** One recorded complex surface: a facet's geometry plus its resolved layers. */
struct FComplexSurfaceCmd {
	const FVector *pPoints;
	//Start offset per polygon, plus one holding the total.
	//Either end then reads without a bounds test.
	const DWORD *pPolyFirst;
	DWORD NumPolys;
	DWORD NumPoints;

	FVector XAxis;
	FVector YAxis;
	FLOAT UDot;
	FLOAT VDot;

	DWORD Color;
	DWORD NumLayers;
	//Per layer pan, scale and atlas origin. By value: a placement is only valid for its own frame.
	FLOAT UPan[MAX_TMUNITS], VPan[MAX_TMUNITS];
	FLOAT UMult[MAX_TMUNITS], VMult[MAX_TMUNITS];
	FLOAT UOffset[MAX_TMUNITS], VOffset[MAX_TMUNITS];
	//One index per layer.
	//Matching bindings fold together, so a surface's layers need not be adjacent.
	INT TexIndex[MAX_TMUNITS];
};


/** One recorded gouraud polygon, as a fan of NumPts points. */
struct FGouraudPolyCmd {
	const FTransTexture *pPoints;
	DWORD NumPts;
	FLOAT UMult, VMult;
	//Which colour conversion the immediate path would use. In the run key too: it picks the declaration.
	DWORD RequestedColorFlags;
#ifdef UTGLR_RUNE_BUILD
	//As the immediate path computes it.
	//Or the value carried from submission on a replay.
	BYTE Alpha;
#endif
};


//Already the record an instanced draw reads.
//A worker expands them into vertices where instancing is unavailable.
struct FQuadCmd {
	FLOAT X1, Y1, X2, Y2;
	FLOAT U1, V1, U2, V2;
	//A line's two ends carry their own depth. A quad sets both the same.
	FLOAT Z1, Z2;
	DWORD Color;
};


/** One command in submission order. */
struct FDrawCmd {
	FDrawStateKey Key;
	union {
		const FComplexSurfaceCmd *pComplex;
		const FGouraudPolyCmd *pGouraud;
		const FQuadCmd *pQuad;
		const void *pPayload;
	};
	//Layer 0 only; -1 for untextured lines and points, with later layers reached through the
	//surface command's own index array because folding matching bindings together has left them
	//non-contiguous.
	INT FirstTexture;

	//Known at record time, so the prefix sum runs before any geometry exists.
	//An instanced quad or line counts one instance slot.
	DWORD VertexCount;
	DWORD IndexCount;
	DWORD VertexBase;
	DWORD IndexBase;
	DWORD OutputBytes;
	//Set after segmentation.
	DWORD Instanced;
};


/** A maximal span of consecutive commands sharing one state key: one draw. */
struct FDrawRun {
	DWORD FirstCmd;
	DWORD NumCmds;
	DWORD VertexBase; //In the vertex ring.
	DWORD IndexBase; //In the index ring.
	DWORD VertexCount;
	DWORD IndexCount;
	DWORD InstanceCount; //Non-zero when instanced.
};


struct FBuildJob {
	DWORD FirstCmd;
	DWORD NumCmds;
	DWORD FirstPoly;
	DWORD NumPolys;
};


//Every pointer here addresses either the recorded commands, which nothing touches during a build,
//or a distinct span of the mapped buffers, so no job ever writes a byte another job writes and
//none of them need to synchronise with each other at all, which is also the reason a job body
//may not reach the device or allocate: the contract is that it reads this context, fills its own
//slice and returns.
struct FBuildContext {
	class UD3D9RenderDevice *pDevice;

	const FDrawCmd *pCmds;
	const FBuildJob *pJobs;

	FGLVertexColor *pVertexColor;
	FGLSecondaryColor *pSecondaryColor;
	FGLTexCoord *pTexCoord[MAX_TMUNITS];
	WORD *pIndex;
	FQuadCmd *pInstance;
};


#endif //UTGLR_D3D9DEFERRED_H
