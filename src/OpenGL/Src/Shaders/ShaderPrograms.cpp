/*=============================================================================
	ShaderPrograms.cpp: the ARB vertex and fragment program sources.

	Data only.
	Which of these is used, and how, is in Shaders/ShaderLoad.cpp.
=============================================================================*/

#include "../OpenGLDrv.h"
#include "../OpenGL.h"


/*-----------------------------------------------------------------------------
	Vertex programs.
-----------------------------------------------------------------------------*/

extern const char *g_vpDefaultRenderingState =
	"!!ARBvp1.0\n"
	"OPTION ARB_position_invariant;\n"

	"MOV result.color, vertex.color;\n"
	"MOV result.texcoord[0], vertex.texcoord[0];\n"

	"END\n";

extern const char *g_vpDefaultRenderingStateWithFog =
	"!!ARBvp1.0\n"
	"OPTION ARB_position_invariant;\n"

	"MOV result.color, vertex.color;\n"
	"MOV result.color.secondary, vertex.color.secondary;\n"
	"MOV result.texcoord[0], vertex.texcoord[0];\n"

	"END\n";

extern const char *g_vpDefaultRenderingStateWithLinearFog =
	"!!ARBvp1.0\n"
	"OPTION ARB_position_invariant;\n"

	"MOV result.color, vertex.color;\n"
	"MOV result.color.secondary, vertex.color.secondary;\n"
	"MOV result.texcoord[0], vertex.texcoord[0];\n"
	"MOV result.fogcoord.x, vertex.position.z;\n"

	"END\n";

extern const char *g_vpComplexSurfaceSingleTexture =
	"!!ARBvp1.0\n"
	"OPTION ARB_position_invariant;\n"

	"ATTRIB iPos = vertex.position;\n"
	"ATTRIB fxInfo = vertex.attrib[6];\n"
	"ATTRIB fyInfo = vertex.attrib[7];\n"
	"ATTRIB texInfo0 = vertex.attrib[8];\n"
	"PARAM defTexCoord = { 0, 0, 0, 1 };\n"
	"TEMP t1;\n"
	"TEMP t2;\n"
	"OUTPUT oTex0 = result.texcoord[0];\n"

	"MOV result.color, vertex.color;\n"

	"DPH t1.x, iPos, fxInfo;\n"
	"DPH t1.y, iPos, fyInfo;\n"

	"MOV t2, defTexCoord;\n"
	"SUB t2.xy, t1.xyxy, texInfo0.xyxy;\n"
	"MUL t2.xy, t2.xyxy, texInfo0.zwzw;\n"
	"MOV oTex0, t2;\n"

	"END\n";

extern const char *g_vpComplexSurfaceDualTexture =
	"!!ARBvp1.0\n"
	"OPTION ARB_position_invariant;\n"

	"ATTRIB iPos = vertex.position;\n"
	"ATTRIB fxInfo = vertex.attrib[6];\n"
	"ATTRIB fyInfo = vertex.attrib[7];\n"
	"ATTRIB texInfo0 = vertex.attrib[8];\n"
	"ATTRIB texInfo1 = vertex.attrib[9];\n"
	"PARAM defTexCoord = { 0, 0, 0, 1 };\n"
	"TEMP t1;\n"
	"TEMP t2;\n"
	"OUTPUT oTex0 = result.texcoord[0];\n"
	"OUTPUT oTex1 = result.texcoord[1];\n"

	"MOV result.color, vertex.color;\n"

	"DPH t1.x, iPos, fxInfo;\n"
	"DPH t1.y, iPos, fyInfo;\n"

	"MOV t2, defTexCoord;\n"
	"SUB t2.xy, t1.xyxy, texInfo0.xyxy;\n"
	"MUL t2.xy, t2.xyxy, texInfo0.zwzw;\n"
	"MOV oTex0, t2;\n"

	"MOV t2, defTexCoord;\n"
	"SUB t2.xy, t1.xyxy, texInfo1.xyxy;\n"
	"MUL t2.xy, t2.xyxy, texInfo1.zwzw;\n"
	"MOV oTex1, t2;\n"

	"END\n";

