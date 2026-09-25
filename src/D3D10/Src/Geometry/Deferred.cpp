//Before the engine's headers; macros.
#include <d3d10.h>
#include <d3dx10.h>
#include <new>
#include <stdlib.h>
#include <string.h>
#include "../D3D10.h"
#include "Deferred.h"
#include "DynamicGeometryBuffer.h"
#include "../Shaders/Shader.h"


DeferredGeometry::DeferredGeometry() :
	recordedSurfaces(0),
	buildJobs(0),
	buildMicroseconds(0),
	m_pJobs(nullptr),
	m_enabled(false),
	m_faultedBuilds(0),
	m_gaveUp(false),
	m_pCmds(nullptr),
	m_numCmds(0),
	m_maxCmds(0),
	m_pJobList(nullptr),
	m_numJobs(0),
	m_maxJobs(0),
	m_surfacesThisFrame(0),
	m_jobsThisFrame(0),
	m_microsecondsThisFrame(0),
	m_pPassVerts(nullptr),
	m_pPassIndices(nullptr),
	m_passBaseVertex(0) {
	m_arena.Init(2 * 1024 * 1024);
}

/** Deliberately does not stop the worker pool, because this can run under the loader lock at process
detach, where waiting on a thread that itself wants the lock deadlocks the process and shutdown never
arrives, so the threads are brought down from Exit while there is still an engine to do it from. */
DeferredGeometry::~DeferredGeometry() {
	free(m_pCmds);
	free(m_pJobList);
	m_arena.Free();
}

//Once, until shutdown.
void DeferredGeometry::start(int threads) {
	if (m_pJobs == nullptr)
		m_pJobs = new (std::nothrow) FJobSystem;
	if (m_pJobs == nullptr)
		return;

	m_pJobs->Start(threads);
}

//Before the DLL unloads.
void DeferredGeometry::stop() {
	if (m_pJobs != nullptr) {
		m_pJobs->Stop();
		delete m_pJobs;
		m_pJobs = nullptr;
	}
	discard();
}

int DeferredGeometry::numThreads() const {
	return (m_pJobs != nullptr) ? m_pJobs->NumParticipants() : 1;
}

void DeferredGeometry::discard() {
	m_numCmds = 0;
	m_arena.Reset();
}

void DeferredGeometry::newFrame() {
	discard();

	recordedSurfaces = m_surfacesThisFrame;
	buildJobs = m_jobsThisFrame;
	buildMicroseconds = m_microsecondsThisFrame;
	m_surfacesThisFrame = 0;
	m_jobsThisFrame = 0;
	m_microsecondsThisFrame = 0;
}

//Doubles.
bool DeferredGeometry::growCommands(unsigned int want) {
	if (want <= m_maxCmds)
		return true;
	unsigned int capacity = m_maxCmds ? m_maxCmds : 256;
	while (capacity < want)
		capacity *= 2;
	FComplexSurfaceRecord *p = (FComplexSurfaceRecord *)realloc(m_pCmds, capacity * sizeof(FComplexSurfaceRecord));
	if (p == nullptr)
		return false;
	m_pCmds = p;
	m_maxCmds = capacity;
	return true;
}


/*
-----------------------------------------------------------------------------
Recording.
-----------------------------------------------------------------------------
*/

