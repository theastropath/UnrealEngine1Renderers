/*=============================================================================
	ShaderPrograms.cpp: the compiled vertex and pixel shaders, and the vertex
	declarations they are fed through.

	Data only. Each array is fxc's output over the source in the comment above it.
	The reconstruction shaders at the end come from regen-recon-shaders.ps1.
=============================================================================*/

#include "../D3D9Drv.h"
#include "../D3D9.h"


/*-----------------------------------------------------------------------------
	Vertex programs.
-----------------------------------------------------------------------------*/

//Vertex shader output registers
//o0 - Transformed position
//o1 - Primary color
//o2 - Secondary color
//o3 - unused
//o4 - TexCoord 0
//o5 - TexCoord 1
//o6 - TexCoord 2
//o7 - TexCoord 3
//o8 - TexCoord 4 / Position

//Vertex shader global constants
//[c0 - c3] - Projection matrix
//c4 - Complex surface XAxis and UDot
//c5 - Complex surface YAxis and VDot
//[c6 - c9] - Up to 4 texture [UPan, VPan, UMult, VMult]
//
//Generated shaders match this table by semantic.
//Read each blob's own declarations before assuming the table holds register for register.

#if 0
static const char *g_tempShaderString =
	"vs_3_0\n"

	"dcl_position v0\n"
	"dcl_color v5\n"
	"dcl_texcoord0 v7\n"

	"dcl_position o0.xyzw\n"
	"dcl_color o1.rgba\n"
	"dcl_texcoord0 o4.xy\n"

	"mov o4.xy, v7\n"

	"mov o1, v5\n"

	"m4x4 o0, v0, c0\n"
;
#endif
extern const DWORD g_vpDefaultRenderingState[] = {
	0xFFFE0300, 0x0200001F, 0x80000000, 0x900F0000, 0x0200001F, 0x8000000A, 0x900F0005, 0x0200001F,
	0x80000005, 0x900F0007, 0x0200001F, 0x80000000, 0xE00F0000, 0x0200001F, 0x8000000A, 0xE00F0001,
	0x0200001F, 0x80000005, 0xE0030004, 0x02000001, 0xE0030004, 0x90E40007, 0x02000001, 0xE00F0001,
	0x90E40005, 0x03000014, 0xE00F0000, 0x90E40000, 0xA0E40000, 0x0000FFFF
};

#if 0
static const char *g_tempShaderString =
	"vs_3_0\n"

	"dcl_position v0\n"
	"dcl_color v5\n"
	"dcl_color1 v6\n"
	"dcl_texcoord0 v7\n"

	"dcl_position o0.xyzw\n"
	"dcl_color o1.rgba\n"
	"dcl_color1 o2.rgba\n"
	"dcl_texcoord0 o4.xy\n"

	"mov o4.xy, v7\n"

	"mov o1, v5\n"
	"mov o2, v6\n"

	"m4x4 o0, v0, c0\n"
;
#endif
extern const DWORD g_vpDefaultRenderingStateWithFog[] = {
	0xFFFE0300, 0x0200001F, 0x80000000, 0x900F0000, 0x0200001F, 0x8000000A, 0x900F0005, 0x0200001F,
	0x8001000A, 0x900F0006, 0x0200001F, 0x80000005, 0x900F0007, 0x0200001F, 0x80000000, 0xE00F0000,
	0x0200001F, 0x8000000A, 0xE00F0001, 0x0200001F, 0x8001000A, 0xE00F0002, 0x0200001F, 0x80000005,
	0xE0030004, 0x02000001, 0xE0030004, 0x90E40007, 0x02000001, 0xE00F0001, 0x90E40005, 0x02000001,
	0xE00F0002, 0x90E40006, 0x03000014, 0xE00F0000, 0x90E40000, 0xA0E40000, 0x0000FFFF
};

#if 0
static const char *g_tempShaderString =
	"vs_3_0\n"

	"dcl_position v0\n"
	"dcl_color v5\n"
	"dcl_texcoord0 v7\n"

	"dcl_position o0.xyzw\n"
	"dcl_color o1.rgba\n"
	"dcl_texcoord0 o4.xy\n"
	"dcl_texcoord4 o8.xyzw\n"

	"mov o4.xy, v7\n"

	"mov o1, v5\n"

	"m4x4 o0, v0, c0\n"
	"mov o8, v0\n"
;
#endif
extern const DWORD g_vpDefaultRenderingStateWithLinearFog[] = {
	0xFFFE0300, 0x0200001F, 0x80000000, 0x900F0000, 0x0200001F, 0x8000000A, 0x900F0005, 0x0200001F,
	0x80000005, 0x900F0007, 0x0200001F, 0x80000000, 0xE00F0000, 0x0200001F, 0x8000000A, 0xE00F0001,
	0x0200001F, 0x80000005, 0xE0030004, 0x0200001F, 0x80040005, 0xE00F0008, 0x02000001, 0xE0030004,
	0x90E40007, 0x02000001, 0xE00F0001, 0x90E40005, 0x03000014, 0xE00F0000, 0x90E40000, 0xA0E40000,
	0x02000001, 0xE00F0008, 0x90E40000, 0x0000FFFF
};

#if 0
static const char *g_tempShaderString =
	"vs_3_0\n"

	"dcl_position v0\n"
	"dcl_color v5\n"

	"dcl_position o0.xyzw\n"
	"dcl_color o1.rgba\n"
	"dcl_texcoord0 o4.xy\n"

	"dp3 r0.x, v0, c4\n"
	"dp3 r0.y, v0, c5\n"
	"add r0.x, r0.x, -c4.w\n"
	"add r0.y, r0.y, -c5.w\n"

	"add r1.xy, r0.xy, -c6.xy\n"
	"mul o4.xy, r1.xy, c6.zw\n"

	"mov o1, v5\n"

	"m4x4 o0, v0, c0\n"
;
#endif
extern const DWORD g_vpComplexSurfaceSingleTexture[] = {
	0xFFFE0300, 0x0200001F, 0x80000000, 0x900F0000, 0x0200001F, 0x8000000A, 0x900F0005, 0x0200001F,
	0x80000000, 0xE00F0000, 0x0200001F, 0x8000000A, 0xE00F0001, 0x0200001F, 0x80000005, 0xE0030004,
	0x03000008, 0x80010000, 0x90E40000, 0xA0E40004, 0x03000008, 0x80020000, 0x90E40000, 0xA0E40005,
	0x03000002, 0x80010000, 0x80000000, 0xA1FF0004, 0x03000002, 0x80020000, 0x80550000, 0xA1FF0005,
	0x03000002, 0x80030001, 0x80540000, 0xA1540006, 0x03000005, 0xE0030004, 0x80540001, 0xA0FE0006,
	0x02000001, 0xE00F0001, 0x90E40005, 0x03000014, 0xE00F0000, 0x90E40000, 0xA0E40000, 0x0000FFFF
};

#if 0
static const char *g_tempShaderString =
	"vs_3_0\n"

	"dcl_position v0\n"
	"dcl_color v5\n"

	"dcl_position o0.xyzw\n"
	"dcl_color o1.rgba\n"
	"dcl_texcoord0 o4.xy\n"
	"dcl_texcoord1 o5.xy\n"

	"dp3 r0.x, v0, c4\n"
	"dp3 r0.y, v0, c5\n"
	"add r0.x, r0.x, -c4.w\n"
	"add r0.y, r0.y, -c5.w\n"

	"add r1.xy, r0.xy, -c6.xy\n"
	"mul o4.xy, r1.xy, c6.zw\n"

	"add r1.xy, r0.xy, -c7.xy\n"
	"mul o5.xy, r1.xy, c7.zw\n"

	"mov o1, v5\n"

	"m4x4 o0, v0, c0\n"
;
#endif
extern const DWORD g_vpComplexSurfaceDualTexture[] = {
	0xFFFE0300, 0x0200001F, 0x80000000, 0x900F0000, 0x0200001F, 0x8000000A, 0x900F0005, 0x0200001F,
	0x80000000, 0xE00F0000, 0x0200001F, 0x8000000A, 0xE00F0001, 0x0200001F, 0x80000005, 0xE0030004,
	0x0200001F, 0x80010005, 0xE0030005, 0x03000008, 0x80010000, 0x90E40000, 0xA0E40004, 0x03000008,
	0x80020000, 0x90E40000, 0xA0E40005, 0x03000002, 0x80010000, 0x80000000, 0xA1FF0004, 0x03000002,
	0x80020000, 0x80550000, 0xA1FF0005, 0x03000002, 0x80030001, 0x80540000, 0xA1540006, 0x03000005,
	0xE0030004, 0x80540001, 0xA0FE0006, 0x03000002, 0x80030001, 0x80540000, 0xA1540007, 0x03000005,
	0xE0030005, 0x80540001, 0xA0FE0007, 0x02000001, 0xE00F0001, 0x90E40005, 0x03000014, 0xE00F0000,
	0x90E40000, 0xA0E40000, 0x0000FFFF
};

