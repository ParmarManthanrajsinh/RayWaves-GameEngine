# Editor setup

RayWaves generates project-local editor files when a project is created or
opened. Generated LSP paths are absolute and machine-specific, so `.clangd`,
`.vscode/`, and `compile_commands.json` are gitignored.

## VS Code + clangd (recommended)

1. Install the recommended extensions (VS Code prompts from
   `.vscode/extensions.json`):
   - `llvm-vs-code-extensions.vscode-clangd`
   - `ms-vscode.cpptools` (optional fallback)
2. Open the project folder that contains `project.raywaves`.
3. Verify:
   ```powershell
   # Windows
   .\.raywaves\bin\rwcli.cmd lsp . --check --json
   ```
   ```sh
   # Linux
   .raywaves/bin/rwcli lsp . --check --json
   ```
4. After moving the project or updating the engine:
   ```powershell
   .\.raywaves\bin\rwcli.cmd lsp . --force --json
   ```
   ```sh
   .raywaves/bin/rwcli lsp . --force --json
   ```

## Generated files

- `.clangd` — compiler target and include paths (Zig/libc++ on Windows, system
  GCC/libstdc++ on Linux), engine/raylib includes, platform defines
- `.vscode/c_cpp_properties.json` — C/C++ IntelliSense fallback
- `.vscode/settings.json` — compile database path and terminal `PATH`
- `.vscode/extensions.json` — extension recommendations
- `compile_commands.json` - written by the project wrapper's `lsp` command
- `.raywaves/tooling.ini` — package `rwcli` path for agents
- `.raywaves/bin/rwcli.cmd` — Windows wrapper for agents and terminals
- `.raywaves/bin/rwcli` — Linux wrapper for agents and terminals

`.clangd` carries explicit include paths and feature defines, so it works
before the first build. `compile_commands.json` adds precise per-file flags.

## Without clangd

`c_cpp_properties.json` sets include paths, defines, C++23, and the host
IntelliSense mode (`windows-clang-x64` on Windows, `linux-clang-x64` on Linux).
It does not require a compiler query, so IntelliSense resolves engine and raylib
headers even when clangd is not installed.

## Troubleshooting

- `clangd` not found: install LLVM or add it to `PATH`.
- Errors after moving the project:
  `.raywaves/bin/rwcli lsp . --force`.
- Errors on `<string>` or `<vector>`: `.clangd` was edited. Rerun `lsp --force`.
- Stale `compile_commands.json`: rerun `lsp`.
- `rwcli` not recognized in a terminal: use the project wrapper or the absolute
  path to the packaged `rwcli`.
- Exact diagnostics:
  ```powershell
  # Windows
  .\.raywaves\bin\rwcli.cmd lsp . --check --json
  ```
  ```sh
  # Linux
  .raywaves/bin/rwcli lsp . --check --json
  ```
  `diagnostics[]` contains the raw clangd `E[...]` lines.

## Other editors

- Visual Studio: open `.raywaves/CMakeLists.txt` as a CMake project, or use the
  root `compile_commands.json`.
- CLion: open the project folder; point the compilation database at the root
  `compile_commands.json`.
- Cursor: clangd setup is identical to VS Code.
- Any terminal: `.raywaves\bin\rwcli.cmd` (Windows) or `.raywaves/bin/rwcli`
  (Linux) resolves the package CLI without PATH.