bool DeferredGeometry::record(const FSurfaceFacet &facet, float uDot, float vDot,
	const FSurfaceLayer *pLayers, unsigned int layerMask, DWORD flags, DWORD texturePasses) {
	unsigned int numPolys = 0, numPoints = 0, numIndices = 0;
	for (const FSavedPoly *pPoly = facet.Polys; pPoly; pPoly = pPoly->Next) {
		if (pPoly->NumPts < 3) //Would underflow the triangle count.
			continue;
		numPolys++;
		numPoints += (unsigned int)pPoly->NumPts;
		numIndices += 3 * ((unsigned int)pPoly->NumPts - 2);
	}
	if (numPolys == 0)
		return true; //Handled, nothing to draw.

	if (!growCommands(m_numCmds + 1))
		return false;

	//The facet is transient.
	Vec3 *pPoints = m_arena.AllocArray<Vec3>(numPoints);
	unsigned int *pPolyFirst = m_arena.AllocArray<unsigned int>(numPolys + 1);
	unsigned int *pPolyIndexFirst = m_arena.AllocArray<unsigned int>(numPolys + 1);
	if (pPoints == nullptr || pPolyFirst == nullptr || pPolyIndexFirst == nullptr)
		return false;

	unsigned int p = 0, pt = 0, ix = 0;
	for (const FSavedPoly *pPoly = facet.Polys; pPoly; pPoly = pPoly->Next) {
		INT n = pPoly->NumPts;
		if (n < 3)
			continue;
		pPolyFirst[p] = pt;
		pPolyIndexFirst[p] = ix;
		p++;
		ix += 3 * ((unsigned int)n - 2);
		FTransform *const *pPts = &pPoly->Pts[0];
		do {
			const FVector &point = (*pPts++)->Point;
			pPoints[pt].x = point.X;
			pPoints[pt].y = point.Y;
			pPoints[pt].z = point.Z;
			pt++;
		} while (--n != 0);
	}
	pPolyFirst[numPolys] = pt;
	pPolyIndexFirst[numPolys] = ix;

	FComplexSurfaceRecord &cmd = m_pCmds[m_numCmds];
	cmd.pPoints = pPoints;
	cmd.pPolyFirst = pPolyFirst;
	cmd.pPolyIndexFirst = pPolyIndexFirst;
	cmd.NumPolys = numPolys;
	cmd.NumPoints = numPoints;
	cmd.NumIndices = numIndices;

	cmd.XAxis[0] = facet.MapCoords.XAxis.X;
	cmd.XAxis[1] = facet.MapCoords.XAxis.Y;
	cmd.XAxis[2] = facet.MapCoords.XAxis.Z;
	cmd.YAxis[0] = facet.MapCoords.YAxis.X;
	cmd.YAxis[1] = facet.MapCoords.YAxis.Y;
	cmd.YAxis[2] = facet.MapCoords.YAxis.Z;
	cmd.UDot = uDot;
	cmd.VDot = vDot;

	for (int i = 0; i < 5; i++)
		cmd.Layers[i] = pLayers[i];
	cmd.LayerMask = layerMask;
	cmd.Flags = flags;
	cmd.TexturePasses = texturePasses;

	m_numCmds++;
	m_surfacesThisFrame++;
	return true;
}


/*
-----------------------------------------------------------------------------
Build.

Runs on several threads at once, each job touching only unshared records and its own slice of the
mapped buffer, which is what makes the whole thing safe without a lock anywhere in the inner loop, the
engine's own thread being one of the participants.
-----------------------------------------------------------------------------
*/

void DeferredGeometry::jobEntry(void *pContext, int jobIndex) {
	static_cast<DeferredGeometry *>(pContext)->runJob(jobIndex);
}

//Once, by one job.
void DeferredGeometry::runJob(int jobIndex) {
	const FSurfaceJob &job = m_pJobList[jobIndex];

	for (unsigned int ci = job.FirstCmd; ci <= job.LastCmd; ci++)
		runJobCommand(job, ci);
}

void DeferredGeometry::runJobCommand(const FSurfaceJob &job, unsigned int cmdIndex) {
	const FComplexSurfaceRecord &cmd = m_pCmds[cmdIndex];

	const unsigned int runFirstPoly = (cmdIndex == job.FirstCmd) ? job.FirstPoly : cmd.PassFirstPoly;
	const unsigned int runEndPoly = (cmdIndex == job.LastCmd) ? job.EndPoly : (cmd.PassFirstPoly + cmd.PassNumPolys);
	if (runFirstPoly >= runEndPoly)
		return;

	const unsigned int cmdFirstPoint = cmd.pPolyFirst[cmd.PassFirstPoly];
	const unsigned int cmdFirstIndex = cmd.pPolyIndexFirst[cmd.PassFirstPoly];
	const unsigned int jobFirstPoint = cmd.pPolyFirst[runFirstPoly];
	const unsigned int jobLastPoint = cmd.pPolyFirst[runEndPoly];

	const unsigned int vertexBase = cmd.PassVertexBase + (jobFirstPoint - cmdFirstPoint);
	const unsigned int indexBase = cmd.PassIndexBase + (cmd.pPolyIndexFirst[runFirstPoly] - cmdFirstIndex);

	const float xa0 = cmd.XAxis[0], xa1 = cmd.XAxis[1], xa2 = cmd.XAxis[2];
	const float ya0 = cmd.YAxis[0], ya1 = cmd.YAxis[1], ya2 = cmd.YAxis[2];
	const float uDot = cmd.UDot, vDot = cmd.VDot;
	const unsigned int layerMask = cmd.LayerMask;
	const DWORD flags = cmd.Flags;
	const DWORD texturePasses = cmd.TexturePasses;

	Vertex_ComplexSurface *pOut = m_pPassVerts + vertexBase;
	const Vec3 *pPoint = cmd.pPoints + jobFirstPoint;

	for (unsigned int i = jobFirstPoint; i < jobLastPoint; i++) {
		const float px = pPoint->x, py = pPoint->y, pz = pPoint->z;

		//Folded in here.
		const float uCoord = (xa0 * px + xa1 * py + xa2 * pz) - uDot;
		const float vCoord = (ya0 * px + ya1 * py + ya2 * pz) - vDot;

		for (unsigned int t = 0; t < 5; t++) {
			if (!(layerMask & (1u << t)))
				continue;
			const FSurfaceLayer &layer = cmd.Layers[t];
			pOut->TexCoord[t].x = (uCoord - layer.PanU) * layer.MultU + layer.OffsetU;
			pOut->TexCoord[t].y = (vCoord - layer.PanV) * layer.MultV + layer.OffsetV;
		}

		pOut->Pos.x = px;
		pOut->Pos.y = py;
		pOut->Pos.z = pz;
		pOut->flags = flags;
		pOut->texturePasses = texturePasses;

		pOut++;
		pPoint++;
	}

	int *pIndex = m_pPassIndices + indexBase;
	const unsigned int vertexOrigin = m_passBaseVertex + vertexBase;
	for (unsigned int poly = runFirstPoly; poly < runEndPoly; poly++) {
		const unsigned int first = vertexOrigin + (cmd.pPolyFirst[poly] - jobFirstPoint);
		const unsigned int count = cmd.pPolyFirst[poly + 1] - cmd.pPolyFirst[poly];
		for (unsigned int k = 1; k < count - 1; k++) {
			*pIndex++ = (int)first; //Fan centre
			*pIndex++ = (int)(first + k);
			*pIndex++ = (int)(first + k + 1);
		}
	}
}


