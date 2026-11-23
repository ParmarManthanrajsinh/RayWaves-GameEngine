# 0005 — A small game-facing API (`rw`)

## Context

A new RayWaves project required roughly a dozen concepts before drawing a
sprite: DLL exports, `MapManager`, `RegisterMap`, `ClearEditorEntities`,
`t_EditorEntity` assembly, camera overrides, texture lifetime, and viewport
bookkeeping. That is engine surface leaking into gameplay, and it made simple
raylib-style code impossible.

The engine cannot drop `GameMap`, `MapManager`, or the editor entity registry:
the editor, hot reload, ABI check, and map transitions are built on them.

## Decision

Add `Engine/rw.h`, a header-only facade over the existing contracts:

- `rw::Level` is a `GameMap` that owns entities, a camera, and the editor
  registration lifecycle.
- `rw::Entity` is plain data (`position`, `size`, `origin`, `rotation`,
  `visible`) held in stable storage, not an ECS.
- `rw::Texture` owns a texture and resolves `Assets/` paths through
  `AssetResolver`.
- `rw::Camera` wraps `Camera2D` with follow, zoom, and bounds.
- `rw::Game` is a `MapManager` with `AddLevel<T>` / `Start` sugar.
- `RW_GAME(Type)` expands to the three exports the loader requires.

Also add `GameMap::AddEntity(name, position, size, options)` so raw `GameMap`
users can register an entity in one line instead of building `t_EditorEntity`
by hand.

The header stays flat in `Engine/` because the distribution package copies
`Engine/*.h` only.

## Consequences

- No ECS, no component registry, no retained scene graph. Entities are drawn
  with normal raylib calls in `OnDraw`.
- Gameplay still runs through `GameMap`, so the editor, hot reload, and
  transitions work unchanged.
- No ABI bump: `rw` adds no virtuals to `GameMap` and no fields to
  `t_EditorEntity`. `AddEntity` is a non-virtual forwarder.
- Raw `GameMap` remains supported, including 3D maps and custom map managers.
- `rw::Level::Initialize`, `Update`, `Draw`, `SaveState`, and `LoadState` are
  `final`; subclasses use the `On*` hooks.
- Entity storage uses `std::unique_ptr` (one allocation per entity) so addresses
  stay stable for the editor registry.
- Entity transform persistence across hot reload is keyed by spawn order; a
  non-deterministic `OnStart` loses that alignment.
- `RW_GAME` defines global `extern "C"` exports, so it can appear only once per
  game module.

The facade later grew beyond drawing primitives; see
[0011 — Split and extend the `rw` game-facing API](0011-rw-facade-headers.md)
for the flat subsystem headers and the input, audio, collision, animation,
timer, and UI additions.