#if 0
static const char *g_tempShaderString =
	"vs_3_0\n"

	"dcl_position v0\n"
	"dcl_color v5\n"

	"dcl_position o0.xyzw\n"
	"dcl_color o1.rgba\n"
	"dcl_texcoord0 o4.xy\n"
	"dcl_texcoord1 o5.xy\n"
	"dcl_texcoord2 o6.xy\n"

	"dp3 r0.x, v0, c4\n"
	"dp3 r0.y, v0, c5\n"
	"add r0.x, r0.x, -c4.w\n"
	"add r0.y, r0.y, -c5.w\n"

	"add r1.xy, r0.xy, -c6.xy\n"
	"mul o4.xy, r1.xy, c6.zw\n"

	"add r1.xy, r0.xy, -c7.xy\n"
	"mul o5.xy, r1.xy, c7.zw\n"

	"add r1.xy, r0.xy, -c8.xy\n"
	"mul o6.xy, r1.xy, c8.zw\n"

	"mov o1, v5\n"

	"m4x4 o0, v0, c0\n"
;
#endif
extern const DWORD g_vpComplexSurfaceTripleTexture[] = {
	0xFFFE0300, 0x0200001F, 0x80000000, 0x900F0000, 0x0200001F, 0x8000000A, 0x900F0005, 0x0200001F,
	0x80000000, 0xE00F0000, 0x0200001F, 0x8000000A, 0xE00F0001, 0x0200001F, 0x80000005, 0xE0030004,
	0x0200001F, 0x80010005, 0xE0030005, 0x0200001F, 0x80020005, 0xE0030006, 0x03000008, 0x80010000,
	0x90E40000, 0xA0E40004, 0x03000008, 0x80020000, 0x90E40000, 0xA0E40005, 0x03000002, 0x80010000,
	0x80000000, 0xA1FF0004, 0x03000002, 0x80020000, 0x80550000, 0xA1FF0005, 0x03000002, 0x80030001,
	0x80540000, 0xA1540006, 0x03000005, 0xE0030004, 0x80540001, 0xA0FE0006, 0x03000002, 0x80030001,
	0x80540000, 0xA1540007, 0x03000005, 0xE0030005, 0x80540001, 0xA0FE0007, 0x03000002, 0x80030001,
	0x80540000, 0xA1540008, 0x03000005, 0xE0030006, 0x80540001, 0xA0FE0008, 0x02000001, 0xE00F0001,
	0x90E40005, 0x03000014, 0xE00F0000, 0x90E40000, 0xA0E40000, 0x0000FFFF
};

#if 0
static const char *g_tempShaderString =
	"vs_3_0\n"

	"dcl_position v0\n"
	"dcl_color v5\n"

	"dcl_position o0.xyzw\n"
	"dcl_color o1.rgba\n"
	"dcl_texcoord0 o4.xy\n"
	"dcl_texcoord1 o5.xy\n"
	"dcl_texcoord2 o6.xy\n"
	"dcl_texcoord3 o7.xy\n"

	"dp3 r0.x, v0, c4\n"
	"dp3 r0.y, v0, c5\n"
	"add r0.x, r0.x, -c4.w\n"
	"add r0.y, r0.y, -c5.w\n"

	"add r1.xy, r0.xy, -c6.xy\n"
	"mul o4.xy, r1.xy, c6.zw\n"

	"add r1.xy, r0.xy, -c7.xy\n"
	"mul o5.xy, r1.xy, c7.zw\n"

	"add r1.xy, r0.xy, -c8.xy\n"
	"mul o6.xy, r1.xy, c8.zw\n"

	"add r1.xy, r0.xy, -c9.xy\n"
	"mul o7.xy, r1.xy, c9.zw\n"

	"mov o1, v5\n"

	"m4x4 o0, v0, c0\n"
;
#endif
extern const DWORD g_vpComplexSurfaceQuadTexture[] = {
	0xFFFE0300, 0x0200001F, 0x80000000, 0x900F0000, 0x0200001F, 0x8000000A, 0x900F0005, 0x0200001F,
	0x80000000, 0xE00F0000, 0x0200001F, 0x8000000A, 0xE00F0001, 0x0200001F, 0x80000005, 0xE0030004,
	0x0200001F, 0x80010005, 0xE0030005, 0x0200001F, 0x80020005, 0xE0030006, 0x0200001F, 0x80030005,
	0xE0030007, 0x03000008, 0x80010000, 0x90E40000, 0xA0E40004, 0x03000008, 0x80020000, 0x90E40000,
	0xA0E40005, 0x03000002, 0x80010000, 0x80000000, 0xA1FF0004, 0x03000002, 0x80020000, 0x80550000,
	0xA1FF0005, 0x03000002, 0x80030001, 0x80540000, 0xA1540006, 0x03000005, 0xE0030004, 0x80540001,
	0xA0FE0006, 0x03000002, 0x80030001, 0x80540000, 0xA1540007, 0x03000005, 0xE0030005, 0x80540001,
	0xA0FE0007, 0x03000002, 0x80030001, 0x80540000, 0xA1540008, 0x03000005, 0xE0030006, 0x80540001,
	0xA0FE0008, 0x03000002, 0x80030001, 0x80540000, 0xA1540009, 0x03000005, 0xE0030007, 0x80540001,
	0xA0FE0009, 0x02000001, 0xE00F0001, 0x90E40005, 0x03000014, 0xE00F0000, 0x90E40000, 0xA0E40000,
	0x0000FFFF
};

/*
Pass-through variants for the batched path, where the CPU writes the texture coordinates because
the per-facet map axes and pan/scale would otherwise change every facet and no batch would survive
the first one, and the input position has to stay a float4 here, since declared float3 the compiler
synthesizes w into a constant register this driver keeps its own data in, the same trap the
instanced quad shader works around by carrying its w in the vertex stream beside the corners.
*/
extern const DWORD g_vpComplexSurfaceSingleTextureCT[] = {
	0xFFFE0300, 0x0200001F, 0x80000000, 0x900F0000, 0x0200001F, 0x8000000A, 0x900F0001, 0x0200001F,
	0x80000005, 0x900F0002, 0x0200001F, 0x80000000, 0xE00F0000, 0x0200001F, 0x8000000A, 0xE00F0001,
	0x0200001F, 0x80000005, 0xE0030002, 0x03000009, 0xE0010000, 0xA0E40000, 0x90E40000, 0x03000009,
	0xE0020000, 0xA0E40001, 0x90E40000, 0x03000009, 0xE0040000, 0xA0E40002, 0x90E40000, 0x03000009,
	0xE0080000, 0xA0E40003, 0x90E40000, 0x02000001, 0xE00F0001, 0x90E40001, 0x02000001, 0xE0030002,
	0x90E40002, 0x0000FFFF
};

extern const DWORD g_vpComplexSurfaceDualTextureCT[] = {
	0xFFFE0300, 0x0200001F, 0x80000000, 0x900F0000, 0x0200001F, 0x8000000A, 0x900F0001, 0x0200001F,
	0x80000005, 0x900F0002, 0x0200001F, 0x80010005, 0x900F0003, 0x0200001F, 0x80000000, 0xE00F0000,
	0x0200001F, 0x8000000A, 0xE00F0001, 0x0200001F, 0x80000005, 0xE0030002, 0x0200001F, 0x80010005,
	0xE0030003, 0x03000009, 0xE0010000, 0xA0E40000, 0x90E40000, 0x03000009, 0xE0020000, 0xA0E40001,
	0x90E40000, 0x03000009, 0xE0040000, 0xA0E40002, 0x90E40000, 0x03000009, 0xE0080000, 0xA0E40003,
	0x90E40000, 0x02000001, 0xE00F0001, 0x90E40001, 0x02000001, 0xE0030002, 0x90E40002, 0x02000001,
	0xE0030003, 0x90E40003, 0x0000FFFF
};

