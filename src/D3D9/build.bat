@echo off
rem =============================================================================
rem  D3D9Drv build script, replacing a VC++ 2008 project modern MSBuild cannot load.
rem  The switches below come from that project, including several the IDE supplied
rem  implicitly and never wrote into the file.
rem
rem  Usage: build.bat [game]
rem    game defaults to UnrealTournament; must be a folder under Games\.
rem =============================================================================

rem Delayed expansion is on for the whole script, not just the toolchain blocks that need it.
rem The cost: a literal '!' is eaten everywhere, including inside vcvarsall and anything it
rem sets, so a path containing one would break the build in a way that looks nothing like its
rem cause. TODO: narrow this to the toolchain block. No such path is involved today.
setlocal EnableDelayedExpansion

set "ROOT=%~dp0"
if "%ROOT:~-1%"=="\" set "ROOT=%ROOT:~0,-1%"

set "GAME=%~1"
if "%GAME%"=="" set "GAME=UnrealTournament"

rem --- What this game asks of the build -----------------------------------------
rem The table is in Shared\gamedefs.bat, shared with the OpenGL renderer. It sets GAMEDEF,
rem CHARSET and GAMEWARNINGS from the game name, ALL_GAMES either way, and says nothing
rem itself about a game it does not recognise: the wording below is this script's, as is
rem the SDK check that follows.
call "%ROOT%\..\Shared\gamedefs.bat" "%GAME%"
if errorlevel 1 (
	echo [error] Unknown or unsupported game "%GAME%".
	echo         Expected one of: %ALL_GAMES%
	exit /b 1
)

set "GAMEDIR=%ROOT%\Games\%GAME%"
if not exist "%GAMEDIR%\Core\Inc\Core.h" (
	echo [error] Game SDK not found at "%GAMEDIR%".
	exit /b 1
)

rem The Unreal 224 SDK predates standard C++ and does not compile as it shipped; three of its
rem headers are corrected in place by patch_unreal_224_sdk.ps1 at the repository root. The Core.h
rem check above passes on an unpatched clone, which then fails deep inside vendor headers with
rem errors that read as a broken toolchain. The patch script writes a marker into every file it
rem edits, so look for that instead.
if /i "%GAME%"=="Unreal_224" (
	findstr /c:"Local corrections for a conforming compiler" "%GAMEDIR%\Core\Inc\UnTemplate.h" >nul 2>&1
	if errorlevel 1 (
		echo [error] The Unreal 224 SDK under "%GAMEDIR%" has not been patched.
		echo         Run patch_unreal_224_sdk.ps1 at the repository root, then build again.
		exit /b 1
	)
)

rem Per-game trees so concurrent targets do not overwrite each other's objects or DLL
set "OUTDIR=%ROOT%\Build\%GAME%\Release"
set "DLLDIR=%ROOT%\System\%GAME%"
rem Both are checked here, so a path that cannot be created is reported as such instead of
rem as a compiler or linker error further down.
if not exist "%OUTDIR%" mkdir "%OUTDIR%" >nul 2>&1
if not exist "%OUTDIR%" (
	echo [error] Could not create the object directory "%OUTDIR%".
	exit /b 1
)
if not exist "%DLLDIR%" mkdir "%DLLDIR%" >nul 2>&1
if not exist "%DLLDIR%" (
	echo [error] Could not create the output directory "%DLLDIR%".
	exit /b 1
)

