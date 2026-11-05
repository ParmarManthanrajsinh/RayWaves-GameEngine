# Engine

Core static library shared by the editor, runtime, and project GameLogic DLLs.

Key files:

- `GameMap`, `MapManager` — scene lifecycle and map registration.
- `Project`, `ProjectManager` — project manifest and per-project CMake generation.
- `Profiler`, `AssetResolver` — diagnostics and asset path resolution.
- `WindowUtils` — Windows title-bar/icon tweaks.

Rules:

- No gameplay code here.
- Add new `.cpp` files explicitly to `cmake/Engine.cmake`.
- Add a test under `tests/unit/` for new behavior.
