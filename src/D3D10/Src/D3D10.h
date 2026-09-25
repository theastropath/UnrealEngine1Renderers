/** \file D3D10.h */

#pragma once

#include "D3D10Drv.h"
#include "Device/D3D.h"

#define SAFE_RELEASE(p) \
	{ \
		if (p) { \
			(p)->Release(); \
			(p) = NULL; \
		} \
	}
#define CLAMP(p, min, max) \
	{ \
		if (p < min) \
			p = min; \
		else if (p > max) \
			p = max; \
	}

//Forward declared; their headers include this one.
class TextureCache;
class TexConverter;
class Shader_GouraudPolygon;
class Shader_Tile;
class Shader_ComplexSurface;
class Shader_FogSurface;
class Shader_Line;

class UD3D10RenderDevice : public URenderDevice {

//UObject glue
#if (UT_URENDERDEVICE || RUNE)
	DECLARE_CLASS(UD3D10RenderDevice, URenderDevice, CLASS_Config, D3D10Drv)
#else
	DECLARE_CLASS(UD3D10RenderDevice, URenderDevice, CLASS_Config)
#endif

private:
	D3D::Options D3DOptions;
	/** User configurable options */
	struct
	{
		int precache; /**< Longer loads. */
		int autoFOV; /**< Widescreen correction. */
		int FPSLimit; /**< Zero is off. */
		int unlimitedViewDistance; /**< Frustum to the map bound. */
		int singleCpuAffinity; /**< Legacy engine timing workaround. */
		int textureCacheBudgetMegs; /**< Zero is unlimited. */
		int lightmapAtlas; /**< Shared pages keep batches. */
		int deferredRecording; /**< Built across threads. */
		int renderThreads; /**< Zero for one per core. */
		float gammaOffset; /**< As in the other two renderers. */
	} options;

	D3D::Options appliedD3DOptions;

	struct
	{
		int unlimitedViewDistance;
		int textureCacheBudgetMegs;
		float gammaOffset;
	} appliedOptions;

	float appliedBrightness; /**< Skips redundant updates. */
	float lastFrameTime; /**< Seconds. */

	/**@name Per device state */
	//@{
	D3D::Context *d3dContext; /**< Direct3D and shader state. */

	TextureCache *textureCache;
	TexConverter *texConverter;

	Shader_GouraudPolygon *shader_GouraudPolygon;
	Shader_Tile *shader_Tile;
	Shader_ComplexSurface *shader_ComplexSurface;
	Shader_FogSurface *shader_FogSurface;
	Shader_Line *shader_Line;

	bool drawingHUD;

	/**@name Tile path diagnostics
	Switched on by LogTiles in the ini, which is read but never registered as a property, and each
	texture is named only once because a realtime one would otherwise grow the log without end and drown
	out everything else anybody might be reading it for.
	*/
	//@{
	bool logTiles;
	enum { MAX_LOGGED_TILES = 64 };
	QWORD loggedTileIds[MAX_LOGGED_TILES];
	int numLoggedTiles;
	void logTileOnce(const FTextureInfo &Info, DWORD polyFlags, DWORD flags);
	//@}

	int customFOV; /**< From the aspect ratio. */
	float zNear;
	float zFar;

	LARGE_INTEGER perfCounterFreq;
	LARGE_INTEGER lastFrameEnd;

	enum { GAMMA_RAMP_SIZE = 256 };
	WORD savedGammaRamp[3][GAMMA_RAMP_SIZE];
	bool gammaRampSaved;
	bool gammaRampNeutralized;

	HANDLE frameTimer;
	bool frameTimerCreated;

	void makeContextCurrent();

	void setViewDistance(int unlimited);
	HANDLE getFrameTimer();
	void releaseFrameTimer();
	void limitFrameRate(const LARGE_INTEGER &startTime, float targetFrameTime);
	//@}

