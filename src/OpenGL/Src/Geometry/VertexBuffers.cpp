/*=============================================================================
	VertexBuffers.cpp: filling the vertex arrays for gouraud geometry.

	One filler per combination of what a triangle carries.
	The naked ones read the device layout directly.
=============================================================================*/

#include "../OpenGLDrv.h"
#include "../OpenGL.h"
#include "VertexBuffers.h"

#ifdef UTGLR_RUNE_BUILD
//Only the per poly alpha path needs the fully general buffering routine
void FASTCALL Buffer3Verts(UOpenGLRenderDevice *pRD, FTransTexture **Pts) {
	FGLTexCoord *pTexCoordArray = &pRD->TexCoordArray[0][pRD->BufferedVerts];
	FGLVertex *pVertexArray = &pRD->VertexArray[pRD->BufferedVerts];
	FGLSingleColor *pSingleColorArray = &pRD->SingleColorArray[pRD->BufferedVerts];
	FGLDoubleColor *pDoubleColorArray = &pRD->DoubleColorArray[pRD->BufferedVerts];
	pRD->BufferedVerts += 3;
	for (INT i = 0; i < 3; i++) {
		const FTransTexture *P = *Pts++;

		pTexCoordArray->u = P->U * pRD->TexInfo[0].UMult;
		pTexCoordArray->v = P->V * pRD->TexInfo[0].VMult;
		pTexCoordArray++;

		pVertexArray->x = P->Point.X;
		pVertexArray->y = P->Point.Y;
		pVertexArray->z = P->Point.Z;
		pVertexArray++;

		if (pRD->m_requestedColorFlags & UOpenGLRenderDevice::CF_DUAL_COLOR_ARRAY) {
			FLOAT f255_Times_One_Minus_FogW = 255.0f * (1.0f - P->Fog.W);
			pDoubleColorArray->color = UOpenGLRenderDevice::FPlaneTo_RGBScaledClamped_A255(&P->Light, f255_Times_One_Minus_FogW);
			pDoubleColorArray->specular = UOpenGLRenderDevice::FPlaneTo_RGBClamped_A0(&P->Fog);
			pDoubleColorArray++;
		} else if (pRD->m_requestedColorFlags & UOpenGLRenderDevice::CF_COLOR_ARRAY) {
#ifdef UTGLR_RUNE_BUILD
			pSingleColorArray->color = UOpenGLRenderDevice::FPlaneTo_RGBClamped_Aub(&P->Light, pRD->m_gpAlpha);
#else
			pSingleColorArray->color = UOpenGLRenderDevice::FPlaneTo_RGBClamped_A255(&P->Light);
#endif
			pSingleColorArray++;
		}
	}
}
#endif

void FASTCALL Buffer3BasicVerts(UOpenGLRenderDevice *pRD, FTransTexture **Pts) {
	FGLTexCoord *pTexCoordArray = &pRD->TexCoordArray[0][pRD->BufferedVerts];
	FGLVertex *pVertexArray = &pRD->VertexArray[pRD->BufferedVerts];
	pRD->BufferedVerts += 3;
	FLOAT UMult = pRD->TexInfo[0].UMult;
	FLOAT VMult = pRD->TexInfo[0].VMult;
	for (INT i = 0; i < 3; i++) {
		const FTransTexture *P = *Pts++;

		pTexCoordArray->u = P->U * UMult;
		pTexCoordArray->v = P->V * VMult;
		pTexCoordArray++;

		pVertexArray->x = P->Point.X;
		pVertexArray->y = P->Point.Y;
		pVertexArray->z = P->Point.Z;
		pVertexArray++;
	}
}

