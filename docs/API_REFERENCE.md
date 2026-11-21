# RayWaves Engine - API Reference

*Where code changes flow like waves 🌊*

This reference guide covers the essential classes and methods you'll use to build games with RayWaves.

---

## ⚡ rw — the simple API

`Engine/rw.h` is the recommended starting point. It is header-only and sits on
top of `GameMap`, so everything below still applies when you need it.

```cpp
#include "rw.h"

class MyLevel : public rw::Level
{
public:
    MyLevel() : rw::Level("MyLevel") {}

protected:
    void OnStart() override;
    void OnUpdate(float dt) override;
    void OnDraw() override;   // world space
    void OnDrawUI() override; // screen space

private:
    rw::Texture m_Tex;
    rw::Entity* m_Player = nullptr;
};

RW_GAME(MyLevel) // exports the DLL entry points
```

### rw::Level

| Member | Description |
|--------|-------------|
| `Spawn(name, position, size, origin)` | Creates an entity, registers it with the editor, returns `Entity*`. A fifth `type` argument sets `Entity::type`. |
| `Destroy(entity)` / `ClearEntities()` | Removes entities and their editor registrations. |
| `Entities()` | `const std::vector<std::unique_ptr<Entity>>&` in spawn (= draw) order. |
| `Cam()` | `rw::Camera` for this level. |
| `SetBackground(color)` | Color passed to `ClearBackground` before `OnDraw`. |
| `OnStart()` | Rebuild the level here. Runs on load, restart, and hot reload. |
| `OnUpdate(dt)` | Per-frame logic. |
| `OnDraw()` | Per-frame world-space drawing, with `Cam()` already begin'd. |
| `OnDrawUI()` | Per-frame screen-space drawing (HUD). |
| `OnSave(out)` / `OnLoad(in)` | Extra `StateBag` state for hot reload; entity transforms are save/restored automatically. |

`rw::Level` implements `GetViewCamera`, `GetMutableViewCamera`, `Initialize`,
`Update`, `Draw`, `SaveState`, and `LoadState` as `final`.

### rw::Entity

| Field | Description |
|-------|-------------|
| `id` | Editor id (`0` when not registered). |
| `name`, `type` | Shown in the Scene hierarchy and Inspector. |
| `position` | World position of the anchor point. |
| `size` | Draw/selection size. |
| `origin` | Offset from `position` to the draw rect's top-left; `rw::Origin::Center` sets `-size/2`. |
| `rotation` | Degrees. |
| `visible` | Toggled by the hierarchy eye icon; respected by `rw::DrawSprite`. |

### rw::Texture / rw::Camera / helpers

| API | Description |
|-----|-------------|
| `rw::Texture::Load(path)` | Loads through `AssetResolver`; unloads the previous handle first. |
| `rw::Texture::Unload()` | Explicit unload; the destructor calls it too. |
| `rw::Camera::Follow(target, dt, speed)` | Smooth follow. |
| `rw::Camera::Snap(target)` | Instant move. |
| `rw::Camera::SetBounds(l, r, t, b)` | Clamp the camera to a level rect. |
| `rw::Camera::SetZoom` / `SetMinZoom` | Zoom control. |
| `rw::Camera::ScreenToWorld` / `WorldToScreen` | Coordinate conversion. |
| `rw::Camera::Begin()` / `rw::Camera::End()` | `BeginMode2D` / `EndMode2D` wrappers. |
| `rw::DrawSprite(texture, entity, tint)` | `DrawTexturePro` using the entity rect/rotation. |
| `rw::DrawEntityBox(entity, color)` | Outline the entity rect. |

### rw::Game (multiple levels)

```cpp
class MyGame : public rw::Game
{
public:
    MyGame()
    {
        AddLevel<MenuLevel>("menu");
        AddLevel<PlayLevel>("play");
        Start("menu");
    }
};
RW_GAME(MyGame)
```

| Method | Description |
|--------|-------------|
| `AddLevel<T>(id, description)` | Registers a `rw::Level` subclass. |
| `Start(id)` | Initial level loaded on first `Initialize`. |
| `GoTo(id, force)` | Request a transition. |

---

## 🗺️ GameMap

The `GameMap` class is the foundation of every level, scene, or screen in your game. Inherit from this class to create your logic.

### Core Methods

| Method | Description |
|--------|-------------|
| `void Initialize()` | Called once when the map is loaded. Setup variables and assets here. |
| `void Update(float delta_time)` | Called every frame (logic). `delta_time` is seconds since last frame. |
| `void Draw()` | Called every frame (rendering). Use Raylib draw functions here. |
| `void Cleanup()` | Called when the map is unloaded or game closes. |

### Transitions & properties