extern const char *g_vpComplexSurfaceTripleTexture =
	"!!ARBvp1.0\n"
	"OPTION ARB_position_invariant;\n"

	"ATTRIB iPos = vertex.position;\n"
	"ATTRIB fxInfo = vertex.attrib[6];\n"
	"ATTRIB fyInfo = vertex.attrib[7];\n"
	"ATTRIB texInfo0 = vertex.attrib[8];\n"
	"ATTRIB texInfo1 = vertex.attrib[9];\n"
	"ATTRIB texInfo2 = vertex.attrib[10];\n"
	"PARAM defTexCoord = { 0, 0, 0, 1 };\n"
	"TEMP t1;\n"
	"TEMP t2;\n"
	"OUTPUT oTex0 = result.texcoord[0];\n"
	"OUTPUT oTex1 = result.texcoord[1];\n"
	"OUTPUT oTex2 = result.texcoord[2];\n"

	"MOV result.color, vertex.color;\n"

	"DPH t1.x, iPos, fxInfo;\n"
	"DPH t1.y, iPos, fyInfo;\n"

	"MOV t2, defTexCoord;\n"
	"SUB t2.xy, t1.xyxy, texInfo0.xyxy;\n"
	"MUL t2.xy, t2.xyxy, texInfo0.zwzw;\n"
	"MOV oTex0, t2;\n"

	"MOV t2, defTexCoord;\n"
	"SUB t2.xy, t1.xyxy, texInfo1.xyxy;\n"
	"MUL t2.xy, t2.xyxy, texInfo1.zwzw;\n"
	"MOV oTex1, t2;\n"

	"MOV t2, defTexCoord;\n"
	"SUB t2.xy, t1.xyxy, texInfo2.xyxy;\n"
	"MUL t2.xy, t2.xyxy, texInfo2.zwzw;\n"
	"MOV oTex2, t2;\n"

	"END\n";

extern const char *g_vpComplexSurfaceQuadTexture =
	"!!ARBvp1.0\n"
	"OPTION ARB_position_invariant;\n"

	"ATTRIB iPos = vertex.position;\n"
	"ATTRIB fxInfo = vertex.attrib[6];\n"
	"ATTRIB fyInfo = vertex.attrib[7];\n"
	"ATTRIB texInfo0 = vertex.attrib[8];\n"
	"ATTRIB texInfo1 = vertex.attrib[9];\n"
	"ATTRIB texInfo2 = vertex.attrib[10];\n"
	"ATTRIB texInfo3 = vertex.attrib[11];\n"
	"PARAM defTexCoord = { 0, 0, 0, 1 };\n"
	"TEMP t1;\n"
	"TEMP t2;\n"
	"OUTPUT oTex0 = result.texcoord[0];\n"
	"OUTPUT oTex1 = result.texcoord[1];\n"
	"OUTPUT oTex2 = result.texcoord[2];\n"
	"OUTPUT oTex3 = result.texcoord[3];\n"

	"MOV result.color, vertex.color;\n"

	"DPH t1.x, iPos, fxInfo;\n"
	"DPH t1.y, iPos, fyInfo;\n"

	"MOV t2, defTexCoord;\n"
	"SUB t2.xy, t1.xyxy, texInfo0.xyxy;\n"
	"MUL t2.xy, t2.xyxy, texInfo0.zwzw;\n"
	"MOV oTex0, t2;\n"

	"MOV t2, defTexCoord;\n"
	"SUB t2.xy, t1.xyxy, texInfo1.xyxy;\n"
	"MUL t2.xy, t2.xyxy, texInfo1.zwzw;\n"
	"MOV oTex1, t2;\n"

	"MOV t2, defTexCoord;\n"
	"SUB t2.xy, t1.xyxy, texInfo2.xyxy;\n"
	"MUL t2.xy, t2.xyxy, texInfo2.zwzw;\n"
	"MOV oTex2, t2;\n"

	"MOV t2, defTexCoord;\n"
	"SUB t2.xy, t1.xyxy, texInfo3.xyxy;\n"
	"MUL t2.xy, t2.xyxy, texInfo3.zwzw;\n"
	"MOV oTex3, t2;\n"

	"END\n";

