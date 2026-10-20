# NetAnomaly SDK adapter

This directory is the SDK-side workspace for adapting IX-Ray tools to the NetAnomaly game. It is intentionally self-contained: the NetAnomaly project is not a submodule, dependency, or build target here.

## Current state

- Based on the IX-Ray 1.6 STCOP fork.
- Work branch: `adaptation/netanomaly-sdk`.
- No game source, assets, or project files have been copied or changed.
- The game-specific file formats, import/export API, and build interface have not yet been defined in this workspace.
- First SDK plugin: `plugins/asset_inventory.py`. It scans a selected directory read-only and writes its JSON report under the user's profile in `NetAnomalySDKReports/`.

## Adaptation boundary

Keep game-specific code in this adapter or in IX-Ray's SDK plugin layer. Do not add a build dependency on the NetAnomaly repository. Prefer editor plugins for workflows that can use the SDK API; change the editor or engine only when the plugin API cannot expose the required operation.

## Next implementation steps

1. Define a small adapter contract for the game's asset types, validation rules, and import/export operations.
2. Add format-specific validation and conversion tools against that contract, without linking to the game project.
3. Add IX-Ray editor plugins only where the existing SDK API can safely support the workflow.
4. Add format fixtures and verification only after the contract and sample data are supplied.

## First plugin

`plugins/asset_inventory.py` is a format-independent starter tool. In a separate IX-Ray SDK/developer installation, copy it to the Level Editor's `plugins/netanomaly/` folder, then run it from the Plugins menu and provide the asset directory. It reads file names and sizes only; it does not open or alter asset contents. Its report is written to `NetAnomalySDKReports/` in the user's profile, never into the selected asset directory or NetAnomaly project.

## Build the adapted SDK

On Windows with Visual Studio 2022 and CMake installed, configure the IX-Ray SDK, editors, utilities, and native plugin targets together:

```powershell
cmake --preset NetAnomaly-SDK-x64-Windows
cmake --build --preset NetAnomaly-SDK-x64-Windows-Debug
```

The build output stays under `build/x64/NetAnomaly-SDK-Windows/`. This preset only builds IX-Ray; it does not configure or build the NetAnomaly game.

## IX-Ray SDK locations

- Editors: `src/Editors/` (`LevelEditor`, `ActorEditor`, `ParticleEditor`, `ShaderEditor`).
- Level Editor plugin host: `src/Editors/LevelEditor/Plugins/`.
- Standalone utilities: `src/utils/`.
- SDK build switches: `IXRAY_EDITORS`, `IXRAY_UTILS`, and `IXRAY_PLUGINS` in the root CMake configuration.

The first adaptation should stay on the SDK side. Engine/runtime changes should be isolated and justified by a concrete incompatibility in the game's data or behavior.