/*
The three implementations disagree on out of range input because this one and the SSE
version wrap a light component outside [0,1] while SSE2 saturates, which stays theoretical
as long as engine lighting stays in range, and it is worth knowing before anyone treats
the three as interchangeable on a machine where only one of them runs.
*/
void FASTCALL Buffer3ColoredVerts(UOpenGLRenderDevice *pRD, FTransTexture **Pts) {
	FGLTexCoord *pTexCoordArray = &pRD->TexCoordArray[0][pRD->BufferedVerts];
	FGLVertex *pVertexArray = &pRD->VertexArray[pRD->BufferedVerts];
	FGLSingleColor *pSingleColorArray = &pRD->SingleColorArray[pRD->BufferedVerts];
	pRD->BufferedVerts += 3;
	for (INT i = 0; i < 3; i++) {
		const FTransTexture *P = *Pts++;

		pTexCoordArray->u = P->U * pRD->TexInfo[0].UMult;
		pTexCoordArray->v = P->V * pRD->TexInfo[0].VMult;
		pTexCoordArray++;

		pVertexArray->x = P->Point.X;
		pVertexArray->y = P->Point.Y;
		pVertexArray->z = P->Point.Z;
		pVertexArray++;

		pSingleColorArray->color = UOpenGLRenderDevice::FPlaneTo_RGBClamped_A255(&P->Light);
		pSingleColorArray++;
	}
}

#ifdef UTGLR_INCLUDE_SSE_CODE
__declspec(naked) void FASTCALL Buffer3ColoredVerts_SSE(UOpenGLRenderDevice *pRD, FTransTexture **Pts) {
	static float f255 = 255.0f;
	__asm {
		//pRD is in ecx
		//Pts is in edx

		push ebx
		push esi
		push edi
		push ebp

		mov eax, [ecx]UOpenGLRenderDevice.BufferedVerts

		lea ebx, [eax*8]
		add ebx, [ecx]UOpenGLRenderDevice.TexCoordArray[0]

		lea esi, [eax*4]
		add esi, [ecx]UOpenGLRenderDevice.SingleColorArray

		lea edi, [eax + eax*2]
		lea edi, [edi*4]
		add edi, [ecx]UOpenGLRenderDevice.VertexArray

		add eax, 3
		mov [ecx]UOpenGLRenderDevice.BufferedVerts, eax

		lea eax, [ecx]UOpenGLRenderDevice.TexInfo
		movss xmm0, [eax]FTexInfo.UMult
		movss xmm1, [eax]FTexInfo.VMult
		movss xmm2, f255

			//Pts in edx
			//Get PtsPlus12B
		lea ebp, [edx + 12]

v_loop:
			mov eax, [edx]
			add edx, 4

			movss xmm3, [eax]FTransTexture.U
			mulss xmm3, xmm0
			movss [ebx]FGLTexCoord.u, xmm3
			movss xmm3, [eax]FTransTexture.V
			mulss xmm3, xmm1
			movss [ebx]FGLTexCoord.v, xmm3
			add ebx, TYPE FGLTexCoord

			mov ecx, [eax]FOutVector.Point.X
			mov [edi]FGLVertex.x, ecx
			mov ecx, [eax]FOutVector.Point.Y
			mov [edi]FGLVertex.y, ecx
			mov ecx, [eax]FOutVector.Point.Z
			mov [edi]FGLVertex.z, ecx
			add edi, TYPE FGLVertex

			movss xmm3, [eax]FTransSample.Light + 0
			mulss xmm3, xmm2
			movss xmm4, [eax]FTransSample.Light + 4
			mulss xmm4, xmm2
			movss xmm5, [eax]FTransSample.Light + 8
			mulss xmm5, xmm2
			cvtss2si eax, xmm3
			and eax, 255
			cvtss2si ecx, xmm4
			and ecx, 255
			shl ecx, 8
			or eax, ecx
			cvtss2si ecx, xmm5
			shl ecx, 16
			or ecx, 0xFF000000
			or eax, ecx
			mov [esi]FGLSingleColor.color, eax
			add esi, TYPE FGLSingleColor

			cmp edx, ebp
			jne v_loop

		pop ebp
		pop edi
		pop esi
		pop ebx

		ret
	}
}