extern const char *g_vpComplexSurfaceSingleTextureWithPos =
	"!!ARBvp1.0\n"
	"OPTION ARB_position_invariant;\n"

	"ATTRIB iPos = vertex.position;\n"
	"ATTRIB fxInfo = vertex.attrib[6];\n"
	"ATTRIB fyInfo = vertex.attrib[7];\n"
	"ATTRIB texInfo0 = vertex.attrib[8];\n"
	"PARAM defTexCoord = { 0, 0, 0, 1 };\n"
	"TEMP t1;\n"
	"TEMP t2;\n"
	"OUTPUT oTex0 = result.texcoord[0];\n"
	"OUTPUT oTex1 = result.texcoord[1];\n"

	"MOV result.color, vertex.color;\n"

	"DPH t1.x, iPos, fxInfo;\n"
	"DPH t1.y, iPos, fyInfo;\n"

	"MOV t2, defTexCoord;\n"
	"SUB t2.xy, t1.xyxy, texInfo0.xyxy;\n"
	"MUL t2.xy, t2.xyxy, texInfo0.zwzw;\n"
	"MOV oTex0, t2;\n"

	"MOV oTex1, iPos;\n"

	"END\n";

extern const char *g_vpComplexSurfaceDualTextureWithPos =
	"!!ARBvp1.0\n"
	"OPTION ARB_position_invariant;\n"

	"ATTRIB iPos = vertex.position;\n"
	"ATTRIB fxInfo = vertex.attrib[6];\n"
	"ATTRIB fyInfo = vertex.attrib[7];\n"
	"ATTRIB texInfo0 = vertex.attrib[8];\n"
	"ATTRIB texInfo1 = vertex.attrib[9];\n"
	"PARAM defTexCoord = { 0, 0, 0, 1 };\n"
	"TEMP t1;\n"
	"TEMP t2;\n"
	"OUTPUT oTex0 = result.texcoord[0];\n"
	"OUTPUT oTex1 = result.texcoord[1];\n"
	"OUTPUT oTex2 = result.texcoord[2];\n"

	"MOV result.color, vertex.color;\n"

	"DPH t1.x, iPos, fxInfo;\n"
	"DPH t1.y, iPos, fyInfo;\n"

	"MOV t2, defTexCoord;\n"
	"SUB t2.xy, t1.xyxy, texInfo0.xyxy;\n"
	"MUL t2.xy, t2.xyxy, texInfo0.zwzw;\n"
	"MOV oTex0, t2;\n"

	"MOV t2, defTexCoord;\n"
	"SUB t2.xy, t1.xyxy, texInfo1.xyxy;\n"
	"MUL t2.xy, t2.xyxy, texInfo1.zwzw;\n"
	"MOV oTex1, t2;\n"

	"MOV oTex2, iPos;\n"

	"END\n";

