# RayWaves Game Developer Guide

*How to make games with RayWaves*

> **Engine maintainer?** See [distribution.md](distribution.md) for packaging the editor itself. This guide is for end users who just want to make games.

---

## Quick Start

1. Launch `RayWaves.exe`. The Project Browser appears.
2. Click **New Project**, pick a name and location, choose a template.
3. Once open, the editor loads. Edit `.cpp` files in your project's `GameLogic/` folder.
4. Click **Compile** (toolbar button). Build runs automatically.
5. Changes hot-reload in ~0.5 seconds. No restarting.

> **Tip:** Double-click a `project.raywaves` file in Explorer (after registering file association under *Tools -> Register .raywaves file association*) to skip the browser.

---

## Workflow

```
Code (GameLogic/*.cpp) -> Click Compile -> DLL hot-reloads -> Play instantly
```

You never close the game window to change code.

---

## What Survives a Hot-Reload

When you click **Compile**, the editor builds a new `GameLogic.dll`, loads it
beside the old one (shadow copy, so the file never locks), saves state, swaps
the map, and restores state. Concretely:

**Survives (yes):**
- Anything you write in `SaveState` and read back in `LoadState` (`StateBag`
  keys: floats, ints, bools, strings, `Vector2`). This is the **only**
  supported channel for carrying game state across a reload.
- Map registrations (`RegisterMap<>` calls inside the `s_GameMapManager == nullptr`
  guard in `RootManager.cpp`) — they run once per process, not per reload.
- Anything `Initialize()` rebuilds from scratch (textures, sounds, fonts).

**Does not survive (no):**
- Raw pointers, texture/sound handles, or object references held in member
  variables — the old DLL is unloaded, so its code and statics are gone.
  Re-acquire them in `Initialize()` / `LoadState()`.
- `static` locals or globals holding gameplay state — they die with the old
  DLL. Put durable state in `StateBag` instead.
- Changes to what `SaveState` writes without a matching `LoadState` reader
  (or vice versa) — missing keys silently fall back to defaults.

**Rejected loads:**
- If the editor reports a *GameLogic ABI mismatch* (or a missing
  `GetGameLogicAbiVersion` export), your DLL was built with a different engine
  version. Just hit **Compile** again to rebuild it — old DLLs are refused
  rather than risk heap corruption across the DLL boundary.

**Weird state?** Press the **Restart** button to reset the map.

---

## Project Structure

```
MyGame/
├── project.raywaves      # Manifest (name, version, scene settings)
├── Assets/                # Textures, sounds, fonts, <MapId>.scene
├── GameLogic/             # YOUR C++ GAMEPLAY CODE
│   ├── RootManager.cpp    # DLL entry point, map registration
│   └── YourMap.cpp        # Any number of maps, entities, systems
└── .raywaves/             # Auto-managed build cache (do not touch)
```

The **FileSystem** panel opens levels by double-clicking `Assets/<MapId>.scene`,
imports assets flat into `Assets/`, and opens `GameLogic/` files in your default
code editor. The **Scene** and **Inspector** panels edit the map's sprite
prop layer and any gameplay entities the map registers.

See [ARCHITECTURE.md](ARCHITECTURE.md) for the full engine repo structure.

---

## The Simple API (`rw`)

`Engine/rw.h` is a thin, header-only layer over raylib and `GameMap`. A whole
2D game fits in one file:

```cpp
#include "rw.h"

class MyLevel : public rw::Level
{
public:
    MyLevel() : rw::Level("MyLevel") {}

protected:
    void OnStart() override
    {
        m_PlayerTexture.Load("player.png");
        m_Player = Spawn("Player", { 100, 300 }, { 48, 64 }, rw::Origin::Center);
        Cam().SetZoom(2.0f);
        Cam().Snap(m_Player->position);
    }

    void OnUpdate(float dt) override
    {
        if (rw::IsKeyDown(KEY_RIGHT)) { m_Player->position.x += 200.0f * dt; }
        Cam().Follow(m_Player->position, dt);
    }

    void OnDraw() override // world space, camera already active
    {
        rw::DrawSprite(m_PlayerTexture, *m_Player);
    }

private:
    rw::Texture m_PlayerTexture;
    rw::Entity* m_Player = nullptr;
};

RW_GAME(MyLevel)
```

What `rw` handles for you:

| You write | Framework does |
|-----------|----------------|
| `RW_GAME(MyLevel)` | The three DLL exports (`GetGameLogicAbiVersion`, `CreateGameMap`, `DestroyGameMap`). |
| `rw::IsKeyDown` / `rw::IsKeyPressed` | Replayable input for `rwcli run --input`; falls back to raylib at runtime. |
| `Spawn(...)` | Stable entity storage, editor registration, selection outline, inspector fields. |
| `Destroy(entity)` / `ClearEntities()` | Unregisters the entity and drops the pointer safely. |
| `rw::Texture` | Resolves `Assets/` paths, unloads on destruction and on reload. |
| `Cam()` | Follow, zoom, level bounds, viewport tracking, editor pan/zoom. |
| `OnStart/OnUpdate/OnDraw` | The full `GameMap` lifecycle, including restart and hot reload. |
| `SaveState/LoadState` | Entity transforms are carried across hot reloads automatically. |

Rules of thumb:

- Build the level in `OnStart()`. It runs again on restart and hot reload, so
  spawn everything there rather than in the constructor.