	/**@name Held back blended models
	*/
	//@{
	struct DeferredGouraudPolygon {
		float depth; /**< The sort key. */
		DWORD flags; /**< Resolved on arrival. */
		DWORD64 textureID; /**< Cached before the hold. */
		float multU, multV; /**< Cached with it. */
		float alpha; /**< Rune only. */
		int firstPoint; /**< Into deferredGouraudPoints. */
		int numPoints;
	};

	enum { MAX_DEFERRED_GOURAUD_POLYGONS = 2048,
		MAX_DEFERRED_GOURAUD_POINTS = 8192 };

	DeferredGouraudPolygon deferredGouraudPolygons[MAX_DEFERRED_GOURAUD_POLYGONS];
	int deferredGouraudOrder[MAX_DEFERRED_GOURAUD_POLYGONS];
	int deferredGouraudOrderScratch[MAX_DEFERRED_GOURAUD_POLYGONS];
	FTransTexture deferredGouraudPoints[MAX_DEFERRED_GOURAUD_POINTS];
	int numDeferredGouraudPolygons;
	int numDeferredGouraudPoints;
	FSceneNode *deferredGouraudFrame;

	//False when it doesn't fit.
	bool deferGouraudPolygon(FSceneNode *Frame, FTransTexture **Pts, int NumPts, DWORD flags, DWORD64 textureID, float multU, float multV, float alpha);
	void sortDeferredGouraudPolygons(int numPolygons);
	void flushDeferredGouraudPolygons();
	void flushDeferredGouraudPolygonsForFrame(FSceneNode *Frame);
	void discardDeferredGouraudPolygons();
	//@}

	/** \note Held by value. Addresses get reused. */
	struct SceneNodeKey {
		INT X, Y, XB, YB;
		FLOAT FX, FY;
		FLOAT FX15, FY15;
		FLOAT RProjZ;
		FLOAT FovAngle;
	};
	SceneNodeKey sceneNodeKey;

	/**@name UnrealEd hit proxy buffer
	\note Reports zero.
	*/
	//@{
	BYTE *hitData; /**< The engine's buffer, or null. */
	INT *hitSize; /**< In: capacity. Out: bytes written. */
	INT hitCount; /**< Nothing writes any yet. */
	//@}

	void buildSceneNodeKey(const FSceneNode *Frame, SceneNodeKey &key) const;
	void setSceneNodeIfChanged(FSceneNode *Frame);

public:
	/**
	Must stay user provided, because "= default" lets the engine's class-construction macro
	value-initialize with empty parentheses, which zeroes the UObject header the engine has already
	filled in and leaves the renderer destroying with a null class at shutdown, which is a crash rather
	than a warning.
	*/
	UD3D10RenderDevice() :
		appliedBrightness(0.0f),
		lastFrameTime(0.0f),
		//Nothing else zeroes the block below. The near clip is 0.5 because the
		//games' own 1.0 cuts off UT weapons at wide fields of view.
		d3dContext(nullptr),
		textureCache(nullptr),
		texConverter(nullptr),
		shader_GouraudPolygon(nullptr),
		shader_Tile(nullptr),
		shader_ComplexSurface(nullptr),
		shader_FogSurface(nullptr),
		shader_Line(nullptr),
		drawingHUD(false),
		logTiles(false),
		loggedTileIds(),
		numLoggedTiles(0),
		customFOV(0),
		zNear(0.5f),
		zFar(0.0f),
		perfCounterFreq(),
		lastFrameEnd(),
		savedGammaRamp(),
		gammaRampSaved(false),
		gammaRampNeutralized(false),
		frameTimer(nullptr),
		frameTimerCreated(false),
		numDeferredGouraudPolygons(0),
		numDeferredGouraudPoints(0),
		deferredGouraudFrame(nullptr),
		sceneNodeKey(),
		hitData(nullptr),
		hitSize(nullptr),
		hitCount(0) {}