extern const char *g_vpComplexSurfaceTripleTextureWithPos =
	"!!ARBvp1.0\n"
	"OPTION ARB_position_invariant;\n"

	"ATTRIB iPos = vertex.position;\n"
	"ATTRIB fxInfo = vertex.attrib[6];\n"
	"ATTRIB fyInfo = vertex.attrib[7];\n"
	"ATTRIB texInfo0 = vertex.attrib[8];\n"
	"ATTRIB texInfo1 = vertex.attrib[9];\n"
	"ATTRIB texInfo2 = vertex.attrib[10];\n"
	"PARAM defTexCoord = { 0, 0, 0, 1 };\n"
	"TEMP t1;\n"
	"TEMP t2;\n"
	"OUTPUT oTex0 = result.texcoord[0];\n"
	"OUTPUT oTex1 = result.texcoord[1];\n"
	"OUTPUT oTex2 = result.texcoord[2];\n"
	"OUTPUT oTex3 = result.texcoord[3];\n"

	"MOV result.color, vertex.color;\n"

	"DPH t1.x, iPos, fxInfo;\n"
	"DPH t1.y, iPos, fyInfo;\n"

	"MOV t2, defTexCoord;\n"
	"SUB t2.xy, t1.xyxy, texInfo0.xyxy;\n"
	"MUL t2.xy, t2.xyxy, texInfo0.zwzw;\n"
	"MOV oTex0, t2;\n"

	"MOV t2, defTexCoord;\n"
	"SUB t2.xy, t1.xyxy, texInfo1.xyxy;\n"
	"MUL t2.xy, t2.xyxy, texInfo1.zwzw;\n"
	"MOV oTex1, t2;\n"

	"MOV t2, defTexCoord;\n"
	"SUB t2.xy, t1.xyxy, texInfo2.xyxy;\n"
	"MUL t2.xy, t2.xyxy, texInfo2.zwzw;\n"
	"MOV oTex2, t2;\n"

	"MOV oTex3, iPos;\n"

	"END\n";


/*-----------------------------------------------------------------------------
	Fragment programs.
-----------------------------------------------------------------------------*/

extern const char *g_fpDefaultRenderingState =
	"!!ARBfp1.0\n"

	"ATTRIB iColor = fragment.color.primary;\n"
	"ATTRIB iTC0 = fragment.texcoord[0];\n"
	"TEMP t0;\n"

	"TEX t0, iTC0, texture[0], 2D;\n"

	"MUL result.color, t0, iColor;\n"

	"END\n";

extern const char *g_fpDefaultRenderingStateWithFog =
	"!!ARBfp1.0\n"

	"ATTRIB iColor = fragment.color.primary;\n"
	"ATTRIB iTC0 = fragment.texcoord[0];\n"
	"TEMP t0;\n"

	"TEX t0, iTC0, texture[0], 2D;\n"

	"MUL result.color.a, t0.a, iColor.a;\n"
	"MAD result.color.rgb, t0, iColor, fragment.color.secondary;\n"

	"END\n";

extern const char *g_fpDefaultRenderingStateWithLinearFog =
	"!!ARBfp1.0\n"
	"OPTION ARB_fog_linear;\n"

	"ATTRIB iColor = fragment.color.primary;\n"
	"ATTRIB iTC0 = fragment.texcoord[0];\n"
	"TEMP t0;\n"

	"TEX t0, iTC0, texture[0], 2D;\n"

	"MUL result.color, t0, iColor;\n"

	"END\n";

extern const char *g_fpComplexSurfaceSingleTexture =
	"!!ARBfp1.0\n"

	"ATTRIB iTC0 = fragment.texcoord[0];\n"

	"TEX result.color, iTC0, texture[0], 2D;\n"

	"END\n";

//See the header.
#include "complexsurface.recon.fp.h"


extern const char *g_fpComplexSurfaceDualTextureModulated =
	"!!ARBfp1.0\n"

	"ATTRIB iColor = fragment.color.primary;\n"
	"ATTRIB iTC0 = fragment.texcoord[0];\n"
	"ATTRIB iTC1 = fragment.texcoord[1];\n"
	"TEMP t0;\n"
	"TEMP t1;\n"

	"TEX t0, iTC0, texture[0], 2D;\n"
	"TEX t1, iTC1, texture[1], 2D;\n"

	"MUL t0, t0, t1;\n"
	"MAD t0.rgb, t0, iColor.aaaa, t0;\n"

	"MOV result.color, t0;\n"

	"END\n";