extern const DWORD g_vpComplexSurfaceTripleTextureCT[] = {
	0xFFFE0300, 0x0200001F, 0x80000000, 0x900F0000, 0x0200001F, 0x8000000A, 0x900F0001, 0x0200001F,
	0x80000005, 0x900F0002, 0x0200001F, 0x80010005, 0x900F0003, 0x0200001F, 0x80020005, 0x900F0004,
	0x0200001F, 0x80000000, 0xE00F0000, 0x0200001F, 0x8000000A, 0xE00F0001, 0x0200001F, 0x80000005,
	0xE0030002, 0x0200001F, 0x80010005, 0xE0030003, 0x0200001F, 0x80020005, 0xE0030004, 0x03000009,
	0xE0010000, 0xA0E40000, 0x90E40000, 0x03000009, 0xE0020000, 0xA0E40001, 0x90E40000, 0x03000009,
	0xE0040000, 0xA0E40002, 0x90E40000, 0x03000009, 0xE0080000, 0xA0E40003, 0x90E40000, 0x02000001,
	0xE00F0001, 0x90E40001, 0x02000001, 0xE0030002, 0x90E40002, 0x02000001, 0xE0030003, 0x90E40003,
	0x02000001, 0xE0030004, 0x90E40004, 0x0000FFFF
};

extern const DWORD g_vpComplexSurfaceQuadTextureCT[] = {
	0xFFFE0300, 0x0200001F, 0x80000000, 0x900F0000, 0x0200001F, 0x8000000A, 0x900F0001, 0x0200001F,
	0x80000005, 0x900F0002, 0x0200001F, 0x80010005, 0x900F0003, 0x0200001F, 0x80020005, 0x900F0004,
	0x0200001F, 0x80030005, 0x900F0005, 0x0200001F, 0x80000000, 0xE00F0000, 0x0200001F, 0x8000000A,
	0xE00F0001, 0x0200001F, 0x80000005, 0xE0030002, 0x0200001F, 0x80010005, 0xE0030003, 0x0200001F,
	0x80020005, 0xE0030004, 0x0200001F, 0x80030005, 0xE0030005, 0x03000009, 0xE0010000, 0xA0E40000,
	0x90E40000, 0x03000009, 0xE0020000, 0xA0E40001, 0x90E40000, 0x03000009, 0xE0040000, 0xA0E40002,
	0x90E40000, 0x03000009, 0xE0080000, 0xA0E40003, 0x90E40000, 0x02000001, 0xE00F0001, 0x90E40001,
	0x02000001, 0xE0030002, 0x90E40002, 0x02000001, 0xE0030003, 0x90E40003, 0x02000001, 0xE0030004,
	0x90E40004, 0x02000001, 0xE0030005, 0x90E40005, 0x0000FFFF
};

//One more vertex shader lives beside its only caller in Geometry/Deferred.cpp.

#if 0
static const char *g_tempShaderString =
	"vs_3_0\n"

	"dcl_position v0\n"
	"dcl_color v5\n"

	"dcl_position o0.xyzw\n"
	"dcl_color o1.rgba\n"
	"dcl_texcoord0 o4.xyzw\n"
	"dcl_texcoord4 o8.xyzw\n"

	"def c10, 4.223f, 4.223f, 0.0f, 1.0f\n"

	"dp3 r0.x, v0, c4\n"
	"dp3 r0.y, v0, c5\n"
	"add r0.x, r0.x, -c4.w\n"
	"add r0.y, r0.y, -c5.w\n"

	"add r1.xy, r0.xy, -c6.xy\n"
	"mul r1.xy, r1.xy, c6.zw\n"
	"mul o4.xyzw, r1.xyxy, c10.wwxy\n"

	"mov o1, v5\n"

	"m4x4 o0, v0, c0\n"
	"mov o8, v0\n"
;
#endif
extern const DWORD g_vpDetailTexture[] = {
	0xFFFE0300, 0x0200001F, 0x80000000, 0x900F0000, 0x0200001F, 0x8000000A, 0x900F0005,
	0x0200001F, 0x80000000, 0xE00F0000, 0x0200001F, 0x8000000A, 0xE00F0001,
	0x0200001F, 0x80000005, 0xE00F0004, 0x0200001F, 0x80040005, 0xE00F0008, 0x05000051, 0xA00F000A,
	0x408722D1, 0x408722D1, 0x00000000, 0x3F800000, 0x03000008, 0x80010000, 0x90E40000, 0xA0E40004,
	0x03000008, 0x80020000, 0x90E40000, 0xA0E40005, 0x03000002, 0x80010000, 0x80000000, 0xA1FF0004,
	0x03000002, 0x80020000, 0x80550000, 0xA1FF0005, 0x03000002, 0x80030001, 0x80540000, 0xA1540006,
	0x03000005, 0x80030001, 0x80540001, 0xA0FE0006, 0x03000005, 0xE00F0004, 0x80440001, 0xA04F000A,
	0x02000001, 0xE00F0001, 0x90E40005, 0x03000014, 0xE00F0000, 0x90E40000, 0xA0E40000, 0x02000001,
	0xE00F0008, 0x90E40000, 0x0000FFFF
};

#if 0
static const char *g_tempShaderString =
	"vs_3_0\n"

	"dcl_position v0\n"
	"dcl_color v5\n"

	"dcl_position o0.xyzw\n"
	"dcl_color o1.rgba\n"
	"dcl_texcoord0 o4.xy\n"
	"dcl_texcoord1 o5.xyzw\n"
	"dcl_texcoord4 o8.xyzw\n"

	"def c10, 4.223f, 4.223f, 0.0f, 1.0f\n"

	"dp3 r0.x, v0, c4\n"
	"dp3 r0.y, v0, c5\n"
	"add r0.x, r0.x, -c4.w\n"
	"add r0.y, r0.y, -c5.w\n"

	"add r1.xy, r0.xy, -c6.xy\n"
	"mul o4.xy, r1.xy, c6.zw\n"

	"add r1.xy, r0.xy, -c7.xy\n"
	"mul r1.xy, r1.xy, c7.zw\n"
	"mul o5.xyzw, r1.xyxy, c10.wwxy\n"

	"mov o1, v5\n"

	"m4x4 o0, v0, c0\n"
	"mov o8, v0\n"
;
#endif
extern const DWORD g_vpComplexSurfaceSingleTextureAndDetailTexture[] = {
	0xFFFE0300, 0x0200001F, 0x80000000, 0x900F0000, 0x0200001F, 0x8000000A, 0x900F0005,
	0x0200001F, 0x80000000, 0xE00F0000, 0x0200001F, 0x8000000A, 0xE00F0001,
	0x0200001F, 0x80000005, 0xE0030004, 0x0200001F, 0x80010005, 0xE00F0005, 0x0200001F, 0x80040005,
	0xE00F0008, 0x05000051, 0xA00F000A, 0x408722D1, 0x408722D1, 0x00000000, 0x3F800000, 0x03000008,
	0x80010000, 0x90E40000, 0xA0E40004, 0x03000008, 0x80020000, 0x90E40000, 0xA0E40005, 0x03000002,
	0x80010000, 0x80000000, 0xA1FF0004, 0x03000002, 0x80020000, 0x80550000, 0xA1FF0005, 0x03000002,
	0x80030001, 0x80540000, 0xA1540006, 0x03000005, 0xE0030004, 0x80540001, 0xA0FE0006, 0x03000002,
	0x80030001, 0x80540000, 0xA1540007, 0x03000005, 0x80030001, 0x80540001, 0xA0FE0007, 0x03000005,
	0xE00F0005, 0x80440001, 0xA04F000A, 0x02000001, 0xE00F0001, 0x90E40005, 0x03000014, 0xE00F0000,
	0x90E40000, 0xA0E40000, 0x02000001, 0xE00F0008, 0x90E40000, 0x0000FFFF
};

#if 0
static const char *g_tempShaderString =
	"vs_3_0\n"

	"dcl_position v0\n"
	"dcl_color v5\n"

	"dcl_position o0.xyzw\n"
	"dcl_color o1.rgba\n"
	"dcl_texcoord0 o4.xy\n"
	"dcl_texcoord1 o5.xy\n"
	"dcl_texcoord2 o6.xyzw\n"
	"dcl_texcoord4 o8.xyzw\n"

	"def c10, 4.223f, 4.223f, 0.0f, 1.0f\n"

	"dp3 r0.x, v0, c4\n"
	"dp3 r0.y, v0, c5\n"
	"add r0.x, r0.x, -c4.w\n"
	"add r0.y, r0.y, -c5.w\n"

	"add r1.xy, r0.xy, -c6.xy\n"
	"mul o4.xy, r1.xy, c6.zw\n"

	"add r1.xy, r0.xy, -c7.xy\n"
	"mul o5.xy, r1.xy, c7.zw\n"

	"add r1.xy, r0.xy, -c8.xy\n"
	"mul r1.xy, r1.xy, c8.zw\n"
	"mul o6.xyzw, r1.xyxy, c10.wwxy\n"

	"mov o1, v5\n"

	"m4x4 o0, v0, c0\n"
	"mov o8, v0\n"