- `OnDraw()` draws in world space with plain raylib. `OnDrawUI()` draws in screen
  space (HUD, text).
- Entity fields are plain data: `position`, `size`, `origin`, `rotation`,
  `visible`. Mutate them directly.
- Need multiple levels? Derive from `rw::Game` instead, call
  `AddLevel<MyLevel>("play")`, `Start("play")`, and `GoTo("menu")` for
  transitions.
- Want a sprite's hitbox to match its draw rect? Use `rw::Origin::Center` for
  centered entities; the editor uses the same rect for selection.

The `Simple2D` template is this example in project form.

---

## Editing Gameplay Entities (raw `GameMap`)

The editor can move live gameplay objects such as the player or enemies without
an ECS or a framework. Register a pointer to the object's position after the
object exists:

```cpp
void MyMap::Initialize()
{
    playerPos = { 400.0f, 300.0f };
    AddEntity("Player", &playerPos, &m_PlayerEditorSize);
}
```

Optional fields expose more to the Inspector:

```cpp
AddEntity("Player", &m_Position, &m_Size, { .Origin = { -32.0f, -32.0f }, .Type = "Player" });
```

Use `t_EditorEntity` directly when you need 3D positions
(`m_Position3D` / `m_Size3D`):

```cpp
t_EditorEntity entity;
entity.m_Name = "Player";
entity.m_Type = "Player";
entity.m_Position = &m_Position;
entity.m_Size = &m_Size;
entity.m_Rotation = &m_Rotation;
entity.m_Visible = &m_Visible;
RegisterEditorEntity(entity);
```

Keep `m_Size` (or `m_Size3D`) pointing at storage that outlives registration:
a map member, a heap object, or an `rw::Entity`.

Rules that keep this safe:

- Register **after** the entity is constructed and its storage is stable.
- The engine clears the registry before every `Initialize()`, so you only need
  `ClearEditorEntities()` when your own code destroys or moves entities mid-run.
- Re-register after spawning, despawning, or resetting entities yourself.
- Use stable storage for dynamic entities: `std::deque<Entity>` or
  `std::vector<std::unique_ptr<Entity>>`.
- A `std::vector<Entity>` by value is fine only if you register after all pushes
  and do not push again until you rebuild.

The editor caches only entity IDs. It rebuilds pointers every frame, so changing
the registry is safe as long as no stale pointer is left behind.

Live edits are runtime only. They are not saved into the scene file. Persisting
entity positions requires stable IDs and a game-side override path.

If the map uses a `Camera2D`, override `GetViewCamera()` so the editor draws the
selection rectangle and handles dragging in the same world space:

```cpp
Camera2D GetViewCamera() const override
{
    return m_Camera.GetRaylibCamera();
}
```

Add the mutable camera hook to enable editor navigation when stopped:

```cpp
Camera2D* GetMutableViewCamera() override
{
    return m_Camera.GetRaylibCameraPtr();
}
```

`rw::Level` implements both for you.

Viewport controls while stopped:

- **2D maps:** middle mouse drag pans, mouse wheel zooms at the cursor.
- **3D maps:** hold right mouse to look, `WASD` to fly, `Q`/`E` to move down/up,
  `Shift` to fly faster. Middle mouse drag pans, mouse wheel dollies. The editor
  camera is derived from the gameplay camera and owned by the editor, so the game
  camera never changes while editing.
- `F`: frame the selected entity or scene object.
- Click a gameplay object to select it, then drag it or use its move gizmo.
  Hold `Ctrl` to snap.
- Play saves the editor camera; pause/stop restores it.

3D maps render through the editor camera automatically when stopped. The only
code they need is `BeginView3D()` / `EndView3D()` instead of `BeginMode3D`:

```cpp
void My3DMap::Draw()
{
    DrawHud();
    BeginView3D();   // editor camera while stopped, gameplay camera while playing
    DrawWorld();
    EndView3D();
}
```

The **Inspector** groups values into `Transform`, `Appearance`, `Rendering`, and
`Gameplay` sections. Position/scale rows use colored X/Y/Z fields, and every
field is labeled, so values can be read at a glance. While playback is running
gameplay fields are read-only; stop to edit them.

---

## Which Template to Start From

| Template | Maps | Complexity | Use When |
|----------|------|-----------|----------|
| **Simple2D** | 1 level (`rw::Level`) | Minimal | Starting a 2D game; the recommended default |
| **Platformer2D** | 1 level (`rw::Level`) + physics | Low | Movement, jumping, and collision on the simple API |
| **ThirdPerson3D** | 1 map (raw `GameMap`, 3D) | Low | Third-person 3D starter |
| **SlimeQuest** | Multi-map (raw `GameMap`) | Full | Reference for a production 2D game |

See [GUIDE_FUNDAMENTALS.md](GUIDE_FUNDAMENTALS.md) for understanding the DLL contract, map lifecycle, StateBag, and your first map tutorial.

See [GUIDE_REFERENCE.md](GUIDE_REFERENCE.md) for camera, input, audio, assets, export, and common patterns.

---

## Need Help?

- **Game crashed?** Check the Console output in the editor.
- **Compile failed?** Look at error messages in the Console / Message Log.
- **Weird state?** Press the **Restart** button to reset the map.
- **First compile slow?** Tools download automatically on first use (~200 MB). Internet required. See [TROUBLESHOOTING.md](TROUBLESHOOTING.md).
