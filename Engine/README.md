# Engine

Core static library shared by the editor, runtime, and project GameLogic DLLs.

Key files:

- `rw` — header-only game-facing facade: `rw::Level`, `rw::Entity`, `rw::DrawSprite`,
  `rw::Texture`, `rw::Camera`, `RW_GAME`.
- `GameMap`, `MapManager` — scene lifecycle, map registration, editor entity registry.
- `Scene` — editor-authored sprite prop layer, saved per map in `Assets/<MapId>.scene`.
- `Project`, `ProjectManager` — project manifest and per-project CMake generation.
- `Profiler`, `AssetResolver` — diagnostics and asset path resolution.
- `WindowUtils` — Windows title-bar/icon tweaks.

Rules:

- No gameplay code here.
- Add new `.cpp` files explicitly to `cmake/Engine.cmake`.
- Header-only additions must stay flat in `Engine/` (the package copies `Engine/*.h`).
- Add a test under `tests/unit/` for new behavior.
