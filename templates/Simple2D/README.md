# Simple2D Template

The smallest complete RayWaves game, built on the `rw` facade in
`Engine/rw.h`.

## What it shows

- `rw::Level` lifecycle: `OnStart`, `OnUpdate`, `OnDraw`, `OnDrawUI`.
- `Spawn()` for entities that appear in the editor hierarchy automatically.
- `rw::Texture` for asset loading through `AssetResolver`.
- `Cam()` for follow, zoom, and level bounds.
- `RW_GAME` instead of hand-written DLL exports.

## Controls

| Input | Action |
|---|---|
| `WASD` / arrow keys | Move the player |
| Middle mouse drag (editor) | Pan while stopped |
| Mouse wheel (editor) | Zoom at the cursor |
| `F` (editor) | Frame the selection |

## Files

- `GameLogic/Game.cpp` -- exports the DLL entry points with `RW_GAME`.
- `GameLogic/Simple2DLevel.h` -- the whole game: entities, camera, drawing.
- `Assets/player.png` -- the player sprite.

## Next steps

- Add a sprite: `m_MyTexture.Load("crate.png");` and `rw::DrawSprite(m_MyTexture, *entity);`
- Add a second level: derive from `rw::Game`, call `AddLevel<T>("id")`, and
  `Start("id")`; switch with `GoTo("id")`.
- Hot reload keeps entity transforms; put custom state in `OnSave`/`OnLoad`.
