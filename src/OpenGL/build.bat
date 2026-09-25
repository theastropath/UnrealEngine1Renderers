@echo off
rem Usage: build.bat [release|debug] [game|all]
rem
rem This script is the build. There is no project file: MSBuild cannot consume the VC++ 2008
rem format the renderer shipped with, and the Build Tools SKU has no devenv to upgrade it.

setlocal

set CONFIG=Release
if /i "%~1"=="debug" set CONFIG=Debug

rem Game SDKs live under Games\. Which games there are, and what each asks of the build,
rem is Shared\gamedefs.bat. Called here with no argument, which sets only ALL_GAMES: that
rem is all this needs to expand "all" below. The per-game call is in :build.
call "%~dp0..\Shared\gamedefs.bat"

set GAMES=%~2
if not defined GAMES set GAMES=UnrealTournament
if /i "%GAMES%"=="all" set GAMES=%ALL_GAMES%

cd /d "%~dp0"

rem -requires so an installation without the C++ workload is not selected: without it the script
rem reports success here and the build fails later with "'cl' is not recognized".
for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set VSPATH=%%i
if not defined VSPATH (
	echo ERROR: No Visual Studio installation with the C++ build tools found.
	exit /b 1
)

rem The renderer contains inline x86 assembly, so the target must be Win32.
rem stderr is discarded with stdout: vcvarsall looks its own version up by running an unqualified
rem "vswhere.exe", which resolves only while the current directory is searched for executables, so
rem where NoDefaultCurrentDirectoryInExePath is set it prints "'vswhere.exe' is not recognized" and
rem loses the banner version this already throws away. errorlevel is what reports the real outcome.
call "%VSPATH%\VC\Auxiliary\Build\vcvarsall.bat" x86 >nul 2>&1
if errorlevel 1 exit /b 1

for %%g in (%GAMES%) do (
	call :build %%g
	if errorlevel 1 exit /b 1
)
exit /b 0


:build
rem Delayed expansion is needed to build the source and object lists in the loop below.
rem The cost is that a literal '!' is eaten inside this routine, including in anything the
rem paths below contain - so a clone into a directory whose name has one would break the
rem build in a way that looks nothing like its cause. No such path is involved today.
setlocal EnableDelayedExpansion
set GAME=%~1

rem GAMEDEF, CHARSET and GAMEWARNINGS, all three from the game name; see Shared\gamedefs.bat
rem for the table and for why each game is where it is in it. It prints nothing about a
rem game it does not recognise, so the wording below stays this script's.
call "%~dp0..\Shared\gamedefs.bat" "%GAME%"
if errorlevel 1 (
	echo ERROR: Unknown game "%GAME%". Expected 'all' or one of: %ALL_GAMES%.
	exit /b 1
)

if not exist "Games\%GAME%\Core\Inc\Core.h" (
	echo ERROR: Game SDK not found at "Games\%GAME%".
	exit /b 1
)

rem The Unreal 224 SDK predates standard C++ and does not compile as it shipped; three of its headers
rem are corrected in place by patch_unreal_224_sdk.ps1 at the repository root. The Core.h check above
rem passes on an unpatched clone, which then fails several hundred lines inside vendor headers with
rem errors that read as a broken toolchain, not as an unpatched SDK. The script writes a marker
rem into every file it edits, so ask for it here instead. Harry Potter's corrections are made by hand
rem and leave no marker, which is why only this game is checked; see the note beside /wd4430 below.
if /i "%GAME%"=="Unreal_224" (
	findstr /c:"Local corrections for a conforming compiler" "Games\%GAME%\Core\Inc\UnTemplate.h" >nul 2>&1
	if errorlevel 1 (
		echo ERROR: The Unreal 224 SDK under "Games\%GAME%" has not been patched.
		echo        Run patch_unreal_224_sdk.ps1 at the repository root, then build again.
		exit /b 1
	)
)

rem /Zp4 and /Zc:wchar_t- are required for ABI compatibility with the prebuilt
rem Core.lib / Engine.lib, whose TCHAR is a 16-bit unsigned short.
rem WINDOWS_IGNORE_PACKING_MISMATCH silences the SDK's /Zp4 assert.
rem
rem CHARSET is /DUNICODE /D_UNICODE for every game but Klingon, whose Core.lib is an ANSI
rem build. Set by Shared\gamedefs.bat, with the reasoning, since the D3D9 renderer needs
rem the same answer for the same SDKs.
set DEFINES=/D_WINDOWS /DWIN32 %CHARSET% /DNO_UNICODE_OS_SUPPORT ^
 /DWIN32_LEAN_AND_MEAN /DWINDOWS_IGNORE_PACKING_MISMATCH /D%GAMEDEF%
