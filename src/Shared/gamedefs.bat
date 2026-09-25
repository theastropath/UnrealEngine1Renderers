@echo off
rem =============================================================================
rem  gamedefs.bat: which game SDKs there are, and what each one asks of the build.
rem
rem  Shared by the D3D9 and OpenGL renderers, which target the same ten games.
rem
rem  Usage, from a renderer's build script:
rem      call "..\Shared\gamedefs.bat"          sets ALL_GAMES and nothing else
rem      call "..\Shared\gamedefs.bat" <game>   sets GAMEDEF, CHARSET and GAMEWARNINGS too
rem
rem  An unrecognised game leaves GAMEDEF empty and returns 1 without printing
rem  anything; the caller reports it, naming ALL_GAMES as the list it expected.
rem
rem  D3D10 is not a caller. It picks its game through MSBuild property sheets, so
rem  it keeps its own list; see D3D10\*.props.
rem
rem  Two rules for editing this file, both forced by how it is called:
rem    - No setlocal. What it sets has to outlive the call.
rem    - No exclamation mark anywhere, prose included. D3D9 calls this script under
rem      EnableDelayedExpansion and OpenGL calls it both ways, so one would survive
rem      one call site and be eaten by the other.
rem =============================================================================

rem Every game with an SDK these renderers know how to target. This is the order the
rem table below tries, and the order a "build all" walks.
set "ALL_GAMES=UnrealTournament XComEnforcer DeusEx Rune Rune_100 Unreal_226_Gold Unreal_224 Nerf HarryPotter Klingon"

if "%~1"=="" exit /b 0


rem --- The per-game build define -----------------------------------------------
rem One UTGLR_*_BUILD macro goes on the compiler command line, and Shared\buildconfig.h
rem derives everything else from it. What follows is why the games sharing a define
rem share one, and why the rest need their own.
rem
rem Rune_100 is Rune 1.00 and shares the Rune define.
rem
rem Unreal_226_Gold builds under UTGLR_UNREAL_226_BUILD, which buildconfig.h folds into
rem the UTGLR_OLD_URENDERDEVICE umbrella: no URenderDevice::DetailTextures, a Flush with
rem no argument, and no GUglyHackFlags - so ZRangeHack is not offered there.
rem UTGLR_UNREAL_227_BUILD would not have fitted, wanting 227 features 226 lacks
rem (PF_AlphaBlend, FTextureInfo::UClampMode). No Unreal 227 SDK ships under Games\ in
rem any case; buildconfig.h accepts that define only for a fork built against one.
rem
rem Nerf is engine 300 and the same generation, but gets UTGLR_NERF_BUILD of its own even
rem though no code here tells the two apart. Its
rem URenderDevice carries Description and DescFlags, which Gold's does not, and those
rem shift the offset of every field after them - so the two DLLs are not interchangeable
rem and the define is what keeps the SDK that was compiled against nameable.
rem
rem Unreal 224 is engine 224 and the oldest of that generation, on UTGLR_UNREAL_224_BUILD.
rem It needs a define of its own for the same field-offset reason: it has neither
rem RecommendedLOD nor PrefersDeferredLoad and puts Pad0 second instead of last, so its
rem URenderDevice is a different shape again. It also predates PrecacheTexture, which it
rem replaces with Precache( ULevel* ). Each renderer has one more thing of its own to say
rem about it: for D3D9 the missing GET_COLOR_DWORD macro in UnTex.h, for OpenGL the
rem missing BLIT_OpenGL, whose bit it uses for BLIT_Race.
rem
rem XComEnforcer is engine 420 and shares UTGLR_UT_BUILD, with no define of its own:
rem its UnRender.h, UnTex.h and Engine.h are identical to Unreal Tournament's, appSeconds
rem returns FTime, GUglyHackFlags and DetailTextures are both present, and
rem DECLARE_ABSTRACT_CLASS takes the four-argument form. UnRenDev.h adds a LOCKR_Bink
rem lock flag, which both renderers ignore as they test only LOCKR_ClearScreen, and drops
rem the 'static' from StaticConstructor, a non-virtual method each declares for itself
rem either way. Nothing else in it differs but the notation of two existing enum values.
rem
rem HarryPotter is engine 433 and needs UTGLR_HP_BUILD of its own, but only for the render
rem interface: KnowWonder replaced the polygon fan entry point with indexed triangles,
rem adding the MaxVertices and DrawTriangles pure virtuals. In every other respect the
rem tree is Unreal Tournament generation, so the define selects the indexed submission
rem path and nothing else. It is also the one SDK needing a compiler flag; see below.
rem
rem Klingon is engine 219, the oldest tree here, and takes UTGLR_KLINGON_BUILD. It joins
rem the UTGLR_OLD_URENDERDEVICE umbrella and needs more besides: its URenderDevice has no
rem SupportsTC, SupportsLazyTextures or PrefersDeferredLoad, its FTextureInfo packs the
rem realtime flags into a TextureFlags DWORD, its engine calls StaticConstructor as a
rem plain function taking the UClass, and its Core has neither GConfig nor appSleep. It is
rem also the one game here built ANSI, not Unicode; see CHARSET below.
rem
rem For what a define then selects inside a renderer, see that renderer's main header -
rem D3D9\Src\D3D9.h or OpenGL\Src\OpenGL.h - and Shared\buildconfig.h.
set "GAMEDEF="
if /i "%~1"=="UnrealTournament" set "GAMEDEF=UTGLR_UT_BUILD"
if /i "%~1"=="XComEnforcer"     set "GAMEDEF=UTGLR_UT_BUILD"
if /i "%~1"=="DeusEx"           set "GAMEDEF=UTGLR_DX_BUILD"
if /i "%~1"=="Rune"             set "GAMEDEF=UTGLR_RUNE_BUILD"
if /i "%~1"=="Rune_100"         set "GAMEDEF=UTGLR_RUNE_BUILD"
if /i "%~1"=="Unreal_226_Gold"  set "GAMEDEF=UTGLR_UNREAL_226_BUILD"
if /i "%~1"=="Unreal_224"       set "GAMEDEF=UTGLR_UNREAL_224_BUILD"
if /i "%~1"=="Nerf"             set "GAMEDEF=UTGLR_NERF_BUILD"
if /i "%~1"=="HarryPotter"      set "GAMEDEF=UTGLR_HP_BUILD"
if /i "%~1"=="Klingon"          set "GAMEDEF=UTGLR_KLINGON_BUILD"
if not defined GAMEDEF exit /b 1


