---
name: raywaves-engine
description: Guide for working with the RayWaves Game Engine architecture and coding standard. Use when writing game logic, engine features, editor panels, 2D/3D templates, tests, benchmarks, or packaging.
---

# RayWaves Engine

A code-first game engine built on Raylib, with an ImGui editor and 2D/3D
project templates. C++23, CMake, Ninja, Zig.

## Core Structure

1. **Engine**: core systems (`GameEngine`, `MapManager`, `ProjectManager`, `Profiler`,
   `AssetResolver`). Static library, target `Engine`, sources in `Engine/`.
2. **Runtime**: host executables and DLL loading. `Runtime/main.cpp` builds the editor,
   `Runtime/game.cpp` builds the standalone runtime. `DllLoader` and `PeCrtCheck` live here.
3. **Editor**: ImGui editor, output `RayWaves.exe`. Sources in `Editor/`. Panels live in
   `Editor/Panels/` and implement `IEditorPanel`.
4. **Third party**: vendored code. `third_party/doctest/`, `Editor/imgui/`,
   `Editor/rlImGui/`, `Editor/tinyfiledialogs/`, `Engine/raygui.h`.
5. **GameLogic**: gameplay code compiled as a DLL for hot-reloading. Built from
   `templates/<Template>/GameLogic/` in the engine repo; lives in the user's
   `GameLogic/` folder inside their project.

## Architecture Rules

- Gameplay code belongs in the project's `GameLogic` DLL, never in `Engine/`.
- Do not mix engine systems with game mechanics (player movement, enemies, slimes).
- GameLogic is a shared library so it can reload while the editor/runtime runs.
- `Engine` is a static library; editor, runtime, and GameLogic link it.
- Editor UI: create or extend a panel in `Editor/Panels/`. Do not bloat
  `GameEditor.cpp`.
- Projects are portable. Engine paths and raylib paths are resolved at generation
  time by `ProjectManager`, using `packaging/Project.CMakeLists.in`.

## Repository Map

```text
Engine/        core library
Editor/        ImGui editor (RayWaves.exe)
Runtime/       entry points + shared host support (DllLoader, GameLogicHost, locks)
Cli/           rwcli.exe (agent build/run/scene CLI)
tests/         unit, integration, smoke, support
benchmarks/    standalone benchmark harness
templates/     Empty, Platformer2D, ThirdPerson3D, FirstPerson3D, SlimeQuest
packaging/     package.ps1 + Project.CMakeLists.in + config.ini
scripts/       build, test, bench, package, check
cmake/         build modules (Engine, Editor, Runtime, Tests, ...)
docs/          architecture, testing, benchmarks, distribution
third_party/   vendored dependencies
Tools/         toolchain scripts + downloaded zig/ninja/cmake
```

## Coding Standard

Variables: `snake_case`. Functions and types: `PascalCase`. Tabs for indentation.
Allman braces. Comments explain Why, not What, and only when needed.

| Prefix | Meaning | Example |
|---|---|---|
| `b_` | bool variable or bool-returning function | `b_IsPlaying`, `b_LoadGameLogic()` |
| `m_` | member variable | `m_GameEngine`, `m_FrameTimes` |
| `m_t` | member struct instance | `m_SceneSettings`, `m_ExportState` |
| `s_` | file-local static / static function | `s_LayoutPath`, `s_fDrawSpinner()` |
| `t_` | type / struct name | `t_WindowConfig`, `t_MapInfo` |
| `F` | editor struct type | `FThemePreset`, `FBuildMessage` |
| `E` | enum type | `EBuildStatus`, `EEditorFont` |
| `c_` / `k_` | compile-time constant | `c_NUM_SEGMENTS`, `k_FrameCount` |

Formatting and lint:

```powershell
Tools/run_analysis.bat format-check
Tools/run_analysis.bat format
Tools/run_analysis.bat tidy
scripts/check.ps1
```

Headers include only what they use. Put `<iostream>`, `<fstream>`, `<filesystem>` in
the `.cpp`, not the header.

## Build System Rules

- CMake with explicit source lists. No `file(GLOB)` in engine modules.
- Adding/removing files: update the corresponding module in `cmake/`:
  `Engine.cmake`, `Editor.cmake`, `Runtime.cmake`, `Tests.cmake`, or
  `Benchmarks.cmake`.
- Keep the root `CMakeLists.txt` thin; targets belong in `cmake/*.cmake`.
- Dependencies: prefer native OS APIs (for example `dwmapi` on Windows) and raylib.
  Avoid large new C++ libraries.
- `Engine` publicly exposes its headers and raylib.
- The generated project template intentionally uses `file(GLOB_RECURSE)` to pick up
  the user's `GameLogic/*.cpp`; do not copy that pattern into engine modules.

