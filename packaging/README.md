# Packaging

Scripts and data for building the RayWaves engine distribution.

## Files

- `package.ps1` — builds the runtime/editor and writes `dist/` (optionally a ZIP + SHA256).
- `Project.CMakeLists.in` — the single per-project CMake recipe used by both the editor and distributions.
- `config.ini` — default game configuration template.

## Usage

```powershell
# Folder only
scripts/package.ps1

# Standalone ZIP with bundled Zig compiler
scripts/package.ps1 -IncludeCompiler -Zip

# Debug package in a custom folder
scripts/package.ps1 -Config Debug -OutputDir out/debug
```

Output layout:

```text
dist/
├─ RayWaves.exe
├─ libraylib.dll
├─ config.ini
├─ VERSION
├─ LICENSE
├─ Core/
│  ├─ runtime.exe
│  ├─ Engine/                 # headers + sources
│  ├─ raylib/{include,lib,bin}
│  ├─ Project.CMakeLists.in
│  └─ Tools/
├─ templates/
└─ docs/
```

See [docs/distribution.md](../docs/distribution.md) for the full guide.
