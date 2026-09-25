/**
\file Draw/FogSurface.cpp

Rune's volumetric fog, inside #if RUNE.
*/

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "../D3D10.h"
#include "../Geometry/VertexFormats.h"
#include "../Shaders/PolyFlags.h"
#include "../Shaders/Shader_GouraudPolygon.h"
#include "../Shaders/Shader_FogSurface.h"


//Agrees because /D NAME defines it as 1.
#if RUNE
/**
Clears to the fog color, then overlays alpha blended planes.
\param Frame The scene.
\param ForSurf Fog planes; alpha is position.z/FogDistance.
\note The pre/post steps aren't needed.
*/
void UD3D10RenderDevice::DrawFogSurface(FSceneNode *Frame, FFogSurf &FogSurf) {
	makeContextCurrent();
	flushDeferredGouraudPolygons();

	//Documented "zero if none", and zero does arrive.
	//Vertex alpha then comes out Inf.
	if (FogSurf.FogDistance <= 0)
		return;

	//Not always announced first.
	setSceneNodeIfChanged(Frame);

	D3D::switchToShader(D3D::SHADER_FOGSURFACE);
	float mult = 1.0 / FogSurf.FogDistance;

	shader_FogSurface->setFlags(PF_AlphaBlend);

	DynamicGeometryBuffer *buf = static_cast<DynamicGeometryBuffer *>(shader_FogSurface->getGeometryBuffer());

	for (FSavedPoly *Poly = FogSurf.Polys; Poly; Poly = Poly->Next) {
		if (Poly->NumPts < 3)
			continue;
		if (!buf->indexTriangleFan(Poly->NumPts))
			continue;
		for (int i = 0; i < Poly->NumPts; i++) {
			Vertex_FogSurface *v = (Vertex_FogSurface *)buf->getVertex();
			v->Color = *((Vec4 *)&FogSurf.FogColor.X);
			v->Pos = *(Vec3 *)&Poly->Pts[i]->Point.X;
			v->Color.w = v->Pos.z * mult;
			v->flags = PF_AlphaBlend;
		}
	}
}

/**
Object fog, in the shader.
\param Frame The scene.
\param FogDistance End distance.
\param FogColor The color.
*/
void UD3D10RenderDevice::PreDrawGouraud(FSceneNode *Frame, FLOAT FogDistance, FPlane FogColor) {
	makeContextCurrent();
	//Held geometry keeps its setting.
	flushDeferredGouraudPolygons();
	D3D::render();
	if (FogDistance > 0) {
		Vec4 *color = ((Vec4 *)&FogColor.X);
		shader_GouraudPolygon->fog(FogDistance, color);
	}
}

/**
Turn fogging off.
\param FogDistance What turned it on.
*/
void UD3D10RenderDevice::PostDrawGouraud(FLOAT FogDistance) {
	makeContextCurrent();
	flushDeferredGouraudPolygons();
	D3D::render();
	if (FogDistance > 0) {
		shader_GouraudPolygon->fog(0, nullptr);
	}
}
#endif
