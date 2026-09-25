#include "../OpenGLDrv.h"
#include "../OpenGL.h"

INT UOpenGLRenderDevice::BufferStaticComplexSurfaceGeometry(const FSurfaceFacet &Facet, FSavedPoly *&pPoly) {
	INT numVerts = 0;

	m_csPolyCount = 0;
	FGLMapDot *pMapDot = &MapDotArray[0];
	FGLVertex *pVertex = &VertexArray[0];
	while (pPoly) {
		INT NumPts = pPoly->NumPts;
		if (NumPts <= 0) {
			pPoly = pPoly->Next;
			continue;
		}

		//Wider than any chunk. Skip it.
		if (NumPts > VERTEX_ARRAY_SIZE) {
			pPoly = pPoly->Next;
			continue;
		}

		DWORD csPolyCount = m_csPolyCount;
		if ((csPolyCount >= (DWORD)(VERTEX_ARRAY_SIZE / 3)) || (NumPts > (VERTEX_ARRAY_SIZE - numVerts))) {
			break;
		}

		MultiDrawFirstArray[csPolyCount] = numVerts;
		MultiDrawCountArray[csPolyCount] = NumPts;
		m_csPolyCount = csPolyCount + 1;

		numVerts += NumPts;
		FTransform **pPts = &pPoly->Pts[0];
		do {
			const FVector &Point = (*pPts++)->Point;

			pMapDot->u = (Facet.MapCoords.XAxis | Point) - m_csUDot;
			pMapDot->v = (Facet.MapCoords.YAxis | Point) - m_csVDot;
			pMapDot++;

			pVertex->x = Point.X;
			pVertex->y = Point.Y;
			pVertex->z = Point.Z;
			pVertex++;
		} while (--NumPts != 0);

		pPoly = pPoly->Next;
	}

	return numVerts;
}

INT UOpenGLRenderDevice::BufferStaticComplexSurfaceGeometry_VP(const FSurfaceFacet &Facet, FSavedPoly *&pPoly) {
	INT numVerts = 0;

	m_csPolyCount = 0;
	FGLVertex *pVertex = &VertexArray[0];
	while (pPoly) {
		INT NumPts = pPoly->NumPts;
		if (NumPts <= 0) {
			pPoly = pPoly->Next;
			continue;
		}

		if (NumPts > VERTEX_ARRAY_SIZE) {
			pPoly = pPoly->Next;
			continue;
		}

		DWORD csPolyCount = m_csPolyCount;
		if ((csPolyCount >= (DWORD)(VERTEX_ARRAY_SIZE / 3)) || (NumPts > (VERTEX_ARRAY_SIZE - numVerts))) {
			break;
		}

		MultiDrawFirstArray[csPolyCount] = numVerts;
		MultiDrawCountArray[csPolyCount] = NumPts;
		m_csPolyCount = csPolyCount + 1;

		numVerts += NumPts;
		FTransform **pPts = &pPoly->Pts[0];
		do {
			const FVector &Point = (*pPts++)->Point;

			pVertex->x = Point.X;
			pVertex->y = Point.Y;
			pVertex->z = Point.Z;
			pVertex++;
		} while (--NumPts != 0);

		pPoly = pPoly->Next;
	}

	return numVerts;
}

DWORD UOpenGLRenderDevice::BufferDetailTextureData(FLOAT NearZ) {
	DWORD *pDetailTextureIsNear = DetailTextureIsNearArray;
	DWORD anyIsNearBits = 0;

	FGLVertex *pVertex = &VertexArray[0];
	INT *pNumPts = &MultiDrawCountArray[0];
	DWORD csPolyCount = m_csPolyCount;
	do {
		INT NumPts = *pNumPts++;
		DWORD isNear = 0;

		do {
			isNear <<= 1;
			if (pVertex->z < NearZ) {
				isNear |= 1;
			}
			pVertex++;
		} while (--NumPts != 0);

		*pDetailTextureIsNear++ = isNear;
		anyIsNearBits |= isNear;
	} while (--csPolyCount != 0);

	return anyIsNearBits;
}

#ifdef UTGLR_INCLUDE_SSE_CODE
__declspec(naked) DWORD UOpenGLRenderDevice::BufferDetailTextureData_SSE2(FLOAT NearZ) {
	__asm {
		movd xmm0, [esp+4]

		push esi
		push edi

		mov esi, [ecx]this.VertexArray
		lea edx, [ecx]this.MultiDrawCountArray
		lea edi, [ecx]this.DetailTextureIsNearArray

		pxor xmm1, xmm1

		mov ecx, [ecx]this.m_csPolyCount

		poly_count_loop:
			mov eax, [edx]
			add edx, 4

			pxor xmm2, xmm2

			num_pts_loop:
				movss xmm3, [esi+8]
				add esi, TYPE FGLVertex

				pslld xmm2, 1

				cmpltss xmm3, xmm0
				psrld xmm3, 31

				por xmm2, xmm3

				dec eax
				jne num_pts_loop

			movd [edi], xmm2
			add edi, 4

			por xmm1, xmm2

			dec ecx
			jne poly_count_loop

		movd eax, xmm1

		pop edi
		pop esi

		ret 4
	}
}
#endif //UTGLR_INCLUDE_SSE_CODE
