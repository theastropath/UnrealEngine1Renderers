/*=============================================================================
	SceneNode.cpp: the viewport and frustum the geometry belongs to.
=============================================================================*/

#include "../OpenGLDrv.h"
#include "../OpenGL.h"

void UOpenGLRenderDevice::SetSceneNode(FSceneNode *Frame) {
	UTGLR_DEBUG_CALL_COUNT(SetSceneNode);
	guard(UOpenGLRenderDevice::SetSceneNode);

	//Against the old viewport.
	EndDeferredGouraudPolys();

	EndBuffering(); //Flush before the projection changes.

	m_sceneNodeCount++;

	//This path always sets AA and projection itself.
	SetDefaultColorState();
	SetDefaultShaderState();
	SetDefaultTextureState();

	FLOAT rcpFrameFX = 1.0f / Frame->FX;

	//Derived from the scene node's own projection.
	//A portal or mirror can keep its parent's projection.
	//Field of view is the fallback.
	const FLOAT MAX_CLIPPING_NUMBER = 100.0f; //Slopes.
	FLOAT prjX = Frame->FX2 * Frame->RProj.Z;
	FLOAT prjY = Frame->FY2 * Frame->RProj.Z;
	//Ortho has no frustum.
	if (!Frame->Viewport->IsOrtho() &&
		(prjX > 0.0f) && (prjX < MAX_CLIPPING_NUMBER) && (prjY > 0.0f) && (prjY < MAX_CLIPPING_NUMBER)) {
		m_RProjZ = prjX;
		m_Aspect = prjY / prjX;
	} else {
		m_Aspect = Frame->FY * rcpFrameFX;
		//No actor between levels.
		FLOAT fovAngle = Viewport->Actor ? Viewport->Actor->FovAngle : 0.0f;
		if ((fovAngle <= 0.0f) || (fovAngle >= 180.0f)) {
			fovAngle = 90.0f;
		}
		m_RProjZ = appTan(fovAngle * PI / 360.0);
	}

	//Per-axis view-space pixel extent at Z=1.
	m_RFX2 = 2.0f * m_RProjZ * rcpFrameFX;
	m_RFY2 = 2.0f * m_Aspect * m_RProjZ / Frame->FY;

	//Cached so an unannounced scene node is still detected.
	//The key covers every field read above.
	BuildSceneNodeKey(Frame, m_sceneNodeKey);

	SetViewport(Frame->XB, Viewport->SizeY - Frame->Y - Frame->YB, Frame->X, Frame->Y);

	//Also covers the editor's coplanar brush faces.
	//The oldest engine cannot single out the weapon.
	m_useZRangeHack = false;
#ifndef UTGLR_OLD_URENDERDEVICE
	if (ZRangeHack) {
		m_useZRangeHack = true;
	}
#endif

	if (Frame->Viewport->IsOrtho()) {
		m_useZRangeHack = false;

		SetOrthoProjection();
	} else {
		SetProjectionStateNoCheck(false);
	}

	if (m_HitData) {
		if (Frame->Viewport->IsOrtho()) {
			float cp[4];
			FLOAT nX = Viewport->HitX - Frame->FX2;
			FLOAT pX = nX + Viewport->HitXL;
			FLOAT nY = Viewport->HitY - Frame->FY2;
			FLOAT pY = nY + Viewport->HitYL;

			nX *= m_RFX2 * 0.5f;
			pX *= m_RFX2 * 0.5f;
			nY *= m_RFY2 * 0.5f;
			pY *= m_RFY2 * 0.5f;

			cp[0] = +1.0;
			cp[1] = 0.0;
			cp[2] = 0.0;
			cp[3] = -nX;
			m_gclip.SetCp(0, cp);
			m_gclip.SetCpEnable(0, true);

			cp[0] = 0.0;
			cp[1] = +1.0;
			cp[2] = 0.0;
			cp[3] = -nY;
			m_gclip.SetCp(1, cp);
			m_gclip.SetCpEnable(1, true);

			cp[0] = -1.0;
			cp[1] = 0.0;
			cp[2] = 0.0;
			cp[3] = +pX;
			m_gclip.SetCp(2, cp);
			m_gclip.SetCpEnable(2, true);

			cp[0] = 0.0;
			cp[1] = -1.0;
			cp[2] = 0.0;
			cp[3] = +pY;
			m_gclip.SetCp(3, cp);
			m_gclip.SetCpEnable(3, true);

			//Near clip plane.
			cp[0] = 0.0f;
			cp[1] = 0.0f;
			cp[2] = 1.0f;
			cp[3] = -0.5f;
			m_gclip.SetCp(4, cp);
			m_gclip.SetCpEnable(4, true);
		} else {
			FVector N[4];
			float cp[4];
			INT i;

			FLOAT nX = Viewport->HitX - Frame->FX2;
			FLOAT pX = nX + Viewport->HitXL;
			FLOAT nY = Viewport->HitY - Frame->FY2;
			FLOAT pY = nY + Viewport->HitYL;

			N[0] = (FVector(nX * Frame->RProj.Z, 0, 1) ^ FVector(0, -1, 0)).SafeNormal();
			N[1] = (FVector(pX * Frame->RProj.Z, 0, 1) ^ FVector(0, +1, 0)).SafeNormal();
			N[2] = (FVector(0, nY * Frame->RProj.Z, 1) ^ FVector(+1, 0, 0)).SafeNormal();
			N[3] = (FVector(0, pY * Frame->RProj.Z, 1) ^ FVector(-1, 0, 0)).SafeNormal();

			for (i = 0; i < 4; i++) {
				cp[0] = N[i].X;
				cp[1] = N[i].Y;
				cp[2] = N[i].Z;
				cp[3] = 0.0f;
				m_gclip.SetCp(i, cp);
				m_gclip.SetCpEnable(i, true);
			}

			//Near clip plane.
			cp[0] = 0.0f;
			cp[1] = 0.0f;
			cp[2] = 1.0f;
			cp[3] = -0.5f;
			m_gclip.SetCp(4, cp);
			m_gclip.SetCpEnable(4, true);
		}
	}

	unguard;
}