rem --- Locate the MSVC toolchain and enter a 32-bit build environment -----------
rem The quotes around the VSWHERE assignment below are load-bearing: its name contains a closing
rem parenthesis, and cmd matches the if block's parentheses at parse time, before expansion.
rem Quoted, it is just text; unquoted it ends the block early and reports a syntax error nowhere
rem near the real line. A stray parenthesis in a rem inside the block does the same, so these
rem notes stay out here.
rem
rem The test is on VSCMD_ARG_TGT_ARCH, which is what vcvarsall records the target as. Testing
rem VSINSTALLDIR is not enough: the default "x64 Native Tools Command Prompt" sets it too, and
rem the x64 compiler rejects the inline __asm below.
if /i not "%VSCMD_ARG_TGT_ARCH%"=="x86" (
	set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
	if not exist "!VSWHERE!" (
		echo [error] vswhere.exe not found; is Visual Studio / Build Tools installed?
		exit /b 1
	)
	for /f "usebackq tokens=*" %%i in (`"!VSWHERE!" -latest -products * -property installationPath`) do set "VSDIR=%%i"
	if not exist "!VSDIR!\VC\Auxiliary\Build\vcvarsall.bat" (
		echo [error] vcvarsall.bat not found under "!VSDIR!".
		exit /b 1
	)
	rem 32-bit only: the game SDK libs and the inline __asm blocks are x86.
	rem stderr is discarded with stdout: vcvarsall looks its own version up by running an unqualified
	rem "vswhere.exe", which resolves only while the current directory is searched for executables, so
	rem where NoDefaultCurrentDirectoryInExePath is set it prints "'vswhere.exe' is not recognized" and
	rem loses the banner version this already throws away. errorlevel is what reports the real outcome.
	call "!VSDIR!\VC\Auxiliary\Build\vcvarsall.bat" x86 >nul 2>&1
	if errorlevel 1 (
		echo [error] Failed to initialize the MSVC x86 build environment.
		exit /b 1
	)
)

rem --- Shared switches ----------------------------------------------------------
rem CHARSET is /DUNICODE /D_UNICODE for every game but Klingon, whose Core.lib is an ANSI
rem build. Set by Shared\gamedefs.bat, with the reasoning, since the OpenGL renderer needs
rem the same answer for the same SDKs. _UNICODE is also what CharacterSet="1" in the old
rem .vcproj came to.
set "DEFINES=/D_WINDOWS /DWIN32 %CHARSET% /DNO_UNICODE_OS_SUPPORT /DWIN32_LEAN_AND_MEAN /D_SECURE_SCL=0 /D%GAMEDEF%"
rem The Windows headers assert against /Zp4, which the game ABI requires.
set "DEFINES=%DEFINES% /DWINDOWS_IGNORE_PACKING_MISMATCH"
set "DEFINES=%DEFINES% /D_CRT_SECURE_NO_WARNINGS /D_CRT_NONSTDC_NO_DEPRECATE /D_WINSOCK_DEPRECATED_NO_WARNINGS"
set "DEFINES=%DEFINES% /DNDEBUG"
rem ..\Shared holds the job system and frame arena, shared with the OpenGL and D3D10 renderers
set "INCLUDES=/I"%GAMEDIR%\Core\Inc" /I"%GAMEDIR%\Engine\Inc" /I"%ROOT%\..\Shared""

rem /Zp4        struct member alignment, must match the prebuilt game DLLs
rem /Zc:wchar_t- Unreal's TCHAR predates wchar_t being a distinct builtin type
rem /EHa        asynchronous exception model
rem /GL         whole program optimization; pairs with /LTCG at link time. Functions holding
rem             inline __asm are left out of it, not rejected.
rem
rem No /Ob1 here: it would override the /Ob2 that /O2 implies, leaving only functions marked
rem inline eligible.
rem
rem No /arch: either, which is not an x87 baseline. /arch:SSE2 has been the x86 default since
rem Visual Studio 2012 and /arch:IA32 is the switch that asks for x87, so this DLL already
rem requires SSE2 and its scalar float math already runs at SSE2's exact 32 and 64 bit precision,
rem not x87's 80 bit. See D3D9.h's UseSSE notes for what does not follow from that: the UseSSE and
rem UseSSE2 ini options gate the hand written intrinsic paths only, and cannot protect a pre-SSE2
rem CPU from ordinary compiled float code.
set "CFLAGS=/nologo /c /W3 /EHa /Zp4 /GS- /fp:fast /Zc:wchar_t- /std:c++14 /O2 /GL /GF /Gy /MT"

rem Harry Potter is the only tree needing a compiler flag to get its headers through.
rem Shared\gamedefs.bat holds those, with which diagnostics and why, because the OpenGL
rem build needs the same two for the same headers. Empty for every other game, hence the
rem guard here.
rem
rem Unreal 224 needs two corrections of the same kind, applied by patch_unreal_224_sdk.ps1
rem at the repository root; the marker check above tells a fresh clone to run it.
rem
rem One detail belongs to this build and not to the table: the /Zc:forScope- that would
rem have answered the UnModel.h and UnActor.h loop-variable reuse makes this toolchain
rem fault with an internal compiler error on D3D9.cpp, with or without /GL, so those two
rem headers are patched in place instead.
if defined GAMEWARNINGS set "CFLAGS=%CFLAGS% %GAMEWARNINGS%"

