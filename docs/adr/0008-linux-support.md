# 0008 — Linux support and a shared platform layer

## Context

RayWaves was Windows-only: CMake presets selected Zig `.bat` wrappers,
`LoadLibrary`/`Job Object`/ConPTY/Win32 registry code sat in shared files, and
scripts were PowerShell-only. A Linux build needed the same editor, runtime,
`rwcli`, hot reload, project generation, and tests without forking the repo.

## Decision

- One CMake graph and one project template for both OSes. `zig-debug` and
  `zig-release` are the only configure presets. `cmake/Toolchain.cmake` picks
  Zig on Windows and system GCC/G++ on Linux before `project()`.
- Platform services are separated:
  - `Engine/PlatformInfo.*` — executable path, pid, config dir, environment,
    atomic lock file creation.
  - `Engine/PlatformNames.h` — compile-time executable/module/library names.
  - `Runtime/platform/{Win32,Posix}/` — process runner, module loader, PE CRT
    scan (POSIX stub).
  - `Editor/platform/{Win32,Posix}/` — shell open, file association, PTY.
- `ProcessRunner` gains an argv-based API; callers no longer build shell strings
  with `cd /d`/`cmd.exe`.
- Project modules store `entryDll=GameLogic` (a logical stem). The host appends
  `.dll` or `.so`.
- Linux hot reload uses a unique shadow copy per build and unloads it with
  `dlclose`. Linux uses the system libstdc++ and `-fno-gnu-unique`; see ADR 0009.
- `rwcli`/editor project wrappers are generated in both forms:
  `.raywaves/bin/rwcli.cmd` and `.raywaves/bin/rwcli`.
- Developer commands are native per OS (`.ps1`/`.sh`) but share CMake presets,
  targets, and the generated project recipe.

## Consequences

- Windows behavior and commands are preserved.
- `rwcli`, runtime, tests, and the editor build and run on Linux.
- Linux project builds need X11/GL/ALSA development headers; CI installs them.
- Linux requires system GCC/G++ and does not bundle a compiler.
- Windows and Linux CI run the same task matrix; script drift is caught there.
