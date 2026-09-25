/**
\file Draw/Support.cpp

Entry points drawing no geometry.
*/

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>

#include "../D3D10.h"
#include "../Geometry/VertexFormats.h"
#include "../Geometry/Deferred.h"
#include "../Texture/TextureCache.h"
#include "../Shaders/Shader_GouraudPolygon.h"
#include "../Shaders/Shader_Tile.h"
#include "../Shaders/Shader_ComplexSurface.h"
#include "../Shaders/Shader_Line.h"


/** \note Commit the vertex buffers first! */
void UD3D10RenderDevice::ClearZ(FSceneNode *Frame) {
	makeContextCurrent();
	flushDeferredGouraudPolygons(); //Belongs to that depth.
	D3D::render();
	//For an unannounced node.
	setSceneNodeIfChanged(Frame);
	shader_GouraudPolygon->clearDepth(); //can be any shader
}

/** UnrealEd's selection stack. */
void UD3D10RenderDevice::PushHit(const BYTE *Data, INT Count) {
	makeContextCurrent();
}

/** With bForce, reports a hit. */
void UD3D10RenderDevice::PopHit(INT Count, UBOOL bForce) {
	makeContextCurrent();
}

/**
\note No size given. 256 TCHARs.
*/
static void copyStatsResult(TCHAR *dest, const TCHAR *src) {
	const int STATS_RESULT_MAX_CHARS = 256;
	int i = 0;
	while (i < (STATS_RESULT_MAX_CHARS - 1) && src[i] != 0) {
		dest[i] = src[i];
		i++;
	}
	dest[i] = 0;
}

/** For the stats overlay. */
void UD3D10RenderDevice::GetStats(TCHAR *Result) {
	makeContextCurrent();
	if (!Result)
		return;

	//Surf, Jobs and Build need deferred recording, Mover counts depth bias switches, and the two
	//memory figures differ over the atlas pages.
	static const LightmapAtlas::Stats noAtlas = { 0, 0, 0 };
	const LightmapAtlas::Stats &atlas = textureCache ? textureCache->lightmapAtlasStats() : noAtlas;

	TCHAR statsStr[512];
	const DeferredGeometry *deferred = D3D::getDeferred();
	appSprintf(statsStr, TEXT("D3D10: %i flushes (%i mover), %i textures (%i/%i MB), %i FPS, Surf=%i Jobs=%i Build=%.2fms, Atlas=%i/%i/%i/%i"),
		(INT)D3D::getBufferFlushCount(),
		(INT)D3D::getMoverBiasSwitchCount(),
		textureCache ? (INT)textureCache->size() : 0,
		textureCache ? (INT)(textureCache->evictableBytes() / (1024 * 1024)) : 0,
		textureCache ? (INT)(textureCache->bytes() / (1024 * 1024)) : 0,
		(INT)(lastFrameTime > 0.0f ? (1.0f / lastFrameTime) + 0.5f : 0.0f),
		deferred ? (INT)deferred->recordedSurfaces : 0,
		deferred ? (INT)deferred->buildJobs : 0,
		deferred ? (FLOAT)deferred->buildMicroseconds / 1000.0f : 0.0f,
		textureCache ? (INT)textureCache->lightmapAtlasPages() : 0,
		(INT)atlas.placed,
		(INT)atlas.rewritten,
		(INT)atlas.failed);

	copyStatsResult(Result, statsStr);
}

/**
Screenshots and previews.
\param Pixels 32 bit, from the back buffer.
*/
void UD3D10RenderDevice::ReadPixels(FColor *Pixels) {
	makeContextCurrent();
	if (!Pixels || !URenderDevice::Viewport)
		return;

	const int width = URenderDevice::Viewport->SizeX;
	const int height = URenderDevice::Viewport->SizeY;
	if (width <= 0 || height <= 0)
		return;

	flushDeferredGouraudPolygons();

	//Otherwise garbage.
	appMemzero(Pixels, (size_t)width * (size_t)height * sizeof(FColor));

	UD3D10RenderDevice::debugs("Dumping screenshot...");
	D3D::getScreenshot((Vec4_byte *)Pixels, width, height);
	UD3D10RenderDevice::debugs("Done");
}

/** Before the HUD. */
void UD3D10RenderDevice::EndFlash() {
	makeContextCurrent();
	flushDeferredGouraudPolygons(); //Part of the scene.

	/** The last pass's buffer, so no postprocessing. */
	if (!drawingHUD) {
		D3D::postprocess();
		shader_Tile->switchBuffers(Shader_Unreal::BUFFER_HUD);
		shader_GouraudPolygon->switchBuffers(Shader_Unreal::BUFFER_HUD); //Rune's main menu logo follows.
		shader_ComplexSurface->switchBuffers(Shader_Unreal::BUFFER_HUD); //For Deus Ex security cams
		shader_Line->switchBuffers(Shader_Unreal::BUFFER_HUD); //Drawn over the HUD.
		shader_ComplexSurface->clearDepth();
		drawingHUD = true;
	}
}