extern const char *g_fpComplexSurfaceTripleTextureModulated =
	"!!ARBfp1.0\n"

	"ATTRIB iColor = fragment.color.primary;\n"
	"ATTRIB iTC0 = fragment.texcoord[0];\n"
	"ATTRIB iTC1 = fragment.texcoord[1];\n"
	"ATTRIB iTC2 = fragment.texcoord[2];\n"
	"TEMP t0;\n"
	"TEMP t1;\n"
	"TEMP t2;\n"

	"TEX t0, iTC0, texture[0], 2D;\n"
	"TEX t1, iTC1, texture[1], 2D;\n"
	"TEX t2, iTC2, texture[2], 2D;\n"

	"MUL t0, t0, t1;\n"
	"MAD t0.rgb, t0, iColor.aaaa, t0;\n"

	"MUL t0, t0, t2;\n"
	"MAD t0.rgb, t0, iColor.aaaa, t0;\n"

	"MOV result.color, t0;\n"

	"END\n";

extern const char *g_fpComplexSurfaceSingleTextureWithFog =
	"!!ARBfp1.0\n"

	"ATTRIB iTC0 = fragment.texcoord[0];\n"
	"ATTRIB iTC1 = fragment.texcoord[1];\n"
	"TEMP t0;\n"
	"TEMP t1;\n"

	"TEX t0, iTC0, texture[0], 2D;\n"
	"TEX t1, iTC1, texture[1], 2D;\n"

	"SUB t1.a, 1.0, t1.a;\n"

	"MOV result.color.a, t0.a;\n"

	"MAD result.color.rgb, t0, t1.aaaa, t1;\n"

	"END\n";

extern const char *g_fpComplexSurfaceDualTextureModulatedWithFog =
	"!!ARBfp1.0\n"

	"ATTRIB iColor = fragment.color.primary;\n"
	"ATTRIB iTC0 = fragment.texcoord[0];\n"
	"ATTRIB iTC1 = fragment.texcoord[1];\n"
	"ATTRIB iTC2 = fragment.texcoord[2];\n"
	"TEMP t0;\n"
	"TEMP t1;\n"
	"TEMP t2;\n"

	"TEX t0, iTC0, texture[0], 2D;\n"
	"TEX t1, iTC1, texture[1], 2D;\n"
	"TEX t2, iTC2, texture[2], 2D;\n"

	"MUL t0, t0, t1;\n"
	"MAD t0.rgb, t0, iColor.aaaa, t0;\n"

	"SUB t2.a, 1.0, t2.a;\n"
	"MAD t0.rgb, t0, t2.aaaa, t2;\n"

	"MOV result.color, t0;\n"

	"END\n";

extern const char *g_fpComplexSurfaceTripleTextureModulatedWithFog =
	"!!ARBfp1.0\n"

	"ATTRIB iColor = fragment.color.primary;\n"
	"ATTRIB iTC0 = fragment.texcoord[0];\n"
	"ATTRIB iTC1 = fragment.texcoord[1];\n"
	"ATTRIB iTC2 = fragment.texcoord[2];\n"
	"ATTRIB iTC3 = fragment.texcoord[3];\n"
	"TEMP t0;\n"
	"TEMP t1;\n"
	"TEMP t2;\n"
	"TEMP t3;\n"

	"TEX t0, iTC0, texture[0], 2D;\n"
	"TEX t1, iTC1, texture[1], 2D;\n"
	"TEX t2, iTC2, texture[2], 2D;\n"
	"TEX t3, iTC3, texture[3], 2D;\n"

	"MUL t0, t0, t1;\n"
	"MAD t0.rgb, t0, iColor.aaaa, t0;\n"

	"MUL t0, t0, t2;\n"
	"MAD t0.rgb, t0, iColor.aaaa, t0;\n"

	"SUB t3.a, 1.0, t3.a;\n"
	"MAD t0.rgb, t0, t3.aaaa, t3;\n"

	"MOV result.color, t0;\n"

	"END\n";

