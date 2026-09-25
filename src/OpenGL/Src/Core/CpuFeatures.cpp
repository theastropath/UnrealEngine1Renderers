/*=============================================================================
	CpuFeatures.cpp: what the processor supports.
=============================================================================*/

#include "../OpenGLDrv.h"
#include "../OpenGL.h"

#ifdef UTGLR_INCLUDE_SSE_CODE
bool UOpenGLRenderDevice::CPU_DetectCPUID(void) {
	__try {
		__asm {
			xor eax, eax
			cpuid
		}
	} __except (EXCEPTION_EXECUTE_HANDLER) {
		return false;
	}

	return true;
}

bool UOpenGLRenderDevice::CPU_DetectSSE(void) {
	bool bSupportsSSE;

	if (CPU_DetectCPUID() != true) {
		return false;
	}

	bSupportsSSE = false;
	__asm {
		mov eax, 1
		cpuid

		test edx, 0x02000000
		jz l_no_sse

		mov bSupportsSSE, 1

l_no_sse:
	}

	if (bSupportsSSE == false) {
		return bSupportsSSE;
	}

	__try {
		__asm {
			xorps xmm0, xmm0
		}
	} __except (EXCEPTION_EXECUTE_HANDLER) {
		bSupportsSSE = false;
	}

	return bSupportsSSE;
}

bool UOpenGLRenderDevice::CPU_DetectSSE2(void) {
	bool bSupportsSSE2;

	if (CPU_DetectCPUID() != true) {
		return false;
	}

	bSupportsSSE2 = false;
	__asm {
		mov eax, 1
		cpuid

		test edx, 0x04000000
		jz l_no_sse2

		mov bSupportsSSE2, 1

l_no_sse2:
	}

	if (bSupportsSSE2 == false) {
		return bSupportsSSE2;
	}

	__try {
		__asm {
			xorpd xmm0, xmm0
		}
	} __except (EXCEPTION_EXECUTE_HANDLER) {
		bSupportsSSE2 = false;
	}

	return bSupportsSSE2;
}
#endif //UTGLR_INCLUDE_SSE_CODE
