# RayWaves Architecture

## Repository layout

```text
RayWaves/
├─ CMakeLists.txt              # Thin entry point: options + module includes
├─ CMakePresets.json           # zig-debug / zig-release configure + test presets
├─ cmake/                      # Build modules (dependencies, targets, tests, packaging)
├─ Engine/                     # Core static library (libEngine.a), version in EngineVersion.h
│  ├─ GameMap, MapManager      # Scene/map lifecycle
│  ├─ Project, ProjectManager  # Project manifest + per-project CMake generation
│  ├─ Profiler, AssetResolver  # Diagnostics and asset path resolution
│  └─ WindowUtils, raygui      # Platform tweaks and GUI helper
├─ Editor/                     # RayWaves.exe (ImGui editor)
│  ├─ GameEditor.cpp           # Main editor loop + orchestration
│  ├─ Panels/                  # Self-registering editor panels
│  ├─ EditorAssets             # Flat asset listing/import helpers
│  ├─ terminal/                # Console log tab + ConPTY shell tabs (libvterm)
│  ├─ ExportService            # Game export pipeline (UI-free)
│  ├─ GameLogicLoader          # DLL lifecycle + ABI validation
│  └─ imgui/, rlImGui/         # Vendored UI dependencies
├─ Runtime/                    # Host executables + shared host support
│  ├─ main.cpp                 # RayWaves.exe entry point
│  ├─ game.cpp                 # game.exe entry point
│  ├─ GameLogicHost            # Shared DLL load/ABI/attach path
│  ├─ ProcessRunner, AgentLock # Build process + cross-process build lock
│  ├─ PlatformInfo             # Win32 helpers without raylib header clashes
│  └─ DllLoader, PeCrtCheck    # Shadow-copy loading + CRT compatibility
├─ Cli/                        # rwcli.exe (agent CLI: build/run/scene/assets/lsp)
├─ tests/
│  ├─ unit/                    # Fast, pure tests
│  ├─ integration/             # DLL/filesystem/project tests
│  ├─ smoke/                   # Hot-reload load/unload cycles
│  └─ support/                 # doctest main + TempDir helper
├─ benchmarks/                 # Standalone benchmark harness + cases
├─ templates/                  # Project templates (Simple2D, Platformer2D, ThirdPerson3D, SlimeQuest)
├─ packaging/                  # package.ps1 + Project.CMakeLists.in + config.ini
├─ scripts/                    # build/test/bench/package/check entry points
├─ docs/                       # Documentation index and guides
├─ third_party/doctest/        # Vendored test framework
├─ third_party/libvterm/       # Vendored VT parser/screen (MIT)
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
| `cmake/RuntimeSupport.cmake` | Shared host support: DLL loading, process runner, agent lock, game logic host |
| `cmake/Editor.cmake` | `RayWaves.exe` editor target |
| `cmake/Runtime.cmake` | `game.exe` runtime target |
| `cmake/Cli.cmake` | `rwcli.exe` agent CLI target |
| `cmake/Tests.cmake` | Enables CTest and adds `tests/` |
| `cmake/Benchmarks.cmake` | Adds `benchmarks/` when enabled |
| `cmake/Packaging.cmake` | Stages `Project.CMakeLists.in` and defines `export_package` |

## Project structure

A RayWaves project is created through the editor and lives in its own folder.

```text
MyNewGame/
├─ project.raywaves         # Manifest (name, version, scene settings)
├─ Assets/                  # Flat: textures, audio, fonts, <MapId>.scene
├─ GameLogic/               # Your C++ gameplay code
│  ├─ RootManager.cpp       # Registers maps with RegisterMap<T>()
│  └─ ...                   # Any number of maps, entities, systems
├─ .raywaves/               # Editor/CLI cache (auto-generated)
│  ├─ CMakeLists.txt        # Generated from packaging/Project.CMakeLists.in
│  ├─ build/                # Editor Ninja build output
│  ├─ build-agent/          # rwcli build output
│  ├─ build-ide/            # compile_commands.json for clangd
│  ├─ tooling.ini           # package rwcli path for agents
│  ├─ bin/rwcli.cmd         # project-local agent CLI wrapper
│  ├─ agent.lock            # Cross-process build lock
│  └─ shadows/              # DLL shadow copies for hot-reload
├─ .agents/skills/          # Generated project skill for coding agents
├─ .clangd, .vscode/        # Generated editor/LSP configs
└─ AGENTS.md                # Generated project agent guide
└─ GameLogic.dll            # Built output (hot-reloaded at runtime)
```

Rules:

- `project.raywaves` always sits at the project root.
- `GameLogic/` contains gameplay code. Do not put gameplay in `Engine/`.
- `Assets/` is flat by design. New imports land in the root; level data
  (`<MapId>.scene`) lives next to the textures it references. The FileSystem
  panel groups files virtually.
- `.raywaves/` is managed by the editor; do not edit it manually.

## Dependency flow

```text
Runtime / Editor / rwcli  ->  RuntimeSupport  ->  Engine  ->  raylib
GameLogic.dll             ->  Engine headers  ->  raylib (import library)
```

`Engine` is a static library. `GameLogic` is a DLL so it can be reloaded while
the editor or runtime keeps running.
