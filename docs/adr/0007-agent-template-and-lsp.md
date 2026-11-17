# 0007 — Project agent bundle and IDE/LSP files

## Context

Templates copied `GameLogic/` and `Assets/` but nothing told an agent how to
build or run the project. Opening a project in VS Code produced false errors
because there was no compilation database, no `.clangd`, and no include paths
outside the build. Zig builds through `Tools/zig-cxx.bat`, which clangd cannot
query as a compiler driver.

## Decision

`ProjectManager` writes project-local agent and editor files when missing:

- `AGENTS.md`
- `.agents/skills/raywaves-game/SKILL.md`
- `.agents/skills/raywaves-game/reference.md`
- `.clangd`
- `.vscode/{c_cpp_properties,settings,extensions}.json`
- `.clang-format`
- `.raywaves/tooling.ini` and `.raywaves/bin/rwcli.cmd`

`tooling.ini` records the package `rwcli.exe` path. The wrapper invokes it, so a
project-scoped agent does not need `rwcli` on `PATH`. Both are regenerated on
project open and by `rwcli lsp`.

Template sources live in `packaging/Project.*.in` and are staged into
`packaging/` next to the executables.

`.clangd` carries the full Zig libc++ configuration (`x86_64-windows-gnu`,
`-nostdinc++`, Zig libcxx/libcxxabi/libc include paths, libc++ feature
defines, engine/raylib includes). clangd therefore parses template files with
zero errors even before a build creates a compilation database.

`rwcli lsp`:

- refreshes the generated files with `--force`
- configures `.raywaves/build-ide` with `CMAKE_EXPORT_COMPILE_COMMANDS=ON`
- copies `compile_commands.json` to the project root
- `--check` runs `clangd --check` over `GameLogic/**/*.cpp` and returns
  diagnostics as JSON

`compile_commands.json`, `.clangd`, `.vscode/`, and `.cache/` are gitignored
because the configs embed absolute engine paths. Regenerate after moving a
project or updating the engine.

## Consequences

- New and existing projects get the same agent/editor setup on open.
- User-edited configs are never overwritten without `rwcli lsp --force`.
- `rwcli lsp --check` is the regression gate for "no false code errors".
- VS Code C/C++ IntelliSense works through `c_cpp_properties.json`; clangd is
  the primary path and is checked in `scripts/agent-smoke.ps1`.
- `packaging/Project.CMakeLists.in` exports compile commands for every project
  build; the root `compile_commands.json` is still produced by `rwcli lsp`.
