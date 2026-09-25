/** \file deferred.h World surfaces, built on a worker pool. */

#pragma once

//Windows types first, for BYTE and DWORD.
#include <d3d10.h>
#include "VertexFormats.h"
#include "jobsystem.h"
#include "framearena.h"

class Shader;
class DynamicGeometryBuffer;


/** Pan carries half texel correction. Offset is the tile origin. */
struct FSurfaceLayer {
	float PanU, PanV;
	float MultU, MultV;
	float OffsetU, OffsetV;
};


struct FComplexSurfaceRecord {
	/**@name Geometry, copied out */
	//@{
	const Vec3 *pPoints;
	/** One extra, so either end reads unguarded. */
	const unsigned int *pPolyFirst;
	const unsigned int *pPolyIndexFirst;
	unsigned int NumPolys;
	unsigned int NumPoints;
	unsigned int NumIndices;
	//@}
	float XAxis[3];
	float YAxis[3];
	float UDot, VDot;

	FSurfaceLayer Layers[5];
	unsigned int LayerMask;
	DWORD Flags;
	DWORD TexturePasses;

	/**@name Per-pass scratch */
	//@{
	unsigned int PassFirstPoly;
	unsigned int PassNumPolys;
	unsigned int PassVertexBase;
	unsigned int PassIndexBase;
	//@}
};


/** One unit of parallel work. Small runs merge. */
struct FSurfaceJob {
	unsigned int FirstCmd;
	unsigned int FirstPoly;
	unsigned int LastCmd; /**< Inclusive */
	unsigned int EndPoly;
};


/** Engine thread only. */
class DeferredGeometry {
public:
	DeferredGeometry();
	~DeferredGeometry();

	/** \param threads Zero for one per core. */
	void start(int threads);
	void stop();
	int numThreads() const;

	inline bool enabled() const { return m_enabled; }
	inline void setEnabled(bool on) { m_enabled = on; }
	inline bool hasCommands() const { return m_numCmds != 0; }

	inline bool gaveUp() const { return m_gaveUp; }

	void discard();

	void newFrame();

	/**
	State already resolved.
	\return false if not recorded.
	*/
	//struct, or C4099.
	bool record(const struct FSurfaceFacet &facet, float uDot, float vDot,
		const FSurfaceLayer *pLayers, unsigned int layerMask, DWORD flags, DWORD texturePasses);

	/**
	Build and draw.
	\return Draws issued.
	\param shader For intermediate passes.
	*/
	unsigned int build(Shader *shader);

	/**@name Previous frame */
	//@{
	unsigned int recordedSurfaces;
	unsigned int buildJobs;
	unsigned int buildMicroseconds;
	//@}

private:
	DeferredGeometry(const DeferredGeometry &);
	DeferredGeometry &operator=(const DeferredGeometry &);

	static void jobEntry(void *pContext, int jobIndex);
	void runJob(int jobIndex);
	void runJobCommand(const FSurfaceJob &job, unsigned int cmdIndex);

	bool growCommands(unsigned int want);

	FJobSystem *m_pJobs;
	FFrameArena m_arena;
	bool m_enabled;

	/**@name Giving up on a faulting build. Consecutive builds */
	//@{
	unsigned int m_faultedBuilds;
	bool m_gaveUp;
	//@}

	FComplexSurfaceRecord *m_pCmds;
	unsigned int m_numCmds;
	unsigned int m_maxCmds;

	FSurfaceJob *m_pJobList;
	unsigned int m_numJobs;
	unsigned int m_maxJobs;

	unsigned int m_surfacesThisFrame;
	unsigned int m_jobsThisFrame;
	unsigned int m_microsecondsThisFrame;

	Vertex_ComplexSurface *m_pPassVerts;
	int *m_pPassIndices;
	unsigned int m_passBaseVertex;
};
