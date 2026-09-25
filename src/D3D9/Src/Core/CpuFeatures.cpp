/*=============================================================================
	CpuFeatures.cpp: what the processor supports.
=============================================================================*/

#include "../D3D9Drv.h"
#include "../D3D9.h"

#ifdef UTGLR_INCLUDE_SSE_CODE
bool UD3D9RenderDevice::CPU_DetectCPUID(void) {
	int cpuInfo[4];

	//An intrinsic.
	//cpuid clobbers EBX and ECX, which inline asm would hide from the compiler.
	__try {
		__cpuid(cpuInfo, 0);
	} __except (EXCEPTION_EXECUTE_HANDLER) {
		return false;
	}

	return true;
}

bool UD3D9RenderDevice::CPU_DetectSSE(void) {
	bool bSupportsSSE;

	if (CPU_DetectCPUID() != true) {
		return false;
	}

	{
		int cpuInfo[4];

		__cpuid(cpuInfo, 1);
		bSupportsSSE = ((cpuInfo[3] & 0x02000000) != 0);
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

bool UD3D9RenderDevice::CPU_DetectSSE2(void) {
	bool bSupportsSSE2;

	if (CPU_DetectCPUID() != true) {
		return false;
	}

	{
		int cpuInfo[4];

		__cpuid(cpuInfo, 1);
		bSupportsSSE2 = ((cpuInfo[3] & 0x04000000) != 0);
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

bool UD3D9RenderDevice::CPU_DetectSSSE3(void) {
	//Memoized: cpuid serializes and what it reports cannot change while the process runs.
	static bool detected = false;
	static bool supported = false;

	if (detected) {
		return supported;
	}

	//SSSE3 shares SSE2's register file, so SSE2's OS support check covers it.
	if (CPU_DetectSSE2() == true) {
		int cpuInfo[4];

		__cpuid(cpuInfo, 1);
		supported = ((cpuInfo[2] & 0x00000200) != 0);
	}

	detected = true;
	return supported;
}
#endif //UTGLR_INCLUDE_SSE_CODE
