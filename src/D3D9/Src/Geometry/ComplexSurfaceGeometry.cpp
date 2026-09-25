
#include "../D3D9Drv.h"
#include "../D3D9.h"

void UD3D9RenderDevice::BuildComplexSurfaceIndices(void) {
	INT PolyNum;

	m_csIndexCount = 0;

	if (m_csPolyCount < 2) {
		return;
	}

	//Upper bound.
	const INT maxIndices = 3 * m_csPtCount;
	if ((m_curIndexBufferPos + maxIndices) > INDEX_RING_SIZE) {
		FlushIndexBuffer();
	}

	LockIndexBuffer();

	WORD *pIndex = m_pIndexArray;
	for (PolyNum = 0; PolyNum < (INT)m_csPolyCount; PolyNum++) {
		WORD first = (WORD)MultiDrawFirstArray[PolyNum];
		INT numPts = MultiDrawCountArray[PolyNum];
		INT i;

		for (i = 2; i < numPts; i++) {
			*pIndex++ = first;
			*pIndex++ = (WORD)(first + (i - 1));
			*pIndex++ = (WORD)(first + i);
		}
	}
	m_csIndexCount = (INT)(pIndex - m_pIndexArray);

	UnlockIndexBuffer();

	m_csIndexBufferPos = m_curIndexBufferPos;
	m_curIndexBufferPos += m_csIndexCount;

	return;
}

void UD3D9RenderDevice::DrawComplexSurfacePolys(void) {
	if (m_csIndexCount != 0) {
		m_d3dDevice->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, m_curVertexBufferPos, 0, m_csPtCount, m_csIndexBufferPos, m_csIndexCount / 3);
		return;
	}

	for (INT PolyNum = 0; PolyNum < (INT)m_csPolyCount; PolyNum++) {
		m_d3dDevice->DrawPrimitive(D3DPT_TRIANGLEFAN, m_curVertexBufferPos + MultiDrawFirstArray[PolyNum], MultiDrawCountArray[PolyNum] - 2);
	}

	return;
}

INT UD3D9RenderDevice::BufferStaticComplexSurfaceGeometry(const FSurfaceFacet &Facet, FSavedPoly *&pPoly) {
	INT Index = 0;

	m_csPolyCount = 0;
	FGLMapDot *pMapDot = &MapDotArray[0];
	FGLVertex *pVertex = &m_csVertexArray[0];
	while (pPoly) {
		//Underflows NumPts - 2.
		INT NumPts = pPoly->NumPts;
		if (NumPts < 3) {
			pPoly = pPoly->Next;
			continue;
		}

		if (NumPts > VERTEX_ARRAY_SIZE) {
			pPoly = pPoly->Next;
			continue;
		}

		if ((m_csPolyCount >= (VERTEX_ARRAY_SIZE / 3)) || ((Index + NumPts) > VERTEX_ARRAY_SIZE)) {
			break;
		}

		DWORD csPolyCount = m_csPolyCount;
		MultiDrawFirstArray[csPolyCount] = Index;
		MultiDrawCountArray[csPolyCount] = NumPts;
		m_csPolyCount = csPolyCount + 1;

		Index += NumPts;
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

	return Index;
}

INT UD3D9RenderDevice::BufferStaticComplexSurfaceGeometry_VP(const FSurfaceFacet &Facet, FSavedPoly *&pPoly) {
	INT Index = 0;

	m_csPolyCount = 0;
	FGLVertex *pVertex = &m_csVertexArray[0];
	while (pPoly) {
		INT NumPts = pPoly->NumPts;
		if (NumPts < 3) {
			pPoly = pPoly->Next;
			continue;
		}

		if (NumPts > VERTEX_ARRAY_SIZE) {
			pPoly = pPoly->Next;
			continue;
		}

		if ((m_csPolyCount >= (VERTEX_ARRAY_SIZE / 3)) || ((Index + NumPts) > VERTEX_ARRAY_SIZE)) {
			break;
		}

		DWORD csPolyCount = m_csPolyCount;
		MultiDrawFirstArray[csPolyCount] = Index;
		MultiDrawCountArray[csPolyCount] = NumPts;
		m_csPolyCount = csPolyCount + 1;

		Index += NumPts;
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

	return Index;
}

DWORD UD3D9RenderDevice::BufferDetailTextureData(FLOAT NearZ) {
	DWORD *pDetailTextureIsNear = DetailTextureIsNearArray;
	DWORD anyIsNearBits = 0;

	FGLVertex *pVertex = &m_csVertexArray[0];
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
__declspec(naked) DWORD UD3D9RenderDevice::BufferDetailTextureData_SSE2(FLOAT NearZ) {
	__asm {
		movd xmm0, [esp+4]

		push esi
		push edi

		lea esi, [ecx]this.m_csVertexArray
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