;
#endif
extern const DWORD g_vpComplexSurfaceDualTextureAndDetailTexture[] = {
	0xFFFE0300, 0x0200001F, 0x80000000, 0x900F0000, 0x0200001F, 0x8000000A, 0x900F0005,
	0x0200001F, 0x80000000, 0xE00F0000, 0x0200001F, 0x8000000A, 0xE00F0001,
	0x0200001F, 0x80000005, 0xE0030004, 0x0200001F, 0x80010005, 0xE0030005, 0x0200001F, 0x80020005,
	0xE00F0006, 0x0200001F, 0x80040005, 0xE00F0008, 0x05000051, 0xA00F000A, 0x408722D1, 0x408722D1,
	0x00000000, 0x3F800000, 0x03000008, 0x80010000, 0x90E40000, 0xA0E40004, 0x03000008, 0x80020000,
	0x90E40000, 0xA0E40005, 0x03000002, 0x80010000, 0x80000000, 0xA1FF0004, 0x03000002, 0x80020000,
	0x80550000, 0xA1FF0005, 0x03000002, 0x80030001, 0x80540000, 0xA1540006, 0x03000005, 0xE0030004,
	0x80540001, 0xA0FE0006, 0x03000002, 0x80030001, 0x80540000, 0xA1540007, 0x03000005, 0xE0030005,
	0x80540001, 0xA0FE0007, 0x03000002, 0x80030001, 0x80540000, 0xA1540008, 0x03000005, 0x80030001,
	0x80540001, 0xA0FE0008, 0x03000005, 0xE00F0006, 0x80440001, 0xA04F000A, 0x02000001, 0xE00F0001,
	0x90E40005, 0x03000014, 0xE00F0000, 0x90E40000, 0xA0E40000, 0x02000001, 0xE00F0008, 0x90E40000,
	0x0000FFFF
};

//Clip space already.
#if 0
static const char *g_tempShaderString =
	"vs_3_0\n"

	"dcl_position v0\n"
	"dcl_texcoord0 v7\n"

	"dcl_position o0.xyzw\n"
	"dcl_texcoord0 o4.xy\n"

	"mov o0, v0\n"
	"mov o4.xy, v7\n"
;
#endif
extern const DWORD g_vpGammaCorrection[] = {
	0xFFFE0300, 0x0200001F, 0x80000000, 0x900F0000, 0x0200001F, 0x80000005, 0x900F0007, 0x0200001F,
	0x80000000, 0xE00F0000, 0x0200001F, 0x80000005, 0xE0030004, 0x02000001, 0xE00F0000, 0x90E40000,
	0x02000001, 0xE0030004, 0x90E40007, 0x0000FFFF
};


//Stream definitions
////////////////////

