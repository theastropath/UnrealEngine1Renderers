
#include "../D3D9Drv.h"
#include "../D3D9.h"

void UD3D9RenderDevice::EndBufferingNoCheck(void) {
	//A batch excludes a log.
	check(m_drawCmds.empty());

	switch (m_bufferedVertsType) {
		case BV_TYPE_COMPLEX_SURFACE:
			EndComplexSurfaceBufferingNoCheck();
			break;

		case BV_TYPE_GOURAUD_POLYS:
			EndGouraudPolygonBufferingNoCheck();
			break;

		case BV_TYPE_TILES:
			EndTileBufferingNoCheck();
			break;

		case BV_TYPE_LINES:
			EndLineBufferingNoCheck();
			break;

		case BV_TYPE_POINTS:
			EndPointBufferingNoCheck();
			break;

		default:;
	}

	m_bufferedVerts = 0;

	return;
}

void UD3D9RenderDevice::EndGouraudPolygonBufferingNoCheck(void) {
	SetDefaultAAState();

	clock(GouraudCycles);

	SetProjectionState(m_requestNearZRangeHackProjection);

	UnlockVertexColorBuffer();
	if (m_requestedColorFlags & CF_FOG_MODE) {
		UnlockSecondaryColorBuffer();
	}
	UnlockTexCoordBuffer(0);

#ifdef UTGLR_DEBUG_ACTOR_WIREFRAME
	m_d3dDevice->SetRenderState(D3DRS_FILLMODE, D3DFILL_WIREFRAME);
#endif

	m_d3dDevice->DrawPrimitive(D3DPT_TRIANGLELIST, m_curVertexBufferPos, m_bufferedVerts / 3);

	m_curVertexBufferPos += m_bufferedVerts;

#ifdef UTGLR_DEBUG_ACTOR_WIREFRAME
	m_d3dDevice->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
#endif

	unclock(GouraudCycles);
}

void UD3D9RenderDevice::EndTileBufferingNoCheck(void) {
	if (NoAATiles) {
		SetDisabledAAState();
	} else {
		SetDefaultAAState();
	}
	SetDefaultProjectionState();

	clock(TileCycles);

	UnlockVertexColorBuffer();
	UnlockTexCoordBuffer(0);

	m_d3dDevice->DrawPrimitive(D3DPT_TRIANGLELIST, m_curVertexBufferPos, m_bufferedVerts / 3);

	m_curVertexBufferPos += m_bufferedVerts;

	unclock(TileCycles);
}

void UD3D9RenderDevice::EndLineBufferingNoCheck(void) {
	UnlockVertexColorBuffer();
	UnlockTexCoordBuffer(0);

	m_d3dDevice->DrawPrimitive(D3DPT_LINELIST, m_curVertexBufferPos, m_bufferedVerts / 2);

	m_curVertexBufferPos += m_bufferedVerts;
}

void UD3D9RenderDevice::EndPointBufferingNoCheck(void) {
	UnlockVertexColorBuffer();
	UnlockTexCoordBuffer(0);

	m_d3dDevice->DrawPrimitive(D3DPT_TRIANGLELIST, m_curVertexBufferPos, m_bufferedVerts / 3);

	m_curVertexBufferPos += m_bufferedVerts;
}
