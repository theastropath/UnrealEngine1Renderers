#include "../OpenGLDrv.h"
#include "../OpenGL.h"

#ifdef UTGLR_RUNE_BUILD
void UOpenGLRenderDevice::PreDrawFogSurface() {
	UTGLR_DEBUG_CALL_COUNT(PreDrawFogSurface);
	guard(UOpenGLRenderDevice::PreDrawFogSurface);

	EndDeferredGouraudPolys();
	EndBuffering();

	SetDefaultAAState();
	SetDefaultProjectionState();
	SetDefaultColorState();
	SetDefaultShaderState();
	SetDefaultTextureState();

	SetBlend(PF_AlphaBlend);
	SetNoTexture(0);

	unguard;
}

void UOpenGLRenderDevice::PostDrawFogSurface() {
	UTGLR_DEBUG_CALL_COUNT(PostDrawFogSurface);
	guard(UOpenGLRenderDevice::PostDrawFogSurface);

	SetBlend(0);

	unguard;
}

void UOpenGLRenderDevice::DrawFogSurface(FSceneNode *Frame, FFogSurf &FogSurf) {
	UTGLR_DEBUG_CALL_COUNT(DrawFogSurface);
	guard(UOpenGLRenderDevice::DrawFogSurface);

	if (FogSurf.FogDistance <= 0.0f) {
		return;
	}

	EndDeferredGouraudPolys();
	EndBuffering();

	if (SceneNodeChanged(Frame)) {
		m_sceneNodeRefreshCount++;
		SetSceneNode(Frame);
	}

	FPlane Modulate(FogSurf.FogColor.X, FogSurf.FogColor.Y, FogSurf.FogColor.Z, 0.0f);

	FLOAT RFogDistance = 1.0f / FogSurf.FogDistance;

	if (FogSurf.PolyFlags & PF_Masked) {
		glDepthFunc(GL_EQUAL);
	}

	for (FSavedPoly *Poly = FogSurf.Polys; Poly; Poly = Poly->Next) {
		glBegin(GL_TRIANGLE_FAN);
		for (INT i = 0; i < Poly->NumPts; i++) {
			Modulate.W = Poly->Pts[i]->Point.Z * RFogDistance;
			if (Modulate.W > 1.0f) {
				Modulate.W = 1.0f;
			} else if (Modulate.W < 0.0f) {
				Modulate.W = 0.0f;
			}
			SetColor4fv(&Modulate.X);
			glVertex3fv(&Poly->Pts[i]->Point.X);
		}
		glEnd();
	}

	if (FogSurf.PolyFlags & PF_Masked) {
		glDepthFunc(ZTrickFunc);
	}

	unguard;
}

void UOpenGLRenderDevice::PreDrawGouraud(FSceneNode *Frame, FLOAT FogDistance, FPlane FogColor) {
	UTGLR_DEBUG_CALL_COUNT(PreDrawGouraud);
	guard(UOpenGLRenderDevice::PreDrawGouraud);

	EndDeferredGouraudPolys();

	if (FogDistance > 0.0f) {
		EndBuffering();

		m_gpFogEnabled = true;
		glEnable(GL_FOG);

		glFogfv(GL_FOG_COLOR, &FogColor.X);
		//		glFogf(GL_FOG_START, 0.0f);
		glFogf(GL_FOG_END, FogDistance);
	}

	unguard;
}

void UOpenGLRenderDevice::PostDrawGouraud(FLOAT FogDistance) {
	UTGLR_DEBUG_CALL_COUNT(PostDrawGouraud);
	guard(UOpenGLRenderDevice::PostDrawGouraud);

	EndDeferredGouraudPolys();

	if (FogDistance > 0.0f) {
		EndBuffering();

		m_gpFogEnabled = false;
		glDisable(GL_FOG);
	}

	unguard;
}
#endif
