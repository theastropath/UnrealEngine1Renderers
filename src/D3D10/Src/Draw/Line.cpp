
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "../D3D10.h"
#include "../Geometry/VertexFormats.h"
#include "../Shaders/PolyFlags.h"
#include "../Shaders/Shader_Line.h"


static inline DWORD linePolyFlags(DWORD LineFlags) {
	return (LineFlags & LINE_Transparent) ? PF_Translucent : 0;
}

void UD3D10RenderDevice::Draw2DLine(FSceneNode *Frame, FPlane Color, DWORD LineFlags, FVector P1, FVector P2) {
	makeContextCurrent();
	flushDeferredGouraudPolygons();
	D3D::switchToShader(D3D::SHADER_LINE);
	setSceneNodeIfChanged(Frame);

	const DWORD flags = linePolyFlags(LineFlags);
	shader_Line->setFlags(flags);

	DynamicGeometryBuffer *buf = static_cast<DynamicGeometryBuffer *>(shader_Line->getGeometryBuffer());
	if (!buf->indexSingleVertex())
		return;

	Vertex_Line *v = (Vertex_Line *)buf->getVertex();
	v->XYXY.x = P1.X;
	v->XYXY.y = P1.Y;
	v->XYXY.z = P2.X;
	v->XYXY.w = P2.Y;
	v->Color = *((Vec4 *)&Color.X);
	v->Color.w = 1.0f;
	v->z = P1.Z;
	v->flags = flags;
	v->isRect = 0;
}

void UD3D10RenderDevice::Draw2DPoint(FSceneNode *Frame, FPlane Color, DWORD LineFlags, FLOAT X1, FLOAT Y1, FLOAT X2, FLOAT Y2, FLOAT Z) {
	makeContextCurrent();
	flushDeferredGouraudPolygons();
	D3D::switchToShader(D3D::SHADER_LINE);
	setSceneNodeIfChanged(Frame);

	const DWORD flags = linePolyFlags(LineFlags);
	shader_Line->setFlags(flags);

	DynamicGeometryBuffer *buf = static_cast<DynamicGeometryBuffer *>(shader_Line->getGeometryBuffer());
	if (!buf->indexSingleVertex())
		return;

	Vertex_Line *v = (Vertex_Line *)buf->getVertex();
	v->XYXY.x = X1;
	v->XYXY.y = Y1;
	v->XYXY.z = X2;
	v->XYXY.w = Y2;
	v->Color = *((Vec4 *)&Color.X);
	v->Color.w = 1.0f;
	v->z = Z;
	v->flags = flags;
	v->isRect = 1;
}
