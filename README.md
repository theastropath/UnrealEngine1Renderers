# Unreal Engine 1 Renderers

# [DOWNLOAD HERE](https://github.com/theastropath/UnrealEngine1Renderers/releases)

For information regarding the settings, please [SEE THIS PAGE](https://github.com/theastropath/UnrealEngine1Renderers/blob/main/notes/RendererSettings.md)

Newly updated renderers for various Unreal Engine 1 games.

 - DirectX 9
 - DirectX 10
 - OpenGL 1.x

DirectX 9 and OpenGL based on [prior work by Chris W. Dohnal](https://www.cwdohnal.com/utglr/)

DirectX 10 based on [prior work by Marijn Kentie](https://kentie.net/article/d3d10drv/index.htm)

---

These renderers are built for the following games:
 - Deus Ex
 - Harry Potter and the Philosopher's Stone
 - Nerf Arena Blast
 - Rune 1.00/1.07
 - Star Trek The Next Generation: Klingon Honor Guard
 - Unreal 224
 - Unreal Gold 226
 - Unreal Tournament
 - X-Com: Enforcer

---

## Texture Overrides

All three renderers can load replacement textures from the `Textures` directory.

### Optional extra files

| File | Effect | Renderers |
|---|---|---|
| `Wall01.dds` | Replaces the texture | all three |
| `Wall01.detail.dds` | Detail map, used even where the level supplies none | all three |
| `Wall01.bump.dds` | Bump map | DirectX 10 only |
| `Wall01.height.dds` | Height map, for parallax occlusion mapping | DirectX 10 only |
| `Wall01.dds.flags` | Extra poly flags, one hex number | all three |

Bump and height maps need shader features that OpenGL 1.x and the Direct3D 9 path do not have,
so those two files are read by the DirectX 10 renderer only. Everything else behaves the same
everywhere, which means one pack serves all three renderers.

### Format

DirectX 9 and OpenGL read **DDS only**, in DXT1, DXT3, DXT5 or uncompressed 32-bit BGRA, sized to
a power of two. Supply the mip chain in the file.
DirectX 10 reads the same files through the same parser, and still accepts anything else D3DX can open.

Set `TextureOverrides=False` in the renderer's ini section to turn the feature off. Where the
`Textures` directory does not exist, nothing is loaded and nothing is looked for.

---

## To Compile

### Prerequisites
 * "DirectX SDK (June 2010)" from [HERE](https://www.microsoft.com/en-ca/download/details.aspx?id=6812) extracted, place the contents of the extracted "DXSDK" folder into a folder called "dxsdk-jun2010" in the "src" directory of this repository, alongside the "D3D10" directory.
 * Extract the "Games" directory from the [game headers](https://www.kentie.net/article/d3d10drv/files/src/games.zip) into the common "Games" directory of this repository in the "src" folder (Alongside the "D3D9", "D3D10", "OpenGL" folders)
 * Grab copies of the headers for Unreal 224v, Nerf Arena Blast, Klingon Honor Guard, Harry Potter, and X-Com: Enforcer from [HERE](https://coding.hanfling.de/launch/) and extract their contents into "Unreal_224", "Nerf", "Klingon", "HarryPotter", and "XComEnforcer" directories respectively in the "Games" directory.
   * Apply the patches from the "HeaderPatches" directory to the set of headers associated with each patch.  This step is necessary, as these headers will not compile otherwise.

 ### Direct3D 9
  * Navigate into the "D3D9" directory and run the build script: ```.\build.bat <GameName>```
    * If no game name is provided, it will default to "UnrealTournament".  
    * If an invalid name is provided, it will list all of the possible build targets.
  * The compiled output will be placed into the ```System/<GameName>``` directories in the "D3D9" folder.

### OpenGL
  * Navigate into the "OpenGL" directory and run the build script: ```.\build.bat <Release|Debug> <GameName>```
    * if no parameters are provided, it will default to "Release UnrealTournament".
    * If an invalid game name is provided, it will list all of the possible build targets.
  * The compiled output will be placed into the ```System/<GameName>``` directories in the "OpenGL" folder.

### Direct3D 10
  * Navigate into the "D3D10" directory and run the Powershell build script: ```powershell.exe -file ./build.ps1```
    * This build script will compile all build targets, both "debug" and "release".
  * Alternately, a single product can be built instead by providing a "configuration" parameter: ```powershell.exe -file ./build.ps1 -Configuration "Your Build Target"```
    * "Your Build Target" can be specified in the form of ```"<Game Name> <Debug|Release>```, such as "Deus Ex Release" or "Unreal Tournament Debug"
  * The compiled output will be placed into the ```packages/<GameName>``` directories in the "D3D10" folder.

### Compile and Package all Renderers
  * Run ```python src\BuildRelease.py```, which will compile all the renderers and package individual ZIP files for each game containing all the renderers in the "dist" directory.
    * Without any parameters, this will compile the renderers for all of the supported games.
    * Alternately, you can provide a space separated list of games to compile, such as ```python src\BuildRelease.py DeusEx UnrealTournament```