extern const char *g_fpDetailTexture =
	"!!ARBfp1.0\n"

	"ATTRIB iColor = fragment.color.primary;\n"
	"ATTRIB iTC0 = fragment.texcoord[0];\n"
	"ATTRIB iTC1 = fragment.texcoord[1];\n"
	"PARAM RNearZ = 0.002631578947;\n"
	"TEMP t0;\n"
	"TEMP tDB;\n"

	"MUL_SAT tDB.x, iTC1.z, RNearZ;\n"
	"SUB t0.x, 0.999, tDB.x;\n"
	"KIL t0.xxxx;\n"

	"TEX t0, iTC0, texture[0], 2D;\n"

	"LRP result.color, tDB.xxxx, iColor, t0;\n"

	"END\n";

extern const char *g_fpDetailTextureTwoLayer =
	"!!ARBfp1.0\n"

	"ATTRIB iColor = fragment.color.primary;\n"
	"ATTRIB iTC0 = fragment.texcoord[0];\n"
	"ATTRIB iTC1 = fragment.texcoord[1];\n"
	"PARAM RNearZ = 0.002631578947;\n"
	"PARAM DetailScale = { 4.223, 4.223, 0, 1 };\n"
	"TEMP t0;\n"
	"TEMP tDB;\n"
	"TEMP tAcc;\n"

	"MUL_SAT tDB.x, iTC1.z, RNearZ;\n"
	"SUB t0.x, 0.999, tDB.x;\n"
	"KIL t0.xxxx;\n"

	"TEX t0, iTC0, texture[0], 2D;\n"

	"LRP tAcc, tDB.xxxx, iColor, t0;\n"

	"MUL t0, iTC0, DetailScale;\n"
	"TEX t0, t0, texture[0], 2D;\n"

	"MUL_SAT tDB.x, tDB.x, DetailScale.x;\n"
	"LRP t0, tDB.xxxx, iColor, t0;\n"

	"MUL tAcc, tAcc, t0;\n"
	"ADD result.color, tAcc, tAcc;\n"

	"END\n";

extern const char *g_fpSingleTextureAndDetailTexture =
	"!!ARBfp1.0\n"

	"ATTRIB iColor = fragment.color.primary;\n"
	"ATTRIB iTC0 = fragment.texcoord[0];\n"
	"ATTRIB iTC1 = fragment.texcoord[1];\n"
	"ATTRIB iTC2 = fragment.texcoord[2];\n"
	"PARAM RNearZ = 0.002631578947;\n"
	"TEMP t0;\n"
	"TEMP t1;\n"
	"TEMP tDB;\n"

	"TEX t0, iTC0, texture[0], 2D;\n"
	"TEX t1, iTC1, texture[1], 2D;\n"

	"MOV result.color.a, t0.a;\n"

	"MUL_SAT tDB.x, iTC2.z, RNearZ;\n"
	"LRP t1, tDB.xxxx, iColor, t1;\n"

	"MUL t0, t0, t1;\n"
	"ADD result.color.rgb, t0, t0;\n"

	"END\n";

