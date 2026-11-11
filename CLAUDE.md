# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

KaijuGame is an Unreal Engine **5.7** C++ project (created from the Blank template, `TP_Blank` → `KaijuGame` redirects in `DefaultEngine.ini`). Engine install: `C:\Program Files\Epic Games\UE_5.7`. It is at a very early stage: the single runtime module `KaijuGame` contains only the module boilerplate; all gameplay so far lives in Blueprints/assets under `Content/`.

## Build

Close the Unreal Editor first (Live Coding blocks the build tool while it is open), then run from PowerShell:

```powershell
& "C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat" KaijuGameEditor Win64 Development -project="C:\Users\some1\Documents\UE_Projects\KaijuGame\KaijuGame.uproject" -waitmutex
```

Regenerate the Visual Studio solution after adding/removing source files or modules:

```
"C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.exe" -projectfiles -project="C:\Users\some1\Documents\UE_Projects\KaijuGame\KaijuGame.uproject" -game -engine
```

Targets: `KaijuGameEditor` (Editor) and `KaijuGame` (Game), both `BuildSettingsVersion.V6` / `EngineIncludeOrderVersion.Unreal5_7`. There are no automated tests yet.

## Rules

- Only edit files in `Source/` and `Config/`.
- Never edit `Binaries/`, `Intermediate/`, `Saved/`, or `.uasset` files.
- Through the Unreal MCP (`mcp__unreal__*` tools), reading and inspecting assets is fine, but **ask before any change** (creating, modifying, deleting, or saving assets, spawning/deleting actors, `execute_python`).
- Use Unreal naming prefixes: `A` for actors, `U` for objects, `F` for structs.

## Unreal MCP

- The `UnrealMCP` editor plugin (`Plugins/UnrealMCP`, a git submodule of https://github.com/aadeshrao123/Unreal-MCP) exposes the editor over TCP. The Python server `unrealmcp` (installed under Python 3.13, not the default 3.15) bridges it to Claude via `.mcp.json`.
- The tools only work while the Unreal Editor is open with this project. Run `health_check` first; if it fails, ask the user to open the editor.
- Fresh clone: `git submodule update --init` to fetch the plugin.

## Code layout

- `Source/KaijuGame/KaijuGame.Build.cs` — module deps: Core, CoreUObject, Engine, InputCore, **EnhancedInput**. Add new engine modules here (e.g. Slate, Niagara) when C++ needs them.
- Input uses the Enhanced Input system (`DefaultPlayerInputClass`/`DefaultInputComponentClass` in `Config/DefaultInput.ini`); there are no legacy action/axis mappings.
- `.uasset`/`.umap` files are binary. For Blueprint or asset changes, describe the steps for the user to do in the editor.

## Content

- `Content/_Dev/` — work-in-progress assets. `Characters/Character_Mech/` holds the mech character (`Heavy_BP`, skeletal mesh/skeleton, idle anims, materials, Niagara VFX for electricity/lightning).
- `Content/Levels/CityTest.umap` and `Content/_Dev/Levels/CityTest.umap` — test city map (two copies exist).
- `GameDefaultMap` is still the engine's `/Engine/Maps/Templates/OpenWorld`.

## Rendering config

`DefaultEngine.ini` targets high-end desktop: Lumen GI/reflections, hardware ray tracing, Virtual Shadow Maps, Substrate materials, DX12 / SM6, no static lighting. Keep new materials/lighting compatible with that setup (no baked lighting).

## Repo notes

- `Binaries/`, `Intermediate/`, `Saved/`, `DerivedDataCache/`, `.vs/`, and `*.sln` are gitignored (generated).
- Git LFS tracks binary assets (`*.uasset`, `*.umap`, textures, models, audio) via `.gitattributes`. Add new binary file types there.

## Known issues / cleanup

- **Duplicate test map:** `CityTest.umap` exists in both `Content/Levels/` and `Content/_Dev/Levels/`. Confirm which one is current before editing or referencing it.
- **Startup map:** `GameDefaultMap` still points to the engine's OpenWorld template map, not CityTest.