rem Everything both configurations share, so the two cannot drift in what they accept. /std:c++14
rem belongs here and not in Release alone because a Debug build on a different language level is a
rem different program: the two would differ in overload resolution and in which library facilities
rem exist, so a defect could be present in one and absent in the other for no reason to do with the
rem code. This project has no C++17 dependency of its own either way, and neither does what it
rem takes from ..\Shared: the build-flag ladder, the red-black tree and the selection clipper all
rem compile at this level, as they do in the D3D9 renderer, which is also built at C++14.
rem Both configurations build at /W4; don't drop Release to /W3, which is where the noisy diagnostics
rem below sit.
rem
rem /MP compiles the three sources concurrently instead of one after another; it is safe here only
rem because nothing uses #import, which it cannot be combined with. /Zc:inline discards the COMDATs
rem of inline functions that turn out to be unreferenced.
rem
rem /external:W0 covers the game SDK headers, and via /external:env:INCLUDE the Windows SDK and CRT
rem headers vcvarsall puts on INCLUDE, so /W4 applies to this project's sources and not to headers
rem it cannot fix. Without it every translation unit reports C4595 and C4297 out of Core\Inc and
rem C4324 out of the Windows SDK, the last provoked by the /Zp4 this build cannot drop. The game SDK
rem goes in as /external:I below for the same reason: a plain /I would leave it at /W4.
rem
rem The three warnings switched off are this project's own, and each has been looked at:
rem   4121 - rbtree::node_t and UOpenGLRenderDevice hold members whose alignment is sensitive to
rem          /Zp4, which this build requires for ABI compatibility with the prebuilt Core.lib /
rem          Engine.lib. Structural.
rem   4458 - locals named Index and TMUnits shadow members of the same name, in functions that use
rem          only the local.
rem   4459 - a local hInstance shadows the global of the same name, likewise.
rem Everything else is on, and the project's own sources compile clean.
rem
rem /GS- belongs with the /DYNAMICBASE:NO /NXCOMPAT:NO below, and is listed here so it is not the
rem one hardening switch nobody wrote down. It drops the stack buffer overrun checks from all three
rem translation units, in a DLL whose job includes parsing texture data out of game packages.
rem Inherited from the original UTGLR project and kept for the same reason as the two link flags:
rem the host processes are legacy 32-bit binaries built without any of it.
rem
rem TODO: revisit /GS-. The cost is a few percent on the texture conversion paths, which is not
rem obviously the wrong trade any more.
set COMMON=/nologo /c /MP /EHa /Zp4 /GS- /fp:fast /Zc:wchar_t- /Zc:inline /std:c++14 ^
 /W4 /wd4121 /wd4458 /wd4459 /external:env:INCLUDE /external:W0

rem The Harry Potter SDK is the only tree here whose headers do not compile clean under a conforming
rem compiler. Shared\gamedefs.bat holds the suppressions, with which diagnostics and why, because
rem the D3D9 build needs the same two for the same headers. Empty for every other game, hence the
rem guard here.
rem
rem They go on COMMON and are not left to the /external:W0 below: C4430 is an error, not a warning,
rem and /external:W0 does not reach it.
if defined GAMEWARNINGS set COMMON=%COMMON% %GAMEWARNINGS%

rem ..\Shared goes in as a plain /I, not /external:I: it holds this project's own code -
rem the build-flag ladder, the red-black tree, the selection clipper, and the job system and
rem frame arena the siblings use - and /W4 should apply to it as it does to Src.
set INCLUDES=/external:I"Games\%GAME%\Core\Inc" /external:I"Games\%GAME%\Engine\Inc" /I"..\Shared"

rem Each configuration adds only what differs from COMMON.
rem /Gw is Release-only for the same reason /GF and /Gy are: it splits globals into their own
rem COMDATs, which is only worth anything to the /OPT:REF the linker defaults to here and to /GL.
rem /LTCG is not optional once /GL is set. Without it the link still succeeds, because the linker
rem finds IL in the objects, says so, and restarts itself with /LTCG - having thrown away the pass
rem it had already done.
if /i "%CONFIG%"=="Debug" (
	set CFLAGS=%COMMON% /Od /RTC1 /MTd /Zi /D_DEBUG /D_REALLY_WANT_DEBUG %DEFINES%
	set LDFLAGS=/DEBUG
) else (
	set CFLAGS=%COMMON% /O2 /GL /GF /Gy /Gw /MT /DNDEBUG %DEFINES%
	set LDFLAGS=/LTCG
)

rem Per game trees so targets do not overwrite each other's objects or DLL.
rem Both are checked here, since everything below writes into them.
set OUTDIR=Build\%GAME%\%CONFIG%
set DLLDIR=System\%GAME%
if not exist "%OUTDIR%" mkdir "%OUTDIR%"
if not exist "%OUTDIR%" (
	echo ERROR: Could not create the object directory %OUTDIR%.
	exit /b 1
)
if not exist "%DLLDIR%" mkdir "%DLLDIR%"
if not exist "%DLLDIR%" (
	echo ERROR: Could not create the output directory %DLLDIR%.
	exit /b 1
)