extern const char *g_fpSingleTextureAndDetailTextureTwoLayer =
	"!!ARBfp1.0\n"

	"ATTRIB iColor = fragment.color.primary;\n"
	"ATTRIB iTC0 = fragment.texcoord[0];\n"
	"ATTRIB iTC1 = fragment.texcoord[1];\n"
	"ATTRIB iTC2 = fragment.texcoord[2];\n"
	"PARAM RNearZ = 0.002631578947;\n"
	"PARAM DetailScale = { 4.223, 4.223, 0, 1 };\n"
	"TEMP t0;\n"
	"TEMP t1;\n"
	"TEMP tDB;\n"

	"TEX t0, iTC0, texture[0], 2D;\n"
	"TEX t1, iTC1, texture[1], 2D;\n"

	"MOV result.color.a, t0.a;\n"

	"MUL_SAT tDB.x, iTC2.z, RNearZ;\n"
	"LRP t1, tDB.xxxx, iColor, t1;\n"

	"MUL t0, t0, t1;\n"
	"ADD t0, t0, t0;\n"

	"MUL t1, iTC1, DetailScale;\n"
	"TEX t1, t1, texture[1], 2D;\n"

	"MUL_SAT tDB.x, tDB.x, DetailScale.x;\n"
	"LRP t1, tDB.xxxx, iColor, t1;\n"

	"MUL t0, t0, t1;\n"
	"ADD result.color.rgb, t0, t0;\n"

	"END\n";

extern const char *g_fpDualTextureAndDetailTexture =
	"!!ARBfp1.0\n"

	"ATTRIB iColor = fragment.color.primary;\n"
	"ATTRIB iTC0 = fragment.texcoord[0];\n"
	"ATTRIB iTC1 = fragment.texcoord[1];\n"
	"ATTRIB iTC2 = fragment.texcoord[2];\n"
	"ATTRIB iTC3 = fragment.texcoord[3];\n"
	"PARAM RNearZ = 0.002631578947;\n"
	"TEMP t0;\n"
	"TEMP t1;\n"
	"TEMP t2;\n"
	"TEMP tDB;\n"

	"TEX t0, iTC0, texture[0], 2D;\n"
	"TEX t1, iTC1, texture[1], 2D;\n"
	"TEX t2, iTC2, texture[2], 2D;\n"

	"MUL t0, t0, t1;\n"
	"MOV result.color.a, t0.a;\n"
	"MAD t0, t0, iColor.aaaa, t0;\n"

	"MUL_SAT tDB.x, iTC3.z, RNearZ;\n"
	"LRP t2, tDB.xxxx, iColor, t2;\n"

	"MUL t0, t0, t2;\n"
	"ADD result.color.rgb, t0, t0;\n"

	"END\n";

extern const char *g_fpDualTextureAndDetailTextureTwoLayer =
	"!!ARBfp1.0\n"

	"ATTRIB iColor = fragment.color.primary;\n"
	"ATTRIB iTC0 = fragment.texcoord[0];\n"
	"ATTRIB iTC1 = fragment.texcoord[1];\n"
	"ATTRIB iTC2 = fragment.texcoord[2];\n"
	"ATTRIB iTC3 = fragment.texcoord[3];\n"
	"PARAM RNearZ = 0.002631578947;\n"
	"PARAM DetailScale = { 4.223, 4.223, 0, 1 };\n"
	"TEMP t0;\n"
	"TEMP t1;\n"
	"TEMP t2;\n"
	"TEMP tDB;\n"

	"TEX t0, iTC0, texture[0], 2D;\n"
	"TEX t1, iTC1, texture[1], 2D;\n"
	"TEX t2, iTC2, texture[2], 2D;\n"

	"MUL t0, t0, t1;\n"
	"MOV result.color.a, t0.a;\n"
	"MAD t0, t0, iColor.aaaa, t0;\n"

	"MUL_SAT tDB.x, iTC3.z, RNearZ;\n"
	"LRP t2, tDB.xxxx, iColor, t2;\n"

	"MUL t0, t0, t2;\n"
	"ADD t0, t0, t0;\n"

	"MUL t2, iTC2, DetailScale;\n"
	"TEX t2, t2, texture[2], 2D;\n"

	"MUL_SAT tDB.x, tDB.x, DetailScale.x;\n"
	"LRP t2, tDB.xxxx, iColor, t2;\n"

	"MUL t0, t0, t2;\n"
	"ADD result.color.rgb, t0, t0;\n"

	"END\n";

