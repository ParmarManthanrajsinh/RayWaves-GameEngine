# Skill: raywaves-engine

# RayWaves Engine

A code-first game engine built on Raylib, with an ImGui editor and 2D/3D
project templates. C++23, CMake, Ninja. Windows builds with the pinned Zig
toolchain; Linux builds with system GCC/G++.

## Core Structure

1. **Engine**: core systems (`GameEngine`, `MapManager`, `ProjectManager`, `Profiler`,
   `AssetResolver`) plus host services (`PlatformInfo`, `PlatformNames`). Static
   library, target `Engine`, sources in `Engine/`.
2. **Runtime**: host executables, module loading, and process/agent-lock support.
   `Runtime/main.cpp` builds the editor, `Runtime/game.cpp` builds the standalone
   runtime. Platform implementations live in `Runtime/platform/{Win32,Posix}/`.
3. **Editor**: ImGui editor, output `RayWaves` (`RayWaves.exe` on Windows). Sources
   in `Editor/`. Panels live in `Editor/Panels/` and implement `IEditorPanel`.
   Platform implementations live in `Editor/platform/{Win32,Posix}/`.
4. **Third party**: vendored code. `third_party/doctest/`, `Editor/imgui/`,
   `Editor/rlImGui/`, `Editor/tinyfiledialogs/`, `Engine/raygui.h`.
5. **GameLogic**: gameplay code compiled as a shared module (`GameLogic.dll` on
   Windows, `GameLogic.so` on Linux) for hot-reloading. Built from
   `templates/<Template>/GameLogic/` in the engine repo; lives in the user's
   `GameLogic/` folder inside their project.

## Architecture Rules

- Gameplay code belongs in the project's `GameLogic` module, never in `Engine/`.
- Do not mix engine systems with game mechanics (player movement, enemies, slimes).
- GameLogic is a shared library so it can reload while the editor/runtime runs.
- `Engine` is a static library; editor, runtime, and GameLogic link it.
- Editor UI: create or extend a panel in `Editor/Panels/`. Do not bloat
  `GameEditor.cpp`.
- Projects are portable. Engine paths and raylib paths are resolved at generation
  time by `ProjectManager`, using `packaging/Project.CMakeLists.in`.
- Shared code must not hardcode platform names or paths. Use
  `Engine/PlatformNames.h` for executable/module/library suffixes and
  `Engine/PlatformInfo.*` for executable path, pid, config dir, environment, and
  atomic file creation.
- OS-specific code belongs in `Runtime/platform/{Win32,Posix}/` or
  `Editor/platform/{Win32,Posix}/`, behind a shared header. Keep `<windows.h>`
  out of shared code; it collides with raylib symbols.
- The project manifest stores `entryDll=GameLogic`, a logical stem. The host
  appends `.dll` or `.so` at load time.

## Repository Map

