/*=============================================================================
	VertexBuffers.cpp: filling the vertex ring for gouraud geometry.

	The naked forms read the render device's layout directly, so they and the class
	have to be changed together.
=============================================================================*/

#include "../D3D9Drv.h"
#include "../D3D9.h"
#include "VertexBuffers.h"

void FASTCALL Buffer3Verts(UD3D9RenderDevice *pRD, FTransTexture **Pts) {
	FGLTexCoord *pTexCoordArray = &pRD->m_pTexCoordArray[0][pRD->m_bufferedVerts];
	FGLVertexColor *pVertexColorArray = &pRD->m_pVertexColorArray[pRD->m_bufferedVerts];
	FGLSecondaryColor *pSecondaryColorArray = &pRD->m_pSecondaryColorArray[pRD->m_bufferedVerts];
	pRD->m_bufferedVerts += 3;
	for (INT i = 0; i < 3; i++) {
		const FTransTexture *P = *Pts++;

		pTexCoordArray->u = P->U * pRD->TexInfo[0].UMult;
		pTexCoordArray->v = P->V * pRD->TexInfo[0].VMult;
		pTexCoordArray++;

		pVertexColorArray->x = P->Point.X;
		pVertexColorArray->y = P->Point.Y;
		pVertexColorArray->z = P->Point.Z;
		if (pRD->m_requestedColorFlags & UD3D9RenderDevice::CF_FOG_MODE) {
			FLOAT f255_Times_One_Minus_FogW = 255.0f * (1.0f - P->Fog.W);

			pVertexColorArray->color = UD3D9RenderDevice::FPlaneTo_BGRScaledClamped_A255(&P->Light, f255_Times_One_Minus_FogW);

			pSecondaryColorArray->specular = UD3D9RenderDevice::FPlaneTo_BGRClamped_A0(&P->Fog);
			pSecondaryColorArray++;
		} else if (pRD->m_requestedColorFlags & UD3D9RenderDevice::CF_COLOR_ARRAY) {
#ifdef UTGLR_RUNE_BUILD
			pVertexColorArray->color = UD3D9RenderDevice::FPlaneTo_BGRClamped_Aub(&P->Light, pRD->m_gpAlpha);
#else
			pVertexColorArray->color = UD3D9RenderDevice::FPlaneTo_BGRClamped_A255(&P->Light);
#endif
		} else {
			pVertexColorArray->color = 0xFFFFFFFF;
		}
		pVertexColorArray++;
	}
}

void FASTCALL Buffer3BasicVerts(UD3D9RenderDevice *pRD, FTransTexture **Pts) {
	FGLTexCoord *pTexCoordArray = &pRD->m_pTexCoordArray[0][pRD->m_bufferedVerts];
	FGLVertexColor *pVertexColorArray = &pRD->m_pVertexColorArray[pRD->m_bufferedVerts];
	pRD->m_bufferedVerts += 3;
	FLOAT UMult = pRD->TexInfo[0].UMult;
	FLOAT VMult = pRD->TexInfo[0].VMult;
	for (INT i = 0; i < 3; i++) {
		const FTransTexture *P = *Pts++;

		pTexCoordArray->u = P->U * UMult;
		pTexCoordArray->v = P->V * VMult;
		pTexCoordArray++;

		pVertexColorArray->x = P->Point.X;
		pVertexColorArray->y = P->Point.Y;
		pVertexColorArray->z = P->Point.Z;
		pVertexColorArray->color = 0xFFFFFFFF;
		pVertexColorArray++;
	}
}

