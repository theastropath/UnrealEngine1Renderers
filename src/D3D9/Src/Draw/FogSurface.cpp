
#include "../D3D9Drv.h"
#include "../D3D9.h"

#ifdef UTGLR_RUNE_BUILD
void UD3D9RenderDevice::PreDrawFogSurface() {
#ifdef UTGLR_DEBUG_SHOW_CALL_COUNTS
	{
		static int si;
		dout << L"utd3d9r: PreDrawFogSurface = " << si++ << std::endl;
	}
#endif
	guard(UD3D9RenderDevice::PreDrawFogSurface);

	if (m_frameSkipped) {
		return;
	}

	EndDeferredGouraudPolys();
	FlushDeferred();
	EndBuffering();

	SetDefaultAAState();
	SetDefaultProjectionState();
	SetDefaultStreamState();
	SetDefaultTextureState();

	SetBlend(PF_AlphaBlend);

	SetNoTexture(0);

	unguard;
}

void UD3D9RenderDevice::PostDrawFogSurface() {
#ifdef UTGLR_DEBUG_SHOW_CALL_COUNTS
	{
		static int si;
		dout << L"utd3d9r: PostDrawFogSurface = " << si++ << std::endl;
	}
#endif
	guard(UD3D9RenderDevice::PostDrawFogSurface);

	if (m_frameSkipped) {
		return;
	}

	EndDeferredGouraudPolys();
	FlushDeferred();
	EndBuffering();

	SetBlend(0);

	unguard;
}

void UD3D9RenderDevice::DrawFogSurface(FSceneNode *Frame, FFogSurf &FogSurf) {
#ifdef UTGLR_DEBUG_SHOW_CALL_COUNTS
	{
		static int si;
		dout << L"utd3d9r: DrawFogSurface = " << si++ << std::endl;
	}
#endif
	guard(UD3D9RenderDevice::DrawFogSurface);

	//Catches NaN too.
	if (m_frameSkipped || !(FogSurf.FogDistance > 0.0f)) {
		return;
	}

	EndDeferredGouraudPolys();
	FlushDeferred();
	EndBuffering();

	if (SceneNodeChanged(Frame)) {
		m_sceneNodeRefreshCount++;
		SetSceneNode(Frame);
	}

	SetBlend(PF_AlphaBlend);
	SetNoTexture(0);

	FPlane Modulate(Clamp(FogSurf.FogColor.X, 0.0f, 1.0f), Clamp(FogSurf.FogColor.Y, 0.0f, 1.0f), Clamp(FogSurf.FogColor.Z, 0.0f, 1.0f), 0.0f);

	FLOAT RFogDistance = 1.0f / FogSurf.FogDistance;

	if (FogSurf.PolyFlags & PF_Masked) {
		m_d3dDevice->SetRenderState(D3DRS_ZFUNC, D3DCMP_EQUAL);
	}

	SetDefaultStreamState();

	for (FSavedPoly *Poly = FogSurf.Polys; Poly; Poly = Poly->Next) {
		INT NumPts = Poly->NumPts;
		if ((NumPts < 3) || (NumPts > VERTEX_ARRAY_SIZE)) {
			continue;
		}

		if ((m_curVertexBufferPos + NumPts) >= VERTEX_RING_SIZE) {
			FlushVertexBuffers();
		}

		LockVertexColorBuffer();
		LockTexCoordBuffer(0);

		FGLVertexColor *pVertexColorArray = m_pVertexColorArray;
		FGLTexCoord *pTexCoordArray = m_pTexCoordArray[0];
		FTransform **pPts = &Poly->Pts[0];
		for (INT i = 0; i < NumPts; i++) {
			const FVector &Point = pPts[i]->Point;

			Modulate.W = Point.Z * RFogDistance;
			if (Modulate.W > 1.0f) {
				Modulate.W = 1.0f;
			} else if (Modulate.W < 0.0f) {
				Modulate.W = 0.0f;
			}

			FGLVertexColor &destVertexColor = pVertexColorArray[i];
			destVertexColor.x = Point.X;
			destVertexColor.y = Point.Y;
			destVertexColor.z = Point.Z;
			//Behind the eye wraps.
			destVertexColor.color = FPlaneTo_BGRAClamped(&Modulate);

			FGLTexCoord &destTexCoord = pTexCoordArray[i];
			destTexCoord.u = 0.0f;
			destTexCoord.v = 0.0f;
		}

		UnlockVertexColorBuffer();
		UnlockTexCoordBuffer(0);

		m_d3dDevice->DrawPrimitive(D3DPT_TRIANGLEFAN, m_curVertexBufferPos, NumPts - 2);

		m_curVertexBufferPos += NumPts;
	}

	if (FogSurf.PolyFlags & PF_Masked) {
		m_d3dDevice->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
	}

	unguard;
}

void UD3D9RenderDevice::PreDrawGouraud(FSceneNode *Frame, FLOAT FogDistance, FPlane FogColor) {
#ifdef UTGLR_DEBUG_SHOW_CALL_COUNTS
	{
		static int si;
		dout << L"utd3d9r: PreDrawGouraud = " << si++ << std::endl;
	}
#endif
	guard(UD3D9RenderDevice::PreDrawGouraud);

	if (m_frameSkipped) {
		return;
	}

	EndDeferredGouraudPolys();
	FlushDeferred();

	if (FogDistance > 0.0f) {
		EndBuffering();

		m_gpFogEnabled = true;
		if (UseFragmentProgram) {
			FLOAT psParams[8];
			FLOAT rcpFogLen;

			psParams[0] = FogColor.X;
			psParams[1] = FogColor.Y;
			psParams[2] = FogColor.Z;
			psParams[3] = FogColor.W;

			rcpFogLen = 1.0f / (FogDistance - 0.0f);
			psParams[4] = rcpFogLen;
			psParams[5] = FogDistance * rcpFogLen;
			psParams[6] = 0.0f;
			psParams[7] = 0.0f;

			m_d3dDevice->SetPixelShaderConstantF(3, psParams, 2);
		} else {
			m_d3dDevice->SetRenderState(D3DRS_FOGENABLE, TRUE);

			m_d3dDevice->SetRenderState(D3DRS_FOGCOLOR, FPlaneTo_BGRAClamped(&FogColor));
			FLOAT fFogDistance = FogDistance;
			m_d3dDevice->SetRenderState(D3DRS_FOGEND, *(DWORD *)&fFogDistance);
		}
	}

	unguard;
}

void UD3D9RenderDevice::PostDrawGouraud(FLOAT FogDistance) {
#ifdef UTGLR_DEBUG_SHOW_CALL_COUNTS
	{
		static int si;
		dout << L"utd3d9r: PostDrawGouraud = " << si++ << std::endl;
	}
#endif
	guard(UD3D9RenderDevice::PostDrawGouraud);

	if (m_frameSkipped) {
		return;
	}

	EndDeferredGouraudPolys();
	FlushDeferred();

	if (FogDistance > 0.0f) {
		EndBuffering();

		m_gpFogEnabled = false;
		if (UseFragmentProgram) {
		} else {
			m_d3dDevice->SetRenderState(D3DRS_FOGENABLE, FALSE);
		}
	}

	unguard;
}
#endif