```text
Engine/        core library + PlatformInfo/PlatformNames
Editor/        ImGui editor (RayWaves / RayWaves.exe)
Editor/platform/{Win32,Posix}/   shell open, file association, PTY
Runtime/       entry points + host support (DllLoader, GameLogicHost, locks)
Runtime/platform/{Win32,Posix}/  module loader, process runner, PE CRT scan
Cli/           rwcli / rwcli.exe (agent build/run/scene CLI)
tests/         unit, integration, smoke, support
benchmarks/    standalone benchmark harness
templates/     Simple2D, Platformer2D, ThirdPerson3D, SlimeQuest
packaging/     package.ps1 + package.sh + Project.CMakeLists.in + config.ini
scripts/       build, test, bench, package, check (PS1 + sh)
cmake/         Toolchain, Platform, and target modules
docs/          architecture, linux, testing, benchmarks, distribution
third_party/   vendored dependencies
Tools/         setup scripts + downloaded zig/ninja/cmake + compiler wrappers
.github/workflows/  Windows + Linux CI and packaging
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

Formatting and lint (Windows):

```powershell
Tools/run_analysis.bat format-check
Tools/run_analysis.bat format
Tools/run_analysis.bat tidy
scripts/check.ps1
```

Formatting and lint (Linux):

```sh
clang-format -i <files>            # format; use the repo .clang-format
clang-format --dry-run --Werror --style=file <files>
./scripts/check.sh                 # format check + docs check + tests
```

Headers include only what they use. Put `<iostream>`, `<fstream>`, `<filesystem>` in
the `.cpp`, not the header.

## Build System Rules

- CMake with explicit source lists. No `file(GLOB)` in engine modules.
- Adding/removing files: update the corresponding module in `cmake/`:
  `Engine.cmake`, `Editor.cmake`, `Runtime.cmake`, `Cli.cmake`, `Tests.cmake`, or
  `Benchmarks.cmake`.
- `cmake/Toolchain.cmake` selects the compiler before `project()`: Zig on
  Windows, system GCC/G++ on Linux. `cmake/Platform.cmake` defines platform facts
  and flags after `project()`.
- Keep the root `CMakeLists.txt` thin; targets belong in `cmake/*.cmake`.
- Dependencies: prefer native OS APIs (for example `dwmapi` on Windows,
  `posix_openpt` on Linux) and raylib. Avoid large new C++ libraries.
- `Engine` publicly exposes its headers and raylib.
- The generated project template intentionally uses `file(GLOB_RECURSE)` to pick up
  the user's `GameLogic/*.cpp`; do not copy that pattern into engine modules.
- Build outputs: `RayWaves` / `RayWaves.exe`, `game` / `game.exe`, `rwcli` /
  `rwcli.exe`, `GameLogic.so` / `GameLogic.dll`.

## Testing and Benchmarks

- doctest + CTest. Three suites:
  - `tests/unit/` — fast, pure engine logic.
  - `tests/integration/` — module builds, filesystem, project/export helpers.
  - `tests/smoke/` — hot-reload load/unload cycles.
- Add new test files to `tests/CMakeLists.txt`.
- Use `tests/support/TempDir.h` for filesystem tests.
- Benchmarks are a separate executable, never part of CTest:
  `benchmarks/`, `scripts/bench.ps1` / `scripts/bench.sh`.
- Performance cases use `BENCHMARK_CASE(name, ops_per_call)`; no timing assertions
  in unit tests.

```powershell
scripts/test.ps1
scripts/test.ps1 -Suite unit
scripts/bench.ps1
ctest --preset zig-debug
```

```sh
./scripts/test.sh
./scripts/test.sh --suite unit
./scripts/bench.sh
ctest --preset zig-debug
xvfb-run -a ./scripts/test.sh      # headless Linux / CI
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
- `PlatformInfo` / `PlatformNames`: host paths, pid, config dir, environment,
  compile-time suffixes, module names.
- `DllLoader`: shadow-copy load/unload. `LoadLibrary` + `FreeLibrary` on Windows,
  `dlopen(RTLD_NOW | RTLD_LOCAL)` + `dlclose` on Linux.
- `ProcessRunner`: argv-based child processes. Callers pass argument vectors; do
  not build shell strings with `cd /d` or `cmd.exe`.
- `AgentLock`: cross-process build lock in `.raywaves/agent.lock`.

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
- Changing `GameMap`, `EditorEntity`, or another module-facing layout requires bumping
  `RAYWAVES_GAMELOGIC_ABI_VERSION` in `Engine/GameMap.h`.

## Editor

- Located in `Editor/`. Uses ImGui and `rlImGui`.
- Core panels: `FileSystemPanel`, `SceneHierarchyPanel`, `InspectorPanel`,
  `SceneWindow`, `ExportPanel`, `SceneSettingsPanel`, `PerformanceOverlay`,
  `MessageLogPanel`, `TerminalPanel`, `EditorPreferencesPanel`, `MainMenuBar`.
- Add extension panels under `Editor/Panels/`, implement `IEditorPanel`, and self-register
  with `b_RegisterPanel<T>()`. Core panels are listed in `s_CorePanelFactories()` in
  `GameEditor.cpp`; do not grow that file with panel logic.
- Terminal: ConPTY on Windows, `posix_openpt` on Linux. Keep PTY backends in
  `Editor/platform/`; shell detection lives in `Editor/terminal/shell_detect.*`.
- File dialogs use `tinyfiledialogs`; Linux needs `zenity` or `kdialog`.
- File association: registry on Windows, `~/.local/share/applications/raywaves.desktop`
  plus MIME registration on Linux.

## Build and Run

Windows:

```powershell
cmake --preset zig-release
cmake --build build/zig-release
build/zig-release/RayWaves.exe
build/zig-release/game.exe
build/zig-release/rwcli.exe run <projectDir> --frames 60 --json
```

Linux:

```sh
cmake --preset zig-release
cmake --build build/zig-release
./build/zig-release/RayWaves
./build/zig-release/game
./build/zig-release/rwcli run <projectDir> --frames 60 --json
```

`rwcli` is the agent-facing console host. Use it for existing projects:
`doctor`, `info`, `maps`, `validate`, `build`, `run`, `scene`, `assets`, `lsp`.
`rwcli lsp <project> --check` validates clangd diagnostics.
Details: `docs/agent/README.md`.

Toolchain: `Tools/setup_tools.ps1` (Windows) and `Tools/setup_tools.sh` (Linux)
fetch missing tools into `Tools/`. Windows also has `Tools/setup_zig.ps1`;
`zig-cc.bat` / `zig-cxx.bat` wrap the pinned compiler. Linux uses the system
GCC/G++ and does not use the Zig wrappers.

## Linux Notes

- Linux requires GCC 13+/G++ 13+, CMake, Ninja, and X11/OpenGL/ALSA development
  headers. Install packages from `docs/linux.md`.
- raylib uses the GLFW X11 backend. Wayland desktops run through XWayland; native
  Wayland is out of scope.
- Headless agents/CI need an X server: `xvfb-run -a <command>`. Use
  `LIBGL_ALWAYS_SOFTWARE=1` when no GPU driver is available.
- Linux uses the system `libstdc++.so.6` and compiles with `-fno-gnu-unique` so
  glibc does not mark GameLogic modules `NODELETE`; that is what makes `dlclose`
  unload them. Do not switch Linux to Zig or static libc++.
- Packaged Linux binaries use `$ORIGIN` RPATHs. Keep new targets RPATH-relative
  through `raywaves_set_origin_rpath` in `cmake/Platform.cmake`.
- `--include-compiler` has no effect on Linux; packages use system GCC/G++.
- Linux is x86_64 only. See `docs/linux.md`, ADR 0008, and ADR 0009.

## Packaging

- Entry points: `scripts/package.ps1` (wraps `packaging/package.ps1`) and
  `scripts/package.sh` (wraps `packaging/package.sh`).
- Project build recipe: `packaging/Project.CMakeLists.in`. Do not add a second
  project CMake recipe.
- Windows: `package.ps1 -IncludeCompiler` bundles Zig for zero-install
  hot-reloading; `-Zip` writes `dist/RayWaves-<version>-win64.zip` plus SHA256.
- Linux: `package.sh --archive` writes
  `dist/RayWaves-<version>-linux-x86_64.tar.gz` plus SHA256, preserving executable
  bits and `$ORIGIN` RPATHs.

```powershell
scripts/package.ps1 -IncludeCompiler -Zip
```

```sh
./scripts/package.sh --archive
```

## Definition of Done

- Build passes for `zig-debug` and `zig-release` on the host OS.
- `scripts/check.ps1` (Windows) or `./scripts/check.sh` (Linux) passes; both
  include the format, docs, and test gates when their tools are available.
- CI matrix in `.github/workflows/ci.yml` covers Windows and Linux; do not add a
  Windows-only step without a Linux equivalent.
- New behavior has a test; new hot path has a benchmark.
- Docs updated when paths, commands, or architecture change. Linux changes also
  update `docs/linux.md`, ADR 0008/0009, and the relevant `.sh` script.
- All templates in `templates/` compile after `GameMap` / ABI changes.
- Editor window IDs stay stable (`###`) unless the layout migration is included.
- No gameplay code in `Engine/`; no new globbing in engine CMake.
- No hardcoded `.exe`, `.dll`, `.so`, path separators, or `cmd.exe` in shared code;
  use `PlatformNames`/`PlatformInfo` or a platform directory.
