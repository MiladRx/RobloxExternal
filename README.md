<div align="center">

# RobloxExternal

**External Roblox client overlay — read-only memory, ImGui overlay, no injection.**

[![Platform](https://img.shields.io/badge/platform-Windows%2010%2F11%20x64-0078D4?style=flat-square&logo=windows)]()
[![Toolset](https://img.shields.io/badge/toolset-MSVC%20v143-5C2D91?style=flat-square&logo=visualstudio)]()
[![C++](https://img.shields.io/badge/C%2B%2B-20-00599C?style=flat-square&logo=cplusplus)]()
[![Status](https://img.shields.io/badge/status-actively%20maintained-brightgreen?style=flat-square)]()
[![Build](https://img.shields.io/badge/build-from%20source-orange?style=flat-square)]()

</div>

---

## Credits

Originally developed by **[awaky1337](https://github.com/awaky1337)** as `jew-dick-hack`.
This fork is maintained by **[MiladRx](https://github.com/MiladRx)** since the upstream stopped shipping updates.

> All credit for the original architecture, feature set, and codebase goes to awaky1337. This fork continues the work with offset resyncs, bug fixes, and small feature additions.

---

## Features

| Category      | What's in it                                                                 |
| ------------- | ---------------------------------------------------------------------------- |
| **Aim**       | RaycastSilent · BoundSilent · MagicBullet · MouseSilent · PhantomSilent · ViewportSilent |
| **Visuals**   | ESP boxes/skeleton/health · Chams (shader / mesh / engine) · Kill effects · Chinahat · Offscreen arrows |
| **World**     | Havoc-style world ESP · Lighting invalidate · WorldEdit · WorldSlots         |
| **Movement**  | Fly · Speed · Third-person                                                   |
| **Misc**      | Hitbox expander · Hitsounds · Player avatars · Explorer · Properties window  |
| **Lua VM**    | Executor · Bytecode · Reflection · Drawing · Fission decompiler bridge       |
| **Games**     | Phantom Forces · Apocalypse Rising                                           |
| **Overlay**   | DX11 · ImGui menu · Custom fonts · Icon set · Config save/load               |

---

## What this fork adds / fixes

- **BoundSilent** — new bound-only silent aim variant in [`src/features/aim/BoundSilent.{h,cpp}`](src/features/aim); module-cave stub install with CFG mark and slot restore, shares `RaycastState` layout with `RaycastSilent`
- **Offset resync** — updated to the current Roblox build so silent aim + ESP work again
- **Auto-load last config** — on startup, the last-used config is applied before the overlay draws; stored in `configs\last.txt`
- **Reduced AV noise** — VERSIONINFO resource embedded, dev-machine PDB paths stripped from the binary, source-only distribution model

---

## Build it yourself

> **No prebuilt binaries are shipped.** Random-downloaded exes get flagged by AV heuristics because external cheats read/write another process's memory — that's true for every Roblox external on GitHub. Building locally gives you a fresh, unscanned binary and lets you audit the source you're running.

### Requirements

- Windows 10 / 11 x64
- Visual Studio 2022 or newer
- **Desktop development with C++** workload (v143 toolset)
- CMake 3.20+ (only for the Fission sub-build)

### Steps

```bash
git clone https://github.com/MiladRx/RobloxExternal.git
cd RobloxExternal
```

Build the Fission Luau decompiler first (produces static libs the main project links against):

```bash
cd tools\fission_src
cmake -B build -A x64
cmake --build build --config Release
cd ..\..
```

Then build the main project:

```bash
msbuild jewsploit.sln /p:Configuration=Release /p:Platform=x64 /m
```

Output binary:

```
x64\Release\jewsploit.exe
```

Or just open `jewsploit.sln` in Visual Studio, pick `Release | x64`, and hit **Build → Build Solution**.

### About antivirus flags

Any tool that reads another process's memory triggers heuristic detections. That's the tradeoff of external cheats. Because you built the exe locally from source you can read, this is not a threat — but Windows Defender may still quarantine it. Add the build folder to Defender exclusions if that happens.

---

## Repository layout

```
RobloxExternal/
├── src/
│   ├── core/
│   │   ├── memory/          Remote memory read/write, module base, RPM/WPM
│   │   ├── offsets/         All Roblox offsets — the file you update per release
│   │   ├── console/         Colored console + status dumps
│   │   ├── globals/         Cached DataModel / Workspace / Players / LocalPlayer
│   │   ├── player/          Player cache thread + PlayerHandler
│   │   ├── roblox/classes/  Instance / BasePart / Camera / Humanoid / Workspace wrappers
│   │   └── config/          Config .cfg save/load + last-used auto-load
│   ├── features/
│   │   ├── aim/             All silent-aim variants
│   │   ├── visuals/         ESP, chams, crosshair, kill effects
│   │   ├── world/           World edits, lighting, slots
│   │   ├── movement/        Fly, speed, third-person
│   │   ├── misc/            Hitbox expander, hitsounds, avatars
│   │   ├── lua/             Lua VM, executor, bridge, drawing
│   │   ├── games/           Per-game overrides (PF, Apocalypse Rising)
│   │   └── explorer/        Instance explorer
│   ├── gui/                 ImGui menu, tabs, widgets, windows
│   ├── renderer/            DX11 overlay
│   └── Main.cpp             Entry — attaches to Roblox, spawns overlay
├── third_party/             Vendored deps (ImGui, FreeType, zstd, Luau, stb)
├── scripts/                 VM smoke tests (.lua)
├── tools/                   Dev tooling (see below)
└── assets/                  Icons, fonts, hitsound data, VERSIONINFO
```

---

## `tools/`

Not part of the main build — utilities used during development and after Roblox updates.

### `tools/fission_src/`

Vendored source of **[Fission](https://github.com/awaky1337/jew-dick-hack)** — a Luau decompiler used by the Lua VM feature to decompile scripts at runtime. Built with CMake; the resulting `Fission.Decompiler.lib` and `Fission.Common.lib` are linked into the main exe.

> **Build Fission before the main solution**, or the linker will fail with missing `.lib` errors.

```bash
cd tools\fission_src
cmake -B build -A x64
cmake --build build --config Release
```

Credit and license: see [`tools/fission_src/README.md`](tools/fission_src/README.md) and [`tools/fission_src/LICENSE`](tools/fission_src/LICENSE).

### `tools/ida/`

IDA Pro helpers for re-syncing offsets after a Roblox update.

| File          | Purpose                                                                              |
| ------------- | ------------------------------------------------------------------------------------ |
| `raycast.py`  | IDAPython script — scans `RobloxPlayerBeta.exe` for `WorldRoot::Raycast` bound-function metadata and prints RVAs for `RaycastBoundDesc` / `RaycastBoundFn`. Fixes "Silent inject fail bad handler". |
| `README.md`   | MCP-style guide describing why the naive "xref to `Raycast` string" heuristic doesn't work (desc body is runtime-populated) and the correct static-analysis approach. |

**Workflow:** load a fresh `RobloxPlayerBeta.exe` IDB → run `raycast.py` → copy the two RVAs → paste into [`src/core/roblox/offsets/Offsets.h`](src/core/roblox/offsets/Offsets.h) → rebuild.

---

## Contributing

PRs and issues welcome. Keep the voice of the codebase — direct comments, no fluff, offsets in `Offsets.h` and nowhere else.

---

## License

Follows the license of the upstream repo. Attribution to **[awaky1337](https://github.com/awaky1337)** and **[MiladRx](https://github.com/MiladRx)** must be preserved in any redistribution.

<div align="center">

`v1.0.0` · maintained by [MiladRx](https://github.com/MiladRx) · fork of [awaky1337/jew-dick-hack](https://github.com/awaky1337/jew-dick-hack)

</div>
