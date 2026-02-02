# Quake BSP import plugin for UE5

Allows importing Quake BSP levels in Unreal Engine. Forked from [UE4_QuakeImport](https://github.com/Perpixel/UE4_QuakeImport).\
This is also an attempt to find a practical solution for retro level design workflow for Unreal Engine using Trenchbroom or similar level editors.

## Features:
- Creates Static Meshes with collision from the BSP data
- Creates and assigns shared Materials and derived Material Instances
- Creates shared Textures

## How to:
- Extract `palette.lmp` from Quake `PAK0.PAK` to `QuakeImport/Content`
- Extract BSP maps from `PAK0.PAK` or `PAK1.PAK` somewhere
- Create a dedicated folder in the content of your project (for example `MyProject/Content/Q1`)
- Import a BSP map to this folder in the Unreal Editor
- Drag the created static meshes from `MyProject/Content/Q1/*mapname*` to your level

### Todo:
- Import lights
- Dialog options before importing
- More iterative approach for the BSP re-importing process
- Add support for big maps

> [!NOTE]
> The primary goal for this repo is to find a solution for a level design workflow and therefore some parts were vibecoded (booo), so I cannot guarantee any high quality here.
