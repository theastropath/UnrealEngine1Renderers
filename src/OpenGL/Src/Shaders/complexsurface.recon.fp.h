/*=============================================================================
	complexsurface.recon.fp.h: ARB fragment programs for ReduceBanding.

	Lightmaps and fog maps arrive as format 0x01, seven bits a channel at one
	texel per 32 world units, so they reach a wall hugely magnified, and bilinear
	magnification creases at every texel centre while the missing bit doubles the
	smallest step the light can take, which a cubic B-spline and a triangular
	dither answer in turn.

	Kept in its own header, because a fragment program only fails at run time.
=============================================================================*/

#ifndef UTGLR_COMPLEXSURFACE_RECON_FP_H
#define UTGLR_COMPLEXSURFACE_RECON_FP_H

//These three depend on what the upload did.
//Every build here doubles the map.
//Klingon leaves it alone.
//In order: leftover expansion, one source code, its reciprocal.
#ifdef UTGLR_KLINGON_BUILD
	#define UTGLR_SBM_WHITE		"1.0"
	#define UTGLR_SBM_ONE_CODE	"0.003921569"
	#define UTGLR_SBM_FADE		"255.0"
#else
	#define UTGLR_SBM_WHITE		"1.003937008"
	#define UTGLR_SBM_ONE_CODE	"0.007843137"
	#define UTGLR_SBM_FADE		"127.5"
#endif

//Declared by any program that reconstructs.
//Texel dimensions change with the bound texture.
//Program environment parameters, indexed by texture unit.
//Index zero goes unused.
#define UTGLR_SBM_DECLS \
	"ATTRIB sbmPos = fragment.position;\n" \
	"PARAM sbmK0 = { 0.5, 1.0, 0.666666667, 0.166666667 };\n" \
	"PARAM sbmK1 = { 0.333333333, 1.5, 2.0, " UTGLR_SBM_FADE " };\n" \
	"PARAM sbmNoise = { 0.06711056, 0.00583715, 52.9829189, " UTGLR_SBM_ONE_CODE " };\n" \
	"PARAM sbmWhite = { " UTGLR_SBM_WHITE ", 0.0, 0.0, 0.0 };\n" \
	"TEMP sbmF;\n" \
	"TEMP sbmP;\n" \
	"TEMP sbmO;\n" \
	"TEMP sbmW;\n" \
	"TEMP sbmG;\n" \
	"TEMP sbmT;\n" \
	"TEMP sbmR;\n" \
	"TEMP sbmS;\n"

/** The size parameter for one unit. */
#define UTGLR_SBM_SIZE(_unit) \
	"PARAM sbmSize" #_unit " = program.env[" #_unit "];\n"