__declspec(naked) void FASTCALL Buffer3ColoredVerts_SSE2(UOpenGLRenderDevice *pRD, FTransTexture **Pts) {
	static __m128 fColorMul = { 255.0f, 255.0f, 255.0f, 0.0f };
	static DWORD alphaOr = 0xFF000000;
	__asm {
		//pRD is in ecx
		//Pts is in edx

		push ebx
		push esi
		push edi
		push ebp

		mov eax, [ecx]UOpenGLRenderDevice.BufferedVerts

		lea ebx, [eax*8]
		add ebx, [ecx]UOpenGLRenderDevice.TexCoordArray[0]

		lea esi, [eax*4]
		add esi, [ecx]UOpenGLRenderDevice.SingleColorArray

		lea edi, [eax + eax*2]
		lea edi, [edi*4]
		add edi, [ecx]UOpenGLRenderDevice.VertexArray

		add eax, 3
		mov [ecx]UOpenGLRenderDevice.BufferedVerts, eax

		lea eax, [ecx]UOpenGLRenderDevice.TexInfo
		movss xmm0, [eax]FTexInfo.UMult
		movss xmm1, [eax]FTexInfo.VMult
		movaps xmm2, fColorMul
		movd xmm3, alphaOr

			//Pts in edx
			//Get PtsPlus12B
		lea ebp, [edx + 12]

v_loop:
			mov eax, [edx]
			add edx, 4

			movss xmm4, [eax]FTransTexture.U
			mulss xmm4, xmm0
			movss [ebx]FGLTexCoord.u, xmm4
			movss xmm4, [eax]FTransTexture.V
			mulss xmm4, xmm1
			movss [ebx]FGLTexCoord.v, xmm4
			add ebx, TYPE FGLTexCoord

			mov ecx, [eax]FOutVector.Point.X
			mov [edi]FGLVertex.x, ecx
			mov ecx, [eax]FOutVector.Point.Y
			mov [edi]FGLVertex.y, ecx
			mov ecx, [eax]FOutVector.Point.Z
			mov [edi]FGLVertex.z, ecx
			add edi, TYPE FGLVertex

			movups xmm4, [eax]FTransSample.Light
			mulps xmm4, xmm2
			cvtps2dq xmm4, xmm4
			packssdw xmm4, xmm4
			packuswb xmm4, xmm4
			por xmm4, xmm3
			movd [esi]FGLSingleColor.color, xmm4
			add esi, TYPE FGLSingleColor

			cmp edx, ebp
			jne v_loop

		pop ebp
		pop edi
		pop esi
		pop ebx

		ret
	}
}
#endif

void FASTCALL Buffer3FoggedVerts(UOpenGLRenderDevice *pRD, FTransTexture **Pts) {
	FGLTexCoord *pTexCoordArray = &pRD->TexCoordArray[0][pRD->BufferedVerts];
	FGLVertex *pVertexArray = &pRD->VertexArray[pRD->BufferedVerts];
	FGLDoubleColor *pDoubleColorArray = &pRD->DoubleColorArray[pRD->BufferedVerts];
	pRD->BufferedVerts += 3;
	for (INT i = 0; i < 3; i++) {
		const FTransTexture *P = *Pts++;

		pTexCoordArray->u = P->U * pRD->TexInfo[0].UMult;
		pTexCoordArray->v = P->V * pRD->TexInfo[0].VMult;
		pTexCoordArray++;

		pVertexArray->x = P->Point.X;
		pVertexArray->y = P->Point.Y;
		pVertexArray->z = P->Point.Z;
		pVertexArray++;

		FLOAT f255_Times_One_Minus_FogW = 255.0f * (1.0f - P->Fog.W);
		pDoubleColorArray->color = UOpenGLRenderDevice::FPlaneTo_RGBScaledClamped_A255(&P->Light, f255_Times_One_Minus_FogW);
		pDoubleColorArray->specular = UOpenGLRenderDevice::FPlaneTo_RGBClamped_A0(&P->Fog);
		pDoubleColorArray++;
	}
}