//One pass per bufferful.
unsigned int DeferredGeometry::build(Shader *shader) {
	if (m_numCmds == 0 || shader == nullptr)
		return 0;

	DynamicGeometryBuffer *buf = static_cast<DynamicGeometryBuffer *>(shader->getGeometryBuffer());
	if (buf == nullptr) {
		m_numCmds = 0;
		return 0;
	}

	unsigned int draws = 0;
	unsigned int cmdIndex = 0;
	unsigned int polyCursor = 0;
	int stalls = 0;
	bool anyPassFaulted = false;

	while (cmdIndex < m_numCmds) {
		//--- As many as the buffer holds ---
		unsigned int passVerts = 0, passIndices = 0, passCmds = 0;
		const unsigned int firstPassCmd = cmdIndex;
		unsigned int c = cmdIndex, cursor = polyCursor;

		while (c < m_numCmds) {
			FComplexSurfaceRecord &cmd = m_pCmds[c];
			const unsigned int remaining = cmd.NumPolys - cursor;
			const unsigned int startPoint = cmd.pPolyFirst[cursor];
			const unsigned int startIndex = cmd.pPolyIndexFirst[cursor];

			unsigned int take = 0;
			for (; take < remaining; take++) {
				const unsigned int v = cmd.pPolyFirst[cursor + take + 1] - startPoint;
				const unsigned int ix = cmd.pPolyIndexFirst[cursor + take + 1] - startIndex;
				if (!buf->wouldFit(passVerts + v, passIndices + ix))
					break;
			}
			if (take == 0)
				break;

			cmd.PassFirstPoly = cursor;
			cmd.PassNumPolys = take;
			cmd.PassVertexBase = passVerts;
			cmd.PassIndexBase = passIndices;
			passVerts += cmd.pPolyFirst[cursor + take] - startPoint;
			passIndices += cmd.pPolyIndexFirst[cursor + take] - startIndex;
			passCmds++;

			cursor += take;
			if (cursor == cmd.NumPolys) {
				c++;
				cursor = 0;
			} else
				break; //Out of room mid-facet.
		}

		if (passCmds == 0) {
			//Nothing fits, so draw and discard and let the next attempt have the whole buffer, and a polygon
			//too big for even an empty one gets skipped after a couple of tries, so this never spins on
			//forever for something that will never fit.
			if (++stalls > 2) {
				UD3D10RenderDevice::debugs("Deferred build: a polygon is larger than the geometry buffer; skipped.");
				polyCursor++;
				if (polyCursor >= m_pCmds[cmdIndex].NumPolys) {
					cmdIndex++;
					polyCursor = 0;
				}
				stalls = 0;
				continue;
			}
			if (buf->hasContents()) {
				shader->apply(); //Unmaps and draws
				draws++;
			}
			buf->requestDiscard();
			continue;
		}
		stalls = 0;

		//Allocated before reserving.
		{
			unsigned int boundJobs = 0;
			for (unsigned int i = 0; i < passCmds; i++)
				boundJobs += m_pCmds[firstPassCmd + i].PassNumPolys;
			if (boundJobs > m_maxJobs) {
				unsigned int capacity = m_maxJobs ? m_maxJobs : 512;
				while (capacity < boundJobs)
					capacity *= 2;
				FSurfaceJob *pNew = (FSurfaceJob *)realloc(m_pJobList, capacity * sizeof(FSurfaceJob));
				if (pNew == nullptr) {
					UD3D10RenderDevice::debugs("Deferred build: out of memory for the job list.");
					//Otherwise a repeating failure never gives up.
					anyPassFaulted = true;
					break;
				}
				m_pJobList = pNew;
				m_maxJobs = capacity;
			}
		}

		void *pVerts = nullptr;
		int *pIndices = nullptr;
		unsigned int baseVertex = 0;
		if (!buf->reserveRange(passVerts, passIndices, &pVerts, &pIndices, &baseVertex)) {
			UD3D10RenderDevice::debugs("Deferred build: could not map the geometry buffer.");
			anyPassFaulted = true;
			break;
		}

		m_pPassVerts = (Vertex_ComplexSurface *)pVerts;
		m_pPassIndices = pIndices;
		m_passBaseVertex = baseVertex;

		//Jobs are balanced on output bytes and the cursor runs across the whole pass, so a job may end part
		//way through one facet and take up again in the next one without any special case for the boundary.
		m_numJobs = 0;
		const unsigned int participants = (unsigned int)numThreads();
		unsigned int jobFirstCmd = firstPassCmd;
		unsigned int jobFirstPoly = m_pCmds[firstPassCmd].PassFirstPoly;
		unsigned int jobBytes = 0;
		for (unsigned int i = 0; i < passCmds; i++) {
			const unsigned int ci = firstPassCmd + i;
			FComplexSurfaceRecord &cmd = m_pCmds[ci];
			const unsigned int firstPoly = cmd.PassFirstPoly;
			const unsigned int endPoly = firstPoly + cmd.PassNumPolys;

			for (unsigned int poly = firstPoly; poly < endPoly; poly++) {
				const unsigned int v = cmd.pPolyFirst[poly + 1] - cmd.pPolyFirst[poly];
				const unsigned int ix = cmd.pPolyIndexFirst[poly + 1] - cmd.pPolyIndexFirst[poly];
				jobBytes += v * sizeof(Vertex_ComplexSurface) + ix * sizeof(int);

				const bool last = ((i + 1 == passCmds) && (poly + 1 == endPoly));
				if (jobBytes >= JS_MIN_JOB_BYTES || last) {
					//Nobody to share with.
					if (participants <= 1 && !last)
						continue;
					m_pJobList[m_numJobs].FirstCmd = jobFirstCmd;
					m_pJobList[m_numJobs].FirstPoly = jobFirstPoly;
					m_pJobList[m_numJobs].LastCmd = ci;
					m_pJobList[m_numJobs].EndPoly = poly + 1;
					m_numJobs++;

					if (poly + 1 < endPoly) {
						jobFirstCmd = ci;
						jobFirstPoly = poly + 1;
					} else if (i + 1 < passCmds) {
						jobFirstCmd = ci + 1;
						jobFirstPoly = m_pCmds[ci + 1].PassFirstPoly;
					}
					jobBytes = 0;
				}
			}
		}

		//False means the dispatch unwound.
		bool passBuilt = true;
		if (m_numJobs > 0) {
			const DWORD start = appCycles();
			if (m_pJobs != nullptr) {
				passBuilt = m_pJobs->Dispatch((int)m_numJobs, jobEntry, this);
			} else {
				for (unsigned int j = 0; j < m_numJobs; j++)
					runJob((int)j);
			}
			m_microsecondsThisFrame += (unsigned int)((appCycles() - start) * GSecondsPerCycle * 1000000.0);
			m_jobsThisFrame += m_numJobs;
		}

		cmdIndex = c;
		polyCursor = cursor;

		if (!passBuilt) {
			//Indices could point anywhere.
			buf->releaseRange(passVerts, passIndices);
			anyPassFaulted = true;
		}
		else if (cmdIndex < m_numCmds) {
			shader->apply();
			draws++;
			buf->requestDiscard();
		}
	}

	//Consecutive faults only.
	if (anyPassFaulted) {
		enum { FAULT_LIMIT = 8 };
		if (++m_faultedBuilds >= FAULT_LIMIT && !m_gaveUp) {
			m_gaveUp = true;
			UD3D10RenderDevice::debugs("Deferred build faulted eight builds in a row; falling back to the inline path.");
		} else {
			UD3D10RenderDevice::debugs("Deferred build: geometry was dropped.");
		}
	} else {
		m_faultedBuilds = 0;
	}

	m_numCmds = 0;
	m_arena.Reset();
	return draws;
}
