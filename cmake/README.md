# CMake Modules

The root `CMakeLists.txt` is a map; targets live here.

- `Dependencies.cmake` — raylib FetchContent, doctest target, raylib staging.
- `Engine.cmake` — `Engine` static library.
- `GameLogic.cmake` — bundled SlimeQuest template DLL.
- `Editor.cmake` — `RayWaves.exe`.
- `Runtime.cmake` — `game.exe`.
- `Tests.cmake` — CTest setup and `tests/`.
- `Benchmarks.cmake` — `benchmarks/`.
- `Packaging.cmake` — project template staging and `export_package`.

Engine rules: explicit source lists, no `file(GLOB)`.
