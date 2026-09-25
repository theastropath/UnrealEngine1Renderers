Patch notes from before Github upload, leading up to v30

---

An attempt to modernize Chris Dohnal's DX9 and OpenGL renderers, and Marjin Kentie's DX10 renderer. Without the solid base they provided, especially Dohnal's DX9, this wouldn't have been possible.

I've attempted to fix longstanding bugs with Unreal Engine 1 renderers, including:
 - Brightness slider not working
 - Z-fighting
 - Several transparency issues
 - Blackscreen when regaining focus
 - Over-brightened lights
 - Wrong coronas
 - OpenGL renderer not having a save screenshot
 - Stability fixes
 - Code optimizations
 - and an attempt at multithreaded rendering that did not result in any meaningful performance gains

Compilation was attempted for all games I could find headers for, with sweeping assumptions for how they should work based on engine version:
 - Deus Ex
 - Harry Potter and the Philosopher's Stone
 - Nerf Arena Blast
 - Rune 1.00
 - Rune 1.07
 - Star Trek TNG: Klingon Honor Guard
 - Unreal 2.24
 - Unreal 2.26 Gold
 - Unreal Tournament
 - X-Com: Enforcer

The Deus Ex renderers were tested by multiple people, and I'm planning to release them in a more official capacity by the end of the week. I don't have much confidence in the other games, but if they work it's a free win in my book.

Special thanks goes to Hanfling for creating unofficial headers for Unreal Engine 1 games. Without those reverse engineering efforts, compilation would not be possible and wouldn't have been a goal.

---

Should be fixed:
 - DX10 fullscreen crash on resolution change (such as when opening the preferences window)
 - DX9 diagonal seam due to uncentered texel position
 - Mover transparency at a distance due to lockstep dev tooling error (Mover Bias is for different things between DX10 and DX9/OpenGL, and also has different units)
 - Sprite transparency if they're hidden by default, due to not being masked when drawing the tile

Changed:
 - PostProcessAA is off by default now

Not fixed:
 - The freezer lightmap bug, but I think the lightmap in that area is problematic on top of that.
 - Klingon fonts and UI elements

---

New OpenGL/D3D9/D3D10 renderers.

Apart from general bugfixing, I've also done the following things:
 - Implemented Hanfling's reduced banding algorithm
 - Changed configuration option names to be consistent across all renderers, where applicable
 - Changed some default values as requested
 - Used AI to reorganize the project files so they won't be mostly 1 huge file. This should be far more manageable for future maintainers.

---

