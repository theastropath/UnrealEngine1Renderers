#include "../OpenGLDrv.h"
#include "../OpenGL.h"

void UOpenGLRenderDevice::EndGouraudPolygonBufferingNoCheck(void) {
	SetDefaultAAState();

	clock(GouraudCycles);

	SetProjectionState(m_requestNearZRangeHackProjection);

	SetColorState();

	//After the color state is settled.
	UploadGouraudStreams(BufferedVerts);

#ifdef UTGLR_DEBUG_ACTOR_WIREFRAME
	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
#endif

	glDrawArrays(GL_TRIANGLES, 0, BufferedVerts);
	InvalidateColorShadow();
	InvalidateTexAttribShadows();

#ifdef UTGLR_DEBUG_ACTOR_WIREFRAME
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
#endif

	BufferedVerts = 0;

	unclock(GouraudCycles);
}

void UOpenGLRenderDevice::EndTileBufferingNoCheck(void) {
	if (NoAATiles) {
		SetDisabledAAState();
	} else {
		SetDefaultAAState();
	}
	SetDefaultProjectionState();

	clock(TileCycles);

	SetColorState();

	UploadTileStreams(BufferedTileVerts);

	glDrawArrays(GL_QUADS, 0, BufferedTileVerts);
	InvalidateColorShadow();
	InvalidateTexAttribShadows();

	BufferedTileVerts = 0;

	unclock(TileCycles);
}

void UOpenGLRenderDevice::EndLineBufferingNoCheck(void) {
	SetColorState();

	UploadTileStreams(BufferedLineVerts);

	glDrawArrays(GL_LINES, 0, BufferedLineVerts);
	InvalidateColorShadow();
	InvalidateTexAttribShadows();

	BufferedLineVerts = 0;
}

void UOpenGLRenderDevice::EndPointBufferingNoCheck(void) {
	SetColorState();

	UploadTileStreams(BufferedPointVerts);

	glDrawArrays(GL_QUADS, 0, BufferedPointVerts);
	InvalidateColorShadow();
	InvalidateTexAttribShadows();

	BufferedPointVerts = 0;
}