extern const D3DVERTEXELEMENT9 g_oneColorStreamDef[] = {
	{ 0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
	{ 0, 12, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0 },
	D3DDECL_END()
};

static const D3DVERTEXELEMENT9 g_standardSingleTextureStreamDef[] = {
	{ 0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
	{ 0, 12, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0 },
	{ 2, 0, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
	D3DDECL_END()
};

static const D3DVERTEXELEMENT9 g_standardDoubleTextureStreamDef[] = {
	{ 0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
	{ 0, 12, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0 },
	{ 2, 0, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
	{ 3, 0, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 1 },
	D3DDECL_END()
};

static const D3DVERTEXELEMENT9 g_standardTripleTextureStreamDef[] = {
	{ 0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
	{ 0, 12, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0 },
	{ 2, 0, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
	{ 3, 0, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 1 },
	{ 4, 0, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 2 },
	D3DDECL_END()
};

static const D3DVERTEXELEMENT9 g_standardQuadTextureStreamDef[] = {
	{ 0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
	{ 0, 12, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0 },
	{ 2, 0, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
	{ 3, 0, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 1 },
	{ 4, 0, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 2 },
	{ 5, 0, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 3 },
	D3DDECL_END()
};

extern const D3DVERTEXELEMENT9 *g_standardNTextureStreamDefs[MAX_TMUNITS] = {
	g_standardSingleTextureStreamDef,
	g_standardDoubleTextureStreamDef,
	g_standardTripleTextureStreamDef,
	g_standardQuadTextureStreamDef
};

extern const D3DVERTEXELEMENT9 g_twoColorSingleTextureStreamDef[] = {
	{ 0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
	{ 0, 12, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0 },
	{ 1, 0, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 1 },
	{ 2, 0, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
	D3DDECL_END()
};


/*-----------------------------------------------------------------------------
	Fragment programs.
-----------------------------------------------------------------------------*/

//Pixel shader input registers
//v0 - unused, except by the dithered gamma shader which samples through it
//v1 - Primary color
//v2 - Secondary color
//v3 - unused
//v4 - TexCoord 0
//v5 - TexCoord 1
//v6 - TexCoord 2
//v7 - TexCoord 3
//v8 - TexCoord 4 / Position

//Pixel shader global constants
//c0.x - Alpha test level
//c0.g - Unused
//c0.b - Unused
//c0.a - Lightmap blend scale factor
//c1   - Gamma pass brightness offset in rgb, log guard value in a
//c2   - Gamma pass per channel exponent in rgb, 1.0 in a
//c3   - Linear fog color
//c4.x - Linear fog [1.0 / (end - start)]
//c4.y - Linear fog [end / (end - start)]
//c5   - Gamma pass dither noise plane in xy, its scale in z, one back buffer code in w
//c6.x - Gamma pass dither: -0.5, centers the noise on zero
//c6.y - Gamma pass dither: 255.0, fades the dither out over the first code
//c7   - Texel dimensions of the seven bit map on layer 1, as width, height, 1/width, 1/height
//c8   - The same for layer 2
//c9   - The same for layer 3
//c10  - Seven bit map reconstruction: 0.5, 1.0, 2/3, 1/6
//c11  - Seven bit map reconstruction: 1/3, 1.5, 2.0, 255.0
//c12  - Seven bit map dither noise plane in xy, its scale in z, one source code in w
//c13  - Seven bit map reconstruction: 255/254 in x, 0.0 in y
//c14  - Seven bit map reconstruction, detail variants: 1/380, 0.999, 4.223

#if 0
static const char *g_tempShaderString =
	"ps_3_0\n"

	"dcl_color v1.rgba\n"
	"dcl_texcoord0 v4.xy\n"
	"dcl_2d s0\n"

	"texld r0, v4, s0\n"

	"mul r0, r0, v1\n"

	"sub r4, r0.aaaa, c0.xxxx\n"
	"texkill r4\n"

	"mov oC0, r0\n"
;
#endif
extern const DWORD g_fpDefaultRenderingState[] = {
	0xFFFF0300, 0x0200001F, 0x8000000A, 0x900F0001, 0x0200001F, 0x80000005, 0x90030004, 0x0200001F,
	0x90000000, 0xA00F0800, 0x03000042, 0x800F0000, 0x90E40004, 0xA0E40800, 0x03000005, 0x800F0000,
	0x80E40000, 0x90E40001, 0x03000002, 0x800F0004, 0x80FF0000, 0xA1000000, 0x01000041, 0x800F0004,
	0x02000001, 0x800F0800, 0x80E40000, 0x0000FFFF
};

#if 0
static const char *g_tempShaderString =
	"ps_3_0\n"

	"dcl_color v1.rgba\n"
	"dcl_color1 v2.rgba\n"
	"dcl_texcoord0 v4.xy\n"
	"dcl_2d s0\n"

	"texld r0, v4, s0\n"

	"mul r0, r0, v1\n"
	"add r0.rgb, r0, v2\n"

	"sub r4, r0.aaaa, c0.xxxx\n"
	"texkill r4\n"

	"mov oC0, r0\n"
;
#endif
extern const DWORD g_fpDefaultRenderingStateWithFog[] = {
	0xFFFF0300, 0x0200001F, 0x8000000A, 0x900F0001, 0x0200001F, 0x8001000A, 0x900F0002, 0x0200001F,
	0x80000005, 0x90030004, 0x0200001F, 0x90000000, 0xA00F0800, 0x03000042, 0x800F0000, 0x90E40004,
	0xA0E40800, 0x03000005, 0x800F0000, 0x80E40000, 0x90E40001, 0x03000002, 0x80070000, 0x80E40000,
	0x90E40002, 0x03000002, 0x800F0004, 0x80FF0000, 0xA1000000, 0x01000041, 0x800F0004, 0x02000001,
	0x800F0800, 0x80E40000, 0x0000FFFF
};

#if 0
static const char *g_tempShaderString =
	"ps_3_0\n"

	"dcl_color v1.rgba\n"
	"dcl_texcoord0 v4.xy\n"
	"dcl_2d s0\n"
	"dcl_texcoord4 v8.xyzw\n"

	"texld r0, v4, s0\n"

	"mul r0, r0, v1\n"

	"sub r4, r0.aaaa, c0.xxxx\n"
	"texkill r4\n"

	"mad_sat r1.x, -v8.z, c4.x, c4.y\n"
	"lrp r0.rgb, r1.xxxx, r0, c3\n"

	"mov oC0, r0\n"
;
#endif
extern const DWORD g_fpDefaultRenderingStateWithLinearFog[] = {
	0xFFFF0300, 0x0200001F, 0x8000000A, 0x900F0001, 0x0200001F, 0x80000005, 0x90030004, 0x0200001F,
	0x90000000, 0xA00F0800, 0x0200001F, 0x80040005, 0x900F0008, 0x03000042, 0x800F0000, 0x90E40004,
	0xA0E40800, 0x03000005, 0x800F0000, 0x80E40000, 0x90E40001, 0x03000002, 0x800F0004, 0x80FF0000,
	0xA1000000, 0x01000041, 0x800F0004, 0x04000004, 0x80110001, 0x91AA0008, 0xA0000004, 0xA0550004,
	0x04000012, 0x80070000, 0x80000001, 0x80E40000, 0xA0E40003, 0x02000001, 0x800F0800, 0x80E40000,
	0x0000FFFF
};

#if 0
static const char *g_tempShaderString =
	"ps_3_0\n"

	"dcl_texcoord0 v4.xy\n"
	"dcl_2d s0\n"

	"texld r0, v4, s0\n"

	"sub r4, r0.aaaa, c0.xxxx\n"
	"texkill r4\n"

	"mov oC0, r0\n"
;
#endif
extern const DWORD g_fpComplexSurfaceSingleTexture[] = {
	0xFFFF0300, 0x0200001F, 0x80000005, 0x90030004, 0x0200001F, 0x90000000, 0xA00F0800, 0x03000042,
	0x800F0000, 0x90E40004, 0xA0E40800, 0x03000002, 0x800F0004, 0x80FF0000, 0xA1000000, 0x01000041,
	0x800F0004, 0x02000001, 0x800F0800, 0x80E40000, 0x0000FFFF
};

#if 0
static const char *g_tempShaderString =
	"ps_3_0\n"

	"dcl_texcoord0 v4.xy\n"
	"dcl_2d s0\n"
	"dcl_texcoord1 v5.xy\n"
	"dcl_2d s1\n"

	"texld r0, v4, s0\n"
	"texld r1, v5, s1\n"

	"mul r0, r0, r1\n"
	"mul r0.rgb, r0, c0.aaaa\n"

	"sub r4, r0.aaaa, c0.xxxx\n"
	"texkill r4\n"

	"mov oC0, r0\n"
;
#endif
extern const DWORD g_fpComplexSurfaceDualTextureModulated[] = {
	0xFFFF0300, 0x0200001F, 0x80000005, 0x90030004, 0x0200001F, 0x90000000, 0xA00F0800, 0x0200001F,
	0x80010005, 0x90030005, 0x0200001F, 0x90000000, 0xA00F0801, 0x03000042, 0x800F0000, 0x90E40004,
	0xA0E40800, 0x03000042, 0x800F0001, 0x90E40005, 0xA0E40801, 0x03000005, 0x800F0000, 0x80E40000,
	0x80E40001, 0x03000005, 0x80070000, 0x80E40000, 0xA0FF0000, 0x03000002, 0x800F0004, 0x80FF0000,
	0xA1000000, 0x01000041, 0x800F0004, 0x02000001, 0x800F0800, 0x80E40000, 0x0000FFFF
};

#if 0
static const char *g_tempShaderString =
	"ps_3_0\n"

	"dcl_texcoord0 v4.xy\n"
	"dcl_2d s0\n"
	"dcl_texcoord1 v5.xy\n"
	"dcl_2d s1\n"
	"dcl_texcoord2 v6.xy\n"
	"dcl_2d s2\n"

	"texld r0, v4, s0\n"
	"texld r1, v5, s1\n"
	"texld r2, v6, s2\n"

	"mul r0, r0, r1\n"
	"mul r0.rgb, r0, c0.aaaa\n"

	"mul r0, r0, r2\n"
	"mul r0.rgb, r0, c0.aaaa\n"

	"sub r4, r0.aaaa, c0.xxxx\n"
	"texkill r4\n"

	"mov oC0, r0\n"
;
#endif
extern const DWORD g_fpComplexSurfaceTripleTextureModulated[] = {
	0xFFFF0300, 0x0200001F, 0x80000005, 0x90030004, 0x0200001F, 0x90000000, 0xA00F0800, 0x0200001F,
	0x80010005, 0x90030005, 0x0200001F, 0x90000000, 0xA00F0801, 0x0200001F, 0x80020005, 0x90030006,
	0x0200001F, 0x90000000, 0xA00F0802, 0x03000042, 0x800F0000, 0x90E40004, 0xA0E40800, 0x03000042,
	0x800F0001, 0x90E40005, 0xA0E40801, 0x03000042, 0x800F0002, 0x90E40006, 0xA0E40802, 0x03000005,
	0x800F0000, 0x80E40000, 0x80E40001, 0x03000005, 0x80070000, 0x80E40000, 0xA0FF0000, 0x03000005,
	0x800F0000, 0x80E40000, 0x80E40002, 0x03000005, 0x80070000, 0x80E40000, 0xA0FF0000, 0x03000002,
	0x800F0004, 0x80FF0000, 0xA1000000, 0x01000041, 0x800F0004, 0x02000001, 0x800F0800, 0x80E40000,
	0x0000FFFF
};

#if 0
static const char *g_tempShaderString =
	"ps_3_0\n"

	"dcl_texcoord0 v4.xy\n"
	"dcl_2d s0\n"
	"dcl_texcoord1 v5.xy\n"
	"dcl_2d s1\n"

	"def c1, 1.0f, 1.0f, 1.0f, 1.0f\n"

	"texld r0, v4, s0\n"
	"texld r1, v5, s1\n"

	"sub r4, r0.aaaa, c0.xxxx\n"
	"texkill r4\n"

	"sub r1.a, c1.a, r1.a\n"
	"mad r0.rgb, r0, r1.aaaa, r1\n"

	"mov oC0, r0\n"
;
#endif
extern const DWORD g_fpComplexSurfaceSingleTextureWithFog[] = {
	0xFFFF0300, 0x0200001F, 0x80000005, 0x90030004, 0x0200001F, 0x90000000, 0xA00F0800, 0x0200001F,
	0x80010005, 0x90030005, 0x0200001F, 0x90000000, 0xA00F0801, 0x05000051, 0xA00F0001, 0x3F800000,
	0x3F800000, 0x3F800000, 0x3F800000, 0x03000042, 0x800F0000, 0x90E40004, 0xA0E40800, 0x03000042,
	0x800F0001, 0x90E40005, 0xA0E40801, 0x03000002, 0x800F0004, 0x80FF0000, 0xA1000000, 0x01000041,
	0x800F0004, 0x03000002, 0x80080001, 0xA0FF0001, 0x81FF0001, 0x04000004, 0x80070000, 0x80E40000,
	0x80FF0001, 0x80E40001, 0x02000001, 0x800F0800, 0x80E40000, 0x0000FFFF
};

#if 0
static const char *g_tempShaderString =
	"ps_3_0\n"

	"dcl_texcoord0 v4.xy\n"
	"dcl_2d s0\n"
	"dcl_texcoord1 v5.xy\n"
	"dcl_2d s1\n"
	"dcl_texcoord2 v6.xy\n"
	"dcl_2d s2\n"

	"def c1, 1.0f, 1.0f, 1.0f, 1.0f\n"

	"texld r0, v4, s0\n"
	"texld r1, v5, s1\n"
	"texld r2, v6, s2\n"

	"mul r0, r0, r1\n"
	"mul r0.rgb, r0, c0.aaaa\n"

	"sub r4, r0.aaaa, c0.xxxx\n"
	"texkill r4\n"

	"sub r2.a, c1.a, r2.a\n"
	"mad r0.rgb, r0, r2.aaaa, r2\n"

	"mov oC0, r0\n"
;
#endif
extern const DWORD g_fpComplexSurfaceDualTextureModulatedWithFog[] = {
	0xFFFF0300, 0x0200001F, 0x80000005, 0x90030004, 0x0200001F, 0x90000000, 0xA00F0800, 0x0200001F,
	0x80010005, 0x90030005, 0x0200001F, 0x90000000, 0xA00F0801, 0x0200001F, 0x80020005, 0x90030006,
	0x0200001F, 0x90000000, 0xA00F0802, 0x05000051, 0xA00F0001, 0x3F800000, 0x3F800000, 0x3F800000,
	0x3F800000, 0x03000042, 0x800F0000, 0x90E40004, 0xA0E40800, 0x03000042, 0x800F0001, 0x90E40005,
	0xA0E40801, 0x03000042, 0x800F0002, 0x90E40006, 0xA0E40802, 0x03000005, 0x800F0000, 0x80E40000,
	0x80E40001, 0x03000005, 0x80070000, 0x80E40000, 0xA0FF0000, 0x03000002, 0x800F0004, 0x80FF0000,
	0xA1000000, 0x01000041, 0x800F0004, 0x03000002, 0x80080002, 0xA0FF0001, 0x81FF0002, 0x04000004,
	0x80070000, 0x80E40000, 0x80FF0002, 0x80E40002, 0x02000001, 0x800F0800, 0x80E40000, 0x0000FFFF
};

#if 0
static const char *g_tempShaderString =
	"ps_3_0\n"

	"dcl_texcoord0 v4.xy\n"
	"dcl_2d s0\n"
	"dcl_texcoord1 v5.xy\n"
	"dcl_2d s1\n"
	"dcl_texcoord2 v6.xy\n"
	"dcl_2d s2\n"
	"dcl_texcoord3 v7.xy\n"
	"dcl_2d s3\n"

	"def c1, 1.0f, 1.0f, 1.0f, 1.0f\n"

	"texld r0, v4, s0\n"
	"texld r1, v5, s1\n"
	"texld r2, v6, s2\n"
	"texld r3, v7, s3\n"

	"mul r0, r0, r1\n"
	"mul r0.rgb, r0, c0.aaaa\n"

	"mul r0, r0, r2\n"
	"mul r0.rgb, r0, c0.aaaa\n"

	"sub r4, r0.aaaa, c0.xxxx\n"
	"texkill r4\n"

	"sub r3.a, c1.a, r3.a\n"
	"mad r0.rgb, r0, r3.aaaa, r3\n"

	"mov oC0, r0\n"
;
#endif
extern const DWORD g_fpComplexSurfaceTripleTextureModulatedWithFog[] = {
	0xFFFF0300, 0x0200001F, 0x80000005, 0x90030004, 0x0200001F, 0x90000000, 0xA00F0800, 0x0200001F,
	0x80010005, 0x90030005, 0x0200001F, 0x90000000, 0xA00F0801, 0x0200001F, 0x80020005, 0x90030006,
	0x0200001F, 0x90000000, 0xA00F0802, 0x0200001F, 0x80030005, 0x90030007, 0x0200001F, 0x90000000,
	0xA00F0803, 0x05000051, 0xA00F0001, 0x3F800000, 0x3F800000, 0x3F800000, 0x3F800000, 0x03000042,
	0x800F0000, 0x90E40004, 0xA0E40800, 0x03000042, 0x800F0001, 0x90E40005, 0xA0E40801, 0x03000042,
	0x800F0002, 0x90E40006, 0xA0E40802, 0x03000042, 0x800F0003, 0x90E40007, 0xA0E40803, 0x03000005,
	0x800F0000, 0x80E40000, 0x80E40001, 0x03000005, 0x80070000, 0x80E40000, 0xA0FF0000, 0x03000005,
	0x800F0000, 0x80E40000, 0x80E40002, 0x03000005, 0x80070000, 0x80E40000, 0xA0FF0000, 0x03000002,
	0x800F0004, 0x80FF0000, 0xA1000000, 0x01000041, 0x800F0004, 0x03000002, 0x80080003, 0xA0FF0001,
	0x81FF0003, 0x04000004, 0x80070000, 0x80E40000, 0x80FF0003, 0x80E40003, 0x02000001, 0x800F0800,
	0x80E40000, 0x0000FFFF
};

#if 0
static const char *g_tempShaderString =
	"ps_3_0\n"

	"dcl_color v1.rgba\n"
	"dcl_texcoord0 v4.xy\n"
	"dcl_2d s0\n"
	"dcl_texcoord4 v8.xyzw\n"

	"def c1, 0.002631578947f, 0.999f, 0.0f, 1.0f\n"

	"mul_sat r1.x, v8.z, c1.x\n"
	"sub r0, c1.yyyy, r1.xxxx\n"
	"texkill r0\n"

	"texld r0, v4, s0\n"
	"lrp r2, r1.xxxx, v1, r0\n"

	"mov oC0, r2\n"
;
#endif
extern const DWORD g_fpDetailTexture[] = {
	0xFFFF0300, 0x0200001F, 0x8000000A, 0x900F0001, 0x0200001F, 0x80000005, 0x90030004, 0x0200001F,
	0x90000000, 0xA00F0800, 0x0200001F, 0x80040005, 0x900F0008, 0x05000051, 0xA00F0001, 0x3B2C7692,
	0x3F7FBE77, 0x00000000, 0x3F800000, 0x03000005, 0x80110001, 0x90AA0008, 0xA0000001, 0x03000002,
	0x800F0000, 0xA0550001, 0x81000001, 0x01000041, 0x800F0000, 0x03000042, 0x800F0000, 0x90E40004,
	0xA0E40800, 0x04000012, 0x800F0002, 0x80000001, 0x90E40001, 0x80E40000, 0x02000001, 0x800F0800,
	0x80E40002, 0x0000FFFF
};

#if 0
static const char *g_tempShaderString =
	"ps_3_0\n"

	"dcl_color v1.rgba\n"
	"dcl_texcoord0 v4.xyzw\n"
	"dcl_2d s0\n"
	"dcl_texcoord4 v8.xyzw\n"

	"def c1, 0.002631578947f, 0.999f, 0.0f, 1.0f\n"
	"def c2, 4.223f, 4.223f, 0.0f, 1.0f\n"

	"mul_sat r1.x, v8.z, c1.x\n"
	"sub r0, c1.yyyy, r1.xxxx\n"
	"texkill r0\n"

	"texld r0, v4.xy, s0\n"
	"lrp r2, r1.xxxx, v1, r0\n"

	"mul_sat r1.x, r1.x, c2.x\n"

	"texld r0, v4.zw, s0\n"
	"lrp r3, r1.xxxx, v1, r0\n"

	"mul r2, r2, r3\n"
	"add r2, r2, r2\n"

	"mov oC0, r2\n"
;
#endif
extern const DWORD g_fpDetailTextureTwoLayer[] = {
	0xFFFF0300, 0x0200001F, 0x8000000A, 0x900F0001, 0x0200001F, 0x80000005, 0x900F0004, 0x0200001F,
	0x90000000, 0xA00F0800, 0x0200001F, 0x80040005, 0x900F0008, 0x05000051, 0xA00F0001, 0x3B2C7692,
	0x3F7FBE77, 0x00000000, 0x3F800000, 0x05000051, 0xA00F0002, 0x408722D1, 0x408722D1, 0x00000000,
	0x3F800000, 0x03000005, 0x80110001, 0x90AA0008, 0xA0000001, 0x03000002, 0x800F0000, 0xA0550001,
	0x81000001, 0x01000041, 0x800F0000, 0x03000042, 0x800F0000, 0x90540004, 0xA0E40800, 0x04000012,
	0x800F0002, 0x80000001, 0x90E40001, 0x80E40000, 0x03000005, 0x80110001, 0x80000001, 0xA0000002,
	0x03000042, 0x800F0000, 0x90FE0004, 0xA0E40800, 0x04000012, 0x800F0003, 0x80000001, 0x90E40001,
	0x80E40000, 0x03000005, 0x800F0002, 0x80E40002, 0x80E40003, 0x03000002, 0x800F0002, 0x80E40002,
	0x80E40002, 0x02000001, 0x800F0800, 0x80E40002, 0x0000FFFF
};

#if 0
static const char *g_tempShaderString =
	"ps_3_0\n"

	"dcl_color v1.rgba\n"
	"dcl_texcoord0 v4.xy\n"
	"dcl_2d s0\n"
	"dcl_texcoord1 v5.xy\n"
	"dcl_2d s1\n"
	"dcl_texcoord4 v8.xyzw\n"

	"def c1, 0.002631578947f, 0.999f, 0.0f, 1.0f\n"

	"texld r0, v4, s0\n"

	"sub r4, r0.aaaa, c0.xxxx\n"
	"texkill r4\n"

	"mul_sat r1.x, v8.z, c1.x\n"
	"if_le r1.x, c1.y\n"
		"texld r2, v5, s1\n"
		"lrp r3, r1.xxxx, v1, r2\n"

		"mul r0.rgb, r0, r3\n"
		"add r0.rgb, r0, r0\n"
	"endif\n"

	"mov oC0, r0\n"
;
#endif
extern const DWORD g_fpSingleTextureAndDetailTexture[] = {
	0xFFFF0300, 0x0200001F, 0x8000000A, 0x900F0001, 0x0200001F, 0x80000005, 0x90030004, 0x0200001F,
	0x90000000, 0xA00F0800, 0x0200001F, 0x80010005, 0x90030005, 0x0200001F, 0x90000000, 0xA00F0801,
	0x0200001F, 0x80040005, 0x900F0008, 0x05000051, 0xA00F0001, 0x3B2C7692, 0x3F7FBE77, 0x00000000,
	0x3F800000, 0x03000042, 0x800F0000, 0x90E40004, 0xA0E40800, 0x03000002, 0x800F0004, 0x80FF0000,
	0xA1000000, 0x01000041, 0x800F0004, 0x03000005, 0x80110001, 0x90AA0008, 0xA0000001, 0x02060029,
	0x80000001, 0xA0550001, 0x03000042, 0x800F0002, 0x90E40005, 0xA0E40801, 0x04000012, 0x800F0003,
	0x80000001, 0x90E40001, 0x80E40002, 0x03000005, 0x80070000, 0x80E40000, 0x80E40003, 0x03000002,
	0x80070000, 0x80E40000, 0x80E40000, 0x0000002B, 0x02000001, 0x800F0800, 0x80E40000, 0x0000FFFF
};

#if 0
static const char *g_tempShaderString =
	"ps_3_0\n"

	"dcl_color v1.rgba\n"
	"dcl_texcoord0 v4.xy\n"
	"dcl_2d s0\n"
	"dcl_texcoord1 v5.xyzw\n"
	"dcl_2d s1\n"
	"dcl_texcoord4 v8.xyzw\n"

	"def c1, 0.002631578947f, 0.999f, 0.0f, 1.0f\n"
	"def c2, 4.223f, 4.223f, 0.0f, 1.0f\n"

	"texld r0, v4, s0\n"

	"sub r4, r0.aaaa, c0.xxxx\n"
	"texkill r4\n"

	"mul_sat r1.x, v8.z, c1.x\n"
	"if_le r1.x, c1.y\n"
		"texld r2, v5.xy, s1\n"
		"lrp r3, r1.xxxx, v1, r2\n"

		"mul r0.rgb, r0, r3\n"
		"add r0.rgb, r0, r0\n"

		"mul_sat r1.x, r1.x, c2.x\n"

		"texld r2, v5.zw, s1\n"
		"lrp r3, r1.xxxx, v1, r2\n"

		"mul r0.rgb, r0, r3\n"
		"add r0.rgb, r0, r0\n"
	"endif\n"

	"mov oC0, r0\n"
;
#endif
extern const DWORD g_fpSingleTextureAndDetailTextureTwoLayer[] = {
	0xFFFF0300, 0x0200001F, 0x8000000A, 0x900F0001, 0x0200001F, 0x80000005, 0x90030004, 0x0200001F,
	0x90000000, 0xA00F0800, 0x0200001F, 0x80010005, 0x900F0005, 0x0200001F, 0x90000000, 0xA00F0801,
	0x0200001F, 0x80040005, 0x900F0008, 0x05000051, 0xA00F0001, 0x3B2C7692, 0x3F7FBE77, 0x00000000,
	0x3F800000, 0x05000051, 0xA00F0002, 0x408722D1, 0x408722D1, 0x00000000, 0x3F800000, 0x03000042,
	0x800F0000, 0x90E40004, 0xA0E40800, 0x03000002, 0x800F0004, 0x80FF0000, 0xA1000000, 0x01000041,
	0x800F0004, 0x03000005, 0x80110001, 0x90AA0008, 0xA0000001, 0x02060029, 0x80000001, 0xA0550001,
	0x03000042, 0x800F0002, 0x90540005, 0xA0E40801, 0x04000012, 0x800F0003, 0x80000001, 0x90E40001,
	0x80E40002, 0x03000005, 0x80070000, 0x80E40000, 0x80E40003, 0x03000002, 0x80070000, 0x80E40000,
	0x80E40000, 0x03000005, 0x80110001, 0x80000001, 0xA0000002, 0x03000042, 0x800F0002, 0x90FE0005,
	0xA0E40801, 0x04000012, 0x800F0003, 0x80000001, 0x90E40001, 0x80E40002, 0x03000005, 0x80070000,
	0x80E40000, 0x80E40003, 0x03000002, 0x80070000, 0x80E40000, 0x80E40000, 0x0000002B, 0x02000001,
	0x800F0800, 0x80E40000, 0x0000FFFF
};

#if 0
static const char *g_tempShaderString =
	"ps_3_0\n"

	"dcl_color v1.rgba\n"
	"dcl_texcoord0 v4.xy\n"
	"dcl_2d s0\n"
	"dcl_texcoord1 v5.xy\n"
	"dcl_2d s1\n"
	"dcl_texcoord2 v6.xy\n"
	"dcl_2d s2\n"
	"dcl_texcoord4 v8.xyzw\n"

	"def c1, 0.002631578947f, 0.999f, 0.0f, 1.0f\n"

	"texld r0, v4, s0\n"
	"texld r1, v5, s1\n"

	"mul r0, r0, r1\n"
	"mul r0.rgb, r0, c0.aaaa\n"

	"sub r4, r0.aaaa, c0.xxxx\n"
	"texkill r4\n"

	"mul_sat r1.x, v8.z, c1.x\n"
	"if_le r1.x, c1.y\n"
		"texld r2, v6, s2\n"
		"lrp r3, r1.xxxx, v1, r2\n"

		"mul r0.rgb, r0, r3\n"
		"add r0.rgb, r0, r0\n"
	"endif\n"

	"mov oC0, r0\n"
;
#endif
extern const DWORD g_fpDualTextureAndDetailTexture[] = {
	0xFFFF0300, 0x0200001F, 0x8000000A, 0x900F0001, 0x0200001F, 0x80000005, 0x90030004, 0x0200001F,
	0x90000000, 0xA00F0800, 0x0200001F, 0x80010005, 0x90030005, 0x0200001F, 0x90000000, 0xA00F0801,
	0x0200001F, 0x80020005, 0x90030006, 0x0200001F, 0x90000000, 0xA00F0802, 0x0200001F, 0x80040005,
	0x900F0008, 0x05000051, 0xA00F0001, 0x3B2C7692, 0x3F7FBE77, 0x00000000, 0x3F800000, 0x03000042,
	0x800F0000, 0x90E40004, 0xA0E40800, 0x03000042, 0x800F0001, 0x90E40005, 0xA0E40801, 0x03000005,
	0x800F0000, 0x80E40000, 0x80E40001, 0x03000005, 0x80070000, 0x80E40000, 0xA0FF0000, 0x03000002,
	0x800F0004, 0x80FF0000, 0xA1000000, 0x01000041, 0x800F0004, 0x03000005, 0x80110001, 0x90AA0008,
	0xA0000001, 0x02060029, 0x80000001, 0xA0550001, 0x03000042, 0x800F0002, 0x90E40006, 0xA0E40802,
	0x04000012, 0x800F0003, 0x80000001, 0x90E40001, 0x80E40002, 0x03000005, 0x80070000, 0x80E40000,
	0x80E40003, 0x03000002, 0x80070000, 0x80E40000, 0x80E40000, 0x0000002B, 0x02000001, 0x800F0800,
	0x80E40000, 0x0000FFFF
};

#if 0
static const char *g_tempShaderString =
	"ps_3_0\n"

	"dcl_color v1.rgba\n"
	"dcl_texcoord0 v4.xy\n"
	"dcl_2d s0\n"
	"dcl_texcoord1 v5.xy\n"
	"dcl_2d s1\n"
	"dcl_texcoord2 v6.xyzw\n"
	"dcl_2d s2\n"
	"dcl_texcoord4 v8.xyzw\n"

	"def c1, 0.002631578947f, 0.999f, 0.0f, 1.0f\n"
	"def c2, 4.223f, 4.223f, 0.0f, 1.0f\n"

	"texld r0, v4, s0\n"
	"texld r1, v5, s1\n"

	"mul r0, r0, r1\n"
	"mul r0.rgb, r0, c0.aaaa\n"

	"sub r4, r0.aaaa, c0.xxxx\n"
	"texkill r4\n"

	"mul_sat r1.x, v8.z, c1.x\n"
	"if_le r1.x, c1.y\n"
		"texld r2, v6.xy, s2\n"
		"lrp r3, r1.xxxx, v1, r2\n"

		"mul r0.rgb, r0, r3\n"
		"add r0.rgb, r0, r0\n"

		"mul_sat r1.x, r1.x, c2.x\n"

		"texld r2, v6.zw, s2\n"
		"lrp r3, r1.xxxx, v1, r2\n"

		"mul r0.rgb, r0, r3\n"
		"add r0.rgb, r0, r0\n"
	"endif\n"

	"mov oC0, r0\n"
;
#endif
extern const DWORD g_fpDualTextureAndDetailTextureTwoLayer[] = {
	0xFFFF0300, 0x0200001F, 0x8000000A, 0x900F0001, 0x0200001F, 0x80000005, 0x90030004, 0x0200001F,
	0x90000000, 0xA00F0800, 0x0200001F, 0x80010005, 0x90030005, 0x0200001F, 0x90000000, 0xA00F0801,
	0x0200001F, 0x80020005, 0x900F0006, 0x0200001F, 0x90000000, 0xA00F0802, 0x0200001F, 0x80040005,
	0x900F0008, 0x05000051, 0xA00F0001, 0x3B2C7692, 0x3F7FBE77, 0x00000000, 0x3F800000, 0x05000051,
	0xA00F0002, 0x408722D1, 0x408722D1, 0x00000000, 0x3F800000, 0x03000042, 0x800F0000, 0x90E40004,
	0xA0E40800, 0x03000042, 0x800F0001, 0x90E40005, 0xA0E40801, 0x03000005, 0x800F0000, 0x80E40000,
	0x80E40001, 0x03000005, 0x80070000, 0x80E40000, 0xA0FF0000, 0x03000002, 0x800F0004, 0x80FF0000,
	0xA1000000, 0x01000041, 0x800F0004, 0x03000005, 0x80110001, 0x90AA0008, 0xA0000001, 0x02060029,
	0x80000001, 0xA0550001, 0x03000042, 0x800F0002, 0x90540006, 0xA0E40802, 0x04000012, 0x800F0003,
	0x80000001, 0x90E40001, 0x80E40002, 0x03000005, 0x80070000, 0x80E40000, 0x80E40003, 0x03000002,
	0x80070000, 0x80E40000, 0x80E40000, 0x03000005, 0x80110001, 0x80000001, 0xA0000002, 0x03000042,
	0x800F0002, 0x90FE0006, 0xA0E40802, 0x04000012, 0x800F0003, 0x80000001, 0x90E40001, 0x80E40002,
	0x03000005, 0x80070000, 0x80E40000, 0x80E40003, 0x03000002, 0x80070000, 0x80E40000, 0x80E40000,
	0x0000002B, 0x02000001, 0x800F0800, 0x80E40000, 0x0000FFFF
};

//Applies the gamma curve, one exponent per channel, to a copy of the frame.
//log/exp each compile to one instruction; c1.a keeps log() off zero.
#if 0
static const char *g_tempShaderString =
	"ps_3_0\n"

	"dcl_texcoord0 v4.xy\n"
	"dcl_2d s0\n"

	"texld r0, v4, s0\n"

	"add_sat r0.rgb, r0, c1\n"
	"max r0.rgb, r0, c1.aaaa\n"

	"log r1.r, r0.r\n"
	"log r1.g, r0.g\n"
	"log r1.b, r0.b\n"

	"mul r1.rgb, r1, c2\n"

	"exp r0.r, r1.r\n"
	"exp r0.g, r1.g\n"
	"exp r0.b, r1.b\n"

	"mov r0.a, c2.aaaa\n"

	"mov oC0, r0\n"
;
#endif
extern const DWORD g_fpGammaCorrection[] = {
	0xFFFF0300, 0x0200001F, 0x80000005, 0x90030004, 0x0200001F, 0x90000000, 0xA00F0800, 0x03000042,
	0x800F0000, 0x90E40004, 0xA0E40800, 0x03000002, 0x80170000, 0x80E40000, 0xA0E40001, 0x0300000B,
	0x80070000, 0x80E40000, 0xA0FF0001, 0x0200000F, 0x80010001, 0x80000000, 0x0200000F, 0x80020001,
	0x80550000, 0x0200000F, 0x80040001, 0x80AA0000, 0x03000005, 0x80070001, 0x80E40001, 0xA0E40002,
	0x0200000E, 0x80010000, 0x80000001, 0x0200000E, 0x80020000, 0x80550001, 0x0200000E, 0x80040000,
	0x80AA0001, 0x02000001, 0x80080000, 0xA0FF0002, 0x02000001, 0x800F0800, 0x80E40000, 0x0000FFFF
};

/*
Static interleaved gradient noise before the gamma curve, trading the banding that the curve's
steep low-end slope causes for fine grain, with the jitter faded out over the first code so that
black pixels do not speckle, and the regenerated output has to stay free of def instructions
because c0, c3 and c4 above belong to other shaders, which is why every value the arithmetic needs
arrives in a constant register the driver fills for it.
*/
extern const DWORD g_fpGammaCorrectionDithered[] = {
	0xFFFF0300, 0x0200001F, 0x80000005, 0x90030000, 0x0200001F, 0x80000000, 0x90031000, 0x0200001F,
	0x90000000, 0xA00F0800, 0x03000005, 0x80030000, 0xA0E40005, 0x90E41000, 0x03000002, 0x80010000,
	0x80550000, 0x80000000, 0x02000013, 0x80010000, 0x80000000, 0x03000005, 0x80010000, 0x80000000,
	0xA0AA0005, 0x02000013, 0x80010000, 0x80000000, 0x03000002, 0x80010000, 0x80000000, 0xA0000006,
	0x03000042, 0x800F0001, 0x90E40000, 0xA0E40800, 0x0300000B, 0x80020000, 0x80550001, 0x80AA0001,
	0x0300000B, 0x80010002, 0x80000001, 0x80550000, 0x03000005, 0x80120000, 0x80000002, 0xA0550006,
	0x03000005, 0x80010000, 0x80550000, 0x80000000, 0x04000004, 0x80070000, 0x80000000, 0xA0FF0005,
	0x80E40001, 0x03000002, 0x80170000, 0x80E40000, 0xA0E40001, 0x0300000B, 0x80070001, 0x80E40000,
	0xA0FF0001, 0x0200000F, 0x80010000, 0x80000001, 0x0200000F, 0x80020000, 0x80550001, 0x0200000F,
	0x80040000, 0x80AA0001, 0x03000005, 0x80070000, 0x80E40000, 0xA0E40002, 0x0200000E, 0x80010800,
	0x80000000, 0x0200000E, 0x80020800, 0x80550000, 0x0200000E, 0x80040800, 0x80AA0000, 0x02000001,
	0x80080800, 0xA0FF0002, 0x0000FFFF
};

//The complex surface combiners for ReduceBanding.
//A cubic filter taken as four bilinear taps runs about sixty instructions against the ten it replaces.
//Regenerate with regen-recon-shaders.ps1.
#include "complexsurface.recon.ps.h"