/*
Reconstructs the map on unit _unit into _dst, where a cubic B-spline's four weights reduce
to two plus the sum of each neighbouring pair and one bilinear tap placed between a pair
produces its weighted sum in hardware, so four taps land in the same texels at these
magnifications, and they are clamped to the first and last texel centres because the
sampler has these wrapped and a tap reaches two texels past the coordinate it was given.
*/
#define UTGLR_SBM_RECONSTRUCT(_unit, _dst) \
	/* Texel space */ \
	"MAD sbmF.xy, iTC" #_unit ", sbmSize" #_unit ", -sbmK0.x;\n" \
	"FLR sbmO.xy, sbmF;\n" \
	"SUB sbmF.xy, sbmF, sbmO;\n" \
	"MUL sbmP.xy, sbmF, sbmF;\n" \
	"MUL sbmP.zw, sbmP.xyxy, sbmF.xyxy;\n" \
	/* Second weight, then fourth */ \
	"MAD sbmW.xy, sbmK0.x, sbmP.zwzw, -sbmP;\n" \
	"ADD sbmW.xy, sbmW, sbmK0.z;\n" \
	"MUL sbmW.zw, sbmP, sbmK0.w;\n" \
	/* Far pair sum, then near pair */ \
	"ADD sbmG.xy, sbmP, sbmF;\n" \
	"MAD sbmG.xy, sbmK0.x, sbmG, sbmK0.w;\n" \
	"MAD sbmG.xy, -sbmK1.x, sbmP.zwzw, sbmG;\n" \
	"SUB sbmG.zw, sbmK0.y, sbmG.xyxy;\n" \
	/* Near tap, half a texel back */ \
	"RCP sbmS.x, sbmG.z;\n" \
	"RCP sbmS.y, sbmG.w;\n" \
	"MUL sbmT.xy, sbmW, sbmS;\n" \
	"ADD sbmT.xy, sbmT, sbmO;\n" \
	"SUB sbmT.xy, sbmT, sbmK0.x;\n" \
	/* Far tap, a texel and a half on */ \
	"RCP sbmS.x, sbmG.x;\n" \
	"RCP sbmS.y, sbmG.y;\n" \
	"MUL sbmT.zw, sbmW, sbmS.xyxy;\n" \
	"ADD sbmT.zw, sbmT, sbmO.xyxy;\n" \
	"ADD sbmT.zw, sbmT, sbmK1.y;\n" \
	/* Clamped, then normalized */ \
	"SUB sbmS.xy, sbmSize" #_unit ", sbmK0.x;\n" \
	"MAX sbmT, sbmT, sbmK0.x;\n" \
	"MIN sbmT, sbmT, sbmS.xyxy;\n" \
	"MUL sbmT, sbmT, sbmSize" #_unit ".zwzw;\n" \
	/* Two taps a row, two rows */ \
	"TEX sbmR, sbmT.xyxy, texture[" #_unit "], 2D;\n" \
	"TEX sbmS, sbmT.zyzy, texture[" #_unit "], 2D;\n" \
	"LRP sbmR, sbmG.x, sbmS, sbmR;\n" \
	"TEX sbmS, sbmT.xwxw, texture[" #_unit "], 2D;\n" \
	"TEX sbmF, sbmT.zwzw, texture[" #_unit "], 2D;\n" \
	"LRP sbmS, sbmG.x, sbmF, sbmS;\n" \
	"LRP " #_dst ", sbmG.y, sbmS, sbmR;\n" \
	/* Interleaved gradient noise, never animated */ \
	"MUL sbmS.xy, sbmPos, sbmNoise;\n" \
	"ADD sbmS.x, sbmS.x, sbmS.y;\n" \
	"FRC sbmS.x, sbmS.x;\n" \
	"MUL sbmS.x, sbmS.x, sbmNoise.z;\n" \
	"FRC sbmS.x, sbmS.x;\n" \
	/* Made triangular by the inverse CDF */ \
	"SUB sbmS.x, sbmS.x, sbmK0.x;\n" \
	"ABS sbmS.y, sbmS.x;\n" \
	"MAD_SAT sbmS.y, -sbmK1.z, sbmS.y, sbmK0.y;\n" \
	"RSQ sbmS.z, sbmS.y;\n" \
	"RCP sbmS.z, sbmS.z;\n" \
	"SUB sbmS.z, sbmK0.y, sbmS.z;\n" \
	"CMP sbmS.z, sbmS.x, -sbmS.z, sbmS.z;\n" \
	/* One source code, faded in over the first */ \
	"MAD_SAT sbmR, sbmS.z, sbmNoise.w, " #_dst ";\n" \
	/* Colour alone, the alpha is discarded */ \
	"MAX sbmS.x, " #_dst ".x, " #_dst ".y;\n" \
	"MAX sbmS.x, sbmS.x, " #_dst ".z;\n" \
	"MUL_SAT sbmS.x, sbmS.x, sbmK1.w;\n" \
	"LRP " #_dst ", sbmS.x, sbmR, " #_dst ";\n" \
	/* See UTGLR_SBM_WHITE above. */ \
	"MUL_SAT " #_dst ", " #_dst ", sbmWhite.x;\n"


extern const char *g_fpComplexSurfaceDualTextureLightRecon =
	"!!ARBfp1.0\n"

	"ATTRIB iColor = fragment.color.primary;\n"
	"ATTRIB iTC0 = fragment.texcoord[0];\n"
	"ATTRIB iTC1 = fragment.texcoord[1];\n"
	"TEMP t0;\n"
	"TEMP t1;\n"
	UTGLR_SBM_SIZE(1)
	UTGLR_SBM_DECLS

	"TEX t0, iTC0, texture[0], 2D;\n"
	UTGLR_SBM_RECONSTRUCT(1, t1)

	"MUL t0, t0, t1;\n"
	"MAD t0.rgb, t0, iColor.aaaa, t0;\n"

	"MOV result.color, t0;\n"

	"END\n"
;

extern const char *g_fpComplexSurfaceTripleTextureLightRecon =
	"!!ARBfp1.0\n"

	"ATTRIB iColor = fragment.color.primary;\n"
	"ATTRIB iTC0 = fragment.texcoord[0];\n"
	"ATTRIB iTC1 = fragment.texcoord[1];\n"
	"ATTRIB iTC2 = fragment.texcoord[2];\n"
	"TEMP t0;\n"
	"TEMP t1;\n"
	"TEMP t2;\n"
	UTGLR_SBM_SIZE(2)
	UTGLR_SBM_DECLS

	"TEX t0, iTC0, texture[0], 2D;\n"
	"TEX t1, iTC1, texture[1], 2D;\n"
	UTGLR_SBM_RECONSTRUCT(2, t2)

	"MUL t0, t0, t1;\n"
	"MAD t0.rgb, t0, iColor.aaaa, t0;\n"

	"MUL t0, t0, t2;\n"
	"MAD t0.rgb, t0, iColor.aaaa, t0;\n"

	"MOV result.color, t0;\n"

	"END\n"
;

