/*=============================================================================
	Support.cpp: the entry points that draw no geometry of their own.

	Depth clears between sends, the editor's hit proxy stack, reading the frame
	buffer back, and the fullscreen flash.
=============================================================================*/

#include "../D3D9Drv.h"
#include "../D3D9.h"

void UD3D9RenderDevice::ClearZ(FSceneNode *Frame) {
#ifdef UTGLR_DEBUG_SHOW_CALL_COUNTS
	{
		static int si;
		dout << L"utd3d9r: ClearZ = " << si++ << std::endl;
	}
#endif
	guard(UD3D9RenderDevice::ClearZ);

	if (m_frameSkipped) {
		return;
	}

	EndDeferredGouraudPolys(); //Belongs to that depth.
	FlushDeferred(); //Recorded geometry too.
	EndBuffering();

	//A clear with no rect is clipped to the current viewport, so the scene node has to be refreshed
	//here as well, and without that a root node the engine never re-announced after a portal or
	//mirror child would clear only the child's sub-rectangle, leaving the player's weapon to
	//depth-clip into the walls.
	if (SceneNodeChanged(Frame)) {
		m_sceneNodeRefreshCount++;
		SetSceneNode(Frame);
	}


	SetBlend(PF_Occlude);
	m_d3dDevice->Clear(0, NULL, D3DCLEAR_ZBUFFER, 0, 1.0f, 0);

	unguard;
}

void UD3D9RenderDevice::PushHit(const BYTE *Data, INT Count) {
	guard(UD3D9RenderDevice::PushHit);

	INT i;

	EndDeferredGouraudPolys();
	//The hit stack names what is drawn after it.
	//Nothing recorded before may still be pending.
	FlushDeferred();
	EndBuffering();

	//Add to stack. The bound is what the read needs.
	for (i = 0; i + 4 <= Count; i += 4) {
		DWORD hitName = *(DWORD *)(Data + i);
		m_gclip.PushHitName(hitName);
	}

	unguard;
}

