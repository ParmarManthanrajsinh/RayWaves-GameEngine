# Distribution

There are two different outputs:

- **Engine distribution** — a package others use to create games (editor, runtime, headers, toolchain, templates).
- **Game export** — a player-facing build produced by the editor's Export panel.

This page covers the engine distribution. Game export is handled by
`Editor/ExportService.cpp` and documented in the
[Game Developer Guide](GAME_DEVELOPER_GUIDE.md).

## Build a package

```powershell
# Folder only
scripts/package.ps1

# Standalone ZIP + SHA256 with bundled compiler
scripts/package.ps1 -IncludeCompiler -Zip

# Debug package
scripts/package.ps1 -Config Debug -OutputDir out/debug
```

The underlying script is `packaging/package.ps1`. It resolves every path from
the repository root, so it can be run from any working directory.

## Package layout

```text
dist/
├─ RayWaves.exe
├─ libraylib.dll
├─ config.ini
├─ VERSION
├─ LICENSE
├─ Core/
│  ├─ runtime.exe
│  ├─ Engine/                 # headers and sources
│  ├─ raylib/{include,lib,bin}
│  ├─ Project.CMakeLists.in
│  └─ Tools/                  # zig wrappers, ninja, cmake, rcedit
├─ templates/
└─ docs/
```

`Core/Project.CMakeLists.in` is the same template the editor uses to generate
`.raywaves/CMakeLists.txt`. Do not add a second project build recipe.

## Validation

The script fails if any required file is missing. It also checks for the raylib
import library and removes build artifacts from templates.

## Versioning

The package version comes from `project(RayWaves VERSION x.y.z)` in the root
`CMakeLists.txt`. `Engine/EngineVersion.h` mirrors it for C++ code. The script
writes it to `dist/VERSION`. Override it with:

```powershell
scripts/package.ps1 -Version 0.2.0
```