extern const char *g_fpComplexSurfaceSingleTextureFogRecon =
	"!!ARBfp1.0\n"

	"ATTRIB iTC0 = fragment.texcoord[0];\n"
	"ATTRIB iTC1 = fragment.texcoord[1];\n"
	"TEMP t0;\n"
	"TEMP t1;\n"
	UTGLR_SBM_SIZE(1)
	UTGLR_SBM_DECLS

	"TEX t0, iTC0, texture[0], 2D;\n"
	UTGLR_SBM_RECONSTRUCT(1, t1)

	"SUB t1.a, 1.0, t1.a;\n"

	"MOV result.color.a, t0.a;\n"

	"MAD result.color.rgb, t0, t1.aaaa, t1;\n"

	"END\n"
;

extern const char *g_fpComplexSurfaceDualTextureLightFogRecon =
	"!!ARBfp1.0\n"

	"ATTRIB iColor = fragment.color.primary;\n"
	"ATTRIB iTC0 = fragment.texcoord[0];\n"
	"ATTRIB iTC1 = fragment.texcoord[1];\n"
	"ATTRIB iTC2 = fragment.texcoord[2];\n"
	"TEMP t0;\n"
	"TEMP t1;\n"
	"TEMP t2;\n"
	UTGLR_SBM_SIZE(1)
	UTGLR_SBM_SIZE(2)
	UTGLR_SBM_DECLS

	"TEX t0, iTC0, texture[0], 2D;\n"
	UTGLR_SBM_RECONSTRUCT(1, t1)
	UTGLR_SBM_RECONSTRUCT(2, t2)

	"MUL t0, t0, t1;\n"
	"MAD t0.rgb, t0, iColor.aaaa, t0;\n"

	"SUB t2.a, 1.0, t2.a;\n"
	"MAD t0.rgb, t0, t2.aaaa, t2;\n"

	"MOV result.color, t0;\n"

	"END\n"
;

//The modulated layer here is a macro texture.
//It carries mips and is minified.
extern const char *g_fpComplexSurfaceDualTextureMacroFogRecon =
	"!!ARBfp1.0\n"

	"ATTRIB iColor = fragment.color.primary;\n"
	"ATTRIB iTC0 = fragment.texcoord[0];\n"
	"ATTRIB iTC1 = fragment.texcoord[1];\n"
	"ATTRIB iTC2 = fragment.texcoord[2];\n"
	"TEMP t0;\n"
	"TEMP t1;\n"
	"TEMP t2;\n"
	UTGLR_SBM_SIZE(2)
	UTGLR_SBM_DECLS

	"TEX t0, iTC0, texture[0], 2D;\n"
	"TEX t1, iTC1, texture[1], 2D;\n"
	UTGLR_SBM_RECONSTRUCT(2, t2)

	"MUL t0, t0, t1;\n"
	"MAD t0.rgb, t0, iColor.aaaa, t0;\n"

	"SUB t2.a, 1.0, t2.a;\n"
	"MAD t0.rgb, t0, t2.aaaa, t2;\n"

	"MOV result.color, t0;\n"

	"END\n"
;

extern const char *g_fpComplexSurfaceTripleTextureLightFogRecon =
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
	UTGLR_SBM_SIZE(2)
	UTGLR_SBM_SIZE(3)
	UTGLR_SBM_DECLS

	"TEX t0, iTC0, texture[0], 2D;\n"
	"TEX t1, iTC1, texture[1], 2D;\n"
	UTGLR_SBM_RECONSTRUCT(2, t2)
	UTGLR_SBM_RECONSTRUCT(3, t3)

	"MUL t0, t0, t1;\n"
	"MAD t0.rgb, t0, iColor.aaaa, t0;\n"

	"MUL t0, t0, t2;\n"
	"MAD t0.rgb, t0, iColor.aaaa, t0;\n"

	"SUB t3.a, 1.0, t3.a;\n"
	"MAD t0.rgb, t0, t3.aaaa, t3;\n"

	"MOV result.color, t0;\n"

	"END\n"
;

//The two detail combiners with the modulated layer reconstructed.
//Detail and a fog map are mutually exclusive.
extern const char *g_fpDualTextureAndDetailTextureLightRecon =
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
	UTGLR_SBM_SIZE(1)
	UTGLR_SBM_DECLS

	"TEX t0, iTC0, texture[0], 2D;\n"
	UTGLR_SBM_RECONSTRUCT(1, t1)
	"TEX t2, iTC2, texture[2], 2D;\n"

	"MUL t0, t0, t1;\n"
	"MOV result.color.a, t0.a;\n"
	"MAD t0, t0, iColor.aaaa, t0;\n"

	"MUL_SAT tDB.x, iTC3.z, RNearZ;\n"
	"LRP t2, tDB.xxxx, iColor, t2;\n"

	"MUL t0, t0, t2;\n"
	"ADD result.color.rgb, t0, t0;\n"

	"END\n"
;

extern const char *g_fpDualTextureAndDetailTextureTwoLayerLightRecon =
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
	UTGLR_SBM_SIZE(1)
	UTGLR_SBM_DECLS

	"TEX t0, iTC0, texture[0], 2D;\n"
	UTGLR_SBM_RECONSTRUCT(1, t1)
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

	"END\n"
;

#endif //UTGLR_COMPLEXSURFACE_RECON_FP_H
