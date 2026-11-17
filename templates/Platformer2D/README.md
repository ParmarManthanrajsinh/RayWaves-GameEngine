# Platformer2D Template

A small 2D platformer built on the `rw` facade in `Engine/rw.h`. It exists to
show how far the simple API goes once you add real movement.

## What it shows

- `rw::Level` with a player entity, velocity, gravity, and jumping.
- Collision against plain rectangles, resolved by landing only (jump-through
  platforms).
- `Cam().Follow()` with level bounds.
- Automatic editor integration: select `Player` in the Scene panel to move it
  with the X/Y gizmo or edit its position/size in the Inspector.

## Controls

| Input | Action |
|---|---|
| `A` / `D` or left / right | Move |
| `W`, `Up`, or `Space` | Jump |
| Middle mouse drag (editor) | Pan while stopped |
| Mouse wheel (editor) | Zoom at the cursor |
| `F` (editor) | Frame the selection |

## Files

- `GameLogic/Game.cpp` -- exports the DLL entry points with `RW_GAME`.
- `GameLogic/Platformer2DLevel.h` -- the whole game.
- `Assets/` -- empty on purpose. The map uses raylib primitives.

## Next steps

- Replace the player rectangle with a sprite: `m_PlayerTexture.Load("player.png")`
  and `rw::DrawSprite(m_PlayerTexture, *m_Player)`.
- Swap the hardcoded platforms for a tilemap or scene props.
- Resizing the player in the Inspector also changes its physics box, because
  collision reads `m_Player->size`.
