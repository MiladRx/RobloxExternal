# RobloxExternal

External Roblox tool — ESP, silent aim, chams, Lua VM, explorer, and misc utilities. Windows x64.

## Credits

Originally developed by [awaky1337](https://github.com/awaky1337) as `jew-dick-hack`. This fork is maintained by **Milad** since the original stopped receiving updates.

## What this fork adds / fixes

- **BoundSilent** — new bound-only silent aim variant (`src/features/aim/BoundSilent.{h,cpp}`); module-cave stub install with CFG mark and slot restore, shares `RaycastState` layout with `RaycastSilent`
- Registered `BoundSilent.{h,cpp}` in the vcxproj / filters
- Offset resync for the current Roblox build so silent aim + ESP work again

## Build it yourself

**No prebuilt binaries are distributed.** Clone the repo and build locally — this avoids AV false-positives on random-downloaded binaries and gives you a fresh, unscanned exe.

Requirements:
- Windows 10/11 x64
- Visual Studio 2022+ (Desktop C++ workload, v143 toolset)

Steps:
1. `git clone https://github.com/MiladRx/RobloxExternal.git`
2. Open `jewsploit.sln`
3. Set config to `Release | x64`
4. Build → output at `x64\Release\jewsploit.exe`

Or from the command line:

```bash
msbuild jewsploit.sln /p:Configuration=Release /p:Platform=x64 /m
```

### About antivirus flags

Any tool that reads/writes another process's memory (the whole point of an external cheat) triggers heuristic detections from most AV vendors. That's true for every Roblox external on GitHub. Because you built the exe locally from source you can read, this is not a threat — but Windows Defender may still quarantine it. Add the build folder to Defender exclusions if needed.

## Layout

- `src/core/` — memory, offsets, console, globals, player cache, roblox class wrappers
- `src/features/aim/` — RaycastSilent, BoundSilent, MagicBullet, MouseSilent, PhantomSilent, ViewportSilent
- `src/features/visuals/` — ESP, chams, crosshair, kill effects
- `src/features/lua/` — Lua VM, executor, bridge, drawing
- `src/gui/` — ImGui-based menu and windows
- `src/renderer/` — overlay renderer
- `third_party/` — vendored deps
- `scripts/` — VM smoke tests
- `tools/` — auxiliary tooling (see below)

## tools/

Not part of the main build — utilities used during development and after Roblox updates.

### `tools/fission_src/`

Vendored source of **Fission**, a Luau decompiler used by the Lua VM feature to decompile scripts at runtime. Built with CMake (`tools/fission_src/CMakeLists.txt`) and linked into the main exe as `Fission.Decompiler.lib` + `Fission.Common.lib`. If those .libs are missing under `tools/fission_src/build/Release/` the main solution won't link — build Fission first:

```bash
cd tools\fission_src
cmake -B build -A x64
cmake --build build --config Release
```

Fission is credited to its upstream author (see `tools/fission_src/README.md` and `LICENSE`).

### `tools/ida/`

IDA Pro helpers for re-syncing offsets after a Roblox update.

- `raycast.py` — IDAPython script that scans `RobloxPlayerBeta.exe` for `WorldRoot::Raycast` bound-function metadata and prints RVAs to paste into `src/core/roblox/offsets/Offsets.h` (`RaycastBoundDesc`, `RaycastBoundFn`). Fixes the "Silent inject fail bad handler" error that appears when Roblox shifts these addresses.
- `README.md` — MCP-style guide describing the layout and the correct static-analysis method (the naive "xref to `Raycast` string" heuristic doesn't work — the desc body is populated at runtime).

Run `raycast.py` inside IDA after loading a fresh `RobloxPlayerBeta.exe` IDB, copy the two RVAs into `Offsets.h`, rebuild.

## License

Follows the license of the upstream repo. Attribution to [awaky1337](https://github.com/awaky1337) preserved.
