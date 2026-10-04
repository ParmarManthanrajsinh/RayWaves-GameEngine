# RayWaves Architecture

## Engine Repository Structure

```
RayWaves/
├── CMakeLists.txt           # Top-level build (FetchContent for raylib)
├── CMakePresets.json        # Build presets (linux-debug, linux-release)
│
├── Engine/                  # Core engine library (static libEngine.a)
│   ├── *.h / *.cpp          # GameMap, MapManager, ProjectManager, Profiler, etc.
│   ├── ProjectManager.h     # Project lifecycle, folder open/create
│   ├── WindowUtils.h        # Linux no-op stubs (signatures unchanged, no platform headers)
│   └── raygui.h             # Immediate-mode GUI helper (bundled)
│
├── Editor/                  # RayWaves editor source (ImGui-based IDE)
│   ├── GameEditor.h/cpp     # Main editor loop + orchestration (delegates below)
│   ├── GameLogicLoader.h/cpp # GameLogic.so lifecycle: load/swap/unload, ABI check
│   ├── ThemeService.h/cpp   # Editor theme rebake + EngineContent path lookup
│   ├── ExportService.h/cpp  # Game export pipeline (build/copy/validate), UI-free
│   ├── Panels/              # MainMenuBar, SceneWindow, ExportPanel, etc.
│   ├── PanelRegistry.h      # Panel factory list; new panels self-register
│   ├── imgui/               # Dear ImGui (vendored)
│   ├── rlImGui/             # raylib-ImGui bridge
│   └── FileAssociation.h/cpp # XDG .raywaves association (desktop entry + shared-mime-info)
│
├── Game/                    # Entry points + shared-library loading
│   ├── main.cpp             # RayWaves — editor entry
│   ├── game.cpp             # game — standalone runtime entry
│   └── DllLoader.h/cpp      # Shadow-copy load/unload (dlopen), log sink, ABI message
│
├── Tools/                   # Helper scripts (committed)
│   └── run_analysis.sh      # clang-format driver (make format)
│
├── Tests/                   # Unit + smoke tests (doctest)
│   ├── SmokeTest.cpp        # GameLogic 50× load/unload stress test
│   └── *t.cpp               # Module tests
│
├── EngineContent/           # Runtime assets: fonts, icons, logo
│
├── Distribution/            # Packaging scripts for engine distribution
│   ├── distribute.sh        # Creates dist/ folder (-BuildConfig Release -OutputDir dist)
│   ├── Templates/           # Project templates (Empty, Platformer2D, SlimeQuest)
│   ├── dist_CMakeLists.txt  # CMakeLists.txt shipped inside dist/Core/
│   └── config.ini           # Default window config template
│
└── Documentation/           # You are here
```

> **Toolchain:** system GCC/Clang + system CMake + system Ninja — nothing is downloaded or bundled. `Tools/` holds committed helper scripts only.

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
│   └── shadows/             # GameLogic.so shadow copies for hot-reload
└── GameLogic.so             # Built output (hot-reloaded at runtime)
```

Key rules:
- `project.raywaves` always sits at the project root.
- `GameLogic/` contains **your** source — edit any file, hit Compile, see changes in ~0.5 s.
- `.raywaves/` is auto-managed; do not edit manually.
- Double-click `project.raywaves` in your file manager (after registering the file association from the editor's Tools menu) to launch the editor directly into that project.