void FASTCALL Buffer3ColoredVerts(UD3D9RenderDevice *pRD, FTransTexture **Pts) {
	FGLTexCoord *pTexCoordArray = &pRD->m_pTexCoordArray[0][pRD->m_bufferedVerts];
	FGLVertexColor *pVertexColorArray = &pRD->m_pVertexColorArray[pRD->m_bufferedVerts];
	pRD->m_bufferedVerts += 3;
	for (INT i = 0; i < 3; i++) {
		const FTransTexture *P = *Pts++;

		pTexCoordArray->u = P->U * pRD->TexInfo[0].UMult;
		pTexCoordArray->v = P->V * pRD->TexInfo[0].VMult;
		pTexCoordArray++;

		pVertexColorArray->x = P->Point.X;
		pVertexColorArray->y = P->Point.Y;
		pVertexColorArray->z = P->Point.Z;
		pVertexColorArray->color = UD3D9RenderDevice::FPlaneTo_BGRClamped_A255(&P->Light);
		pVertexColorArray++;
	}
}

#ifdef UTGLR_INCLUDE_SSE_CODE
__declspec(naked) void FASTCALL Buffer3ColoredVerts_SSE(UD3D9RenderDevice *pRD, FTransTexture **Pts) {
	static float f255 = 255.0f;
	__asm {
		//pRD is in ecx, Pts is in edx

		push ebx
		push esi
		push edi

		mov eax, [ecx]UD3D9RenderDevice.m_bufferedVerts

		lea ebx, [eax*8]
		add ebx, [ecx]UD3D9RenderDevice.m_pTexCoordArray[0]

		mov edi, eax
		shl edi, 4
		add edi, [ecx]UD3D9RenderDevice.m_pVertexColorArray

			//m_bufferedVerts += 3
		add eax, 3
		mov [ecx]UD3D9RenderDevice.m_bufferedVerts, eax

		lea eax, [ecx]UD3D9RenderDevice.TexInfo
		movss xmm0, [eax]FTexInfo.UMult
		movss xmm1, [eax]FTexInfo.VMult
		movss xmm2, f255

			//Pts in edx
			//Get PtsPlus12B
		lea esi, [edx + 12]

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
			mov [edi]FGLVertexColor.x, ecx
			mov ecx, [eax]FOutVector.Point.Y
			mov [edi]FGLVertexColor.y, ecx
			mov ecx, [eax]FOutVector.Point.Z
			mov [edi]FGLVertexColor.z, ecx

			movss xmm3, [eax]FTransSample.Light + 0
			mulss xmm3, xmm2
			movss xmm4, [eax]FTransSample.Light + 4
			mulss xmm4, xmm2
			movss xmm5, [eax]FTransSample.Light + 8
			mulss xmm5, xmm2
			cvtss2si eax, xmm3
			shl eax, 16
			cvtss2si ecx, xmm4
			and ecx, 255
			shl ecx, 8
			or eax, ecx
			cvtss2si ecx, xmm5
			and ecx, 255
			or ecx, 0xFF000000
			or eax, ecx
			mov [edi]FGLVertexColor.color, eax
			add edi, TYPE FGLVertexColor

			cmp edx, esi
			jne v_loop

		pop edi
		pop esi
		pop ebx

		ret
	}
}

__declspec(naked) void FASTCALL Buffer3ColoredVerts_SSE2(UD3D9RenderDevice *pRD, FTransTexture **Pts) {
	static __m128 fColorMul = { 255.0f, 255.0f, 255.0f, 0.0f };
	static DWORD alphaOr = 0xFF000000;
	__asm {
		//pRD is in ecx, Pts is in edx

		push ebx
		push esi
		push edi

		mov eax, [ecx]UD3D9RenderDevice.m_bufferedVerts

		lea ebx, [eax*8]
		add ebx, [ecx]UD3D9RenderDevice.m_pTexCoordArray[0]

		mov edi, eax
		shl edi, 4
		add edi, [ecx]UD3D9RenderDevice.m_pVertexColorArray

			//m_bufferedVerts += 3
		add eax, 3
		mov [ecx]UD3D9RenderDevice.m_bufferedVerts, eax

		lea eax, [ecx]UD3D9RenderDevice.TexInfo
		movss xmm0, [eax]FTexInfo.UMult
		movss xmm1, [eax]FTexInfo.VMult
		movaps xmm2, fColorMul
		movd xmm3, alphaOr

			//Pts in edx
			//Get PtsPlus12B
		lea esi, [edx + 12]

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
			mov [edi]FGLVertexColor.x, ecx
			mov ecx, [eax]FOutVector.Point.Y
			mov [edi]FGLVertexColor.y, ecx
			mov ecx, [eax]FOutVector.Point.Z
			mov [edi]FGLVertexColor.z, ecx

			movups xmm4, [eax]FTransSample.Light
			shufps xmm4, xmm4, 0xC6
			mulps xmm4, xmm2
			cvtps2dq xmm4, xmm4
			packssdw xmm4, xmm4
			packuswb xmm4, xmm4
			por xmm4, xmm3
			movd [edi]FGLVertexColor.color, xmm4
			add edi, TYPE FGLVertexColor

			cmp edx, esi
			jne v_loop

		pop edi
		pop esi
		pop ebx

		ret
	}
}
#endif

