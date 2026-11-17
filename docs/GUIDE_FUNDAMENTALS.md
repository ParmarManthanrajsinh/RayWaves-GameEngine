# RayWaves Fundamentals

*Every game developer must understand these concepts*

---

## 1. The DLL Contract

RayWaves compiles your `GameLogic/` code into a **DLL** (`GameLogic.dll`). The editor loads it at runtime, calls into it, and hot-reloads it when you recompile.

Every project **must** export three symbols. Raw `GameMap` projects write them by
hand in `RootManager.cpp`; `rw` projects generate them with `RW_GAME`.

```cpp
#include "Engine/GameMap.h"
#include <cstdint>

static MapManager* s_GameMapManager = nullptr;

extern "C" __declspec(dllexport) uint32_t GetGameLogicAbiVersion()
{
    return RAYWAVES_GAMELOGIC_ABI_VERSION;
}

extern "C" __declspec(dllexport) GameMap* CreateGameMap()
{
    if (s_GameMapManager == nullptr)
    {
        s_GameMapManager = new MapManager();

        // Register all your maps here (one-time setup)
        s_GameMapManager->RegisterMap<MyLevel>("MyLevel");
    }

    // Set which map loads first
    s_GameMapManager->b_GotoMap("MyLevel");
    return s_GameMapManager;
}

extern "C" __declspec(dllexport) void DestroyGameMap(GameMap* map_manager)
{
    delete map_manager;
    if (map_manager == s_GameMapManager)
        s_GameMapManager = nullptr;
}
```

### Key Rules

- **GetGameLogicAbiVersion** must return `RAYWAVES_GAMELOGIC_ABI_VERSION` (defined in `Engine/GameMap.h`). The editor and the standalone runtime **refuse to load** any DLL that is missing this export or returns a mismatched version — rebuild your GameLogic with the current engine version if you see this error.
- **CreateGameMap** is called once when the DLL loads. `RegisterMap<>` calls must happen inside the `if (s_GameMapManager == nullptr)` guard — they register map types, not map instances.
- **DestroyGameMap** is called when the editor closes or unloads the DLL. It must clean up all memory.
- The `static` pointer outside the guard means **map registrations survive hot-reloads**. On recompile, `CreateGameMap` is called again, but `s_GameMapManager` is not null (since the global variable in the DLL persists), so registration is skipped. Only `b_GotoMap` runs to restore the current map.

---

## 2. Map Lifecycle

Every class inheriting `GameMap` follows this lifecycle:

```
Constructor  (set map name, default member values)
    |
Initialize()  (load textures, sounds, allocate resources)
    |
Update(dt) <---> Draw()   (game loop, every frame)
    |
SaveState(out)           (before hot-reload unload)
    |
~Destructor              (unload textures, free memory)
    |
LoadState(in)            (after new instance constructed + Initialize())
```

`rw::Level` maps this to hooks: `OnStart` = `Initialize`, `OnUpdate` = `Update`,
`OnDraw`/`OnDrawUI` = `Draw`, `OnSave`/`OnLoad` = `SaveState`/`LoadState`.
Entity transforms are saved and restored automatically.

### Critical: What Happens on Hot-Reload

When you click **Compile**:

