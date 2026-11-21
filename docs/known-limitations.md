# Known Limitations

Current, intentional limits. Update this page when one changes.

## Platform

- Windows and Linux are supported. macOS is not maintained.
- Linux builds target X11/XWayland; native Wayland is not part of the first
  pass. Install the X11/GL/ALSA development packages listed in
  [linux.md](linux.md).
- Linux requires GCC/G++ and the system libstdc++; the distribution does not
  bundle a Linux compiler.
- Linux file dialogs use `zenity` or `kdialog` through `tinyfiledialogs`.
- Cross-compilation is not supported; release builds run natively on each OS
  in CI.

## Editor

- **One project per editor window.** Opening another project closes the current one.
- No tabbed multi-project workspace.
- **Scene sprites are world-space.** They follow the map camera and pan/zoom with
  the viewport.
- **Gameplay entity edits are live-only.** The editor writes into the entity's
  registered `Vector2`; changes are not saved into `Assets/<MapId>.scene`.
- **Gameplay registrations must be rebuilt after structural changes.** Clear the
  registry before destroying or moving entities, then register again. Dynamic
  entities should use stable storage (`std::deque` or `std::unique_ptr`).
  `rw::Level` does this automatically for `rw::Entity` objects.
- **Camera-based maps must expose `GetViewCamera()`.** Without it, the editor uses
  identity and the entity selection rectangle and drag position will not match a
  zoomed or scrolling camera. `rw::Level` exposes its camera automatically.
- **Viewport navigation requires `GetMutableViewCamera()`.** Maps that do not
  expose a mutable camera can still select and drag entities, but cannot pan or
  zoom the viewport. `rw::Level` exposes one automatically.
- **3D viewport navigation requires `BeginView3D()`.** 3D maps that call
  `BeginMode3D` with their own camera keep working, but the editor cannot fly
  through them. The editor camera itself is session-only and starts from the
  gameplay camera each time a project opens; orthographic gameplay cameras are
  shown as perspective.
- **Levels need a map registered in `GameLogic`.** The FileSystem panel can
  create, open, and delete `Assets/<MapId>.scene` files, but it cannot define
  gameplay maps; those stay in code.
- **Code opens in the system default editor.** There is no built-in C++ editor.
- **`Assets/` imports are flat by design.** The FileSystem panel groups files
  virtually by extension; it does not create per-type subfolders.
- Compile from the bare launcher with no project open depends on system CMake; the
  normal project path uses the bundled toolchain.
- **Terminal memory:** Windows uses ConPTY (Windows 10 1809 or newer); Linux
  uses `posix_openpt`. Scrollback is capped at 5000 lines per tab and is not
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
- `-IncludeCompiler` bundles Zig on Windows and increases package size. Linux packages use system GCC/G++.
- `scripts/check.ps1` / `scripts/check.sh` are the local gates; GitHub Actions
  runs the same tasks on Windows and Linux. Linux uses GCC/G++, Windows uses Zig.

## Benchmarks and docs

- Benchmark timings are machine-dependent. Use Release builds and quiet machines.
- API reference is maintained by hand; there is no generated documentation site.
- Documentation is plain Markdown. There is no versioned docs portal.