#ifdef UTGLR_INCLUDE_SSE_CODE
__declspec(naked) void FASTCALL Buffer3FoggedVerts_SSE(UOpenGLRenderDevice *pRD, FTransTexture **Pts) {
	static float f255 = 255.0f;
	static float f1 = 1.0f;
	__asm {
		//pRD is in ecx
		//Pts is in edx

		push ebx
		push esi
		push edi
		push ebp
		sub esp, 4

		mov eax, [ecx]UOpenGLRenderDevice.BufferedVerts

		lea ebx, [eax*8]
		add ebx, [ecx]UOpenGLRenderDevice.TexCoordArray[0]

		lea esi, [eax*8]
		add esi, [ecx]UOpenGLRenderDevice.DoubleColorArray

		lea edi, [eax + eax*2]
		lea edi, [edi*4]
		add edi, [ecx]UOpenGLRenderDevice.VertexArray

		add eax, 3
		mov [ecx]UOpenGLRenderDevice.BufferedVerts, eax

		lea eax, [ecx]UOpenGLRenderDevice.TexInfo
		movss xmm0, [eax]FTexInfo.UMult
		movss xmm1, [eax]FTexInfo.VMult
		movss xmm2, f255

			//Pts in edx
			//Get PtsPlus12B
		lea ebp, [edx + 12]

v_loop:
			mov eax, [edx]
			add edx, 4

			movss xmm3, [eax]FTransTexture.U
			mulss xmm3, xmm0
			movss [ebx]FGLTexCoord.u, xmm3
			movss xmm3, [eax]FTransTexture.V
			mulss xmm3, xmm1
			movss [ebx]FGLTexCoord.v, xmm3
			add ebx, TYPE FGLTexCoord

			mov [esp], ebx

			movss xmm6, f1
			subss xmm6, [eax]FTransSample.Fog + 12
			mulss xmm6, xmm2

			mov ecx, [eax]FOutVector.Point.X
			mov [edi]FGLVertex.x, ecx
			mov ecx, [eax]FOutVector.Point.Y
			mov [edi]FGLVertex.y, ecx
			mov ecx, [eax]FOutVector.Point.Z
			mov [edi]FGLVertex.z, ecx
			add edi, TYPE FGLVertex

			movss xmm3, [eax]FTransSample.Light + 0
			mulss xmm3, xmm6
			movss xmm4, [eax]FTransSample.Light + 4
			mulss xmm4, xmm6
			movss xmm5, [eax]FTransSample.Light + 8
			mulss xmm5, xmm6
			cvtss2si ebx, xmm3
			and ebx, 255
			cvtss2si ecx, xmm4
			and ecx, 255
			shl ecx, 8
			or ebx, ecx
			cvtss2si ecx, xmm5
			shl ecx, 16
			or ecx, 0xFF000000
			or ebx, ecx
			mov [esi]FGLDoubleColor.color, ebx

			mov ebx, [esp]

			movss xmm3, [eax]FTransSample.Fog + 0
			mulss xmm3, xmm2
			movss xmm4, [eax]FTransSample.Fog + 4
			mulss xmm4, xmm2
			movss xmm5, [eax]FTransSample.Fog + 8
			mulss xmm5, xmm2
			cvtss2si eax, xmm3
			and eax, 255
			cvtss2si ecx, xmm4
			and ecx, 255
			shl ecx, 8
			or eax, ecx
			cvtss2si ecx, xmm5
			and ecx, 255
			shl ecx, 16
			or eax, ecx
			mov [esi]FGLDoubleColor.specular, eax
			add esi, TYPE FGLDoubleColor

			cmp edx, ebp
			jne v_loop

		add esp, 4
		pop ebp
		pop edi
		pop esi
		pop ebx

		ret
	}
}

__declspec(naked) void FASTCALL Buffer3FoggedVerts_SSE2(UOpenGLRenderDevice *pRD, FTransTexture **Pts) {
	static __m128 fColorMul = { 255.0f, 255.0f, 255.0f, 0.0f };
	static DWORD alphaOr = 0xFF000000;
	static float f1 = 1.0f;
	__asm {
		//pRD is in ecx
		//Pts is in edx

		push ebx
		push esi
		push edi
		push ebp

		mov eax, [ecx]UOpenGLRenderDevice.BufferedVerts

		lea ebx, [eax*8]
		add ebx, [ecx]UOpenGLRenderDevice.TexCoordArray[0]

		lea esi, [eax*8]
		add esi, [ecx]UOpenGLRenderDevice.DoubleColorArray

		lea edi, [eax + eax*2]
		lea edi, [edi*4]
		add edi, [ecx]UOpenGLRenderDevice.VertexArray

		add eax, 3
		mov [ecx]UOpenGLRenderDevice.BufferedVerts, eax

		lea eax, [ecx]UOpenGLRenderDevice.TexInfo
		movss xmm0, [eax]FTexInfo.UMult
		movss xmm1, [eax]FTexInfo.VMult
		movaps xmm2, fColorMul
		movd xmm3, alphaOr

			//Pts in edx
			//Get PtsPlus12B
		lea ebp, [edx + 12]

v_loop:
			mov eax, [edx]
			add edx, 4

			movss xmm4, [eax]FTransTexture.U
			mulss xmm4, xmm0
			movss [ebx]FGLTexCoord.u, xmm4
			movss xmm4, [eax]FTransTexture.V
			mulss xmm4, xmm1
			movss [ebx]FGLTexCoord.v, xmm4
			add ebx, TYPE FGLTexCoord

			movss xmm6, f1
			subss xmm6, [eax]FTransSample.Fog + 12
			mulss xmm6, xmm2
			shufps xmm6, xmm6, 0x00

			mov ecx, [eax]FOutVector.Point.X
			mov [edi]FGLVertex.x, ecx
			mov ecx, [eax]FOutVector.Point.Y
			mov [edi]FGLVertex.y, ecx
			mov ecx, [eax]FOutVector.Point.Z
			mov [edi]FGLVertex.z, ecx
			add edi, TYPE FGLVertex

			movups xmm4, [eax]FTransSample.Light
			mulps xmm4, xmm6
			cvtps2dq xmm4, xmm4

			movups xmm5, [eax]FTransSample.Fog
			mulps xmm5, xmm2
			cvtps2dq xmm5, xmm5
			packssdw xmm4, xmm5
			packuswb xmm4, xmm4
			por xmm4, xmm3
			movq QWORD PTR ([esi]FGLDoubleColor.color), xmm4
			add esi, TYPE FGLDoubleColor

			cmp edx, ebp
			jne v_loop

		pop ebp
		pop edi
		pop esi
		pop ebx

		ret
	}
}
#endif