| Method | Description |
|--------|-------------|
| `RequestGotoMap("MapID")` | Request a switch to another registered map. |
| `GetMapName()` | Returns the ID string of the current map. |
| `GetViewCamera()` | Camera used to render the map; the editor uses it for selection and dragging. Default is identity. |
| `GetMutableViewCamera()` | Optional mutable camera pointer. Enables viewport pan, zoom, and frame in the editor. |
| `b_Uses3DView()` | True for 3D maps. The editor switches to world-space gizmos, 3D unprojection, and an editor-owned orbit camera. |
| `GetViewCamera3D()` | Gameplay Camera3D. Used in play mode, in the standalone runtime, and as the starting view for the editor camera. |
| `BeginView3D()` / `EndView3D()` | Begin/end 3D drawing. Uses the editor camera while stopped and `GetViewCamera3D()` otherwise. |
| `GetRenderCamera3D()` | Camera that will actually be used by `BeginView3D()` (editor override or gameplay camera). |
| `SetEditorViewCamera3D(camera)` | Host-only override applied by the editor. Do not call from gameplay code. |
| `SetTargetFPS(int fps)` | Set the target frame rate for this map. |
| `b_IsStandalone()` | True in the exported game runtime, false in the editor. Useful for cursor capture and runtime-only systems. |

### Example Implementation

```cpp
#include "../Engine/GameMap.h"

class Level1 : public GameMap 
{
public:
    void Initialize() override 
    {
        // Load textures, sounds, etc.
    }

    void Update(float delta_time) override 
    {
        // Move player
        if (IsKeyDown(KEY_RIGHT)) playerPos.x += 100 * delta_time;
        
        // Go to next level
        if (playerPos.x > 1000) RequestGotoMap("Level2");
    }

    void Draw() override 
    {
        ClearBackground(RAYWHITE);
        DrawCircleV(playerPos, 20, RED);
    }
    
private:
    Vector2 playerPos = {0, 0};
};
```

---

## 💾 State Preservation (Hot-Reloading)

To preserve data when the engine reloads your C++ code, you must opt-in by overriding the `SaveState` and `LoadState` virtual methods. The engine passes a `StateBag` object to safely store and retrieve primitive types. These functions are called automatically during a hot-reload transition: `SaveState` right before the old map instance is destroyed, and `LoadState` right after the new instance is constructed.

| Method | Description |
|--------|-------------|
| `void SaveState(StateBag& out) const` | Store your variables into the `out` bag here. |
| `void LoadState(const StateBag& in)` | Retrieve your variables from the `in` bag here. |

### StateBag Methods

The `StateBag` class provides safe getters and setters. Getters include a default value parameter which is returned if the key does not exist.

| Method | Description |
|--------|-------------|
| `SetFloat(key, val)` / `GetFloat(key, def)` | Store or retrieve a `float`. |
| `SetInt(key, val)` / `GetInt(key, def)` | Store or retrieve an `int`. |
| `SetBool(key, val)` / `GetBool(key, def)` | Store or retrieve a `bool`. |
| `SetString(key, val)` / `GetString(key, def)` | Store or retrieve a `std::string`. |
| `SetVector2(key, val)` / `GetVector2(key, def)` | Store or retrieve a Raylib `Vector2`. |

---

## 📦 MapManager

The `MapManager` handles registration and switching of maps. You typically interact with this in `RootManager.cpp`.

### Key Methods

| Method | Description |
|--------|-------------|
| `RegisterMap<T>("ID")` | Registers a map class `T` with a unique string ID. |
| `b_GotoMap("ID")` | Switches to the specified map immediately. |
| `GetCurrentMapId()` | Returns the ID of the active map. |
| `GetAvailableMaps()` | Returns a list of all registered map IDs. |

### Registration Example

```cpp
// In GameLogic/RootManager.cpp — the three required exports
extern "C" __declspec(dllexport) uint32_t GetGameLogicAbiVersion()
{
    return RAYWAVES_GAMELOGIC_ABI_VERSION; // from Engine/GameMap.h
}

extern "C" __declspec(dllexport) GameMap* CreateGameMap() 
{
    if (s_GameMapManager == nullptr) 
    {
        s_GameMapManager = new MapManager();
        s_GameMapManager->RegisterMap<Level1>("Level1"); // "Level1" is the ID
        s_GameMapManager->RegisterMap<Level2>("Level2");
    }
    return s_GameMapManager;
}
```

> **ABI check:** The editor and runtime verify `GetGameLogicAbiVersion()` before loading a GameLogic DLL. A missing or mismatched version means the DLL was built against a different engine build — the load is rejected with a clear error instead of risking heap corruption across the DLL boundary. Additionally, `LoadDll` compares CRT imports of the host EXE and the DLL and refuses static-vs-dynamic CRT mixes for the same reason.

---

## ⚙️ GameConfig

Access and modify global engine settings like window resolution and VSync.

### t_WindowConfig Structure

```cpp
struct t_WindowConfig {
    int width = 1280;
    int height = 720;
    bool b_Fullscreen = false;
    bool b_Resizable = true;
    bool b_Vsync = true;
    int target_fps = 60;
    std::string title = "RayWaves Game";
};
```

### Accessing Config

```cpp
// Get the singleton instance
GameConfig& config = GameConfig::GetInstance();

// Load from file (config.ini)
config.m_bLoadFromFile();

// Access settings
t_WindowConfig& settings = config.GetWindowConfig();
settings.width = 1920;
settings.height = 1080;

// Save back to file
config.m_bSaveToFile();
```

