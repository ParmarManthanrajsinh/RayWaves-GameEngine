# 0006 — Editor-owned 3D camera

## Context

2D maps render through `GameMap::GetViewCamera()`, which the editor can mutate,
so pan and zoom work. 3D maps render through their gameplay camera, which is
usually locked to a player. The editor only knew `GetViewCamera3D()` for gizmo
projection, so a 3D viewport could not be navigated at all.

Routing navigation into the gameplay camera is not acceptable: it would fight
the player controller and change what the game sees.

## Decision

The editor owns a separate `EditorCamera3D` (Unreal-style fly: right mouse look,
WASD/QE movement, Shift boost, middle-drag pan, wheel dolly). `GameMap` stores an
optional host override:

- `SetEditorViewCamera3D(camera)` while a 3D map is stopped and being edited.
- `ClearEditorViewCamera3D()` while playing or for 2D maps.
- `GetRenderCamera3D()` returns the override or `GetViewCamera3D()`.
- Maps draw 3D with `BeginView3D()` / `EndView3D()`.

The editor derives the camera once per map from `GetViewCamera3D()`, keeps it
for the session, and re-derives after map switches. Selection uses a ray cast
against editor AABBs built from `AddEntity` data.

## Consequences

- Gameplay code writes zero camera/editor code; one wrapper call replaces
  `BeginMode3D`.
- Flying never changes gameplay camera state.
- `GameMap` gains a `Camera3D` and a bool, so the GameLogic ABI moves to 7 and
  projects must rebuild.
- 3D maps that still call `BeginMode3D(theirCamera)` keep working but get no
  editor navigation until migrated.
- The editor camera is session-only; it is re-derived from the gameplay camera
  after a restart.
- Orthographic gameplay cameras are approximated as perspective in the editor
  camera.
