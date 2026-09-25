/*=============================================================================
	VertexBuffers.h: the per-triangle vertex fillers for gouraud geometry.

	Include after OpenGL.h.
=============================================================================*/

#pragma once

class UOpenGLRenderDevice;

void FASTCALL Buffer3Verts(UOpenGLRenderDevice *pRD, FTransTexture **Pts);
void FASTCALL Buffer3BasicVerts(UOpenGLRenderDevice *pRD, FTransTexture **Pts);
void FASTCALL Buffer3ColoredVerts(UOpenGLRenderDevice *pRD, FTransTexture **Pts);
void FASTCALL Buffer3FoggedVerts(UOpenGLRenderDevice *pRD, FTransTexture **Pts);

#ifdef UTGLR_INCLUDE_SSE_CODE
void FASTCALL Buffer3ColoredVerts_SSE(UOpenGLRenderDevice *pRD, FTransTexture **Pts);
void FASTCALL Buffer3ColoredVerts_SSE2(UOpenGLRenderDevice *pRD, FTransTexture **Pts);
void FASTCALL Buffer3FoggedVerts_SSE(UOpenGLRenderDevice *pRD, FTransTexture **Pts);
void FASTCALL Buffer3FoggedVerts_SSE2(UOpenGLRenderDevice *pRD, FTransTexture **Pts);
#endif
