# Unreal Engine 1 Renderers

# [DOWNLOAD HERE](https://github.com/theastropath/UnrealEngine1Renderers/releases)

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

## To Compile

### Prerequisites
 * "DirectX SDK (June 2010)" from [HERE](https://www.microsoft.com/en-ca/download/details.aspx?id=6812) extracted, place the contents of the extracted "DXSDK" folder into a folder called "dxsdk-jun2010" in the "src" directory of this repository, alongside the "D3D10" directory.
 * Extract the "Games" directory from the [game headers](https://www.kentie.net/article/d3d10drv/files/src/games.zip) into each of the "D3D9", "D3D10", "OpenGL" directories of this repository, so that there is a "Games" folder alongside the "src" folder of each.
 * Grab copies of the headers for Unreal 224v, Nerf Arena Blast, Klingon Honor Guard, Harry Potter, and X-Com: Enforcer from [HERE](https://coding.hanfling.de/launch/) and extract their contents into "Unreal_224", "Nerf", "Klingon", "HarryPotter", and "XComEnforcer" directories respectively in the "Games" directory.

 ### Direct3D 9
  * Navigate into the "D3D9" directory and run the build script: ```.\build.bat <GameName>```
    * If no game name is provided, it will default to "UnrealTournament".  
    * If an invalid name is provided, it will list all of the possible build targets.

### OpenGL
  * Navigate into the "OpenGL" directory and run the build script: ```.\build.bat <Release|Debug> <GameName>```
    * if no parameters are provided, it will default to "Release UnrealTournament".
    * If an invalid game name is provided, it will list all of the possible build targets.

### Direct3D 10
  * Navigate into the "D3D10" directory and run the Powershell build script: ```powershell.exe ./build.ps1```
    * This build script will compile all build targets, both "debug" and "release".

