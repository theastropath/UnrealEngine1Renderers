
/*
POLYFLAGS

Shared with the C++ side (polyflags.h).
*/
#ifndef POLYFLAGS_INCLUDED
#define POLYFLAGS_INCLUDED

//Unreal poly flags
#define		PF_Invisible		0x00000001	/**< Invisible. */
#define 	PF_Masked			0x00000002	/**< Drawn masked. */
#define 	PF_Translucent	 	0x00000004	/**< Transparent. */
#define		PF_NotSolid			 0x00000008	/**< Blocks nothing. */
#define		PF_Environment   	 0x00000010	/**< Environment mapped. */
#define		PF_ForceViewZone	 0x00000010	/**< Reuses Environment. */
#define		PF_Semisolid	  	 0x00000020	/**< Semi-solid. */
#define 	PF_Modulated 		 0x00000040	/**< Modulation transparency. */
#define 	PF_FakeBackdrop		 0x00000080	/**< Looks like backdrop. */
#define 	PF_TwoSided			 0x00000100	/**< Both sides. */
#define 	PF_AutoUPan		 	 0x00000200	/**< Pans in U. */
#define 	PF_AutoVPan 		 0x00000400	/**< Pans in V. */
#define 	PF_NoSmooth			 0x00000800	/**< Unfiltered. */
#define 	PF_BigWavy 			 0x00001000	/**< Big wavy pattern. */
#define 	PF_SpecialPoly		 0x00001000	/**< Reuses BigWavy. */
#define		PF_AlphaBlend		 0x00001000	/**< RUNE only. */
#define 	PF_SmallWavy		 0x00002000	/**< Small wavy pattern. */
#define 	PF_Flat				 0x00004000	/**< Flat surface. */
#define 	PF_LowShadowDetail	 0x00008000	/**< Low detail shadows. */
#define 	PF_NoMerge			 0x00010000	/**< No node merge. */
#define 	PF_CloudWavy		 0x00020000	/**< Wavy like clouds. */
#define 	PF_DirtyShadows		 0x00040000	/**< Dirty shadows. */
#define 	PF_BrightCorners	 0x00080000	/**< Brighten convex corners. */
#define 	PF_SpecialLit		 0x00100000	/**< Speciallit lights only. */
#define 	PF_Gouraud			 0x00200000	/**< Gouraud shaded. */
#define 	PF_NoBoundRejection  0x00200000	/**< Reuses Gouraud. */
#define 	PF_Unlit			 0x00400000	/**< Unlit. */
#define 	PF_HighShadowDetail	 0x00800000	/**< High detail shadows. */
#define 	PF_Portal			 0x04000000	/**< Portal between iZones. */
#define 	PF_Mirrored			 0x08000000	/**< Reflective. */

// Editor flags.
#define 	PF_Memorized     	 0x01000000	/**< Editor: remembered. */
#define 	PF_Selected      	 0x02000000	/**< Editor: selected. */
#define 	PF_Highlighted       0x10000000	/**< Editor: highlighted. */ 
#define 	PF_FlatShaded		 0x40000000	/**< Split by SplitPolyWithPlane. */

// Internal.
#define 	PF_EdProcessed 		 0x40000000	/**< Seen by editorBuildFPolys. */
#define 	PF_EdCut       		 0x80000000	/**< Split by SplitPolyWithPlane. */
/* 0x40000000 a third time. */
#define 	PF_RenderFog		 0x40000000	/**< Fogmapped. */
#define 	PF_Occlude			 0x80000000	/**< Occludes anyway. */
#define 	PF_RenderHint        0x01000000   /**< Rendering hint. */

//Combinations. Parenthesised, or `x & PF_NoOcclude` binds wrong.
#define 	PF_NoOcclude		 (PF_Masked | PF_Translucent | PF_Invisible | PF_Modulated)
#define 	PF_NoEdit			 (PF_Memorized | PF_Selected | PF_EdProcessed | PF_NoMerge | PF_EdCut)
#define 	PF_NoImport			 (PF_NoEdit | PF_NoMerge | PF_Memorized | PF_Selected | PF_EdProcessed | PF_EdCut)
#define 	PF_AddLast			 (PF_Semisolid | PF_NotSolid)
#define 	PF_NoAddToBSP		 (PF_EdCut | PF_EdProcessed | PF_Selected | PF_Memorized)
#define 	PF_NoShadows		 (PF_Unlit | PF_Invisible | PF_Environment | PF_FakeBackdrop)
#define 	PF_Transient		 PF_Highlighted

#endif //POLYFLAGS_INCLUDED