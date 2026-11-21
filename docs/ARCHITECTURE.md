# RayWaves Architecture

## Repository layout

```text
RayWaves/
├─ CMakeLists.txt              # Thin entry point: options + module includes
├─ CMakePresets.json           # zig-debug / zig-release names on both OSes
├─ cmake/                      # Build modules (toolchain, platform, targets, tests, packaging)
├─ Engine/                     # Core static library (libEngine.a), version in EngineVersion.h
│  ├─ GameMap, MapManager      # Scene/map lifecycle
│  ├─ Project, ProjectManager  # Project manifest + per-project CMake generation
│  ├─ Profiler, AssetResolver  # Diagnostics and asset path resolution
│  ├─ PlatformInfo             # Exe path, pid, config dir, env, module names
│  └─ WindowUtils, raygui      # Platform tweaks and GUI helper
├─ Editor/                     # RayWaves / RayWaves.exe (ImGui editor)
│  ├─ GameEditor.cpp           # Main editor loop + orchestration
│  ├─ Panels/                  # Self-registering editor panels
│  ├─ EditorAssets             # Flat asset listing/import helpers
│  ├─ terminal/                # Console log tab + PTY shell tabs (libvterm)
│  ├─ platform/{Win32,Posix}   # xdg-open/ShellExecute, file association, PTY backend
│  ├─ ExportService            # Game export pipeline (UI-free)
│  ├─ GameLogicLoader          # DLL lifecycle + ABI validation
│  └─ imgui/, rlImGui/         # Vendored UI dependencies
├─ Runtime/                    # Host executables + shared host support
│  ├─ main.cpp                 # RayWaves / RayWaves.exe entry point
│  ├─ game.cpp                 # game / game.exe entry point
│  ├─ GameLogicHost            # Shared module load/ABI/attach path
│  ├─ AgentLock                # Cross-process build lock
│  ├─ platform/{Win32,Posix}   # ProcessRunner, DllLoader, PeCrtCheck
│  └─ DllLoader*               # Shadow-copy loading shared helpers
├─ Cli/                        # rwcli (agent CLI: build/run/scene/assets/lsp)
├─ tests/
│  ├─ unit/                    # Fast, pure tests
│  ├─ integration/             # module/filesystem/project tests
│  ├─ smoke/                   # Hot-reload load/unload cycles
│  └─ support/                 # doctest main + TempDir helper
├─ benchmarks/                 # Standalone benchmark harness + cases
├─ templates/                  # Project templates (Simple2D, Platformer2D, ThirdPerson3D, SlimeQuest)
├─ packaging/                  # package.ps1/.sh + Project.CMakeLists.in + config.ini
├─ scripts/                    # build/test/bench/package/check entry points
├─ docs/                       # Documentation index and guides
├─ third_party/doctest/        # Vendored test framework
├─ third_party/libvterm/       # Vendored VT parser/screen (MIT)
├─ EngineContent/              # Runtime assets: fonts, icons, logo
└─ Tools/                      # Committed scripts + downloaded zig/ninja/cmake
```

Windows downloads Zig, Ninja, CMake, and rcedit through
`Tools/setup_tools.ps1`. Linux uses system GCC/G++ and downloads CMake/Ninja
through `Tools/setup_tools.sh`. Versions are pinned in `Tools/toolchain.ini`.

## Build map

| Module | Responsibility |
|---|---|
| `cmake/Dependencies.cmake` | FetchContent raylib, doctest target, `stage_raylib`, runtime asset copies |
| `cmake/Engine.cmake` | `Engine` static library |
| `cmake/GameLogic.cmake` | Bundled SlimeQuest GameLogic module |
| `cmake/RuntimeSupport.cmake` | Shared host support: DLL loading, process runner, agent lock, game logic host |
| `cmake/Editor.cmake` | `RayWaves` / `RayWaves.exe` editor target |
| `cmake/Runtime.cmake` | `game` / `game.exe` runtime target |
| `cmake/Cli.cmake` | `rwcli` agent CLI target |
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
│  ├─ bin/rwcli.cmd         # Windows project-local agent CLI wrapper
│  ├─ bin/rwcli             # Linux project-local agent CLI wrapper
│  ├─ agent.lock            # Cross-process build lock
│  └─ shadows/              # DLL shadow copies for hot-reload
├─ .agents/skills/          # Generated project skill for coding agents
├─ .clangd, .vscode/        # Generated editor/LSP configs
└─ AGENTS.md                # Generated project agent guide
└─ GameLogic.dll / .so      # Built output (hot-reloaded at runtime)
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
GameLogic.dll / .so       ->  Engine headers  ->  raylib (import library)
```

`Engine` is a static library. `GameLogic` is a DLL so it can be reloaded while
the editor or runtime keeps running.
