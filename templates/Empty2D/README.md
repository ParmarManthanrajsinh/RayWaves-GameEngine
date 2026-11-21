# Empty2D Template

The smallest multi-level RayWaves project. No assets, no gameplay systems:
just one `rw::Level` and an `rw::Game` entry point ready to extend.

## What it shows

- `rw::Game` + `AddLevel<T>` + `Start` in `GameLogic/Game.cpp`.
- A blank `rw::Level` with the full `OnStart` / `OnUpdate` / `OnDraw` lifecycle.
- A 64px world grid so camera pan and zoom are visible immediately.

## Files

- `GameLogic/Game.cpp` -- module entry point (`RW_GAME`).
- `GameLogic/EmptyLevel.h` -- the blank level.

## Next steps

- Scaffold another level without leaving the terminal:

  ```text
  rwcli new-level . MyLevel
  ```

  It creates `GameLogic/MyLevel.h` and adds `AddLevel<MyLevel>("MyLevel")`.

- Or add one manually in `Game.cpp`, then switch with `GoTo("MyLevel")`.
- Spawn an entity in `OnStart`:

  ```cpp
  m_Player = Spawn("Player", { 100, 300 }, { 48, 64 }, rw::Origin::Center);
  ```

- See `docs/game/cookbook.md` for movement, collision, audio, and menus.
