# Renderer Settings

This page should explain all of the settings available in the renderers. 

✅ = Setting is available on this renderer

❌ = Setting is NOT available on this renderer

| Setting Name | Direct3D 9 | Direct3D 10 | OpenGL | Description |
| ------------ | :--------: | :---------: | :----: | ----------- |
| 16BitTextures | ✅ | ❌ | ✅ | Enables lower quality, more compact, textures.  This may increase performance. |
| 565Textures | ✅ | ❌ | ❌ | Enables R5G6B5 pixel format with 5 bits for red, 6 bits for green, and 5 bits for blue. |
| AlphaPalette | ❌ | ❌ | ✅ | Workaround for very old GeForce drivers. This should generally be set to true, unless issues are seen. |
| AlphaToCoverage | ❌ | ✅ | ❌ | Smoothens the edges of 'masked' textures such as grates and leaves. Unfortunately, this does lead to artifacts where the textures don't tile (example). Requires at least 4x anti aliasing enabled to take effect. Valid settings: true/false. Default: false. Note: on some hardware this setting seems to result in black backgrounds around HUD icons, etc. I suspect this is a driver issue. |
| Anisotropy | ✅ | ✅ | ✅ | Controls anisotropic texture filtering, which makes textures look less blurry at a distance.  0 is disabled, 1 is isotropic texture filtering, anything higher (up to 16) is the maximum degree of anisotropy to use for texture filtering. |
| Antialiasing | ✅ | ✅ | ✅ | The number of samples to use per fragment for antialiasing, which filters jagged lines.  2, 4, or 8 (default) should generally work. |
| AutoFOV | ❌ | ✅ | ❌ | Automatically sets the field of view depending on the window/screen size. Might want to turn this off if you want to set an extra-wide FOV for multiplayer games. Valid settings: true/false. Default: true. |
| BGRATextures | ❌ | ❌ | ✅ | Allows textures to be uploaded in BGRA format rather than RGBA if the GL_EXT_bgra extension is supported. |
| BufferTileQuads | ❌ | ❌ | ✅ | Enables buffering in the DrawTile path, which may improve text rendering performance. |
| BumpMapping | ❌ | ✅ | ❌ | Can be ignored unless you've got special textures installed. Attempts to fake bump mapping if textures have normal maps present. Requires a normal map to be either present in the texture's bump map slot, or provided as an extra external texture. Valid settings: true/false. Default: false. |
| CacheStaticMaps | ✅ | ❌ | ✅ | Keep a lightmaps upload rather than re-converting it whenever a surface is drawn again. |
| ClassicLighting | ❌ | ✅ | ❌ | With this enabled, the lighting matches that of the original renderers. When disabled, HDR is used (in which case reverting to classic lighting improves performance). Valid settings: true/false. Default: true. |
| ClipboardScreenshots | ❌ | ✅ | ❌ | Screenshots taken using the in-game screenshot key (Rather than PrintScreen) will be copied to the clipboard as well. |
| DebugDrawDetailTextures | ✅ | ❌ | ✅ | Used for debugging detail textures on the fly by using a very obvious (wrong) detail texture.  Leave disabled if not actively debugging. |
| DecalDepthBias | ❌ | ✅ | ❌ | Allows moving decals back and forth if they disappear into geometry. |
| DeferredRecording | ✅ | ✅ | ❌ | Records draw calls on a separate thread and reuses them as long as they aren't invalidated.  May cause a performance hit on CPUs with less than 4 cores. |
| DetailClipping | ✅ | ❌ | ✅ | Enables the use of an experimental detail texture mode.  Costs more CPU time, but may improve performance in fill rate limited situations. |
| DetailMax | ✅ | ❌ | ✅ | Set to 0 or 1 for standard one layer detail texturing if detail textures are enabled.  Set to 2 to enable a second detail texture layer.  The second layer will not show up unless SinglePassDetail is disabled. |
| DetailTextures | ✅ | ❌ | ✅ | Enables detail textures. |
| DynamicTexIdRecycleLevel | ✅ | ❌ | ✅ | Should be set to the default value of 100. |
| FragmentProgram | ✅ | ❌ | ✅ | Enables fragment program mode.  May improve performance on newer video hardware. |
| FrameRateLimit | ✅ | ✅ | ✅ | CPU controlled frame rate limiter.  0 to disable, anything higher is the frame rate limit in frames per second. |
| GammaOffset | ✅ | ✅ | ✅ | Offset for gamma correction.  Can be used to adjust gamma beyond the ends of the in-game brightness sliders.  0.0 causes no change, negative values make the game darker, and positive values make the game lighter. |
| GammaOffsetBlue | ✅ | ❌ | ✅ | Gamma offset for the blue color channel specifically.  Not applied to screenshots. |
| GammaOffsetGreen | ✅ | ❌ | ✅ | Gamma offset for the green color channel specifically.  Not applied to screenshots. |
| GammaOffsetRed | ✅ | ❌ | ✅ | Gamma offset for the red color channel specifically.  Not applied to screenshots. |
| GenerateMipMaps | ✅ | ❌ | ❌ | Enables automatic mipmap generation.  Recommend that this setting is disabled, as support for this feature is often not supported properly. |
| HardwareGamma | ✅ | ❌ | ✅ | When enabled, gamma adjustments will rely purely on the hardware rather than applying a gamma correction pass. |
| Instancing | ❌ | ✅ | ❌ | Enable draw instancing.  Attempts to bundle all draw calls of the same together to optimize CPU <---> GPU messaging.  Seemingly no actual performance benefits. |
| LightmapAtlas | ✅ | ✅ | ❌ | Packs lightmaps into shared pages. |
| LODBias | ✅ | ✅ | ✅ | Allows mipmap selection bias to be adjusted.  Use negative values to pseudo sharpen textures.  Use positive values to blur textures and potentially improve performance. |
| MaxLogTextureSize | ✅ | ❌ | ✅ | Set to 8 or 0. |
| MaxTMUnits | ✅ | ❌ | ✅ | Limit the number of texture units used by the renderer.  Disabled if set to 0.  Useful for debugging. |
| MinLogTextureSize | ✅ | ❌ | ✅ | Set to 0. |
| MultiDrawArrays | ❌ | ❌ | ✅ | Enables the use of the GL_EXT_multi_draw_arrays extension. |
| MultiTexture | ✅ | ❌ | ✅ | Controls the use of multitexturing.  Should always be enabled, as there may be glitches when disabled. |
| NoAATiles | ✅ | ❌ | ✅ | Enable this option to disable antialiasing when drawing tiles as seen from the lower half renderer perspective. This should eliminate HUD corruption that can occur when antialiasing is enabled. Some video hardware / drivers do not support the functionality required to enable this option. Note that corruption with antialiasing enabled can still occur on the logo background if using Entry.unr on startup (it's not made of tiles from the renderer perspective). |
| NoFiltering | ✅ | ❌ | ✅ | Disables filtering on all textures. |
| OneXBlending | ✅ | ✅ | ✅ | If enabled, matches what the D3D renderer does for blending in multitexture mode when applying lightmaps to world geometry. I can't say for sure which way is correct. In single texture mode, the D3D renderer does appear to do blending like the OpenGL renderer in single texture mode or multitexture mode without OneXBlending enabled. |
| Palette | ❌ | ❌ | ✅ | Controls the use of paletted textures. If there is hardware support for paletted textures, using them can significantly improve performance. |
| ParallaxOcclusionMapping | ❌ | ✅ | ❌ | Gives surfaces 3D relief. Pretty GPU intensive, and you might not like the way it looks. Will use an external height map texture if present, otherwise the detail texture is used. Valid settings: true/false. Default: false. |
| PostProcessAA | ❌ | ✅ | ❌ | Applies an antialiasing filter on the finished frame, instead of on each object draw, causing a smoother/blurrier result.  Cheaper performance, but worse looking result. |
| Precache | ✅ | ✅ | ✅ | Controls texture precaching. Texture precaching may improve performance by initializing internal data structures for a number of world textures and most likely getting them loaded into video memory at level load time. It will also slow level loading down some. |
| PureDevice | ✅ | ❌ | ❌ | Enables Direct3D "Pure Device" behaviour and passes many calls directly to the hardware.  Can give some performance benefits. |
| ReduceBanding | ✅ | ✅ | ✅ | Attempts to reduce color banding by reconstructing lightmaps and fog maps.  |
| RefreshRate | ✅ | ❌ | ✅ | Can be used to request a specific refresh rate when running full screen. If set to 0, a default refresh rate is used. If this value is set to an invalid or unsupported refresh rate based on video card or monitor capabilities, the renderer will fail to initialize. |
| RenderThreads | ✅ | ✅ | ❌ | How many threads can queue up draw calls (0 is infinite).  Seemingly no actual performance benefits. |
| S3TC | ✅ | ❌ | ✅ | Enables support for S3TC (Texture Compression).  When disabled, high resolution textures may look incorrect.  Requires restarting the game after changing the setting. |
| ShareLists | ❌ | ❌ | ✅ | Objects live in global memory rather than device memory.  There is some potential for erroneous cache invalidations.  Recommended to leave enabled. |
| SingleCpuAffinity | ❌ | ✅ | ❌ | Enabling this restricts the renderer to only run on a single specific core. |
| SinglePassDetail | ✅ | ❌ | ✅ | Enables single pass detail texture mode. This should generally be the highest performance detail texture mode. It requires 4 texture units. It also requires the UseDetailAlpha option to be enabled. |
| SinglePassFog | ✅ | ❌ | ✅ | Enables single pass fog mode. This should generally be the highest performance fog mode. It requires 3 texture units. For the OpenGL renderer, it also requires support for either the GL_ATI_texture_env_combine3 extension or the GL_NV_texture_env_combine4 extension. |
| SmoothMaskedTextures | ✅ | ❌ | ✅ | Allow applying a smoothing algorithm to masked textures with an alpha.  Recommended to leave disabled, as it can cause an outline around transparent sections of textures. |
| SoftwareVertexProcessing | ✅ | ❌ | ❌ | Enables software vertex processing. |
| SurfaceBatching | ✅ | ❌ | ❌ | When enabled, handling of complex surfaces will be batched together. |
| TexDXT1ToDXT3 | ✅ | ❌ | ✅ | A workaround for poor image quality on NVIDIA GeForce1 - GeForce4 series hardware when using DXT1 format S3TC compressed textures. If enabled, converts all DXT1 textures to DXT3 textures on upload. This improves image quality on the previously mentioned NVIDIA hardware at the expense of twice as much texture memory usage for these textures. The NVIDIA DXT1 image quality problems or most noticeable on certain skybox textures. Keep this in mind when deciding whether or not to trade image quality for speed here. This option should not be enabled on any hardware that draws DXT1 textures with the same quality as DXT3 textures of course. |
| TexIdPool | ✅ | ❌ | ✅ | Should be set to True. |
| TexPool | ✅ | ❌ | ✅ | Should be set to True. |
| TextureCacheBudgetMegs | ✅ | ✅ | ✅ | In MB, the threshold of memory that can be actively used before reporting a warning about memory usage.  Disabled when set to 0. |
| TextureFiltering | ❌ | ✅ | ❌ | Set to 0 gives point filtering, 1 gives linear filtering, 2 gives anisotropic filtering |
| Trilinear | ✅ | ❌ | ✅ | Enables trilinear texture filtering |
| TripleBuffering | ✅ | ❌ | ❌ | Enables triple buffering. |
| UnlimitedViewDistance | ❌ | ✅ | ❌ | Sets view distance to the maximum supported map size. By request. No reason to touch this. |
| UseSSE | ✅ | ❌ | ✅ | Controls the use of SSE instructions. Set to True to auto detect CPU and OS support for SSE instructions and use them if supported. Set to False to disable the use of SSE instructions. |
| UseSSE2 | ✅ | ❌ | ✅ | Controls the use of SSE2 instructions. Set to True to auto detect CPU and OS support for SSE2 instructions and use them if supported. Set to False to disable the use of SSE2 instructions. |
| VBO | ❌ | ❌ | ✅ | Enables use of OpenGL Vertex Buffer Objects. |
| VSync | ✅ | ✅ | ✅ | In D3D9/OpenGL, -1 respects the driver setting, 1 enables VSync, 2 allows half-rate (twice the frame rate).  In D3D10, this can just be enabled or disabled. |
| ZRangeHack | ✅ | ❌ | ✅ | An experimental option that can make the z-buffer work better for far away objects. Might cause unexpected problems, but doesn't seem to break anything major so far. Will fix problems with decals flickering in the distance with 24-bit z-buffers, which is the most you can get on many video cards. Will also fix the issue with the Redeemer covering up part of the HUD. Partially breaks weapon rendering on the first person view one if using wireframe debug mode (will clip near parts of it). Doesn't help enough to make 16-bit z-buffers work correctly. |
| ZTrick | ❌ | ❌ | ✅ | Can avoid some z-buffer clears at the expense of cutting z-buffer precision in half. This may improve performance on some video cards. On video cards with z-buffer optimization hardware, enabling this setting may significantly reduce performance as it interferes with some hardware z-buffer optimization implementations. |

---

Some setting descriptions have been lifted from their original documentation pages:
 - [UTGLR Settings](https://www.cwdohnal.com/utglr/settings.html)
 - [Direct3D 10 Settings](https://www.kentie.net/article/d3d10drv/index.htm#settings)