void UD3D9RenderDevice::PopHit(INT Count, UBOOL bForce) {
	guard(UD3D9RenderDevice::PopHit);

	EndDeferredGouraudPolys();
	FlushDeferred();
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


void UD3D9RenderDevice::ReadPixels(FColor *Pixels) {
	guard(UD3D9RenderDevice::ReadPixels);

	//Drained in the order every other sync point uses:
	//replay held polygons, flush what the replay records, then close any open batch.
	EndDeferredGouraudPolys();
	FlushDeferred();
	EndBuffering();

	INT x, y;
	INT SizeX, SizeY;
	INT StartX = 0, StartY = 0;
	HRESULT hResult;
	IDirect3DSurface9 *d3dsFrontBuffer = NULL;
	D3DDISPLAYMODE d3ddm;
	HDC hDibDC = 0;
	HBITMAP hDib = 0;
	LPVOID pDibData = 0;
	DWORD *pScreenshot = 0;
	INT screenshotPitch;

	SizeX = Viewport->SizeX;
	SizeY = Viewport->SizeY;

	//The engine hands in an uninitialized array, so a failed capture comes out black.
	if ((SizeX > 0) && (SizeY > 0)) {
		appMemzero(Pixels, SizeX * SizeY * sizeof(FColor));
	}

	if (!m_d3dDevice) {
		return;
	}

	hResult = m_d3dDevice->GetDisplayMode(0, &d3ddm);
	if (FAILED(hResult)) {
		return;
	}

	//The front buffer is the only capture that still works windowed on a composited desktop.
	bool canReadFrontBuffer = true;
	if (m_d3dpp.Windowed) {
		POINT clientOrigin;

		canReadFrontBuffer = false;

		clientOrigin.x = 0;
		clientOrigin.y = 0;
		if (ClientToScreen(m_hWnd, &clientOrigin)) {
			HMONITOR hMonitor = m_d3d9->GetAdapterMonitor(D3DADAPTER_DEFAULT);
			MONITORINFO monitorInfo;

			monitorInfo.cbSize = sizeof(monitorInfo);
			if (hMonitor && GetMonitorInfo(hMonitor, &monitorInfo)) {
				clientOrigin.x -= monitorInfo.rcMonitor.left;
				clientOrigin.y -= monitorInfo.rcMonitor.top;
			}

			//Window capture instead.
			if ((clientOrigin.x >= 0) && (clientOrigin.y >= 0) &&
				((clientOrigin.x + SizeX) <= (INT)d3ddm.Width) &&
				((clientOrigin.y + SizeY) <= (INT)d3ddm.Height)) {
				StartX = clientOrigin.x;
				StartY = clientOrigin.y;
				canReadFrontBuffer = true;
			}
		}
	}

	if (canReadFrontBuffer) {
		//D3DPOOL_SYSTEMMEM is what the capture API documents.
		hResult = m_d3dDevice->CreateOffscreenPlainSurface(d3ddm.Width, d3ddm.Height, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &d3dsFrontBuffer, NULL);
		if (FAILED(hResult)) {
			debugf(NAME_Warning, TEXT("Screenshot surface creation failed (0x%08X)"), hResult);
			d3dsFrontBuffer = NULL;
		} else {
			hResult = m_d3dDevice->GetFrontBufferData(0, d3dsFrontBuffer);
			if (FAILED(hResult)) {
				debugf(NAME_Warning, TEXT("GetFrontBufferData failed (0x%08X)"), hResult);

				d3dsFrontBuffer->Release();
				d3dsFrontBuffer = NULL;
			}
		}
	}

	if (d3dsFrontBuffer) {
		if ((StartX + SizeX) > (INT)d3ddm.Width) SizeX = (INT)d3ddm.Width - StartX;
		if ((StartY + SizeY) > (INT)d3ddm.Height) SizeY = (INT)d3ddm.Height - StartY;
		if (SizeX < 0) SizeX = 0;
		if (SizeY < 0) SizeY = 0;

		D3DLOCKED_RECT lockRect;
		hResult = d3dsFrontBuffer->LockRect(&lockRect, NULL, D3DLOCK_NOSYSLOCK | D3DLOCK_READONLY);
		if (FAILED(hResult)) {
			d3dsFrontBuffer->Release();
			d3dsFrontBuffer = NULL;
		} else {
			pScreenshot = (DWORD *)lockRect.pBits;
			screenshotPitch = lockRect.Pitch;
		}
	}

	if (!pScreenshot && m_d3dpp.Windowed) {
		struct {
			BITMAPINFOHEADER bmiHeader;
			DWORD bmiColors[3];
		} bmi;
		HBITMAP hOldBitmap;

		StartX = 0;
		StartY = 0;
		SizeX = Viewport->SizeX;
		SizeY = Viewport->SizeY;

		hDibDC = CreateCompatibleDC(m_hDC);
		if (!hDibDC) {
			return;
		}

		appMemzero(&bmi.bmiHeader, sizeof(bmi.bmiHeader));
		bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
		bmi.bmiHeader.biWidth = SizeX;
		bmi.bmiHeader.biHeight = -SizeY;
		bmi.bmiHeader.biPlanes = 1;
		bmi.bmiHeader.biBitCount = 32;
		bmi.bmiHeader.biCompression = BI_BITFIELDS;
		bmi.bmiHeader.biSizeImage = SizeX * SizeY * 4;
		bmi.bmiHeader.biXPelsPerMeter = 0;
		bmi.bmiHeader.biYPelsPerMeter = 0;
		bmi.bmiHeader.biClrUsed = 0;
		bmi.bmiHeader.biClrImportant = 0;
		bmi.bmiColors[0] = 0x00FF0000;
		bmi.bmiColors[1] = 0x0000FF00;
		bmi.bmiColors[2] = 0x000000FF;

		hDib = CreateDIBSection(
			hDibDC,
			(BITMAPINFO *)&bmi,
			DIB_RGB_COLORS,
			&pDibData,
			NULL,
			0);
		if (!hDib) {
			DeleteDC(hDibDC);
			return;
		}

		hOldBitmap = (HBITMAP)SelectObject(hDibDC, hDib);
		BitBlt(hDibDC, 0, 0, SizeX, SizeY, m_hDC, 0, 0, SRCCOPY);
		SelectObject(hDibDC, hOldBitmap);

		pScreenshot = (DWORD *)pDibData;
		screenshotPitch = bmi.bmiHeader.biWidth * 4;
	}


	if (pScreenshot) {
		INT DestSizeX = Viewport->SizeX;

		/*
		Display order: the source word is A8R8G8B8, so byte 0 is blue and byte 2 is red.
		Do not "fix" it by swapping the arguments - that swaps red and blue in every
		screenshot.
		*/
		pScreenshot = (DWORD *)((BYTE *)pScreenshot + (StartY * screenshotPitch));
		for (y = 0; y < SizeY; y++) {
			for (x = 0; x < SizeX; x++) {
				DWORD dwPixel = pScreenshot[StartX + x];
				const BYTE blue = (BYTE)((dwPixel >> 0) & 0xFF);
				const BYTE green = (BYTE)((dwPixel >> 8) & 0xFF);
				const BYTE red = (BYTE)((dwPixel >> 16) & 0xFF);
				Pixels[(y * DestSizeX) + x] = FColor(blue, green, red, 0xFF);
			}
			pScreenshot = (DWORD *)((BYTE *)pScreenshot + screenshotPitch);
		}
	}


	if (d3dsFrontBuffer) {
		d3dsFrontBuffer->UnlockRect();

		d3dsFrontBuffer->Release();
	}
	if (hDib) {
		DeleteObject(hDib);
	}
	if (hDibDC) {
		DeleteDC(hDibDC);
	}


	//Gamma belongs to the display.

	unguard;
}

void UD3D9RenderDevice::EndFlash() {
#ifdef UTGLR_DEBUG_SHOW_CALL_COUNTS
	{
		static int si;
		dout << L"utd3d9r: EndFlash = " << si++ << std::endl;
	}
#endif
	guard(UD3D9RenderDevice::EndFlash);

	if (m_frameSkipped) {
		return;
	}

	EndDeferredGouraudPolys();
	FlushDeferred();

	if ((FlashScale != FPlane(0.5f, 0.5f, 0.5f, 0.0f)) || (FlashFog != FPlane(0.0f, 0.0f, 0.0f, 0.0f))) {
		EndBuffering();

		SetDefaultAAState();
		SetDefaultProjectionState();
		SetDefaultStreamState();
		SetDefaultTextureState();

		SetBlend(PF_Highlighted);
		SetNoTexture(0);

		//Clamped like every other screen space colour here.
		//FlashFog comes from the game, and a value above 1.0 wraps modulo 256 and tints the overlay the other way.
		FPlane tempPlane = FPlane(FlashFog.X, FlashFog.Y, FlashFog.Z, 1.0f - Min(FlashScale.X * 2.0f, 1.0f));
		DWORD flashColor = FPlaneTo_BGRAClamped(&tempPlane);

		FLOAT RFX2 = m_RProjZ;
		FLOAT RFY2 = m_RProjZ * m_Aspect;

		FLOAT ZCoord = 1.0f;
		if (m_useZRangeHack) {
			ZCoord = (((ZCoord - 0.5f) / 7.5f) * 4.0f) + 4.0f;
		}

		if ((m_curVertexBufferPos + 4) >= VERTEX_RING_SIZE) {
			FlushVertexBuffers();
		}

		LockVertexColorBuffer();
		LockTexCoordBuffer(0);

		FGLTexCoord *pTexCoordArray = m_pTexCoordArray[0];
		FGLVertexColor *pVertexColorArray = m_pVertexColorArray;

		pTexCoordArray[0].u = 0.0f;
		pTexCoordArray[0].v = 0.0f;

		pTexCoordArray[1].u = 1.0f;
		pTexCoordArray[1].v = 0.0f;

		pTexCoordArray[2].u = 1.0f;
		pTexCoordArray[2].v = 1.0f;

		pTexCoordArray[3].u = 0.0f;
		pTexCoordArray[3].v = 1.0f;

		pVertexColorArray[0].x = RFX2 * (-1.0f * ZCoord);
		pVertexColorArray[0].y = RFY2 * (-1.0f * ZCoord);
		pVertexColorArray[0].z = ZCoord;
		pVertexColorArray[0].color = flashColor;

		pVertexColorArray[1].x = RFX2 * (+1.0f * ZCoord);
		pVertexColorArray[1].y = RFY2 * (-1.0f * ZCoord);
		pVertexColorArray[1].z = ZCoord;
		pVertexColorArray[1].color = flashColor;

		pVertexColorArray[2].x = RFX2 * (+1.0f * ZCoord);
		pVertexColorArray[2].y = RFY2 * (+1.0f * ZCoord);
		pVertexColorArray[2].z = ZCoord;
		pVertexColorArray[2].color = flashColor;

		pVertexColorArray[3].x = RFX2 * (-1.0f * ZCoord);
		pVertexColorArray[3].y = RFY2 * (+1.0f * ZCoord);
		pVertexColorArray[3].z = ZCoord;
		pVertexColorArray[3].color = flashColor;

		UnlockVertexColorBuffer();
		UnlockTexCoordBuffer(0);

		m_d3dDevice->DrawPrimitive(D3DPT_TRIANGLEFAN, m_curVertexBufferPos, 2);

		m_curVertexBufferPos += 4;
	}
	unguard;
}
