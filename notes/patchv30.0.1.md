Add your patchnotes for release v30.0.1 here

## Major Changes
 - OpenGL & D3D9: Prevent brightness from overflowing.
 - OpenGL & D3D9: External DDS texture support (aka Texture Override).
 - D3D9: Extend and clamp gamma with more range, since it has an exponential growth curve.
 - D3D9: Set antialiasing to 8 by default, like the other renderers.
 - D3D10: Normalize blue shift properly on HDR.
 - D3D10: Center texels for complex surfaces.
 - D3D10: Mask translucent tiles.
 - D3D10: Clamp fog alpha.

## Minor Changes

<details>
<summary>Click to expand Minor Changes</summary>

  - Do not mask Highlighted polyflags when drawing complex geometry, to allow shadows to draw properly with semi-translucent geometry.
  - Enabled S3TC by default for Deus Ex.  This allows New Vision to function correctly out of the box.
  - OpenGL & D3D9: Turn off the framelimiter if it failed.
  - OpenGL: Add guards to ortho zoom.
  - OpenGL: After failure, clear texture info if type was not defined.
  - OpenGL: If gamma failed to apply, relaese it so it won't get stuck.
  - OpenGL: Handle NaN fog distance.
  - OpenGL: Set resolution to be the same as viewport size.
  - OpenGL: Clamp texture size.
  - OpenGL: Clamp TMUNITS.
  - OpenGL: Detect CPU features in a less intensive way.
  - OpenGL: DXT1 color testing.
  - D3D9: Add debug log and skip frame when output device cannot be acquired.
  - D3D10: Standardize error reporting on texture conversion failure.
  - Use consistent renderer names.
  - OpenGL & D3D9: "Brightness" option is no longer exposed in the renderer settings.

</details>
