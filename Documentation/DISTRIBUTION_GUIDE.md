# RayWaves Distribution Guide

*For engine maintainers — how to package the engine for other developers.*

> **End-user looking to make a game?** See [GAME_DEVELOPER_GUIDE.md](GAME_DEVELOPER_GUIDE.md).

---

## Quick Start

### One-Command Script (Recommended)

```bash
make dist
```

Equivalent:

```bash
Distribution/distribute.sh -BuildConfig Release -OutputDir dist
```

Strips the profiler and packages everything into `dist/`. The distribution bundles no compiler — game builds on the recipient's machine need system `cmake`, `ninja`, and `g++`/`clang++` on `PATH`.

### Manual CMake Build (Equivalent)

```bash
cmake --preset linux-release -DRAYWAVES_DISTRIBUTION_BUILD=ON
cmake --build build/linux-release --target main game GameLogic
Distribution/distribute.sh -BuildConfig Release -OutputDir dist
```

> **Warning:** Always pass `-DRAYWAVES_DISTRIBUTION_BUILD=ON` for a proper distribution. Omitting it produces a package with profiler overhead. Recipients still need system `cmake`, `ninja`, and `g++`/`clang++` on `PATH` to build games.

---

## Distribution Build Internals

### 1. Profiler Stripped (`-DRAYWAVES_DISTRIBUTION_BUILD=ON`)

With this flag:
- `SCOPED_TIMER` macro → `((void)0)` — zero runtime cost
- `Profiler::Record`, `NextFrame`, `GetAverages` → empty, inlined away
- `PerformanceOverlay` renders an empty breakdown
- `Profiler.cpp` compiles to an empty translation unit

### 2. Toolchain (System, Not Bundled)

The distribution ships no compiler and no build tools. The editor's in-app Compile runs system `cmake` from `PATH` (`cd <project>/.raywaves && cmake -G Ninja . -B build ...`), and packaging itself uses system CMake + Ninja. Recipients need `cmake`, `ninja`, and `g++`/`clang++` installed to build GameLogic.

---

## What's Inside the Box

| File/Folder | Purpose |
|---|---|
| `RayWaves` | Visual game editor |
| `Core/runtime` | Standalone game player (used by Export) |
| `Core/Engine/` | Header files for inheriting `GameMap` |
| `Core/raylib/{include,lib,bin}` | Raylib dev files (headers, libs, `libraylib.so`) |
| `libraylib.so*` | Shared library chain at the root and in `Core/` |
| `Core/CMakeLists.txt` | CMake config for building GameLogic (from `dist_CMakeLists.txt`) |
| `Core/EngineContent/` | Engine fonts and `icon.png` |
| `Templates/` | Project templates (Empty, DemoGame) |
| `config.ini` | Default game settings |
| `Documentation/` | User guides and API reference |

See [Distribution/README.md](../Distribution/README.md) (stub redirect) for the raw script inventory.

---

## End-User Workflow

1. Unzip the distribution.
2. Run `./RayWaves`.
3. Create a new project or open an existing one via the Project Browser.
4. Edit code in your project's `GameLogic/` folder.
5. Click **Compile** in the editor toolbar — changes hot-reload in ~0.5 s.
6. Export your game via the **Export Panel**.

---

## Customizing the Distribution

Edit `Distribution/distribute.sh` to tweak:

- **Toolchain:** nothing is bundled — recipients need system `cmake`, `ninja`, and `g++`/`clang++` on `PATH`.
- **Default config:** Modify `Distribution/config.ini`.
- **Branding:** Change icon or name in the script.

Build manually:
```bash
cmake --preset linux-release -DRAYWAVES_DISTRIBUTION_BUILD=ON
cmake --build build/linux-release
Distribution/distribute.sh -BuildConfig Release -OutputDir dist
```

---

## Shipping Checklist

1.  **Test on a clean machine:** Run `dist/` on a computer without RayWaves installed (system `cmake`, `ninja`, `g++`/`clang++` still required for game builds).
2.  **Verify hot-reload:** Edit a file in your project's `GameLogic/`, click Compile, confirm editor reloads.
3.  **Verify profiler stripped:** Open Performance Overlay — breakdown table shows no entries.
4.  **Toolchain present:** Confirm `cmake --version`, `ninja --version`, and `g++ --version` (or `clang++ --version`) work on `PATH`.
5.  **Docs shipped:** Confirm `GAME_DEVELOPER_GUIDE.md` is included.

---

*Now go share your engine with the world!*
