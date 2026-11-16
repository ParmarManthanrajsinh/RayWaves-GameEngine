# ThirdPerson3D Template

Starter for a third-person 3D game. Includes an orbit camera, a capsule
character with gravity and jumping, and axis-separated collision against
procedural boxes.

## Controls

| Input | Action |
|---|---|
| `WASD` / arrow keys | Move relative to the camera |
| `Space` | Jump |
| `Shift` | Sprint |
| Right mouse drag | Orbit (editor) |
| Mouse movement | Orbit (standalone game) |
| Mouse wheel | Zoom |

The editor keeps a visible cursor and orbits on right-drag. The standalone
runtime disables the cursor through `GameMap::b_IsStandalone()` and orbits
continuously; the cursor is restored when the map is destroyed.

## Files

- `ThirdPersonMap` -- arena setup, input translation, and HUD.
- `ThirdPersonCamera` -- yaw/pitch/distance orbit with smooth follow.
- `Player` -- capsule movement, gravity, jump, AABB collision, `StateBag` state.
- `SceneTarget` -- 2x supersampling target (SSAA) for smoother 3D edges.
- `RootManager.cpp` -- DLL exports and map registration.

The template is asset-free and uses only raylib primitives. Replace the
procedural boxes and capsule with your own models when ready.

## Antialiasing

Raylib render textures cannot request MSAA, so the scene is rendered at twice
the viewport resolution and downsampled with bilinear filtering (SSAA). This
works inside the editor viewport as well as the standalone runtime. Tune
`SCALE` in `SceneTarget.h` if you need a different quality/cost tradeoff.

## Hot reload

`Player` saves its position through `SaveState`/`LoadState`; the camera saves
yaw, pitch, and distance. Compiling keeps you in the same spot.

## Notes

RayWaves does not provide a 3D scene editor yet. The camera lives in gameplay
code, which makes it easy to tune but means no viewport gizmos.
