# RayWaves Distribution Guide

*For engine maintainers — how to package the engine for other developers.*

> **End-user looking to make a game?** See [GAME_DEVELOPER_GUIDE.md](GAME_DEVELOPER_GUIDE.md).

---

## Quick Start

### One-Command Script (Recommended)

```cmd
Distribution\create_distribution.bat -IncludeCompiler
```

Bundles compiler + build tools (Zig, Ninja, CMake), strips profiler, packages everything into `dist/`.

### Manual CMake Build (Equivalent)

```cmd
cmake --preset zig-release -DRAYWAVES_DISTRIBUTION_BUILD=ON
cmake --build build\zig-release --target main game GameLogic
Distribution\distribute.ps1 -IncludeCompiler
```

> **Warning:** Always pass `-IncludeCompiler` AND `-DRAYWAVES_DISTRIBUTION_BUILD=ON` for a proper distribution. Omitting either produces a package with profiler overhead or missing compiler/build tools.

---

## Distribution Build Internals

### 1. Profiler Stripped (`-DRAYWAVES_DISTRIBUTION_BUILD=ON`)

With this flag:
- `SCOPED_TIMER` macro → `((void)0)` — zero runtime cost
- `Profiler::Record`, `NextFrame`, `GetAverages` → empty, inlined away
- `PerformanceOverlay` renders an empty breakdown
- `Profiler.cpp` compiles to an empty translation unit

### 2. Build Tools Bundled (with `-IncludeCompiler`)

The distribution script copies these into `dist/Core/Tools/`:

| Tool | Location | Purpose |
|------|----------|---------|
| Zig | `Tools/zig/` | C++ compiler for GameLogic |
| Ninja | `Tools/ninja/ninja.exe` | Build system (CMake generator) |
| CMake | `Tools/cmake/` | Build system orchestrator (includes required `share/` modules) |
| rcedit | `Tools/rcedit.exe` | Icon embedding (used during engine builds, not by end users) |

The `setup_zig.ps1` script and compiler wrappers (`zig-cc.bat`, `zig-cxx.bat`) are also copied so that auto-fetch works on first run if any tool is missing. Recipients get zero-install compilation — no PATH setup required.

---

## What's Inside the Box

| File/Folder | Purpose |
|---|---|
| `RayWaves.exe` | Visual game editor |
| `Core/runtime.exe` | Standalone game player (used by Export) |
| `Core/Engine/` | Header files for inheriting `GameMap` |
| `Core/raylib/` | Raylib dev files (headers, libs, DLL) |
| `Core/CMakeLists.txt` | CMake config for building GameLogic |
| `Core/Tools/zig/` | Zig compiler (zero-install) |
| `Core/Tools/ninja/` | Ninja build system |
| `Core/Tools/cmake/` | CMake build system |
| `Core/Tools/setup_zig.ps1` | Auto-fetch script for tool updates |
| `Core/Tools/zig-cc.bat` | Compiler wrapper scripts |
| `GameLogic.dll` | Pre-compiled game code |
| `build_gamelogic.bat` | One-click rebuild script |
| `Templates/` | Project templates (Empty, DemoGame) |
| `config.ini` | Default game settings |
| `Documentation/` | User guides and API reference |

See [Distribution/README.md](../Distribution/README.md) (stub redirect) for the raw script inventory.

---

## End-User Workflow

1. Unzip the distribution.
2. Run `RayWaves.exe`.
3. Create a new project or open an existing one via the Project Browser.
4. Edit code in your project's `GameLogic/` folder.
5. Click **Compile** in the editor toolbar — changes hot-reload in ~0.5 s.
6. Export your game via the **Export Panel**.

---

## Customizing the Distribution

Edit `Distribution/distribute.ps1` to tweak:

- **Bundled tools:** `-IncludeCompiler` includes Zig, Ninja, CMake (recommended).
- **Default config:** Modify `Distribution/config.ini`.
- **Branding:** Change icon or name in the script.

Build manually:
```cmd
cmake -B build\zig-release -DRAYWAVES_DISTRIBUTION_BUILD=ON
cmake --build build\zig-release
Distribution\distribute.ps1 -IncludeCompiler
```

---

## Shipping Checklist

1.  **Test on a clean PC:** Run `dist/` on a computer without any dev tools.
2.  **Verify hot-reload:** Edit a file in your project's `GameLogic/`, click Compile, confirm editor reloads.
3.  **Verify profiler stripped:** Open Performance Overlay — breakdown table shows no entries.
4.  **Tools bundled:** Confirm `dist/Core/Tools/{zig,ninja,cmake}` exist.
5.  **Docs shipped:** Confirm `GAME_DEVELOPER_GUIDE.md` is included.

---

*Now go share your engine with the world!*
