# FirstPerson3D Template

Starter for a first-person 3D game. Includes a free-looking camera and flat
plane movement over a procedural arena.

## Controls

| Input | Action |
|---|---|
| `WASD` / arrow keys | Move on the ground plane |
| `Shift` | Sprint |
| Hold right mouse | Look around (editor) |
| Mouse movement | Look around (standalone game) |

The editor keeps a visible cursor and looks on right-drag. The standalone
runtime disables the cursor through `GameMap::b_IsStandalone()` and looks
continuously; the cursor is restored when the map is destroyed.

## Files

- `FirstPersonMap` -- arena, movement clamping, markers, crosshair, and HUD.
- `FirstPersonCamera` -- yaw/pitch look, position, and forward/right vectors.
- `SceneTarget` -- 2x supersampling target (SSAA) for smoother 3D edges.
- `RootManager.cpp` -- DLL exports and map registration.

The template is asset-free and uses only raylib primitives.

## Antialiasing

Raylib render textures cannot request MSAA, so the scene is rendered at twice
the viewport resolution and downsampled with bilinear filtering (SSAA). This
works inside the editor viewport as well as the standalone runtime. Tune
`SCALE` in `SceneTarget.h` if you need a different quality/cost tradeoff.

## Hot reload

Position, yaw, and pitch are stored in `StateBag`, so compiling keeps the
camera where it was.

## Notes

RayWaves does not provide a 3D scene editor yet. Everything in the viewport
comes from gameplay code.
