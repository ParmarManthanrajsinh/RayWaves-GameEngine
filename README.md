<div align="center">
  <img src="EngineContent/logo.png" alt="RayWaves Logo" width="220"/>
  <h1>RayWaves Game Engine</h1>
  <p><b>A lightweight, code-first game engine built on Raylib with hot-reloading.</b></p>
</div>

RayWaves is a C++23 game engine for developers who want to edit gameplay code
and see the result without restarting the game. Gameplay is compiled into a
`GameLogic.dll` that the editor and runtime reload through a shadow copy, so game
state survives code changes via opt-in `SaveState` / `LoadState`.

## Features

- Hot-reloading gameplay DLL, typically well under a second.
- Zero-install toolchain: Zig, Ninja, CMake, and rcedit are fetched on demand.
- ImGui editor with scene view, file browser, inspector, export panel, and profiler overlay.
- Edit live gameplay entity positions from the editor with one registration line per entity.
- Viewport navigation while stopped: middle-mouse pan, wheel zoom at cursor, `F` frame.
- Sprite prop layer authored in the editor and saved per map in `Assets/<MapId>.scene`.
- Simple map system built on `GameMap` and `MapManager`.
- One-command project templates and distribution packaging.
- `rwcli` builds, validates, screenshots, and inspects existing projects for agents.

## Screenshots

![Level editor](docs/assets/output1.png)
*Scene editing, asset browser, and live reload in the editor.*

![Export panel](docs/assets/output2.png)
*Export settings for a standalone build.*

## Quick start

Requirements: Windows and CMake. The rest is downloaded automatically.

```powershell
cmake --preset zig-release
cmake --build build/zig-release
build/zig-release/RayWaves.exe
```

Run the test suites:

```powershell
scripts/test.ps1
```

Run benchmarks:

```powershell
scripts/bench.ps1
```

Package a distribution:

```powershell
scripts/package.ps1 -IncludeCompiler -Zip
```

## Repository map

| Path | Purpose |
|---|---|
| `Engine/` | Core engine static library |
| `Editor/` | ImGui editor (`RayWaves.exe`) |
| `Runtime/` | Editor/runtime entry points and DLL loading |
| `tests/` | Unit, integration, and smoke suites |
| `benchmarks/` | Standalone performance harness |
| `templates/` | Starter projects (Simple2D, Platformer2D, ThirdPerson3D, SlimeQuest) |
| `packaging/` | Distribution scripts and the shared project CMake template |
| `scripts/` | Day-to-day developer commands |
| `cmake/` | Build modules |
| `docs/` | Guides and reference |

## Commands

| Task | Command |
|---|---|
| Configure release | `cmake --preset zig-release` |
| Build editor | `cmake --build build/zig-release --target main` |
| Build runtime | `cmake --build build/zig-release --target game` |
| Build agent CLI | `cmake --build build/zig-release --target rwcli` |
| Run a project headless | `build/zig-release/rwcli.exe run <project> --frames 60 --screenshot shot.png --json` |
| Configure editor/LSP | `build/zig-release/rwcli.exe lsp <project> --check --json` |
| Unit tests | `scripts/test.ps1 -Suite unit` |
| All tests | `scripts/test.ps1` |
| Benchmarks | `scripts/bench.ps1` |
| Formatting + tests | `scripts/check.ps1` |
| Distribution | `scripts/package.ps1` |

## Documentation

- [Documentation index](docs/README.md)
- [Agent CLI](docs/agent/README.md)
- [Architecture](docs/ARCHITECTURE.md)
- [Developer guide](docs/DEVELOPER_GUIDE.md)
- [Testing](docs/testing.md)
- [Benchmarks](docs/benchmarks.md)
- [Distribution](docs/distribution.md)
- [Game developer guide](docs/GAME_DEVELOPER_GUIDE.md)
- [Troubleshooting](docs/TROUBLESHOOTING.md)

## Adding a map

```cpp
#include "GameMap.h"

class MyLevel : public GameMap
{
public:
    MyLevel() : GameMap("MyLevel") {}

    void Update(float delta_time) override
    {
        if (IsKeyDown(KEY_SPACE)) Jump();
    }

    void Draw() override
    {
        DrawText("Hello World!", 100, 100, 20, WHITE);
    }
};
```

Register it in your project's `RootManager.cpp`:

```cpp
s_GameMapManager->RegisterMap<MyLevel>("Level_1");
```

Gameplay code belongs in your project's `GameLogic/`, never in `Engine/`.

## License

See [LICENSE](LICENSE).
