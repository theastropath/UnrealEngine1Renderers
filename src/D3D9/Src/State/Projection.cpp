/*=============================================================================
	Projection.cpp: the projection matrix, near range hack included.
=============================================================================*/

#include "../D3D9Drv.h"
#include "../D3D9.h"

void UD3D9RenderDevice::SetProjectionStateNoCheck(bool requestNearZRangeHackProjection) {
	float left, right, bottom, top, zNear, zFar;
	float invRightMinusLeft, invTopMinusBottom, invNearMinusFar;
	D3DMATRIX d3dProj;

	m_nearZRangeHackProjectionActive = requestNearZRangeHackProjection;

	FLOAT zNearVal = 0.5f;

	FLOAT zScaleVal = 1.0f;
	if (requestNearZRangeHackProjection) {
#ifdef UTGLR_DEBUG_Z_RANGE_HACK_WIREFRAME
		m_d3dDevice->SetRenderState(D3DRS_FILLMODE, D3DFILL_WIREFRAME);
#endif

		zScaleVal = 0.125f;
		zNearVal = 0.5;
	} else {
#ifdef UTGLR_DEBUG_Z_RANGE_HACK_WIREFRAME
		m_d3dDevice->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
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

	d3dProj.m[0][0] = 2.0f * zNear * invRightMinusLeft;
	d3dProj.m[0][1] = 0.0f;
	d3dProj.m[0][2] = 0.0f;
	d3dProj.m[0][3] = 0.0f;

	d3dProj.m[1][0] = 0.0f;
	d3dProj.m[1][1] = 2.0f * zNear * invTopMinusBottom;
	d3dProj.m[1][2] = 0.0f;
	d3dProj.m[1][3] = 0.0f;

	d3dProj.m[2][0] = 1.0f / (FLOAT)m_sceneNodeX;
	d3dProj.m[2][1] = -1.0f / (FLOAT)m_sceneNodeY;
	d3dProj.m[2][2] = zScaleVal * (zFar * invNearMinusFar);
	d3dProj.m[2][3] = -1.0f;

	d3dProj.m[3][0] = 0.0f;
	d3dProj.m[3][1] = 0.0f;
	d3dProj.m[3][2] = zScaleVal * zScaleVal * (zNear * zFar * invNearMinusFar);
	d3dProj.m[3][3] = 0.0f;

	m_d3dDevice->SetTransform(D3DTS_PROJECTION, &d3dProj);

	if (UseFragmentProgram) {
		FLOAT vsTransMatrix[16];

		//Transpose and scale by -1y and -1z
		vsTransMatrix[0] = d3dProj.m[0][0];
		vsTransMatrix[1] = -d3dProj.m[1][0];
		vsTransMatrix[2] = -d3dProj.m[2][0];
		vsTransMatrix[3] = d3dProj.m[3][0];
		vsTransMatrix[4] = d3dProj.m[0][1];
		vsTransMatrix[5] = -d3dProj.m[1][1];
		vsTransMatrix[6] = -d3dProj.m[2][1];
		vsTransMatrix[7] = d3dProj.m[3][1];
		vsTransMatrix[8] = d3dProj.m[0][2];
		vsTransMatrix[9] = -d3dProj.m[1][2];
		vsTransMatrix[10] = -d3dProj.m[2][2];
		vsTransMatrix[11] = d3dProj.m[3][2];
		vsTransMatrix[12] = d3dProj.m[0][3];
		vsTransMatrix[13] = -d3dProj.m[1][3];
		vsTransMatrix[14] = -d3dProj.m[2][3];
		vsTransMatrix[15] = d3dProj.m[3][3];

		m_d3dDevice->SetVertexShaderConstantF(0, vsTransMatrix, 4);
	}

	return;
}

void UD3D9RenderDevice::SetOrthoProjection(void) {
	float left, right, bottom, top, zNear, zFar;
	float invRightMinusLeft, invTopMinusBottom, invNearMinusFar;
	D3DMATRIX d3dProj;

	m_nearZRangeHackProjectionActive = false;

	left = -m_RProjZ * 0.5f;
	right = +m_RProjZ * 0.5f;
	bottom = -m_Aspect * m_RProjZ * 0.5f;
	top = +m_Aspect * m_RProjZ * 0.5f;
	zNear = 1.0f * 0.5f;
	zFar = 32768.0f;

	invRightMinusLeft = 1.0f / (right - left);
	invTopMinusBottom = 1.0f / (top - bottom);
	invNearMinusFar = 1.0f / (zNear - zFar);

	d3dProj.m[0][0] = 2.0f * invRightMinusLeft;
	d3dProj.m[0][1] = 0.0f;
	d3dProj.m[0][2] = 0.0f;
	d3dProj.m[0][3] = 0.0f;

	d3dProj.m[1][0] = 0.0f;
	d3dProj.m[1][1] = 2.0f * invTopMinusBottom;
	d3dProj.m[1][2] = 0.0f;
	d3dProj.m[1][3] = 0.0f;

	d3dProj.m[2][0] = 0.0f;
	d3dProj.m[2][1] = 0.0f;
	d3dProj.m[2][2] = 1.0f * invNearMinusFar;
	d3dProj.m[2][3] = 0.0f;

	d3dProj.m[3][0] = -1.0f / (FLOAT)m_sceneNodeX;
	d3dProj.m[3][1] = 1.0f / (FLOAT)m_sceneNodeY;
	d3dProj.m[3][2] = zNear * invNearMinusFar;
	d3dProj.m[3][3] = 1.0f;

	m_d3dDevice->SetTransform(D3DTS_PROJECTION, &d3dProj);

	if (UseFragmentProgram) {
		FLOAT vsTransMatrix[16];

		//Transpose and scale by -1y and -1z
		vsTransMatrix[0] = d3dProj.m[0][0];
		vsTransMatrix[1] = -d3dProj.m[1][0];
		vsTransMatrix[2] = -d3dProj.m[2][0];
		vsTransMatrix[3] = d3dProj.m[3][0];
		vsTransMatrix[4] = d3dProj.m[0][1];
		vsTransMatrix[5] = -d3dProj.m[1][1];
		vsTransMatrix[6] = -d3dProj.m[2][1];
		vsTransMatrix[7] = d3dProj.m[3][1];
		vsTransMatrix[8] = d3dProj.m[0][2];
		vsTransMatrix[9] = -d3dProj.m[1][2];
		vsTransMatrix[10] = -d3dProj.m[2][2];
		vsTransMatrix[11] = d3dProj.m[3][2];
		vsTransMatrix[12] = d3dProj.m[0][3];
		vsTransMatrix[13] = -d3dProj.m[1][3];
		vsTransMatrix[14] = -d3dProj.m[2][3];
		vsTransMatrix[15] = d3dProj.m[3][3];

		m_d3dDevice->SetVertexShaderConstantF(0, vsTransMatrix, 4);
	}

	return;
}
