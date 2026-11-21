# Distribution

There are two different outputs:

- **Engine distribution** — a package others use to create games (editor, runtime, headers, toolchain, templates).
- **Game export** — a player-facing build produced by the editor's Export panel.

This page covers the engine distribution. Game export is handled by
`Editor/ExportService.cpp` and documented in the
[Game Developer Guide](GAME_DEVELOPER_GUIDE.md).

## Build a package

```powershell
# Windows: folder only
scripts/package.ps1

# Windows: standalone ZIP + SHA256 with bundled compiler
scripts/package.ps1 -IncludeCompiler -Zip

# Windows: debug package
scripts/package.ps1 -Config Debug -OutputDir out/debug
```

```sh
# Linux: folder only
./scripts/package.sh

# Linux: standalone tar.gz + SHA256 (uses system GCC/G++)
./scripts/package.sh --archive

# Linux: debug package
./scripts/package.sh --config Debug --output-dir out/debug
```

The underlying scripts are `packaging/package.ps1` and
`packaging/package.sh`. Both resolve every path from the repository root, so
they can be run from any working directory.

## Package layout

Windows:

```text
dist/
├─ RayWaves.exe
├─ rwcli.exe                 # agent CLI; must stay next to the editor
├─ libraylib.dll
├─ config.ini
├─ VERSION
├─ LICENSE
├─ Core/
│  ├─ runtime.exe
│  ├─ Engine/                 # headers and sources
│  ├─ raylib/{include,lib,bin}
│  ├─ Project.CMakeLists.in
│  ├─ Project.AGENTS.md.in    # generated AGENTS.md
│  ├─ Project.SKILL.md.in     # generated agent skill
│  ├─ Project.rwcli-reference.md.in
│  ├─ Project.clangd.in
│  ├─ Project.vscode.*.json.in
│  ├─ .clang-format
│  └─ Tools/                  # zig wrappers, ninja, cmake, rcedit
├─ templates/
└─ docs/
```

Linux:

```text
dist/
├─ RayWaves
├─ rwcli
├─ libraylib.so, libraylib.so.600
├─ config.ini
├─ VERSION
├─ LICENSE
├─ Core/
│  ├─ runtime
│  ├─ Engine/
│  ├─ raylib/{include,lib,bin}
│  ├─ Project.CMakeLists.in
│  ├─ Project.AGENTS.md.in
│  ├─ Project.SKILL.md.in
│  ├─ Project.rwcli-reference.md.in
│  ├─ Project.clangd.in
│  ├─ Project.vscode.*.json.in
│  ├─ .clang-format
│  └─ Tools/                  # setup script, ninja, cmake (no compiler)
├─ templates/
└─ docs/
```

`Core/Project.CMakeLists.in` is the same template the editor uses to generate
`.raywaves/CMakeLists.txt`. Do not add a second project build recipe.

## Validation

The scripts fail if any required file is missing. They also check for the
raylib import/runtime library and remove build artifacts from templates.
Windows archives are ZIPs; Linux archives are tar.gz files that preserve
executable bits and `$ORIGIN` RPATHs. Linux targets must have GCC/G++ and
libstdc++ installed; the package does not bundle a compiler.

Linux release artifacts target glibc 2.39 / glibcxx 3.4.32 (Ubuntu 24.04). Build
them on that baseline (or an older LTS with an equivalent GCC), not on the
developer's newest distro. `packaging/package.sh` prints the ABI version each
binary requires; set `RAYWAVES_ENFORCE_GLIBC=1` in release CI to make a newer
requirement fail the build.

The package includes `THIRD_PARTY_NOTICES.md` and `docs/licenses/` for raylib,
Dear ImGui, tinyfiledialogs, libvterm, Ninja, and CMake.

## Versioning

The package version comes from `project(RayWaves VERSION x.y.z)` in the root
`CMakeLists.txt`. `Engine/EngineVersion.h` mirrors it for C++ code. The script
writes it to `dist/VERSION`. Override it with:

```powershell
scripts/package.ps1 -Version 0.2.0
```

```sh
./scripts/package.sh --version 0.2.0
```
