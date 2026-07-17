# RayWaves Fundamentals

*Every game developer must understand these concepts*

---

## 1. The DLL Contract

RayWaves compiles your `GameLogic/` code into a **DLL** (`GameLogic.dll`). The editor loads it at runtime, calls into it, and hot-reloads it when you recompile.

Every project **must** provide two exported C functions in `RootManager.cpp`:

```cpp
static MapManager* s_GameMapManager = nullptr;

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

## 4. Tutorial: Your First Map

This walks you through creating a simple playable level from scratch.

### Step 1: Create a New Project

1. Launch `RayWaves.exe`.
2. Click **New Project**.
3. Name it "MyGame", choose "Empty" template.
4. Click **Create and Open**.

You now have `MyGame/GameLogic/RootManager.cpp` with a minimal inline map.

### Step 2: Create Your Map Files

Create `MyGame/GameLogic/MyLevel.h`:

```cpp
#pragma once
#include "Engine/GameMap.h"

class MyLevel : public GameMap
{
public:
    MyLevel();

    void Initialize() override;
    void Update(float delta_time) override;
    void Draw() override;

private:
    Vector2 m_PlayerPos{ 100.0f, 100.0f };
    float m_Speed = 200.0f;
};
```

Create `MyGame/GameLogic/MyLevel.cpp`:

```cpp
#include "MyLevel.h"
#include <iostream>

MyLevel::MyLevel() : GameMap("MyLevel") {}

void MyLevel::Initialize()
{
    std::cout << "MyLevel initialized!\n";
}

void MyLevel::Update(float delta_time)
{
    if (IsKeyDown(KEY_RIGHT)) m_PlayerPos.x += m_Speed * delta_time;
    if (IsKeyDown(KEY_LEFT))  m_PlayerPos.x -= m_Speed * delta_time;
    if (IsKeyDown(KEY_UP))    m_PlayerPos.y -= m_Speed * delta_time;
    if (IsKeyDown(KEY_DOWN))  m_PlayerPos.y += m_Speed * delta_time;
}

void MyLevel::Draw()
{
    ClearBackground(RAYWHITE);
    DrawRectangleV(m_PlayerPos, {32, 32}, RED);
    DrawText("My First Level", 10, 10, 20, DARKGRAY);
}
```

### Step 3: Register in RootManager.cpp

Replace the content of `RootManager.cpp`:

```cpp
#include "Engine/MapManager.h"
#include "MyLevel.h"

static MapManager* s_GameMapManager = nullptr;

extern "C" __declspec(dllexport) GameMap* CreateGameMap()
{
    if (s_GameMapManager == nullptr)
    {
        s_GameMapManager = new MapManager();
        s_GameMapManager->RegisterMap<MyLevel>("MyLevel");
    }

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

### Step 4: Compile and Play

1. Click **Compile** in the editor toolbar.
2. After build completes (~2-5 seconds), the map loads automatically.
3. Use arrow keys to move the red square.

### Step 5: Add a Second Map

Create `MyGame/GameLogic/MyMenu.h`:

```cpp
#pragma once
#include "Engine/GameMap.h"

class MyMenu : public GameMap
{
public:
    MyMenu();
    void Initialize() override;
    void Update(float delta_time) override;
    void Draw() override;

private:
    float m_Time = 0;
};
```

Create `MyGame/GameLogic/MyMenu.cpp`:

```cpp
#include "MyMenu.h"

MyMenu::MyMenu() : GameMap("Menu") {}

void MyMenu::Initialize() {}

void MyMenu::Update(float dt)
{
    m_Time += dt;
    if (IsKeyPressed(KEY_SPACE))
        RequestGotoMap("MyLevel");   // <-- triggers transition
}

void MyMenu::Draw()
{
    ClearBackground(DARKBLUE);
    DrawText("Press SPACE to play", 100, 200, 30, WHITE);
}
```

Register it alongside `MyLevel` in `RootManager.cpp`:

```cpp
s_GameMapManager->RegisterMap<MyMenu>("Menu");
s_GameMapManager->RegisterMap<MyLevel>("MyLevel");
```

And change the initial map:

```cpp
s_GameMapManager->b_GotoMap("Menu");   // start at menu
```

Compile again. Press SPACE to transition from menu to level.

### RequestGotoMap

`RequestGotoMap(map_id, force_reload)` is the safe way to change maps. Use it anywhere in your map code:
- Call it during `Update()` on user input (key press, collision).
- Set `force_reload = true` to reload the same map (reset state).
- The actual transition happens after `Update()` returns — not immediately.

---

## 5. Debugging

### Console Panel

The editor's Console shows output from `std::cout` and `std::cerr`. Use it liberally:

```cpp
std::cout << "Player position: " << m_PlayerPos.x << ", " << m_PlayerPos.y << std::endl;
```

### Common Crash Causes

| Symptom | Probable cause |
|---------|---------------|
| Access violation on hot-reload | Stored a pointer/texture in StateBag, or didn't reload resources in `Initialize()` |
| Texture black/purple on reload | Texture loaded in constructor (before `Initialize`) — GPU resource lost on DLL unload. **Load all resources in `Initialize()`, unload in destructor.** |
| "Unresolved external symbol" | Typo in function signature mismatch. Check that your override matches `GameMap` exactly. |
| Linker duplicate symbol | Defined a function in a header without `inline`. |
| StateBag values wrong | Forgot to set default value in `GetFloat(key, default)`. |

### Debug Checklist

1. Did you load textures/sounds **in `Initialize()`**, not the constructor?
2. Did you **unload textures/sounds in the destructor**?
3. Are you storing only **primitives** in StateBag (no pointers, no textures)?
4. Did you call `AssetResolver::Resolve()` for asset paths?
5. Did you use **forward slashes** in paths?

---

## See Also

- [GAME_DEVELOPER_GUIDE.md](GAME_DEVELOPER_GUIDE.md) — overview and quick start
- [GUIDE_REFERENCE.md](GUIDE_REFERENCE.md) — camera, input, audio, assets, export, patterns
- [TROUBLESHOOTING.md](TROUBLESHOOTING.md) — common issues
