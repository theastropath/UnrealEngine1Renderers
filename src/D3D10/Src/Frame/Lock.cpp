/** \file Frame/Lock.cpp */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "../D3D10.h"
#include "../Geometry/VertexFormats.h"
#include "../Texture/TextureCache.h"
#include "../Texture/TexConverter.h"
#include "../Shaders/Shader_GouraudPolygon.h"
#include "../Shaders/Shader_Tile.h"
#include "../Shaders/Shader_ComplexSurface.h"
#include "../Shaders/Shader_Line.h"


/**
Empties the texture cache.
\param AllowPrecache Longer load times.
\note Runs on some brightness changes.
*/
#if OLD_URENDERDEVICE
void UD3D10RenderDevice::Flush()
#else
void UD3D10RenderDevice::Flush(UBOOL AllowPrecache)
#endif
{
	makeContextCurrent();
	discardDeferredGouraudPolygons();

	if (textureCache)
		textureCache->flush();
	updateBrightness();
	neutralizeSystemGamma(); //The engine reapplies here.
#if (!OLD_URENDERDEVICE)
	if (AllowPrecache && options.precache)
		URenderDevice::PrecacheOnFlip = 1;
#endif
}

/**
Clears the buffers for a new frame.
\param FlashScale Effect scale.
\param FlashFog Effect color.
\param ScreenClear Clear color, for Rune fog.
\param RenderLockFlags Whether to clear the screen. Depth always is.
\param InHitData UnrealEd's hit proxy buffer, or null.
\param InHitSize In: capacity. Out: bytes written.
*/
void UD3D10RenderDevice::Lock(FPlane FlashScale, FPlane FlashFog, FPlane ScreenClear, DWORD RenderLockFlags, BYTE *InHitData, INT *InHitSize) {
	makeContextCurrent();

	hitData = InHitData;
	hitSize = InHitSize;
	hitCount = 0;
	if (hitSize != nullptr)
		*hitSize = 0;
	discardDeferredGouraudPolygons();

	if (D3D::isDeviceLost()) {
		UD3D10RenderDevice::debugs("Rebuilding the renderer after device loss.");
		if (!rebuildRenderer()) {
			errorOut().Log(TEXT("Lock: the graphics device was lost and the renderer could not be rebuilt."));
			return;
		}
	}

	if (!applyOptionChanges()) {
		errorOut().Log(TEXT("Lock: the renderer could not be rebuilt for the changed options."));
		return;
	}

	if (!textureCache || !texConverter)
		return;

	float deltaTime;
	LARGE_INTEGER time;
	if (lastFrameEnd.QuadPart == 0)
		QueryPerformanceCounter(&lastFrameEnd);

	if (options.FPSLimit > 0)
		limitFrameRate(lastFrameEnd, 1.0f / options.FPSLimit);

	QueryPerformanceCounter(&time);
	deltaTime = (time.QuadPart - lastFrameEnd.QuadPart) / (float)perfCounterFreq.QuadPart;
	lastFrameEnd.QuadPart = time.QuadPart;
	lastFrameTime = deltaTime;


//The game resets the field of view on every level switch and it cannot be set from config, so it is
//pushed back through the console each time, and Klingon reads the value of its own "fov" command
//because that SDK predates the default field entirely.
#if KLINGON
#define UTGLR_PAWN_FOV DesiredFOV
#else
#define UTGLR_PAWN_FOV DefaultFOV
#endif
	if (options.autoFOV && Viewport->Actor && Viewport->Actor->UTGLR_PAWN_FOV != customFOV) {
		//Sized as a margin.
		TCHAR buf[16] = TEXT("fov ");
		_itot_s(customFOV, &buf[4], _countof(buf) - 4, 10);
		Viewport->Actor->UTGLR_PAWN_FOV = customFOV; //Set even where the command doesn't take.
		URenderDevice::Viewport->Exec(buf, logOut());
	}
#undef UTGLR_PAWN_FOV

	updateBrightness();

	D3D::newFrame(deltaTime);

	textureCache->newFrame();

	appMemzero(&sceneNodeKey, sizeof(sceneNodeKey));

#if RUNE_100 //Fix for Rune v1.1 green screen
	FlashFog.Y = 0.0f;
#endif
	//Carries both halves. Without alpha, right only at 0.5.
	Vec4 flashFog = Vec4(FlashFog.X, FlashFog.Y, FlashFog.Z, Min(FlashScale.X * 2.0f, 1.0f));
	D3D::flash(flashFog);

	//Shaders rebind targets only on a change.
	D3D::switchToShader(-1);

	shader_Tile->switchBuffers(Shader_Unreal::BUFFER_MULTIPASS);
	shader_GouraudPolygon->switchBuffers(Shader_Unreal::BUFFER_MULTIPASS);
	shader_ComplexSurface->switchBuffers(Shader_Unreal::BUFFER_MULTIPASS);
	shader_Line->switchBuffers(Shader_Unreal::BUFFER_MULTIPASS);

	shader_GouraudPolygon->clearDepth(); //Depth is always cleared
	shader_GouraudPolygon->clear(*((Vec4 *)&ScreenClear.X));

	drawingHUD = false;
}

/**
Finishes rendering.
\param Blit Whether to swap buffers.
*/
void UD3D10RenderDevice::Unlock(UBOOL Blit) {
	makeContextCurrent();

	if (hitSize != nullptr)
		*hitSize = hitCount;
	if (!textureCache)
		return;

	flushDeferredGouraudPolygons(); //Still draws what is held.

	if (Blit && !drawingHUD) {
		D3D::postprocess();
		drawingHUD = true;
	}

	if (Blit) {
		D3D::present();
	}

	textureCache->evictOverBudget();
}