1. `SaveState()` is called on the current map — you save values you want to preserve (player position, score, etc.) into the `StateBag`.
2. The old `GameLogic.dll` is unloaded — all its memory is freed.
3. The new `GameLogic.dll` is loaded — `CreateGameMap()` is called.
4. `s_GameMapManager` is **null** in the new DLL (it's a fresh global), so `new MapManager()` runs and maps are re-registered.
5. The editor requests the previous map. `Initialize()` runs on the new map instance.
6. `LoadState()` is called with the same `StateBag` — you restore preserved values.

> **Important:** `SaveState`/`LoadState` use `StateBag` which can only store primitives: `float`, `int`, `bool`, `string`, `Vector2`. **Do not store pointers, textures, or sounds** in StateBag — they will be invalid after reload. Load them fresh in `Initialize()`.

---

## 3. StateBag Deep Dive

`StateBag` is a typed key-value store. It is the **only** way to preserve game state across hot-reloads.

```cpp
// Save
void MyMap::SaveState(StateBag& out) const
{
    out.SetFloat("player_x", m_Player.GetPosition().x);
    out.SetFloat("player_y", m_Player.GetPosition().y);
    out.SetInt("score", m_Score);
    out.SetBool("has_key", m_bHasKey);
}

// Restore
void MyMap::LoadState(const StateBag& in)
{
    m_Player.SetPosition({
        in.GetFloat("player_x", 100.0f),
        in.GetFloat("player_y", 100.0f)
    });
    m_Score = in.GetInt("score", 0);
    m_bHasKey = in.GetBool("has_key", false);
}
```

### What Can Be Stored

| Type | Methods | Notes |
|------|---------|-------|
| float | `SetFloat` / `GetFloat` | Second arg is default if key missing |
| int | `SetInt` / `GetInt` | |
| bool | `SetBool` / `GetBool` | |
| string | `SetString` / `GetString` | |
| Vector2 | `SetVector2` / `GetVector2` | Stored as two floats internally |

### What Cannot Be Stored

- **Pointers** — memory addresses are invalid after reload
- **Textures** — GPU resources are destroyed on DLL unload
- **Sounds** — audio resources are destroyed on DLL unload
- **Complex objects** with internal allocations (unless they've saved into StateBag field by field)

### CRT Heap Warning

StateBag allocates memory internally (for `std::string` and `std::unordered_map`). Both `RayWaves.exe` and `GameLogic.dll` **must** use the same C Runtime (CRT) to avoid heap corruption across the DLL boundary. The Zig toolchain (default) always uses dynamic CRT — safe by default. If building with MSVC, ensure both targets use `/MD` (dynamic), not `/MT` (static).

### Static vs Member Variables on Reload

| Variable type | Survives reload? | Notes |
|--------------|-----------------|-------|
| `static` local/global | **Yes** | Persistent in DLL even across reload — use for singletons, caches |
| Member variables | **No** | Reset to constructor defaults on reload — use StateBag to preserve |
| `static constexpr` | **Yes** | Compile-time constant, always valid |

---

## 4. Tutorial: Your First Level

This walks you through a playable level using the simple `rw` API.

### Step 1: Create a New Project

1. Launch `RayWaves.exe`.
2. Click **New Project**.
3. Name it `MyGame`, choose the **Simple2D** template (the default).
4. Click **Create**.

You get:

```text
MyGame/
├── GameLogic/
│   ├── Game.cpp            # RW_GAME export
│   └── Simple2DLevel.h     # the whole game
└── Assets/
    └── player.png
```

### Step 2: Edit the Level

Replace `GameLogic/Simple2DLevel.h` with your own level. `rw::Level` handles the
`GameMap` lifecycle, the camera, and the editor registration:

```cpp
#pragma once
#include "rw.h"

class MyLevel : public rw::Level
{
public:
    MyLevel() : rw::Level("MyLevel") {}

protected:
    void OnStart() override
    {
        m_Player = Spawn("Player", { 100, 100 }, { 32, 32 }, rw::Origin::TopLeft, "Player");
    }

    void OnUpdate(float delta_time) override
    {
        constexpr float Speed = 200.0f;
        if (IsKeyDown(KEY_RIGHT)) m_Player->position.x += Speed * delta_time;
        if (IsKeyDown(KEY_LEFT)) m_Player->position.x -= Speed * delta_time;
        if (IsKeyDown(KEY_UP)) m_Player->position.y -= Speed * delta_time;
        if (IsKeyDown(KEY_DOWN)) m_Player->position.y += Speed * delta_time;
    }

    void OnDraw() override
    {
        DrawRectangleV(m_Player->position, m_Player->size, RED);
        DrawText("My First Level", 10, 10, 20, DARKGRAY);
    }

private:
    rw::Entity* m_Player = nullptr;
};
```

Point `GameLogic/Game.cpp` at the new class:

```cpp
#include "MyLevel.h"
RW_GAME(MyLevel)
```

`RW_GAME` expands to the three DLL exports. You never write them by hand.

### Step 3: Compile and Play

1. Click **Compile** in the editor toolbar.
2. The level loads automatically after the build (~2-5 seconds).
3. Use the arrow keys to move the red square.
4. Select `Player` in the Scene panel to move it with the gizmo or edit it in
   the Inspector.

### Step 4: Add a Second Level

Create `GameLogic/MyMenu.h`. Menus draw in screen space, so use `OnDrawUI`:

```cpp
#pragma once
#include "rw.h"

class MyMenu : public rw::Level
{
public:
    MyMenu() : rw::Level("Menu") {}

protected:
    void OnUpdate(float) override
    {
        if (IsKeyPressed(KEY_SPACE))
        {
            RequestGotoMap("MyLevel");
        }
    }

    void OnDrawUI() override
    {
        ClearBackground(DARKBLUE);
        DrawText("Press SPACE to play", 100, 200, 30, WHITE);
    }
};
```

Create a small game class that registers both levels. `rw::Game` is a
`MapManager`, so level IDs are what transitions use:

```cpp
#pragma once
#include "MyLevel.h"
#include "MyMenu.h"

class MyGame : public rw::Game
{
public:
    MyGame()
    {
        AddLevel<MyMenu>("Menu");
        AddLevel<MyLevel>("MyLevel");
        Start("Menu");
    }
};
```

And in `Game.cpp`:

```cpp
#include "MyGame.h"
RW_GAME(MyGame)
```

Compile again. Press SPACE to move from the menu to the level.

### RequestGotoMap

`RequestGotoMap(map_id, force_reload)` is the safe way to change maps. Use it anywhere in your level code:
- Call it during `OnUpdate()` on user input (key press, collision).
- Set `force_reload = true` to reload the same map (reset state).
- The actual transition happens after `Update()` returns — not immediately.

When you need the low-level API instead (custom cameras, non-entity state),
`SlimeQuest` shows the raw `GameMap` + `RootManager.cpp` path.

---

## 5. Debugging

### Terminal and Console

The editor has one **Terminal** window with tabs:

- **Console** is a non-interactive tab. It shows `std::cout`, `std::cerr`, raylib logs, loader diagnostics, and build output. It supports selection, copy, timestamps, and clearing.
- **Shell tabs** are real Windows shells attached to a ConPTY. They start in the project root, accept input, run interactive commands, and keep their own scrollback.

```cpp
std::cout << "Player position: " << m_PlayerPos.x << ", " << m_PlayerPos.y << std::endl;
```

Shell shortcuts:

| Shortcut | Action |
|---|---|
| `Ctrl+C` | Copy selection, or send SIGINT to the shell |
| `Ctrl+V` / `Ctrl+Shift+V` | Paste |
| `Ctrl+Shift+A` | Select all |
| `Ctrl+Shift+T` | New shell tab |
| `Ctrl+Shift+W` | Close active shell tab |
| `Ctrl+Home` / `Ctrl+End` | Scroll to top / bottom |
| `Shift+PageUp` / `Shift+PageDown` | Scroll history |

The shell is auto-detected in this order: PowerShell 7 (`pwsh.exe`), Windows PowerShell, then `cmd.exe`. ConPTY requires Windows 10 version 1809 or newer; on older systems the Console tab still works.

### Common Crash Causes

| Symptom | Probable cause |
|---------|---------------|
| Access violation on hot-reload | Stored a pointer/texture in StateBag, or didn't reload resources in `Initialize()` / `OnStart()` |
| Texture black/purple on reload | Texture loaded in constructor (before `Initialize`) — GPU resource lost on DLL unload. **Load all resources in `Initialize()` / `OnStart()`, unload in destructor.** |
| "Unresolved external symbol" | Typo in function signature mismatch. Check that your override matches `GameMap` exactly. |
| Linker duplicate symbol | Defined a function in a header without `inline`. |
| StateBag values wrong | Forgot to set default value in `GetFloat(key, default)`. |

### Debug Checklist

1. Did you load textures/sounds **in `Initialize()` / `OnStart()`**, not the constructor?
2. Did you **unload textures/sounds in the destructor**?
3. Are you storing only **primitives** in StateBag (no pointers, no textures)?
4. Did you call `AssetResolver::Resolve()` for asset paths?
5. Did you use **forward slashes** in paths?

---

## See Also

- [GAME_DEVELOPER_GUIDE.md](GAME_DEVELOPER_GUIDE.md) — overview and quick start
- [GUIDE_REFERENCE.md](GUIDE_REFERENCE.md) — camera, input, audio, assets, export, patterns
- [TROUBLESHOOTING.md](TROUBLESHOOTING.md) — common issues