void FASTCALL Buffer3FoggedVerts(UD3D9RenderDevice *pRD, FTransTexture **Pts) {
	FGLTexCoord *pTexCoordArray = &pRD->m_pTexCoordArray[0][pRD->m_bufferedVerts];
	FGLVertexColor *pVertexColorArray = &pRD->m_pVertexColorArray[pRD->m_bufferedVerts];
	FGLSecondaryColor *pSecondaryColorArray = &pRD->m_pSecondaryColorArray[pRD->m_bufferedVerts];
	pRD->m_bufferedVerts += 3;
	for (INT i = 0; i < 3; i++) {
		const FTransTexture *P = *Pts++;

		pTexCoordArray->u = P->U * pRD->TexInfo[0].UMult;
		pTexCoordArray->v = P->V * pRD->TexInfo[0].VMult;
		pTexCoordArray++;

		pVertexColorArray->x = P->Point.X;
		pVertexColorArray->y = P->Point.Y;
		pVertexColorArray->z = P->Point.Z;
		FLOAT f255_Times_One_Minus_FogW = 255.0f * (1.0f - P->Fog.W);
		pVertexColorArray->color = UD3D9RenderDevice::FPlaneTo_BGRScaledClamped_A255(&P->Light, f255_Times_One_Minus_FogW);
		pVertexColorArray++;

		pSecondaryColorArray->specular = UD3D9RenderDevice::FPlaneTo_BGRClamped_A0(&P->Fog);
		pSecondaryColorArray++;
	}
}