rem --- Character set ------------------------------------------------------------
rem UNICODE and _UNICODE go together: windows.h keys TCHAR off UNICODE while Core.h keys
rem TEXT() off _UNICODE, so either one alone gives a wide TCHAR with narrow literals, or
rem the reverse. Neither fails to compile.
rem
rem Klingon is the exception, and has to be: its Core.lib is an ANSI build. Every exported
rem entry point there takes char const* - dumpbin shows PBD where the other SDKs show PBG -
rem and its Core.h selects TCHAR from _UNICODE just as theirs do, so defining it would ask
rem the linker for wide symbols the library does not contain. UNICODE is dropped with it so
rem that the Windows API selection stays on the same side as TCHAR instead of splitting
rem the two.
set "CHARSET=/DUNICODE /D_UNICODE"
if /i "%~1"=="Klingon" set "CHARSET="


rem --- Diagnostics the SDK's own headers need silenced ---------------------------
rem Harry Potter is the only tree here whose headers do not compile clean under a
rem conforming compiler. UnAudio.h declares SetupFromFilename with no return type, which
rem C++ has never allowed; the compiler assumes int and the member is never called from
rem either renderer, so suppressing the diagnostic is enough. C4430 is the error and C4183
rem the warning left once it is gone. The OpenGL build's /external:W0 does not cover it,
rem because C4430 is an error, not a warning.
rem
rem Two further sites, in UnModel.h and UnActor.h, reuse a for initialiser variable after
rem its loop. That needs the declaration moved, not a diagnostic silenced:
rem /Zc:forScope- restores the VC6 behaviour but makes this toolchain fault with an
rem internal compiler error. Both headers are patched in place instead, identically in all
rem three renderers' copies of the tree.
rem
rem Those patches are made by hand and leave no marker, unlike the ones
rem patch_unreal_224_sdk.ps1 applies, so a fresh Harry Potter SDK drop silently regresses
rem and there is nothing either build script can test for. Scripting them the way 224's
rem are would be the better shape for both.
set "GAMEWARNINGS="
if /i "%~1"=="HarryPotter" set "GAMEWARNINGS=/wd4430 /wd4183"

exit /b 0