## Testing and Benchmarks

- doctest + CTest. Three suites:
  - `tests/unit/` — fast, pure engine logic.
  - `tests/integration/` — DLL builds, filesystem, project/export helpers.
  - `tests/smoke/` — hot-reload load/unload cycles.
- Add new test files to `tests/CMakeLists.txt`.
- Use `tests/support/TempDir.h` for filesystem tests.
- Benchmarks are a separate executable, never part of CTest:
  `benchmarks/`, `scripts/bench.ps1`.
- Performance cases use `BENCHMARK_CASE(name, ops_per_call)`; no timing assertions
  in unit tests.

```powershell
scripts/test.ps1
scripts/test.ps1 -Suite unit
scripts/bench.ps1
ctest --preset zig-debug
```

## Common Systems

- `GameMap`: map lifecycle plus 2D/3D view hooks (`GetViewCamera`, `GetMutableViewCamera`,
  `b_Uses3DView`, `GetViewCamera3D`) and editor entities (`RegisterEditorEntity`,
  `ClearEditorEntities`).
- `MapManager`: global map registration, transitions, standalone/3D mode forwarding.
- `Scene`: editor-authored sprite layer, saved as `Assets/<map>.scene`.
- `EditorEntity`: live gameplay object exposed to the editor through stable pointers.
- `ProjectManager`: project manifest, recent projects, per-project CMake generation.
- `AssetResolver`: relative-to-absolute asset paths inside the project.
- `Profiler`: scoped timers, frame timings, overlay data.
- `GameConfig`: window and exported-game configuration.

## Editor Integration

- 2D maps override `GetViewCamera()` and `GetMutableViewCamera()`; render through that
  camera so editor pan/zoom and gizmos line up.
- 3D maps override `b_Uses3DView()` and `GetViewCamera3D()` and return the gameplay camera.
- Call `ClearEditorEntities()` before rebuilding gameplay objects, then
  `RegisterEditorEntity(...)` with stable storage. Use `m_Position3D` / `m_Size3D` for 3D.
- The editor clears cached entity pointers before map resets, reloads, and switches.
  Never keep editor entity pointers across frames.
- Scene objects get a 2D move/scale gizmo. Selected 3D gameplay entities get a world-space
  translate gizmo.
- Changing `GameMap`, `EditorEntity`, or another DLL-facing layout requires bumping
  `RAYWAVES_GAMELOGIC_ABI_VERSION` in `Engine/GameMap.h`.

## Editor

- Located in `Editor/`. Uses ImGui and `rlImGui`.
- Core panels: `FileSystemPanel`, `SceneHierarchyPanel`, `InspectorPanel`,
  `SceneWindow`, `ExportPanel`, `SceneSettingsPanel`, `PerformanceOverlay`,
  `MessageLogPanel`, `TerminalPanel`, `EditorPreferencesPanel`, `MainMenuBar`.
- Add extension panels under `Editor/Panels/`, implement `IEditorPanel`, and self-register
  with `b_RegisterPanel<T>()`. Core panels are listed in `s_CorePanelFactories()` in
  `GameEditor.cpp`; do not grow that file with panel logic.

## Build and Run

```powershell
cmake --preset zig-release
cmake --build build/zig-release
build/zig-release/RayWaves.exe
build/zig-release/game.exe
build/zig-release/rwcli.exe run <projectDir> --frames 60 --json
```

`rwcli` is the agent-facing console host. Use it for existing projects:
`doctor`, `info`, `maps`, `validate`, `build`, `run`, `scene`, `assets`, `lsp`.
`rwcli lsp <project> --check` validates clangd diagnostics.
Details: `docs/agent/README.md`.

Setup script: `Tools/setup_zig.ps1`. Toolchain is bundled under `Tools/` after first
configure. `zig-cc.bat` / `zig-cxx.bat` wrap the compiler.

## Packaging

- Entry point: `scripts/package.ps1` (wraps `packaging/package.ps1`).
- Project build recipe: `packaging/Project.CMakeLists.in`. Do not add a second
  project CMake recipe.
- `package.ps1 -IncludeCompiler` bundles Zig for zero-install hot-reloading.
- `-Zip` writes `dist/RayWaves-<version>-win64.zip` plus SHA256.

```powershell
scripts/package.ps1 -IncludeCompiler -Zip
```

## Definition of Done

- Build passes for `zig-debug` and `zig-release`.
- `scripts/test.ps1` passes.
- New behavior has a test; new hot path has a benchmark.
- Docs updated when paths, commands, or architecture change.
- All templates in `templates/` compile after `GameMap` / ABI changes.
- Editor window IDs stay stable (`###`) unless the layout migration is included.
- No gameplay code in `Engine/`; no new globbing in engine CMake.