//Must be called with (NumPts > 3)
void UOpenGLRenderDevice::BufferAdditionalClippedVerts(FTransTexture **Pts, INT NumPts) {
	INT FirstVert = BufferedVerts - 3;
	INT i;

	i = 3;
	do {
		TexCoordArray[0][BufferedVerts] = TexCoordArray[0][FirstVert];
		if (m_requestedColorFlags & CF_DUAL_COLOR_ARRAY) {
			DoubleColorArray[BufferedVerts] = DoubleColorArray[FirstVert];
		} else if (m_requestedColorFlags & CF_COLOR_ARRAY) {
			SingleColorArray[BufferedVerts] = SingleColorArray[FirstVert];
		}
		VertexArray[BufferedVerts] = VertexArray[FirstVert];
		BufferedVerts++;

		TexCoordArray[0][BufferedVerts] = TexCoordArray[0][BufferedVerts - 2];
		if (m_requestedColorFlags & CF_DUAL_COLOR_ARRAY) {
			DoubleColorArray[BufferedVerts] = DoubleColorArray[BufferedVerts - 2];
		} else if (m_requestedColorFlags & CF_COLOR_ARRAY) {
			SingleColorArray[BufferedVerts] = SingleColorArray[BufferedVerts - 2];
		}
		VertexArray[BufferedVerts] = VertexArray[BufferedVerts - 2];
		BufferedVerts++;

		const FTransTexture *P = Pts[i];
		FGLTexCoord &destTexCoordArray = TexCoordArray[0][BufferedVerts];
		destTexCoordArray.u = P->U * TexInfo[0].UMult;
		destTexCoordArray.v = P->V * TexInfo[0].VMult;
		if (m_requestedColorFlags & CF_DUAL_COLOR_ARRAY) {
			FLOAT f255_Times_One_Minus_FogW = 255.0f * (1.0f - P->Fog.W);
			DoubleColorArray[BufferedVerts].color = FPlaneTo_RGBScaledClamped_A255(&P->Light, f255_Times_One_Minus_FogW);
			DoubleColorArray[BufferedVerts].specular = FPlaneTo_RGBClamped_A0(&P->Fog);
		} else if (m_requestedColorFlags & CF_COLOR_ARRAY) {
#ifdef UTGLR_RUNE_BUILD
			SingleColorArray[BufferedVerts].color = FPlaneTo_RGBClamped_Aub(&P->Light, m_gpAlpha);
#else
			SingleColorArray[BufferedVerts].color = FPlaneTo_RGBClamped_A255(&P->Light);
#endif
		}
		FGLVertex &destVertexArray = VertexArray[BufferedVerts];
		destVertexArray.x = P->Point.X;
		destVertexArray.y = P->Point.Y;
		destVertexArray.z = P->Point.Z;
		BufferedVerts++;
	} while (++i < NumPts);

	return;
}