rem Debug links /DEBUG, which puts the pdb beside /OUT - into the staging folder - while Release
rem produces none. Deleted before every link, not only in the Release branch, so the folder holds a
rem pdb exactly when the build that made it was a Debug one. Otherwise a Release build leaves the
rem previous Debug pdb in place and deploy.ps1 ships it, since it copies the whole folder.
rem
rem Both configurations link to the same DLLDIR, so nothing in the staged output says which one
rem produced it. Keeping this exact is what lets deploy.ps1 use the pdb as the marker and refuse to
rem install a Debug build unasked.
rem
rem The delete is checked: a debugger or a running game holding the file is enough for a Release
rem link to succeed with the stale pdb still beside it, and deploy.ps1 would then refuse a good
rem build for being a Debug one.
if exist "%DLLDIR%\OpenGL1xDrv.pdb" del /q "%DLLDIR%\OpenGL1xDrv.pdb"
if exist "%DLLDIR%\OpenGL1xDrv.pdb" (
	echo ERROR: Could not remove the stale %DLLDIR%\OpenGL1xDrv.pdb.
	echo        Close anything holding it - a debugger, or the game itself - and build again.
	exit /b 1
)

rem --- Sources ------------------------------------------------------------------
rem Walked from Src, not listed here: the renderer is some forty translation units now,
rem and a hand-written list drifts. The object list comes from the same walk, so the two
rem cannot disagree.
rem
rem /Fo puts every object in one directory, so two sources may not share a base name:
rem the second would overwrite the first's object and the link would quietly use one
rem translation unit twice. Checked here, because nothing downstream would report it.
rem The clash is recorded and reported after the loop, since an "exit /b" within a for
rem body does not reliably carry its code out of the script.
rem
rem ..\Shared is walked too: the selection clipper is one file shared with the D3D9
rem renderer, so it has to be compiled from there.
set "SOURCES="
set "OBJECTS="
set "CLASHES="
call :collect "%~dp0Src"
call :collect "%~dp0..\Shared"
if defined CLASHES (
	echo ERROR: more than one source under Src is named:!CLASHES!
	echo        All objects are written to one directory, so base names must be unique.
	exit /b 1
)
if not defined SOURCES (
	echo ERROR: no .cpp files found under "%~dp0Src".
	exit /b 1
)

echo === Compiling (%CONFIG%, %GAME%) ===
cl %CFLAGS% %INCLUDES% /Fo"%OUTDIR%\\" /Fd"%OUTDIR%\\" !SOURCES!
if errorlevel 1 exit /b 1

echo === Linking (%CONFIG%, %GAME%) ===
rem The DLL name has to match IMPLEMENT_PACKAGE in Src\OpenGLDrv.cpp: the engine binds
rem a package by loading <package>.dll. The object names still follow the source files,
rem which keep their original UTGLR names.
rem /DYNAMICBASE:NO /NXCOMPAT:NO are inherited from the original UTGLR project, which patched
rem opcodes at runtime and so needed a fixed load address and writable code. That path is gone -
rem UTGLR_INCLUDE_OPCODE_PATCH_CODE is commented out in Src\OpenGL.h - so these only cost the DLL
rem its ASLR and DEP. Kept for now because the games they load into are themselves built without
rem either, and changing it is a separate question from this script; worth revisiting.
link /nologo /DLL /INCREMENTAL:NO /MACHINE:X86 /SUBSYSTEM:WINDOWS ^
	/DYNAMICBASE:NO /NXCOMPAT:NO %LDFLAGS% ^
	/MAP:"%OUTDIR%\OpenGL1xDrv.map" ^
	/OUT:"%DLLDIR%\OpenGL1xDrv.dll" /IMPLIB:"%OUTDIR%\OpenGL1xDrv.lib" ^
	!OBJECTS! ^
	kernel32.lib user32.lib gdi32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib ^
	winmm.lib "Games\%GAME%\Core\Lib\Core.lib" "Games\%GAME%\Engine\Lib\Engine.lib"
if errorlevel 1 exit /b 1

rem The launcher builds its renderer list from the .int files in System, so this one has
rem to ship beside the DLL for the renderer to be offered at all.
rem
rem Taken from the repository root, where all three renderers' .int files live together.
copy /y "..\ints\OpenGL1xDrv.int" "%DLLDIR%\" >nul
if errorlevel 1 (
	echo ERROR: Failed to stage ..\ints\OpenGL1xDrv.int into %DLLDIR%.
	exit /b 1
)

echo === Done: %DLLDIR%\OpenGL1xDrv.dll ===
exit /b 0


rem Appends every .cpp under one directory to SOURCES, and its object to OBJECTS. Its own
rem routine because "for /r" will not take its root from a loop variable. Runs in the
rem caller's variable scope, so the two lists survive it.
:collect
for /r "%~1" %%f in (*.cpp) do (
	if defined SEEN_%%~nf set "CLASHES=!CLASHES! %%~nf.cpp"
	set "SEEN_%%~nf=1"
	set "SOURCES=!SOURCES! "%%f""
	set "OBJECTS=!OBJECTS! "%OUTDIR%\%%~nf.obj""
)
exit /b 0
