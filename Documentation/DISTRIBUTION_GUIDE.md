# RayWaves Distribution Guide

This guide explains how to package the RayWaves engine so other developers can make games with it.

> **Not what you're looking for?** If you want to *export your game* to play on another computer, see the [Developer Guide](DEVELOPER_GUIDE.md). This guide is for distributing the *engine tools* themselves.

---

## Quick Start

### One-Command Script (Recommended)

```cmd
Distribution\create_distribution.bat -IncludeCompiler
```

Script bundles compiler, strips profiler, packages everything into `dist/`.

### Manual CMake Build (Equivalent)

```cmd
cmake --preset zig-release -DRAYWAVES_DISTRIBUTION_BUILD=ON
cmake --build build\zig-release --target main game GameLogic
Distribution\distribute.ps1 -IncludeCompiler
```

This is what the batch script does internally. Use this if you need full control.

> **Warning:** Always pass both `-IncludeCompiler` AND `-DRAYWAVES_DISTRIBUTION_BUILD=ON` for a proper distribution build. Omitting either produces a package with unnecessary profiler overhead or missing compiler, forcing recipients to install Zig manually.

---

## Distribution Build Internals

The script handles two critical optimizations automatically:

### 1. Profiler Stripped (`-DRAYWAVES_DISTRIBUTION_BUILD=ON`)

Normally the engine records per-frame timers for every system (Update, Draw, each editor panel) and displays a breakdown table in the Performance Overlay. This adds ~2-3 microseconds per frame.

`RAYWAVES_DISTRIBUTION_BUILD=ON` compiles all profiling code to no-ops:
- `SCOPED_TIMER` macro → `((void)0)` — zero instructions, zero allocations
- `Profiler::Record`, `NextFrame`, `GetAverages` → empty functions, inlined away
- `PerformanceOverlay` renders an empty breakdown table
- `Profiler.cpp` compiles to an empty translation unit (entire body wrapped in `#ifndef`)

The resulting binary has no profiler code, no ring buffer allocation, no timer syscalls. The distribution script sets this flag automatically.

### 2. Compiler Bundled (with `-IncludeCompiler`)

The Zig compiler is copied into the distribution so recipients can rebuild `GameLogic.dll` from source. Without this flag, they must install Zig manually.

---

## What's Inside the Box

| File/Folder | Purpose |
|---|---|
| `editor.exe` | Visual game editor |
| `GameLogic.dll` | Pre-compiled game code |
| `game_config.ini` | Default settings (resolution, VSync) |
| `build_gamelogic.bat` | One-click rebuild of game code |
| `GameLogic/` | Source files — edit these to change gameplay |
| `Engine/` | Header files for inheriting `GameMap` |
| `Assets/` | Textures, sounds, fonts |
| `zig/` | *(only with `-IncludeCompiler`)* Compiler for hot-reload |

---

## End-User Workflow

1. Unzip the folder
2. Open `GameLogic/Level1.cpp` in any editor
3. Run `editor.exe` — game runs immediately
4. Edit code (change jump speed, add logic)
5. Run `build_gamelogic.bat`
6. Editor hot-reloads changes in ~0.5 seconds

---

## Customizing the Distribution

Edit `Distribution/distribute.ps1` to tweak:

- **Include Zig Compiler:** `distribute.ps1 -IncludeCompiler` (same as the batch shortcut)
- **Default config:** Modify `Distribution/config.ini`
- **Extra assets:** Add files to `GameLogic/` before building
- **Branding:** Change icon or name of `editor.exe` in the script

To build manually with both distribution flags (profiler off + release config):

```cmd
cmake -B build\zig-release -DRAYWAVES_DISTRIBUTION_BUILD=ON
cmake --build build\zig-release
Distribution\distribute.ps1 -IncludeCompiler
```

`-DRAYWAVES_DISTRIBUTION_BUILD=ON` is a CMake option (default OFF). It sets the preprocessor define `RAYWAVES_PROFILER_DISABLED` globally across all targets (main.exe, game.exe, GameLogic.dll, tests.exe). Omitting it leaves the profiler active.

---

## Shipping Checklist

1.  **Test on a clean PC:** Run `dist/` on a computer without your dev environment
2.  **Verify hot-reload:** Edit a file in `GameLogic/`, run `build_gamelogic.bat`, confirm editor reloads
3.  **Verify profiler stripped:** Open Performance Overlay in editor — breakdown table shows no entries
4.  **IncludeCompiler was used:** Check that `zig/` folder exists in `dist/`
5.  **Docs shipped:** Confirm `README_DISTRIBUTION.md` covers the basics for end users

---

*Now go share your engine with the world!*