	/**@name Helpers */
	//@{
	static void debugs(const char *s);
	int getOption(const TCHAR *name, int defaultVal, bool isBool);
	void setEngineFeatureFlagDefault(const TCHAR *name, BITFIELD &flag);
	bool rebuildRenderer();
	bool applyOptionChanges();
	void updateBrightness();
	void saveSystemGamma();
	void neutralizeSystemGamma();
	void restoreSystemGamma();
	//@}

	/**@name Abstract in parent class */
	//@{
	UBOOL Init(UViewport *InViewport, INT NewX, INT NewY, INT NewColorBytes, UBOOL Fullscreen);
	UBOOL SetRes(INT NewX, INT NewY, INT NewColorBytes, UBOOL Fullscreen);
	void Exit();
#if OLD_URENDERDEVICE
	void Flush();
#else
	void Flush(UBOOL AllowPrecache);
#endif
	void Lock(FPlane FlashScale, FPlane FlashFog, FPlane ScreenClear, DWORD RenderLockFlags, BYTE *HitData, INT *HitSize);
	void Unlock(UBOOL Blit);
	void DrawComplexSurface(FSceneNode *Frame, FSurfaceInfo &Surface, FSurfaceFacet &Facet);
	void DrawGouraudPolygon(FSceneNode *Frame, FTextureInfo &Info, FTransTexture **Pts, int NumPts, DWORD PolyFlags, FSpanBuffer *Span);
#if HARRYPOTTER
	/**@name Harry Potter's indexed triangle interface
	*/
	//@{
	INT MaxVertices();
	void DrawTriangles(FSceneNode *Frame, FTextureInfo &Info, FTransTexture **Pts, INT NumPts, _WORD *Indices, INT NumIndices, DWORD PolyFlags, class FSpanBuffer *Span);

	/** Whether Indices[first] is in range. */
	static inline bool indexedTriangleInRange(const _WORD *Indices, INT first, INT NumPts) {
		if (!Indices)
			return true;
		return Indices[first + 0] < NumPts && Indices[first + 1] < NumPts && Indices[first + 2] < NumPts;
	}
//@}
#endif
	void DrawTile(FSceneNode *Frame, FTextureInfo &Info, FLOAT X, FLOAT Y, FLOAT XL, FLOAT YL, FLOAT U, FLOAT V, FLOAT UL, FLOAT VL, class FSpanBuffer *Span, FLOAT Z, FPlane Color, FPlane Fog, DWORD PolyFlags);
	void Draw2DLine(FSceneNode *Frame, FPlane Color, DWORD LineFlags, FVector P1, FVector P2);
	void Draw2DPoint(FSceneNode *Frame, FPlane Color, DWORD LineFlags, FLOAT X1, FLOAT Y1, FLOAT X2, FLOAT Y2, FLOAT Z);
	void ClearZ(FSceneNode *Frame);
	void PushHit(const BYTE *Data, INT Count);
	void PopHit(INT Count, UBOOL bForce);
	void GetStats(TCHAR *Result);
	void ReadPixels(FColor *Pixels);
	//@}

	/**@name Optional but implemented*/
	//@{
	UBOOL Exec(const TCHAR *Cmd, FOutputDevice &Ar);
	void SetSceneNode(FSceneNode *Frame);
	void PrecacheTexture(FTextureInfo &Info, DWORD PolyFlags);
	void precacheLightmap(FTextureInfo &Info);
	void EndFlash();
/**
Klingon's engine (219) registers a class through a plain function, the other SDKs through a
method on the class default object.
*/
#if KLINGON
	static void StaticConstructor(UClass *Class);
#else
	void StaticConstructor();
#endif
	void staticConstructorBody();

	static UClass *scClass;
	//@}

#if (RUNE)
	/**@name Rune fog*/
	//@{
	void DrawFogSurface(FSceneNode *Frame, FFogSurf &FogSurf);
	void PreDrawGouraud(FSceneNode *Frame, FLOAT FogDistance, FPlane FogColor);
	void PostDrawGouraud(FLOAT FogDistance);
//@}
#endif
};
