# RayWaves Architecture

## Engine Repository Structure

```
RayWaves/
├── CMakeLists.txt           # Top-level build (FetchContent for raylib)
├── CMakePresets.json        # Build presets (zig-debug, zig-release, x64-*, x86-*)
│
├── Engine/                  # Core engine library (static libEngine.a)
│   ├── *.h / *.cpp          # GameMap, MapManager, ProjectManager, Profiler, etc.
│   ├── ProjectManager.h     # Project lifecycle, folder open/create
│   └── raygui.h             # Immediate-mode GUI helper (bundled)
│
├── Editor/                  # RayWaves.exe source (ImGui-based IDE)
│   ├── GameEditor.h/cpp     # Main editor loop, panels, DLL hot-reload
│   ├── Panels/              # MainMenuBar, SceneWindow, ExportPanel, etc.
│   ├── imgui/               # Dear ImGui (vendored)
│   ├── rlImGui/             # raylib-ImGui bridge
│   └── FileAssociation.h/cpp # Windows .raywaves file association (HKCU)
│
├── Game/                    # Entry points
│   ├── main.cpp             # RayWaves.exe — editor entry
│   └── game.cpp             # game.exe — standalone runtime entry
│
├── Tools/                   # Build toolchain (auto-downloaded, .gitignored)
│   ├── setup_zig.ps1        # Fetches Zig / Ninja / CMake / rcedit on demand
│   ├── zig-cc.bat           # Compiler wrapper → Tools/zig/zig.exe
│   ├── zig-cxx.bat
│   ├── zig/                 # Zig compiler (pinned 0.16.0)
│   ├── ninja/               # Ninja build system (pinned 1.13.2)
│   ├── cmake/               # CMake (pinned 4.3.4)
│   └── rcedit.exe           # Resource editor for .ico embedding
│
├── Tests/                   # Unit + smoke tests (doctest)
│   ├── SmokeTest.cpp        # DLL 50× load/unload stress test
│   └── *t.cpp               # Module tests
│
├── EngineContent/           # Runtime assets: fonts, icons, logo
│
├── Distribution/            # Packaging scripts for engine distribution
│   ├── distribute.ps1       # Creates dist/ folder
│   ├── create_distribution.bat
│   ├── dist_CMakeLists.txt  # CMakeLists.txt shipped inside dist/Core/
│   └── config.ini           # Default window config template
│
└── Documentation/           # You are here
```

> **Tools/ content** (zig, ninja, cmake, rcedit) is downloaded on first compile or explicitly via `Tools/setup_zig.ps1`. These are listed in `.gitignore` — not committed.

---

## Project Structure

A RayWaves **project** is created via *New Project Wizard* or *Open Existing Project* in the editor. It lives in its own folder — never at the engine repo root.

```
MyNewGame/                   # <--- your project folder
├── project.raywaves         # Manifest (name, version, scene settings)
├── Assets/                  # Textures, audio, fonts — use AssetResolver
├── GameLogic/               # 🔥 YOUR C++ GAMEPLAY CODE
│   ├── RootManager.cpp      # Registers maps via RegisterMap<>
│   └── ...                  # Any number of maps, entities, systems
├── .raywaves/               # Editor cache (auto-generated)
│   ├── CMakeLists.txt       # Per-project cmake script (generated)
│   ├── build/               # Ninja build output
│   └── shadows/             # DLL shadow copies for hot-reload
└── GameLogic.dll            # Built output (hot-reloaded at runtime)
```

Key rules:
- `project.raywaves` always sits at the project root.
- `GameLogic/` contains **your** source — edit any file, hit Compile, see changes in ~0.5 s.
- `.raywaves/` is auto-managed; do not edit manually.
- Double-click `project.raywaves` in Explorer (after registering file association) to launch the editor directly into that project.
