# RayWaves Documentation

## Start here

| Document | Audience | Contents |
|---|---|---|
| [Getting started](../README.md) | Everyone | Clone, build, run the editor |
| [Engine docs](engine/README.md) | Engine contributors | Index for engine, build, tests, packaging |
| [Game docs](game/README.md) | Game creators | Index for maps, assets, hot reload, export |
| [Architecture](ARCHITECTURE.md) | Engine contributors | Repository map, build modules, project layout |
| [Developer guide](DEVELOPER_GUIDE.md) | Engine contributors | Conventions, patterns, diagnostics |
| [Game developer guide](GAME_DEVELOPER_GUIDE.md) | Game creators | Maps, assets, hot reload, export |
| [Testing](testing.md) | Contributors | Test suites, labels, how to add a test |
| [Benchmarks](benchmarks.md) | Contributors | Running, measuring, and checking regressions |
| [Distribution](distribution.md) | Maintainers | Building and validating a release package |
| [Known limitations](known-limitations.md) | Everyone | Current intentional limits |
| [Decisions (ADR)](adr/README.md) | Contributors | Why the architecture looks like this |
| [Troubleshooting](TROUBLESHOOTING.md) | Everyone | Common failures and fixes |
| [API reference](API_REFERENCE.md) | Game creators | Engine class reference |
| [Fundamentals](GUIDE_FUNDAMENTALS.md) | Game creators | Core concepts |
| [Reference](GUIDE_REFERENCE.md) | Game creators | Detailed engine reference |

## Repository map

| Folder | Purpose | Start with |
|---|---|---|
| `Engine/` | Core engine library | `GameMap.h`, `MapManager.h` |
| `Editor/` | ImGui editor | `GameEditor.cpp`, `Panels/` |
| `Runtime/` | Editor/runtime entry points + DLL loading | `main.cpp`, `DllLoader.h` |
| `tests/` | Unit, integration, smoke tests | `tests/CMakeLists.txt` |
| `benchmarks/` | Standalone performance harness | `benchmarks/main.cpp` |
| `templates/` | New-project templates | `templates/SlimeQuest/` |
| `packaging/` | Distribution scripts and template | `packaging/README.md` |
| `scripts/` | Day-to-day developer commands | `scripts/test.ps1` |
| `cmake/` | Build modules | `cmake/Dependencies.cmake` |
