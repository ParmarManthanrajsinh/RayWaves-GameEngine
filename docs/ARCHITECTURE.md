# RayWaves Architecture

## Repository layout

```text
RayWaves/
├─ CMakeLists.txt              # Thin entry point: options + module includes
├─ CMakePresets.json           # zig-debug / zig-release configure + test presets
├─ cmake/                      # Build modules (dependencies, targets, tests, packaging)
├─ Engine/                     # Core static library (libEngine.a)
│  ├─ GameMap, MapManager      # Scene/map lifecycle
│  ├─ Project, ProjectManager  # Project manifest + per-project CMake generation
│  ├─ Profiler, AssetResolver  # Diagnostics and asset path resolution
│  └─ WindowUtils, raygui      # Platform tweaks and GUI helper
├─ Editor/                     # RayWaves.exe (ImGui editor)
│  ├─ GameEditor.cpp           # Main editor loop + orchestration
│  ├─ Panels/                  # Self-registering editor panels
│  ├─ ExportService            # Game export pipeline (UI-free)
│  ├─ GameLogicLoader          # DLL lifecycle + ABI validation
│  └─ imgui/, rlImGui/         # Vendored UI dependencies
├─ Runtime/                    # Host executables
│  ├─ main.cpp                 # RayWaves.exe entry point
│  ├─ game.cpp                 # game.exe entry point
│  └─ DllLoader, PeCrtCheck    # Shadow-copy loading + CRT compatibility
├─ tests/
│  ├─ unit/                    # Fast, pure tests
│  ├─ integration/             # DLL/filesystem/project tests
│  ├─ smoke/                   # Hot-reload load/unload cycles
│  └─ support/                 # doctest main + TempDir helper
├─ benchmarks/                 # Standalone benchmark harness + cases
├─ templates/                  # Project templates (Empty, Platformer2D, SlimeQuest)
├─ packaging/                  # package.ps1 + Project.CMakeLists.in + config.ini
├─ scripts/                    # build/test/bench/package/check entry points
├─ docs/                       # Documentation index and guides
├─ third_party/doctest/        # Vendored test framework
├─ EngineContent/              # Runtime assets: fonts, icons, logo
└─ Tools/                      # Committed scripts + downloaded zig/ninja/cmake
```

`Tools/` binaries (Zig, Ninja, CMake, rcedit) are downloaded by
`Tools/setup_zig.ps1` and are not committed.

## Build map

| Module | Responsibility |
|---|---|
| `cmake/Dependencies.cmake` | FetchContent raylib, doctest target, `stage_raylib`, runtime asset copies |
| `cmake/Engine.cmake` | `Engine` static library |
| `cmake/GameLogic.cmake` | Bundled SlimeQuest `GameLogic.dll` |
| `cmake/Editor.cmake` | `RayWaves.exe` editor target |
| `cmake/Runtime.cmake` | `game.exe` runtime target |
| `cmake/Tests.cmake` | Enables CTest and adds `tests/` |
| `cmake/Benchmarks.cmake` | Adds `benchmarks/` when enabled |
| `cmake/Packaging.cmake` | Stages `Project.CMakeLists.in` and defines `export_package` |

## Project structure

A RayWaves project is created through the editor and lives in its own folder.

```text
MyNewGame/
├─ project.raywaves         # Manifest (name, version, scene settings)
├─ Assets/                  # Textures, audio, fonts (use AssetResolver)
├─ GameLogic/               # Your C++ gameplay code
│  ├─ RootManager.cpp       # Registers maps with RegisterMap<T>()
│  └─ ...                   # Any number of maps, entities, systems
├─ .raywaves/               # Editor cache (auto-generated)
│  ├─ CMakeLists.txt        # Generated from packaging/Project.CMakeLists.in
│  ├─ build/                # Ninja build output
│  └─ shadows/              # DLL shadow copies for hot-reload
└─ GameLogic.dll            # Built output (hot-reloaded at runtime)
```

Rules:

- `project.raywaves` always sits at the project root.
- `GameLogic/` contains gameplay code. Do not put gameplay in `Engine/`.
- `.raywaves/` is managed by the editor; do not edit it manually.

## Dependency flow

```text
Runtime / Editor  ->  Engine  ->  raylib
GameLogic.dll     ->  Engine headers  ->  raylib (import library)
```

`Engine` is a static library. `GameLogic` is a DLL so it can be reloaded while
the editor or runtime keeps running.
