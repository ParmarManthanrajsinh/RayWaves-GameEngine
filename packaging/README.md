# Packaging

Scripts and data for building the RayWaves engine distribution.

## Files

- `package.ps1` — Windows distribution builder (folder, optional ZIP + SHA256).
- `package.sh` — Linux distribution builder (folder, optional tar.gz + SHA256).
- `Project.CMakeLists.in` — the single per-project CMake recipe used by both the editor and distributions. It selects the host compiler wrappers, Ninja, raylib library, and platform libraries.
- `config.ini` — default game configuration template.

## Usage

```powershell
scripts/package.ps1
scripts/package.ps1 -IncludeCompiler -Zip
scripts/package.ps1 -Config Debug -OutputDir out/debug
```

```sh
./scripts/package.sh
./scripts/package.sh --archive
./scripts/package.sh --config Debug --output-dir out/debug
```

Windows output layout:

```text
dist/
├─ RayWaves.exe
├─ rwcli.exe
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

Linux output layout:

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
│  ├─ Engine/                 # headers + sources
│  ├─ raylib/{include,lib,bin}
│  ├─ Project.CMakeLists.in
│  └─ Tools/
├─ templates/
└─ docs/
```

Linux packages use the system GCC/G++; `--include-compiler` has no effect there.
Windows packages can still bundle Zig.

See [docs/distribution.md](../docs/distribution.md) for the full guide.
