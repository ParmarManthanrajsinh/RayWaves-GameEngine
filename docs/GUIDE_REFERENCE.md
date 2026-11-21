# RayWaves Reference

*Camera, input, audio, assets, export, and common patterns*

---

## 1. Camera System

If you use `rw::Level`, the camera is built in: `Cam().SetZoom(...)`,
`Cam().SetBounds(...)`, `Cam().Follow(target, dt)`, `Cam().Snap(target)`. See
[GAME_DEVELOPER_GUIDE.md](GAME_DEVELOPER_GUIDE.md#the-simple-api-rw).

The `GameCamera` class (included in the SlimeQuest template) provides smooth scrolling with bounds clamping for raw `GameMap` projects.

```cpp
GameCamera m_Camera;

void MyLevel::Initialize()
{
    m_Camera.Initialize({0, 0}, 2.5f);       // target position, zoom
    m_Camera.SetMinZoom(2.5f);                // prevent zoom-out past this
    m_Camera.SetBounds(-100, 2000, 0, 1000);  // left, right, top, bottom
}

void MyLevel::Update(float dt)
{
    m_Camera.FollowTarget(m_Player.GetPosition(), dt, 5.0f);  // smooth follow
}

void MyLevel::Draw()
{
    m_Camera.Begin();
    // All drawing here is in world coordinates — camera handles offset + zoom
    DrawRectangle(100, 100, 32, 32, RED);
    m_Camera.End();

    // Drawing after End() is in screen coordinates (for HUD)
    DrawText("HUD text", 10, 10, 20, WHITE);
}
```

When the viewport changes (window resize), call `m_Camera.UpdateViewport(width, height)`.

### Camera Methods

| Method | Purpose |
|--------|---------|
| `Initialize(target, zoom)` | Set initial position and zoom |
| `FollowTarget(pos, dt, smooth)` | Smoothly track a target position |
| `SetBounds(l, r, t, b)` | Clamp camera so it doesn't go outside level |
| `SetZoom(zoom)` | Set zoom directly |
| `SetMinZoom(min)` | Prevent zooming out past this value |
| `Begin() / End()` | Wrap world-space drawing calls |
| `UpdateViewport(w, h)` | Update when window is resized |

### 3D Cameras

The `ThirdPerson3D` template ships a gameplay-side 3D camera. It is a template
class, not an engine class; copy it into your project.

**Third-person orbit** (`ThirdPersonCamera`):

```cpp
m_Camera.Initialize(target);          // start behind the target
m_Camera.UpdateInput();               // right-drag orbit, wheel zoom
m_Camera.Follow(target, delta_time);  // smooth follow
m_Camera.Begin();
// draw 3D world here
m_Camera.End();
```

The editor keeps a visible cursor: the camera looks only while right-dragging.
The standalone runtime calls `DisableCursor()` and looks continuously, using
`GameMap::b_IsStandalone()` to tell the two apart. `MapManager` forwards that
mode to every new map, so transitions and hot starts behave the same. The
cursor is restored when a standalone map is destroyed.

Note: while the project is stopped, right mouse looks and `WASD` flies the
**editor** camera, not the gameplay camera. Gameplay look/orbit is unchanged
during play.

3D drawing works inside `GameMap::Draw()`: call `BeginView3D()`, draw, then
`EndView3D()`. `BeginView3D()` uses the editor's orbit camera while the project
is stopped and the gameplay camera while playing or in the standalone runtime.
The editor renders the result into the scene texture, depth buffer included.

3D maps override `GameMap::b_Uses3DView()` and `GameMap::GetViewCamera3D()`,
then register their player as an editor entity with `AddEntity`. Selecting that
entry in the Scene panel draws a world-space translate gizmo in the viewport.
Hold right mouse to look and fly with `WASD`/`Q`/`E` (`Shift` faster), middle-drag
to pan, wheel to dolly, and `F` to frame the selection. Clicking the viewport
ray-picks the nearest entity.
2D gameplay entities use a screen-space move gizmo; scene props additionally
get scale handles.

The 3D template includes `SceneTarget`, a 2x supersampling target (SSAA).
Raylib render textures cannot request MSAA, so the scene renders at twice the
viewport size and downsamples with bilinear filtering. HUD is drawn after the
resolve, so text stays sharp.

---

## 2. Input Handling

Put all input code in your map's `Update()` method.

### Keyboard

```cpp
void MyLevel::Update(float dt)
{
    // Continuous — true every frame while held
    if (IsKeyDown(KEY_RIGHT)) m_PlayerPos.x += speed * dt;

    // Single press — true only on the frame key goes down
    if (IsKeyPressed(KEY_SPACE)) m_Player.Jump();

    // Single release
    if (IsKeyReleased(KEY_SHIFT)) m_Player.StopRunning();
}
```

### Mouse

```cpp
Vector2 mouse = GetMousePosition();            // screen coordinates
bool clicked = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);

// Screen-relative to world coordinates under camera
Vector2 world = GetScreenToWorld2D(mouse, camera.GetRaylibCamera());
```

### Gamepad

```cpp
if (IsGamepadAvailable(0))
{
    float moveX = GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_X);
    float moveY = GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_Y);
    if (IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN))
        m_Player.Jump();
}
```

### Common Key Constants

`KEY_RIGHT`, `KEY_LEFT`, `KEY_UP`, `KEY_DOWN`, `KEY_SPACE`, `KEY_ENTER`, `KEY_ESCAPE`, `KEY_W`, `KEY_A`, `KEY_S`, `KEY_D`. See `raylib.h` for the full list.

---

## 3. Audio

```cpp
Sound m_JumpSound;

void MyLevel::Initialize()
{
    // InitAudioDevice() is called automatically by the engine
    m_JumpSound = LoadSound(AssetResolver::Resolve("Sounds/jump.wav").c_str());
}

void MyLevel::Update(float dt)
{
    if (IsKeyPressed(KEY_SPACE))
        PlaySound(m_JumpSound);
}

void MyLevel::~MyLevel()   // <-- must be ~MyLevel, not ~GameMap
{
    UnloadSound(m_JumpSound);
}
```

### Music (Streaming)

```cpp
Music m_BGM;

// Load
m_BGM = LoadMusicStream(AssetResolver::Resolve("Music/bgm.ogg").c_str());
PlayMusicStream(m_BGM);

// Update every frame
UpdateMusicStream(m_BGM);

// Control
SetMusicVolume(m_BGM, 0.5f);
StopMusicStream(m_BGM);

// Unload
UnloadMusicStream(m_BGM);
```

### Supported Formats

- **Sounds**: `.wav`, `.ogg`, `.mp3`
- **Music**: `.ogg`, `.flac`, `.mp3`, `.xm`, `.mod`

---

## 4. Assets and Resources

### Loading Assets

Use `AssetResolver::Resolve()` for **every** asset path. It resolves relative paths against the project's `Assets/` folder and prevents directory traversal.

```cpp
#include "Engine/AssetResolver.h"

// Correct — works in editor and standalone
Texture2D tex = LoadTexture(AssetResolver::Resolve("player.png").c_str());
Sound snd = LoadSound(AssetResolver::Resolve("Sounds/jump.wav").c_str());

// Wrong — breaks when project is opened from a different working directory
Texture2D tex = LoadTexture("Assets/player.png");
```

### Path Rules

- **Always use forward slashes** (`Assets/player.png`), even on Windows.
- Assets are relative to the project's `Assets/` folder.
- Do NOT include `Assets/` prefix when calling `AssetResolver::Resolve()` — it's already added internally: `AssetResolver::Resolve("player.png")`.

### Supported File Formats

| Type | Formats |
|------|---------|
| Images | `.png`, `.jpg`, `.bmp`, `.tga`, `.gif`, `.qoi` |
| Sounds | `.wav`, `.ogg`, `.mp3` |
| Music | `.ogg`, `.flac`, `.mp3`, `.xm`, `.mod` |
| Fonts | `.ttf`, `.fnt` (sprite fonts) |

### Unloading Discipline

**Load in `Initialize()`, unload in destructor.** Never in constructor or `Update()`.

```cpp
class MyLevel : public GameMap
{
public:
    MyLevel() : GameMap("MyLevel") {}       // constructor: no resources

    void Initialize() override
    {
        m_Tex = LoadTexture(AssetResolver::Resolve("hero.png").c_str());
    }

    ~MyLevel() override                     // destructor: unload everything
    {
        UnloadTexture(m_Tex);
    }

private:
    Texture2D m_Tex;
};
```

### Organizing Assets

```
MyGame/Assets/
├── player.png
├── tileset.png
├── hero/
│   ├── idle.png
│   └── run.png
├── Sounds/
│   ├── jump.wav
│   └── coin.wav
└── Music/
    └── bgm.ogg
```

---

## 5. Export and Distribution

### Export Panel

1. Open the **Export Panel** (toolbar icon).
2. Choose an output folder.
3. Click **Start Export**.

The exported folder contains:
```
MyGame_Export/
├── game / game.exe     # Standalone runtime (no editor UI)
├── GameLogic.dll/.so   # Your compiled game
├── raylib runtime      # libraylib.dll / libraylib.so*
├── Assets/             # All your game assets (copied from project)
├── config.ini          # Window config for standalone mode
└── EngineContent/      # Engine fonts (rarely needed)
```

### Debug vs Release

- **Editor build** (clicking Compile): Debug configuration, profiler active.
- **Exported build**: Release configuration, profiler stripped (smaller, faster).

### Verifying an Export

Run `game.exe` (Windows) or `./game` (Linux) from the exported folder. If it crashes:
- Check that `Assets/` contains all required files.
- Check that `config.ini` exists (generated during export).
- Run from a terminal to see error output.

### Setting a Custom Icon

Windows export embeds `EngineContent/icon.ico` into `game.exe` with rcedit. Linux uses the raylib window icon instead.

---

## 6. Common Patterns

### Entity Pattern

Separate game objects (player, enemies, items) into their own classes with a consistent interface:

```cpp
class Player
{
public:
    void Initialize(const char* texturePath);
    void Update(float dt);
    void Draw();

    void SaveState(StateBag& out) const;
    void LoadState(const StateBag& in);

    Vector2 GetPosition() const { return m_Position; }
    Rectangle GetHitbox() const { return {m_Position.x, m_Position.y, 32, 32}; }

private:
    Texture2D m_Texture;
    Vector2 m_Position;
    Sound m_JumpSound;
};
```

Full example: `templates/SlimeQuest/GameLogic/Player.h`

### AABB Collision

Use raylib's built-in rectangle collision:

```cpp
#include <raylib.h>

Rectangle a = { m_Player.GetPosition().x, m_Player.GetPosition().y, 32, 32 };
Rectangle b = { enemy.x, enemy.y, 32, 32 };

if (CheckCollisionRecs(a, b))
{
    // Handle collision
}
```

For tile-based collision resolution, see `Player::ResolveCollisions()` in the SlimeQuest template.

### Timer-Based Animation

```cpp
struct AnimState {
    float timer = 0;
    int currentFrame = 0;
    int frameCount = 4;
    float frameTime = 0.125f;   // 8 FPS
};

void UpdateAnim(AnimState& anim, float dt)
{
    anim.timer += dt;
    if (anim.timer >= anim.frameTime)
    {
        anim.timer -= anim.frameTime;
        anim.currentFrame = (anim.currentFrame + 1) % anim.frameCount;
    }
}
```

For sprite-sheet drawing with row/column animation, see `Player::Draw()` and `Slime::Update()` in the SlimeQuest template.

### Passing Data Between Maps

When switching maps, data doesn't persist automatically. Options:

**Option 1: Global Singleton** (simple, survives reload)

```cpp
// GlobalState.h
struct GameState {
    int score = 0;
    int health = 100;
};
extern GameState g_GameState;

// In any map:
g_GameState.score += 100;
RequestGotoMap("NextLevel");
```

> **Caution:** Global variables survive hot-reloads because the loaded module's data segment persists while the host keeps it mapped. This is convenient but resets when the editor restarts. Use StateBag for persistence across restarts.

**Option 2: StateBag in the manager map** (survives reload and restart)

Override `SaveState`/`LoadState` on your `rw::Game`/`MapManager` subclass and register it with `RW_GAME`. StateBag is available to all child maps via `GameMap::SaveState`/`LoadState`.

### HUD Overlay

Draw HUD outside the camera block so it stays fixed on screen:

```cpp
void MyLevel::Draw()
{
    m_Camera.Begin();
    // World-space drawing
    m_Player.Draw();
    m_Camera.End();

    // Screen-space drawing (HUD)
    DrawText(TextFormat("Score: %d", m_Score), 10, 10, 20, WHITE);
    DrawFPS(10, 40);
}
```

---

## See Also

- [GUIDE_FUNDAMENTALS.md](GUIDE_FUNDAMENTALS.md) — DLL contract, map lifecycle, StateBag, tutorial, debugging
- [GAME_DEVELOPER_GUIDE.md](GAME_DEVELOPER_GUIDE.md) — overview and quick start
- [TROUBLESHOOTING.md](TROUBLESHOOTING.md) — common issues
