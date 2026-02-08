# Quake BSP import plugin for UE5

Allows importing Quake BSP levels in Unreal Engine. Forked from [UE4_QuakeImport](https://github.com/Perpixel/UE4_QuakeImport).\
This is also an attempt to find a practical solution for retro level design workflow for Unreal Engine using Trenchbroom or similar level editors.\
Currently only tested in UE 5.5.

## Features:
- Creates chunked Static Meshes with collision from the BSP data
- Creates shared Textures from BSP data
- Creates and assigns Material Instances to imported meshes
- Creates and updates Level Instances
- Can import BSP World and BSP Entities separately
- Can import lightmaps

## How to:
- Extract `palette.lmp` from Quake `PAK0.PAK` to `QuakeImport/Content`
- Extract BSP map files from `PAK0.PAK` or `PAK1.PAK` somewhere
- Create a dedicated folder in the content of your project (for example `MyProject/Content/Q1`)
- Import a BSP file to this folder in the Unreal Editor
- Set up the newly created `Quake BSP Import Asset` to your liking and then press `Import BSP World` and optionally `Import BSP Entities`
- Drag the created Level Instances or Static Meshes from `MyProject/Content/Q1/*mapname*` to your level

### Todo:
- Bug fixing

> [!NOTE]
> The primary goal for this repo is to find a solution for a level design workflow and therefore some parts were vibecoded (booo), so I cannot guarantee any high quality here.