rem --- Sources ------------------------------------------------------------------
rem Enumerated from Src, not listed here. The renderer is some forty translation units now,
rem and a hand-written list drifts: the failure that gives, an unresolved symbol or a stale
rem object still being linked, names anything but the omission. The object list comes from
rem the same walk, so the two cannot disagree.
rem
rem Each path is quoted individually. Unquoted, a clone into a directory containing a space
rem made the loop split each path into fragments and cl then fail on a nonexistent file.
rem
rem /Fo puts every object in one directory, so two sources may not share a base name: the
rem second would overwrite the first's object and the link would quietly use one translation
rem unit twice. The clash is recorded and reported after the loop, because an "exit /b" in a
rem for body does not reliably carry its code out of the script.
rem
rem ..\Shared is walked as well as Src: the selection clipper is one file shared with the
rem OpenGL renderer, so it has to be compiled from there.
set "SOURCES="
set "OBJECTS="
set "CLASHES="
call :collect "%ROOT%\Src"
call :collect "%ROOT%\..\Shared"
if defined CLASHES (
	echo [error] More than one source under Src is named:!CLASHES!
	echo         All objects are written to "%OUTDIR%", so base names must be unique.
	exit /b 1
)
if not defined SOURCES (
	echo [error] No .cpp files found under "%ROOT%\Src".
	exit /b 1
)

rem d3d9.dll is loaded at runtime via LoadLibrary, so d3d9.lib is not linked. The
rem Windows libs below are the set the IDE added implicitly to every project.
set "LIBS=kernel32.lib user32.lib gdi32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib winmm.lib"
set "LIBS=%LIBS% "%GAMEDIR%\Core\Lib\Core.lib" "%GAMEDIR%\Engine\Lib\Engine.lib""

echo === D3D9Drv : %GAME% / Release (%GAMEDEF%) ===
echo.

cl %CFLAGS% %DEFINES% %INCLUDES% /Fo"%OUTDIR%\\" /Fd"%OUTDIR%\\" !SOURCES!
if errorlevel 1 (
	echo.
	echo [error] Compilation failed.
	exit /b 1
)

echo.
rem /LTCG is required to consume the IL that /GL puts in the object files
link /nologo /DLL /INCREMENTAL:NO /LTCG /MACHINE:X86 /NXCOMPAT:NO /DYNAMICBASE:NO ^
	/OUT:"%DLLDIR%\D3D9Drv.dll" ^
	/IMPLIB:"%OUTDIR%\D3D9Drv.lib" ^
	/MAP:"%OUTDIR%\D3D9Drv.map" ^
	!OBJECTS! %LIBS%
if errorlevel 1 (
	echo.
	echo [error] Link failed.
	exit /b 1
)

rem The launcher builds its renderer list from the .int files in the game's System folder,
rem so this one has to ship beside the DLL for the renderer to be offered at all. Taken
rem from the repository root, where all three renderers' .int files live together.
copy /y "%ROOT%\..\ints\D3D9Drv.int" "%DLLDIR%\" >nul
if errorlevel 1 (
	echo.
	echo [error] Failed to stage ints\D3D9Drv.int into "%DLLDIR%".
	exit /b 1
)

echo.
echo === Built %DLLDIR%\D3D9Drv.dll ===
exit /b 0


rem Appends every .cpp under one directory to SOURCES, and its object to OBJECTS. A separate
rem routine, not a nested loop, because "for /r" will not take its root from a loop variable.
rem Runs in the caller's variable scope, so the two lists it builds survive it.
:collect
for /r "%~1" %%f in (*.cpp) do (
	if defined SEEN_%%~nf set "CLASHES=!CLASHES! %%~nf.cpp"
	set "SEEN_%%~nf=1"
	set "SOURCES=!SOURCES! "%%f""
	set "OBJECTS=!OBJECTS! "%OUTDIR%\%%~nf.obj""
)
exit /b 0
