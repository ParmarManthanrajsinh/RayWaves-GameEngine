# ThirdPerson3D Template

Starter for a third-person 3D game. Includes an orbit camera, a capsule
character with gravity and jumping, and axis-separated collision against
procedural boxes.

## Controls

### Editor (stopped)

| Input | Action |
|---|---|
| Hold right mouse | Look around |
| `WASD` (while looking) | Fly forward/back/strafe |
| `Q` / `E` | Move down / up |
| `Shift` | Fly faster |
| Middle mouse drag | Pan |
| Mouse wheel | Dolly in/out |
| `F` | Frame the selected entity |
| Left click | Select the nearest entity |

The editor camera is derived from the gameplay camera and owned by the editor,
so flying around the scene never changes what the game sees. `ThirdPersonMap::Draw`
calls `BeginView3D()`/`EndView3D()`, which swap in the editor camera while
stopped and the gameplay camera while playing.

### Play / standalone

| Input | Action |
|---|---|
| `WASD` / arrow keys | Move relative to the camera |
| `Space` | Jump |
| `Shift` | Sprint |
| Right mouse drag | Orbit (editor play) |
| Mouse movement | Orbit (standalone game) |
| Mouse wheel | Zoom |

The standalone runtime disables the cursor through `GameMap::b_IsStandalone()`
and orbits continuously; the cursor is restored when the map is destroyed.

## Files

- `ThirdPersonMap` -- arena setup, input translation, and HUD.
- `ThirdPersonCamera` -- yaw/pitch/distance orbit with smooth follow.
- `Player` -- capsule movement, gravity, jump, AABB collision, `StateBag` state.
- `SceneTarget` -- 2x supersampling target (SSAA) for smoother 3D edges.
- `RootManager.cpp` -- DLL exports and map registration.

The template is asset-free and uses only raylib primitives. Replace the
procedural boxes and capsule with your own models when ready.

The map exposes its 3D camera and registers the player as an editor entity with
`GameMap::AddEntity`, so selecting `Player` in the Scene panel shows a 3D
translate gizmo in the viewport while stopped. The Inspector edits the live
position and size the gameplay code uses.

## Antialiasing

Raylib render textures cannot request MSAA, so the scene is rendered at twice
the viewport resolution and downsampled with bilinear filtering (SSAA). This
works inside the editor viewport as well as the standalone runtime. Tune
`SCALE` in `SceneTarget.h` if you need a different quality/cost tradeoff.

## Hot reload

`Player` saves its position through `SaveState`/`LoadState`; the camera saves
yaw, pitch, and distance. Compiling keeps you in the same spot.

## Notes

This template uses the raw `GameMap` API because it is 3D; the `rw` facade in
`Engine/rw.h` is 2D-only. For a 2D starter use `Simple2D` or `Platformer2D`.
The only editor-visible object is `Player`; everything else is drawn from
gameplay code.
