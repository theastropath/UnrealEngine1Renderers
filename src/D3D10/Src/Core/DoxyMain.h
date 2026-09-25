/**
\mainpage Unreal Engine 1 Direct3D 10 driver Marijn Kentie 2009 \n

Five games, one per build configuration: Unreal
Tournament, Unreal Gold, Deus Ex, Rune, Rune 1.00.

\section layout Project layout
	- D3D10Drv.h: build configuration and the SDK shims.
	- D3D10.h / D3D10Drv.cpp: render device class and package.
	- Core, Device, Frame: odds and ends, device and swap chain, per-frame work.
	- Draw: entry points, one file per primitive kind.
	- Texture: cache, eviction, conversion, lightmap atlas.
	- Geometry: vertex formats, buffers, deferred recorder.
	- Shaders: effect drivers and the shared flags.

The effects live in D3D10\\Shaders. They ship beside the DLL as text.

\section buildset Build settings
	- Struct member alignment: 4 bytes.
	- Unicode, with wchar_t not a built-in type.
	- Set by d3d10drv.sln, the .props sheets beside it and build.ps1.

\section renderer Renderer Geometry arrives worldview transformed and viewport clipped.

\section glue Unreal Engine glue
	- An .int file names the renderer for the game options list.
	- The engine prefixes the configured name with U to find the class.
	- Link against the game's libraries and build per \ref buildset.

\section lifecycle Lifecycle Construction binds the preferences and init sets up the API, then Lock
	clears the buffers, geometry arrives and unlock draws the scene, with the depth buffer cleared
	between sends so that the skybox sits behind everything and the weapon in front.
*/

#pragma once