---

## 🎮 GameEngine

The core engine class that manages the window and main loop. Usually, you won't need to touch this unless you are modifying the engine core.

| Method | Description |
|--------|-------------|
| `LaunchWindow(w, h, title)` | Opens the main game window. |
| `SetMap(map)` | Sets the active game map manually. |
| `ToggleFullscreen()` | Toggles between windowed and fullscreen. |
| `GetScene()` | Editor-authored sprite layer, saved as `Assets/<MapId>.scene`. |
| `b_SaveActiveScene()` | Writes the current scene file. |
| `b_LoadActiveScene()` | Reloads the current map's scene file. |

## 🧩 Scene

`Scene` is a flat list of `t_SceneObject` sprites. `GameEngine` draws it after
`GameMap::Draw()`, so gameplay code does not need to change. Objects use plain
raylib values:

```cpp
Scene& scene = engine.GetScene();
t_SceneObject& object = scene.AddObject("Crate");
object.m_TexturePath = "crate.png";
object.m_Position = { 400.0f, 300.0f };
object.m_Scale = { 2.0f, 2.0f };
```

Scene files live directly in `Assets/` as `<MapId>.scene` and are hand-editable.

## 🎮 Editor Entities

Gameplay objects can be edited in place without an ECS. The map registers a
pointer to the object's position:

```cpp
void MyMap::Initialize()
{
    AddEntity("Player", &m_PlayerPosition);
}
```

Optional fields (origin, type, rotation, visibility) use a designated
initializer:

```cpp
AddEntity("Player", &m_Position, &m_Size, { .Origin = { -32.0f, -32.0f }, .Type = "Player" });
```

| Method | Description |
|---|---|
| `AddEntity(name, position, size, options)` | One-line registration for 2D or 3D (`Vector3*`) positions. |
| `RegisterEditorEntity(name, position, size, id)` | Legacy short form. |
| `RegisterEditorEntity(t_EditorEntity)` | Full descriptor, including `m_Position3D`. |
| `UnregisterEditorEntity(id)` | Removes one entity. |
| `ClearEditorEntities()` | Clears the registry; the engine does this before every `Initialize()`. |
| `GetEditorEntities()` | Returns the current registry (`std::list<t_EditorEntity>`); entity pointers remain stable. |

The editor caches IDs and rebuilds pointers every frame. Live edits are not
saved into the scene file. Use stable storage for dynamic entities and
re-register after spawn, despawn, or reset.

If the map renders through a `Camera2D`, expose it so the editor's selection
rectangle and dragging match the rendered world:

```cpp
Camera2D GetViewCamera() const override
{
    return m_Camera.GetRaylibCamera();
}
```

Maps without a camera can ignore this; identity is used by default.

To enable editor viewport navigation, also expose a mutable pointer:

```cpp
Camera2D* GetMutableViewCamera() override
{
    return m_Camera.GetRaylibCameraPtr();
}
```

When stopped, the viewport supports middle-mouse pan, mouse-wheel zoom at the
cursor, and `F` to frame the selection. The camera is saved on play and restored
on stop.

## 🖼️ UI with RayGUI

RayWaves now includes native support for the `raygui` library to easily create in-game user interfaces like menus, HUDs, and buttons.

### Using RayGUI

1. Include the header in your map file:
   ```cpp
   #include <raygui.h>
   ```
2. Call `raygui` functions directly in your `Draw()` method.

> [!WARNING]
> The engine already compiles the `raygui` implementation for you in the core static library. **Do NOT** use `#define RAYGUI_IMPLEMENTATION` in your game logic code, as this will cause linker errors.

### Example UI

```cpp
#include "../Engine/GameMap.h"
#include <raygui.h>

class UIMap : public GameMap 
{
    void Draw() override 
    {
        ClearBackground(RAYWHITE);
        
        // Draw a styled button
        if (GuiButton({ 100, 100, 200, 50 }, "Click Me!"))
        {
            // Handle button click
            RequestGotoMap("NextLevel");
        }
    }
};
```

---

## 📚 Raylib Cheat Sheet

Since RayWaves is built on Raylib, you have access to all standard Raylib functions. Here are the most common ones:

### Input
- `IsKeyDown(KEY_SPACE)` - True if key is held
- `IsKeyPressed(KEY_SPACE)` - True once when pressed
- `GetMousePosition()` - Returns Vector2 mouse coordinates
- `IsMouseButtonPressed(MOUSE_LEFT_BUTTON)` - True on click

### Drawing
- `DrawTexture(tex, x, y, WHITE)` - Draw a loaded texture
- `DrawText("Hello", x, y, size, COLOR)` - Draw simple text
- `DrawRectangle(x, y, w, h, RED)` - Draw a filled rectangle
- `DrawCircle(x, y, r, BLUE)` - Draw a circle

### Audio
- `PlaySound(sound)` - Play a sound effect
- `PlayMusicStream(music)` - Play background music
- `UpdateMusicStream(music)` - Must call this every frame for music!

---

*For full Raylib documentation, visit the [Raylib Cheatsheet](https://www.raylib.com/cheatsheet/cheatsheet.html).*