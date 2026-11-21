# CMake Modules

The root `CMakeLists.txt` is a map; targets live here.

- `Toolchain.cmake` — compiler selection before `project()`: Zig on Windows, system GCC/G++ on Linux.
- `Platform.cmake` — platform suffixes, source directories, flags, `$ORIGIN` RPATH helper.
- `Dependencies.cmake` — raylib FetchContent, doctest target, raylib staging.
- `Engine.cmake` — `Engine` static library.
- `GameLogic.cmake` — bundled SlimeQuest template module.
- `Editor.cmake` — `RayWaves` / `RayWaves.exe`.
- `Runtime.cmake` — `game` / `game.exe`.
- `Cli.cmake` — `rwcli` / `rwcli.exe`.
- `Tests.cmake` — CTest setup and `tests/`.
- `Benchmarks.cmake` — `benchmarks/`.
- `Packaging.cmake` — project template staging and `export_package`.

Engine rules: explicit source lists, no `file(GLOB)`.