#ifdef UTGLR_INCLUDE_SSE_CODE
__declspec(naked) void FASTCALL Buffer3FoggedVerts_SSE(UD3D9RenderDevice *pRD, FTransTexture **Pts) {
	static float f255 = 255.0f;
	static float f1 = 1.0f;
	__asm {
		//pRD is in ecx, Pts is in edx

		push ebx
		push esi
		push edi
		push ebp
		sub esp, 4

		mov eax, [ecx]UD3D9RenderDevice.m_bufferedVerts

		lea ebx, [eax*8]
		add ebx, [ecx]UD3D9RenderDevice.m_pTexCoordArray[0]

		mov edi, eax
		shl edi, 4
		add edi, [ecx]UD3D9RenderDevice.m_pVertexColorArray

		lea esi, [eax*4]
		add esi, [ecx]UD3D9RenderDevice.m_pSecondaryColorArray

			//m_bufferedVerts += 3
		add eax, 3
		mov [ecx]UD3D9RenderDevice.m_bufferedVerts, eax

		lea eax, [ecx]UD3D9RenderDevice.TexInfo
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
			mov [edi]FGLVertexColor.x, ecx
			mov ecx, [eax]FOutVector.Point.Y
			mov [edi]FGLVertexColor.y, ecx
			mov ecx, [eax]FOutVector.Point.Z
			mov [edi]FGLVertexColor.z, ecx

			movss xmm3, [eax]FTransSample.Light + 0
			mulss xmm3, xmm6
			movss xmm4, [eax]FTransSample.Light + 4
			mulss xmm4, xmm6
			movss xmm5, [eax]FTransSample.Light + 8
			mulss xmm5, xmm6
			cvtss2si ebx, xmm3
			shl ebx, 16
			cvtss2si ecx, xmm4
			and ecx, 255
			shl ecx, 8
			or ebx, ecx
			cvtss2si ecx, xmm5
			and ecx, 255
			or ecx, 0xFF000000
			or ebx, ecx
			mov [edi]FGLVertexColor.color, ebx
			add edi, TYPE FGLVertexColor

			mov ebx, [esp]

			movss xmm3, [eax]FTransSample.Fog + 0
			mulss xmm3, xmm2
			movss xmm4, [eax]FTransSample.Fog + 4
			mulss xmm4, xmm2
			movss xmm5, [eax]FTransSample.Fog + 8
			mulss xmm5, xmm2
			cvtss2si eax, xmm3
			and eax, 255
			shl eax, 16
			cvtss2si ecx, xmm4
			and ecx, 255
			shl ecx, 8
			or eax, ecx
			cvtss2si ecx, xmm5
			and ecx, 255
			or eax, ecx
			mov [esi]FGLSecondaryColor.specular, eax
			add esi, TYPE FGLSecondaryColor

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

__declspec(naked) void FASTCALL Buffer3FoggedVerts_SSE2(UD3D9RenderDevice *pRD, FTransTexture **Pts) {
	static __m128 fColorMul = { 255.0f, 255.0f, 255.0f, 0.0f };
	static DWORD alphaOr = 0xFF000000;
	static float f1 = 1.0f;
	__asm {
		//pRD is in ecx, Pts is in edx

		push ebx
		push esi
		push edi
		push ebp

		mov eax, [ecx]UD3D9RenderDevice.m_bufferedVerts

		lea ebx, [eax*8]
		add ebx, [ecx]UD3D9RenderDevice.m_pTexCoordArray[0]

		mov edi, eax
		shl edi, 4
		add edi, [ecx]UD3D9RenderDevice.m_pVertexColorArray

		lea esi, [eax*4]
		add esi, [ecx]UD3D9RenderDevice.m_pSecondaryColorArray

			//m_bufferedVerts += 3
		add eax, 3
		mov [ecx]UD3D9RenderDevice.m_bufferedVerts, eax

		lea eax, [ecx]UD3D9RenderDevice.TexInfo
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
			mov [edi]FGLVertexColor.x, ecx
			mov ecx, [eax]FOutVector.Point.Y
			mov [edi]FGLVertexColor.y, ecx
			mov ecx, [eax]FOutVector.Point.Z
			mov [edi]FGLVertexColor.z, ecx

			movups xmm4, [eax]FTransSample.Light
			shufps xmm4, xmm4, 0xC6
			mulps xmm4, xmm6
			cvtps2dq xmm4, xmm4
			packssdw xmm4, xmm4
			packuswb xmm4, xmm4
			por xmm4, xmm3
			movd [edi]FGLVertexColor.color, xmm4
			add edi, TYPE FGLVertexColor

			movups xmm4, [eax]FTransSample.Fog
			shufps xmm4, xmm4, 0xC6
			mulps xmm4, xmm2
			cvtps2dq xmm4, xmm4
			packssdw xmm4, xmm4
			packuswb xmm4, xmm4
			movd [esi]FGLSecondaryColor.specular, xmm4
			add esi, TYPE FGLSecondaryColor

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
void UD3D9RenderDevice::BufferAdditionalClippedVerts(FTransTexture **Pts, INT NumPts) {
	INT i;

	i = 3;
	do {
		const FTransTexture *P;
		FGLTexCoord *pTexCoordArray;
		FGLVertexColor *pVertexColorArray;

		P = Pts[0];
		pTexCoordArray = &m_pTexCoordArray[0][m_bufferedVerts];
		pTexCoordArray->u = P->U * TexInfo[0].UMult;
		pTexCoordArray->v = P->V * TexInfo[0].VMult;
		pVertexColorArray = &m_pVertexColorArray[m_bufferedVerts];
		pVertexColorArray->x = P->Point.X;
		pVertexColorArray->y = P->Point.Y;
		pVertexColorArray->z = P->Point.Z;
		if (m_requestedColorFlags & CF_FOG_MODE) {
			FLOAT f255_Times_One_Minus_FogW = 255.0f * (1.0f - P->Fog.W);
			pVertexColorArray->color = FPlaneTo_BGRScaledClamped_A255(&P->Light, f255_Times_One_Minus_FogW);

			m_pSecondaryColorArray[m_bufferedVerts].specular = FPlaneTo_BGRClamped_A0(&P->Fog);
		} else if (m_requestedColorFlags & CF_COLOR_ARRAY) {
#ifdef UTGLR_RUNE_BUILD
			pVertexColorArray->color = FPlaneTo_BGRClamped_Aub(&P->Light, m_gpAlpha);
#else
			pVertexColorArray->color = FPlaneTo_BGRClamped_A255(&P->Light);
#endif
		} else {
			pVertexColorArray->color = 0xFFFFFFFF;
		}
		m_bufferedVerts++;

		P = Pts[i - 1];
		pTexCoordArray = &m_pTexCoordArray[0][m_bufferedVerts];
		pTexCoordArray->u = P->U * TexInfo[0].UMult;
		pTexCoordArray->v = P->V * TexInfo[0].VMult;
		pVertexColorArray = &m_pVertexColorArray[m_bufferedVerts];
		pVertexColorArray->x = P->Point.X;
		pVertexColorArray->y = P->Point.Y;
		pVertexColorArray->z = P->Point.Z;
		if (m_requestedColorFlags & CF_FOG_MODE) {
			FLOAT f255_Times_One_Minus_FogW = 255.0f * (1.0f - P->Fog.W);
			pVertexColorArray->color = FPlaneTo_BGRScaledClamped_A255(&P->Light, f255_Times_One_Minus_FogW);

			m_pSecondaryColorArray[m_bufferedVerts].specular = FPlaneTo_BGRClamped_A0(&P->Fog);
		} else if (m_requestedColorFlags & CF_COLOR_ARRAY) {
#ifdef UTGLR_RUNE_BUILD
			pVertexColorArray->color = FPlaneTo_BGRClamped_Aub(&P->Light, m_gpAlpha);
#else
			pVertexColorArray->color = FPlaneTo_BGRClamped_A255(&P->Light);
#endif
		} else {
			pVertexColorArray->color = 0xFFFFFFFF;
		}
		m_bufferedVerts++;

		P = Pts[i];
		pTexCoordArray = &m_pTexCoordArray[0][m_bufferedVerts];
		pTexCoordArray->u = P->U * TexInfo[0].UMult;
		pTexCoordArray->v = P->V * TexInfo[0].VMult;
		pVertexColorArray = &m_pVertexColorArray[m_bufferedVerts];
		pVertexColorArray->x = P->Point.X;
		pVertexColorArray->y = P->Point.Y;
		pVertexColorArray->z = P->Point.Z;
		if (m_requestedColorFlags & CF_FOG_MODE) {
			FLOAT f255_Times_One_Minus_FogW = 255.0f * (1.0f - P->Fog.W);
			pVertexColorArray->color = FPlaneTo_BGRScaledClamped_A255(&P->Light, f255_Times_One_Minus_FogW);

			m_pSecondaryColorArray[m_bufferedVerts].specular = FPlaneTo_BGRClamped_A0(&P->Fog);
		} else if (m_requestedColorFlags & CF_COLOR_ARRAY) {
#ifdef UTGLR_RUNE_BUILD
			pVertexColorArray->color = FPlaneTo_BGRClamped_Aub(&P->Light, m_gpAlpha);
#else
			pVertexColorArray->color = FPlaneTo_BGRClamped_A255(&P->Light);
#endif
		} else {
			pVertexColorArray->color = 0xFFFFFFFF;
		}
		m_bufferedVerts++;
	} while (++i < NumPts);

	return;
}
