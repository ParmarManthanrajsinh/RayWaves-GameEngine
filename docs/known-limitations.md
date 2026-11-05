# Known Limitations

Current, intentional limits. Update this page when one changes.

## Platform

- **Windows only.** Hot reload, DLL shadow loading, and file association use Win32 APIs.
- No macOS/Linux runtime path is maintained.

## Editor

- **One project per editor window.** Opening another project closes the current one.
- No tabbed multi-project workspace.
- Compile from the bare launcher with no project open depends on system CMake; the
  normal project path uses the bundled toolchain.
- **Terminal memory:** the terminal is Windows-only (ConPTY). Shell tabs need
  Windows 10 1809 or newer. Scrollback is capped at 5000 lines per tab and is not
  reflowed when the window width changes. Private-use Nerd Font prompt glyphs need
  a fallback font added to the ImGui atlas; raw application mouse reporting is not
  forwarded.

## Hot reload

- Only state written by `SaveState` survives a reload. Pointers, GPU resources,
  and `static`/global gameplay state are lost with the old DLL.
- GameLogic ABI is tied to `RAYWAVES_GAMELOGIC_ABI_VERSION`. The editor rejects
  mismatched or missing exports; rebuild instead of forcing a load.
- A crash inside gameplay code can take down the editor because the DLL runs in
  the host process.

## Build and packaging

- Project CMake is regenerated on project open, not watched continuously.
- Distribution packages are unsigned. Add code signing separately if needed.
- `-IncludeCompiler` increases package size because it bundles Zig.
- No CI workflow is configured; `scripts/check.ps1` is the local gate.

## Benchmarks and docs

- Benchmark timings are machine-dependent. Use Release builds and quiet machines.
- API reference is maintained by hand; there is no generated documentation site.
- Documentation is plain Markdown. There is no versioned docs portal.
