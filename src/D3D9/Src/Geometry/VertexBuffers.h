/*=============================================================================
	VertexBuffers.h: the per-triangle vertex fillers for gouraud geometry.

	Include after D3D9.h.
=============================================================================*/

#pragma once

class UD3D9RenderDevice;

void FASTCALL Buffer3Verts(UD3D9RenderDevice *pRD, FTransTexture **Pts);
void FASTCALL Buffer3BasicVerts(UD3D9RenderDevice *pRD, FTransTexture **Pts);
void FASTCALL Buffer3ColoredVerts(UD3D9RenderDevice *pRD, FTransTexture **Pts);
void FASTCALL Buffer3FoggedVerts(UD3D9RenderDevice *pRD, FTransTexture **Pts);

#ifdef UTGLR_INCLUDE_SSE_CODE
void FASTCALL Buffer3ColoredVerts_SSE(UD3D9RenderDevice *pRD, FTransTexture **Pts);
void FASTCALL Buffer3ColoredVerts_SSE2(UD3D9RenderDevice *pRD, FTransTexture **Pts);
void FASTCALL Buffer3FoggedVerts_SSE(UD3D9RenderDevice *pRD, FTransTexture **Pts);
void FASTCALL Buffer3FoggedVerts_SSE2(UD3D9RenderDevice *pRD, FTransTexture **Pts);
#endif
