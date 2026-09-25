/*=============================================================================
	Projection.cpp: the projection matrix, near range hack included.
=============================================================================*/

#include "../OpenGLDrv.h"
#include "../OpenGL.h"

void UOpenGLRenderDevice::SetProjectionStateNoCheck(bool requestNearZRangeHackProjection) {
	float left, right, bottom, top, zNear, zFar;
	float invRightMinusLeft, invTopMinusBottom, invNearMinusFar;
	GLfloat glProj[16];

	m_nearZRangeHackProjectionActive = requestNearZRangeHackProjection;

	FLOAT zNearVal = 0.5f;

	FLOAT zScaleVal = 1.0f;
	if (requestNearZRangeHackProjection) {
#ifdef UTGLR_DEBUG_Z_RANGE_HACK_WIREFRAME
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
#endif

		zScaleVal = 0.125f;
		zNearVal = 0.5f;
	} else {
#ifdef UTGLR_DEBUG_Z_RANGE_HACK_WIREFRAME
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
#endif

		if (m_useZRangeHack) {
			zNearVal = 4.0f;
		}
	}

	left = -m_RProjZ * zNearVal;
	right = +m_RProjZ * zNearVal;
	bottom = -m_Aspect * m_RProjZ * zNearVal;
	top = +m_Aspect * m_RProjZ * zNearVal;
	zNear = 1.0f * zNearVal;

#ifdef UTGLR_UNREAL_227_BUILD
	zFar = 49152.0f;
#else
	zFar = 32768.0f;
#endif
	if (requestNearZRangeHackProjection) {
		zFar *= zScaleVal;
	}

	invRightMinusLeft = 1.0f / (right - left);
	invTopMinusBottom = 1.0f / (top - bottom);
	invNearMinusFar = 1.0f / (zNear - zFar);

	//Packs the hack's geometry into the front eighth of the depth buffer.
	//GL clips and interpolates depth over [-w, w], so the row becomes 2z - w.
	//That is the +1 term.
	const FLOAT depthZ = 2.0f * (zScaleVal * (zFar * invNearMinusFar)) + 1.0f;
	const FLOAT depthW = 2.0f * (zScaleVal * zScaleVal * (zNear * zFar * invNearMinusFar));

	//Column major, as glLoadMatrixf wants.
	//The frustum is symmetric.
	glProj[0] = 2.0f * zNear * invRightMinusLeft;
	glProj[1] = 0.0f;
	glProj[2] = 0.0f;
	glProj[3] = 0.0f;

	glProj[4] = 0.0f;
	glProj[5] = 2.0f * zNear * invTopMinusBottom;
	glProj[6] = 0.0f;
	glProj[7] = 0.0f;

	glProj[8] = 0.0f;
	glProj[9] = 0.0f;
	glProj[10] = depthZ;
	glProj[11] = -1.0f;

	glProj[12] = 0.0f;
	glProj[13] = 0.0f;
	glProj[14] = depthW;
	glProj[15] = 0.0f;

	glMatrixMode(GL_PROJECTION);
	glLoadMatrixf(glProj);

	return;
}

void UOpenGLRenderDevice::SetOrthoProjection(void) {
	m_nearZRangeHackProjectionActive = false;

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	glOrtho(-m_RProjZ * 0.5, +m_RProjZ * 0.5, -m_Aspect * m_RProjZ * 0.5, +m_Aspect * m_RProjZ * 0.5, 1.0 * 0.5, 32768.0);

	return;
}
