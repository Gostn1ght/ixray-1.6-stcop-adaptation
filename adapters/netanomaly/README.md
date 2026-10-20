# NetAnomaly SDK adapter

This adapter keeps NetAnomaly-specific developer tools beside the IX-Ray SDK fork. NetAnomaly is not a submodule, dependency, or build target. Its repository was inspected read-only to identify formats and packaging rules; no game files, assets, or source were changed by this adapter work.

## Included tools

The scripts under `plugins/` use the IX-Ray Level Editor's Python plugin interface. Install Python, then copy the scripts into the `plugins/netanomaly/` folder of a **separate IX-Ray SDK / Level Editor installation**. The editor reads the `# desc:` and `# input:` lines at the top of each script and passes the selected paths as `-argument="path"` command-line values.

- `asset_inventory.py` scans a selected directory without following links. It reports extensions, largest files, NetAnomaly asset roles (`resources/`, `gamedata/`, `client/scripts/`, `server/scripts/`), recognizable X-Ray DB/CFS-like headers, and the structure of `LZPACK1` protected archives. It does not decrypt or authenticate protected archives. JSON reports go to `%USERPROFILE%/NetAnomalySDKReports/`, outside the selected asset tree.
- `room_package_check.py` validates `personal_room.room` (NCRM v2) and its optional `.camera` (NCRC v1/v2/v3) and `.pick` (NCRP v1) sidecars. It checks the runtime's header, lengths, material/vertex limits, and coordinate constraints. Missing camera data means the game uses defaults; missing pick data means room objects cannot be clicked.
- `patch_stage_check.py` checks a staged patch against the `lzpack` input policy: allowed paths, CP1251 archive-path length, duplicate names, links, private/debug/build files, and the 850 MiB client/server volume cap. It is read-only; it does not create archives or read any key.

The same scripts can run outside the editor with a Python interpreter:

```powershell
python .\asset_inventory.py -asset_root="D:\Game\resources"
python .\room_package_check.py -room_file="D:\Game\gamedata\meshes\netcoop\personal_room.room"
python .\patch_stage_check.py -stage_root="D:\PatchStage"
```

For patch staging, the selected root must contain entries beneath `gamedata/`, `client/scripts/`, and/or `server/scripts/`. Shared `gamedata/` files count toward both client and server volume totals. This validator checks policy only; use the game's own release process to build and sign/protect an archive.

## Format notes from source inspection

- Runtime room geometry is `NCRM` version 2: 1–128 material batches, each with a NUL-terminated CP1251 texture name and 36-byte `<3f3fI2f` vertices. Each batch has a nonzero triangle-list count divisible by three and no more than 300,000 vertices. The file must end after the last batch.
- Camera sidecars use `NCRC`: v1 has four views and a lamp; v2 adds two views and a seat; v3 adds a seat heading. The runtime accepts exact lengths of 116, 176, or 180 bytes and checks finite/ranged coordinates and non-degenerate camera directions.
- Pick sidecars use `NCRP` v1 with five AABBs and an exact length of 132 bytes. A box with minimum greater than maximum on any axis is inactive.
- Protected `LZPACK1` archives have a 48-byte header and 64 KiB authenticated blocks. Without the private key the inventory can check structural length only; it cannot verify block authentication.
- Patch staging accepts only `gamedata/`, `client/scripts/`, and `server/scripts/`; it excludes `.pdb`, `.key`, `.secret`, `appdata`, `.git`, `_build`, and `netcoop_debug_*`, and caps each role's selected patch input at 850 MiB.

These checks are for NetAnomaly's current custom runtime formats. They do not make IX-Ray's stock Level Editor understand the game's custom room format, nor do they edit game files.

## Build the separate IX-Ray SDK host

With Visual Studio 2022 and CMake installed, configure and build the IX-Ray development host with its editors, utilities, and native plugin targets enabled:

```powershell
cmake --preset NetAnomaly-SDK-x64-Windows
cmake --build --preset NetAnomaly-SDK-x64-Windows-Debug
```

The output stays under `build/x64/NetAnomaly-SDK-Windows/`. This preset builds IX-Ray only; it does not configure or build NetAnomaly. The three Python tools are scripts and are not compiled into native plugins.

## Adaptation boundary

Keep NetAnomaly-specific workflows in this adapter or the separate SDK's plugin layer. Prefer these read-only validation and inventory tools for game assets. Add an importer/exporter only when the target data contract is known and the operation can be kept outside the game checkout. Engine/runtime changes belong in a distinct, explicitly scoped task.
