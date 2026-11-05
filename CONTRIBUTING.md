# Contributing

## Local loop

```powershell
cmake --preset zig-debug
cmake --build build/zig-debug
scripts/test.ps1 -Preset zig-debug
```

Before opening a PR:

```powershell
scripts/check.ps1
```

`check.ps1` runs the format check when `clang-format` is available, then all test
suites. `Tools/run_analysis.bat tidy` runs clang-tidy.

## Naming convention

Follow the existing prefixes; do not introduce new styles.

| Prefix | Meaning | Example |
|---|---|---|
| `b_` | bool variable or bool-returning function | `b_IsPlaying`, `b_LoadGameLogic()` |
| `m_` | Member variable | `m_GameEngine`, `m_FrameTimes` |
| `s_` | File-local static / static function | `s_LayoutPath`, `s_fDrawSpinner()` |
| `t_` | Type / struct name | `t_WindowConfig`, `t_MapInfo` |
| `F` | Editor struct type | `FThemePreset`, `FBuildMessage` |
| `E` | Enum type | `EBuildStatus`, `EEditorFont` |
| `c_` / `k_` | Compile-time constant | `c_NUM_SEGMENTS`, `k_FrameCount` |
| `m_t` | Member struct instance | `m_SceneSettings`, `m_ExportState` |

Formatting: tabs, Allman braces, `snake_case` variables, `PascalCase`
functions and types.

## Where new code goes

- **Gameplay (players, enemies, levels):** the project's `GameLogic/` DLL,
  never `Engine/`. Register maps in `RootManager.cpp`.
- **Editor UI:** a new file pair under `Editor/Panels/` implementing
  `IEditorPanel`, plus one `b_RegisterPanel<T>()` line in its own `.cpp`.
- **Engine systems:** `Engine/`, an explicit entry in `cmake/Engine.cmake`
  (no `file(GLOB)`), and a test under `tests/unit/` or `tests/integration/`.
- **Build logic:** `cmake/*.cmake`; keep `CMakeLists.txt` thin.
- **Developer commands:** `scripts/`; do not scatter runners in source folders.

## Tests

- Unit tests: `tests/unit/<Module>_t.cpp`.
- Integration tests: `tests/integration/` (DLL builds, filesystem, processes).
- Smoke: `tests/smoke/` (hot-reload cycles).
- Shared helpers: `tests/support/` (`TempDir`, doctest main).
- Add new files to `tests/CMakeLists.txt`.

See [docs/testing.md](docs/testing.md).

## Benchmarks

Add cases under `benchmarks/` and list them in `benchmarks/CMakeLists.txt`.
Keep correctness tests out of the benchmark binary. See
[docs/benchmarks.md](docs/benchmarks.md).

## Documentation checklist

When making code changes:

1. Changed folder structure? Update `docs/ARCHITECTURE.md` and the repository map
   in `README.md`.
2. Renamed an exe, target, or file? Search the repo for the old name:
   `git grep -n "oldname"`.
3. Removed a feature? Remove stale references from `README.md` and `docs/`.
4. Changed build steps or toolchain? Update `README.md`,
   `docs/DEVELOPER_GUIDE.md`, and `docs/TROUBLESHOOTING.md`.

## Pull requests

- One concern per PR.
- Include tests for behavior changes.
- Run `scripts/check.ps1`.
- Update docs when paths or commands change.