/*
Remaps texture[0] through the 256x1 ramp on texture[1] and each ramp texel holds all three
channels, so each channel is fetched separately with RampScale.xy mapping a [0,1] intensity
to its texel center and .z naming the row it sits on, of which there is only one, three
dependent reads per pixel being the price of doing gamma in the pipeline.
*/
extern const char *g_fpGammaRamp =
	"!!ARBfp1.0\n"

	"ATTRIB iTC0 = fragment.texcoord[0];\n"
	"PARAM RampScale = { 0.99609375, 0.001953125, 0.5, 1 };\n"
	"TEMP scene;\n"
	"TEMP rampTC;\n"
	"TEMP corrected;\n"

	"TEX scene, iTC0, texture[0], 2D;\n"

	"MAD scene, scene, RampScale.x, RampScale.y;\n"
	"MOV rampTC, RampScale.z;\n"

	"MOV rampTC.x, scene.x;\n"
	"TEX corrected.x, rampTC, texture[1], 2D;\n"
	"MOV rampTC.x, scene.y;\n"
	"TEX corrected.y, rampTC, texture[1], 2D;\n"
	"MOV rampTC.x, scene.z;\n"
	"TEX corrected.z, rampTC, texture[1], 2D;\n"
	"MOV corrected.w, RampScale.w;\n"

	"MOV result.color, corrected;\n"

	"END\n";

/*
Static interleaved gradient noise before the ramp lookup trades the banding of the ramp's
steep low end for fine grain and the jitter fades out over the first code so black pixels
stay black, and it needs the ramp texture filtered GL_LINEAR because under GL_NEAREST a
jittered lookup lands on one of two neighbouring entries and the bands come back as coarse
two-level noise.
*/
extern const char *g_fpGammaRampDithered =
	"!!ARBfp1.0\n"

	"ATTRIB iTC0 = fragment.texcoord[0];\n"
	"ATTRIB iPos = fragment.position;\n"
	"PARAM RampScale = { 0.99609375, 0.001953125, 0.5, 1 };\n"
	//xy is the noise plane, z the scale, w one color code.
	"PARAM Dither = { 0.06711056, 0.00583715, 52.9829189, 0.00392156862 };\n"
	//x centers the noise on zero, y is the reciprocal of one code.
	"PARAM DitherFade = { -0.5, 255, 0, 0 };\n"
	"TEMP scene;\n"
	"TEMP rampTC;\n"
	"TEMP corrected;\n"
	"TEMP noise;\n"
	"TEMP fade;\n"

	"TEX scene, iTC0, texture[0], 2D;\n"

	"MUL noise.xy, iPos, Dither;\n"
	"ADD noise.x, noise.x, noise.y;\n"
	"FRC noise.x, noise.x;\n"
	"MUL noise.x, noise.x, Dither.z;\n"
	"FRC noise.x, noise.x;\n"
	"ADD noise.x, noise.x, DitherFade.x;\n"

	"MAX fade.x, scene.x, scene.y;\n"
	"MAX fade.x, fade.x, scene.z;\n"
	"MUL_SAT fade.x, fade.x, DitherFade.y;\n"

	"MUL noise.x, noise.x, fade.x;\n"
	"MAD scene.xyz, noise.x, Dither.w, scene;\n"

	//GL_CLAMP_TO_EDGE handles the rest.
	"MAD scene, scene, RampScale.x, RampScale.y;\n"
	"MOV rampTC, RampScale.z;\n"

	"MOV rampTC.x, scene.x;\n"
	"TEX corrected.x, rampTC, texture[1], 2D;\n"
	"MOV rampTC.x, scene.y;\n"
	"TEX corrected.y, rampTC, texture[1], 2D;\n"
	"MOV rampTC.x, scene.z;\n"
	"TEX corrected.z, rampTC, texture[1], 2D;\n"
	"MOV corrected.w, RampScale.w;\n"

	"MOV result.color, corrected;\n"

	"END\n";
