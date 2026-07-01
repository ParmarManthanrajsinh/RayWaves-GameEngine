# RayWaves Engine - API Reference

*Where code changes flow like waves 🌊*

This reference guide covers the essential classes and methods you'll use to build games with RayWaves.

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
| `SetTargetFPS(int fps)` | Set the target frame rate for this map. |

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
// In GameLogic/RootManager.cpp
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