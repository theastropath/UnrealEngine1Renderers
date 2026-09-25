/*=============================================================================
	Support.cpp: the entry points that draw no geometry of their own.

	Depth clears between sends, the editor's hit proxy stack, reading the frame
	buffer back, and the fullscreen flash.
=============================================================================*/

#include "../OpenGLDrv.h"
#include "../OpenGL.h"

void UOpenGLRenderDevice::ClearZ(FSceneNode *Frame) {
	UTGLR_DEBUG_CALL_COUNT(ClearZ);
	guard(UOpenGLRenderDevice::ClearZ);

	EndDeferredGouraudPolys();
	EndBuffering();

	SetBlend(PF_Occlude);
	glClear(GL_DEPTH_BUFFER_BIT);

	unguard;
}

void UOpenGLRenderDevice::PushHit(const BYTE *Data, INT Count) {
	guard(UOpenGLRenderDevice::PushHit);

	INT i;

	EndDeferredGouraudPolys();
	EndBuffering();

	for (i = 0; i + 4 <= Count; i += 4) {
		DWORD hitName = *(DWORD *)(Data + i);
		m_gclip.PushHitName(hitName);
	}

	unguard;
}

void UOpenGLRenderDevice::PopHit(INT Count, UBOOL bForce) {
	guard(UOpenGLRenderDevice::PopHit);

	EndDeferredGouraudPolys();
	EndBuffering();

	INT i;
	bool selHit;

	selHit = m_gclip.CheckNewSelectHit();
	if (m_HitData && (selHit || bForce)) {
		DWORD nHitNameBytes;

		nHitNameBytes = m_gclip.GetHitNameStackSize() * 4;
		if ((m_HitBufSize > 0) && (nHitNameBytes <= (DWORD)m_HitBufSize)) {
			m_gclip.GetHitNameStackValues((unsigned int *)m_HitData, nHitNameBytes / 4);
			m_HitCount = nHitNameBytes;
		} else {
			m_HitCount = 0;
		}
	}

	for (i = 0; i + 4 <= Count; i += 4) {
		m_gclip.PopHitName();
	}

	unguard;
}


void UOpenGLRenderDevice::ReadPixels(FColor *Pixels) {
	guard(UOpenGLRenderDevice::ReadPixels);

	INT x, y;
	INT SizeX, SizeY;

	if (!Pixels || !Viewport) {
		return;
	}

	SizeX = Viewport->SizeX;
	SizeY = Viewport->SizeY;
	if ((SizeX <= 0) || (SizeY <= 0)) {
		return;
	}

	appMemzero(Pixels, SizeX * SizeY * sizeof(FColor));

#ifdef _WIN32
	if (!m_hRC || !m_hDC) {
		return;
	}
#endif

	MakeCurrent();

	EndDeferredGouraudPolys();
	EndBuffering();

	glReadPixels(0, 0, SizeX, SizeY, GL_RGBA, GL_UNSIGNED_BYTE, Pixels);

	//Flips top to bottom and swaps red and blue in one pass.
	for (y = 0; y < SizeY / 2; y++) {
		FColor *pTopRow = &Pixels[y * SizeX];
		FColor *pBottomRow = &Pixels[(SizeY - 1 - y) * SizeX];
		for (x = 0; x < SizeX; x++) {
			FColor &top = pTopRow[x];
			FColor &bottom = pBottomRow[x];

			Exchange(top.R, bottom.B);
			Exchange(top.G, bottom.G);
			Exchange(top.B, bottom.R);

			top.A = 255;
			bottom.A = 255;
		}
	}
	//An odd height leaves the middle row unpaired.
	if (SizeY & 1) {
		FColor *pMiddleRow = &Pixels[(SizeY / 2) * SizeX];
		for (x = 0; x < SizeX; x++) {
			FColor &middle = pMiddleRow[x];

			Exchange(middle.R, middle.B);
			middle.A = 255;
		}
	}

	unguard;
}

void UOpenGLRenderDevice::EndFlash() {
	UTGLR_DEBUG_CALL_COUNT(EndFlash);
	guard(UOpenGLRenderDevice::EndFlash);

	EndDeferredGouraudPolys();

	if ((FlashScale != FPlane(0.5f, 0.5f, 0.5f, 0.0f)) || (FlashFog != FPlane(0.0f, 0.0f, 0.0f, 0.0f))) {
		EndBuffering();

		SetDefaultAAState();
		SetDefaultProjectionState();
		SetDefaultColorState();
		SetDefaultShaderState();
		SetDefaultTextureState();

		SetBlend(PF_Highlighted);
		SetNoTexture(0);

		SetColor4f(FlashFog.X, FlashFog.Y, FlashFog.Z, 1.0f - Min(FlashScale.X * 2.0f, 1.0f));

		FLOAT RFX2 = m_RProjZ;
		FLOAT RFY2 = m_RProjZ * m_Aspect;

		FLOAT ZCoord = 1.0f;
		if (m_useZRangeHack) {
			ZCoord = (((ZCoord - 0.5f) / 7.5f) * 4.0f) + 4.0f;
		}

		glBegin(GL_TRIANGLE_FAN);
		glVertex3f(RFX2 * (-1.0f * ZCoord), RFY2 * (-1.0f * ZCoord), ZCoord);
		glVertex3f(RFX2 * (+1.0f * ZCoord), RFY2 * (-1.0f * ZCoord), ZCoord);
		glVertex3f(RFX2 * (+1.0f * ZCoord), RFY2 * (+1.0f * ZCoord), ZCoord);
		glVertex3f(RFX2 * (-1.0f * ZCoord), RFY2 * (+1.0f * ZCoord), ZCoord);
		glEnd();
	}
	unguard;
}
