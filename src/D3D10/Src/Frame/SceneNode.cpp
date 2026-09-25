
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "../D3D10.h"
#include "../Shaders/Shader_GouraudPolygon.h"

void UD3D10RenderDevice::SetSceneNode(FSceneNode *Frame) {
	makeContextCurrent();
	flushDeferredGouraudPolygons();

	D3D::setViewPort(Frame->X, Frame->Y, Frame->XB, Frame->YB);
	shader_GouraudPolygon->setViewportSize(Frame->X, Frame->Y);

	const float rProjZ = Frame->RProj.Z;
	float prjXM = Frame->FX15 * rProjZ, prjXP = (Frame->FX - Frame->FX15) * rProjZ;
	float prjYM = Frame->FY15 * rProjZ, prjYP = (Frame->FY - Frame->FY15) * rProjZ;
	const float MAX_CLIPPING_NUMBER = 100.0f;
	if (!(prjXM > 0.0f && prjXM < MAX_CLIPPING_NUMBER && prjXP > 0.0f && prjXP < MAX_CLIPPING_NUMBER && prjYM > 0.0f && prjYM < MAX_CLIPPING_NUMBER && prjYP > 0.0f && prjYP < MAX_CLIPPING_NUMBER)) {
		float aspect = (Frame->FX != 0.0f) ? (Frame->FY / Frame->FX) : 1.0f;
		float fovAngle = Viewport->Actor ? Viewport->Actor->FovAngle : 90.0f;
		float RProjZ = appTan(fovAngle * PI / 360.0);
		prjXM = prjXP = RProjZ;
		prjYM = prjYP = aspect * RProjZ;
	}
	shader_GouraudPolygon->setProjection(prjXM, prjXP, prjYM, prjYP, zNear, zFar);

	buildSceneNodeKey(Frame, sceneNodeKey);
}

void UD3D10RenderDevice::buildSceneNodeKey(const FSceneNode *Frame, SceneNodeKey &key) const {
	key.X = Frame->X;
	key.Y = Frame->Y;
	key.XB = Frame->XB;
	key.YB = Frame->YB;
	key.FX = Frame->FX;
	key.FY = Frame->FY;
	key.FX15 = Frame->FX15;
	key.FY15 = Frame->FY15;
	key.RProjZ = Frame->RProj.Z;
	key.FovAngle = Viewport->Actor ? Viewport->Actor->FovAngle : 90.0f;
}

void UD3D10RenderDevice::setSceneNodeIfChanged(FSceneNode *Frame) {
	SceneNodeKey key;
	buildSceneNodeKey(Frame, key);
	if (appMemcmp(&key, &sceneNodeKey, sizeof(key)) != 0)
		SetSceneNode(Frame);
}
