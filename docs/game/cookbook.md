# Game Cookbook

Short, copy-paste recipes built on the current `rw` and `GameMap` APIs. Every
snippet goes in your project's `GameLogic/`, never in `Engine/`.

## Create a project and a level

```text
rwcli templates
rwcli new MyGame --template Empty2D
rwcli new-level MyGame MyLevel
```

`new-level` creates `GameLogic/MyLevel.h` and inserts
`AddLevel<MyLevel>("MyLevel")` before `Start(...)` in the entry point. Open
`Game.cpp` and call `Start("MyLevel")` if you want the new level first.

## Spawn and move an entity

```cpp
class PlayLevel : public rw::Level
{
public:
    PlayLevel() : rw::Level("PlayLevel") {}

protected:
    void OnStart() override
    {
        m_Player = Spawn("Player", { 320.0f, 240.0f }, { 48.0f, 64.0f }, rw::Origin::Center);
        m_Texture.Load("player.png");
        Cam().SetZoom(1.0f);
        Cam().SetBounds(0.0f, 1280.0f, 0.0f, 720.0f);
        Cam().Snap(m_Player->position);
    }

    void OnUpdate(float delta_time) override
    {
        constexpr float speed = 240.0f;
        if (rw::IsKeyDown(KEY_LEFT) || rw::IsKeyDown(KEY_A))
        {
            m_Player->position.x -= speed * delta_time;
        }
        if (rw::IsKeyDown(KEY_RIGHT) || rw::IsKeyDown(KEY_D))
        {
            m_Player->position.x += speed * delta_time;
        }
        Cam().Follow(m_Player->position, delta_time);
    }

    void OnDraw() override { rw::DrawSprite(m_Texture, *m_Player); }

private:
    rw::Texture m_Texture;
    rw::Entity* m_Player = nullptr;
};

RW_GAME(PlayLevel)
```

## Axis-separated AABB movement

```cpp
struct Player
{
    Rectangle body{ 100.0f, 100.0f, 32.0f, 48.0f };
    Vector2 velocity{};
    bool b_Grounded = false;
};

void Step(Player& player, float dt, const std::vector<Rectangle>& solids)
{
    const rw::MoveResult hit = rw::MoveAndCollide(player.body, player.velocity, dt, solids);
    if (hit.b_HitX) { player.velocity.x = 0.0f; }
    if (hit.b_HitY) { player.velocity.y = 0.0f; }
    player.b_Grounded = hit.b_Grounded;
}
```

`rw::MoveAndCollide` is the engine version of the hand-rolled loop: it resolves
X before Y, clamps to the nearest edge, reports grounding, and skips solids the
body already overlaps. Keep the returned flags and zero the velocity components
yourself; the helper does not mutate the input velocity.

## Scene markers as spawn points

Tag a scene object in the Inspector (**Type / Tag**) or from the CLI:

```text
rwcli scene add . --map PlayLevel --name Start --type spawn --position 120,300
```

Read the tags in `OnSceneLoaded`. The engine keeps drawing the marker, so use a
small placeholder texture or leave it textureless.

```cpp
protected:
    void OnSceneLoaded() override
    {
        for (const t_SceneObjectInfo* marker : SceneObjectsOfType("spawn"))
        {
            Spawn(marker->m_Name, marker->m_Position, { 48.0f, 64.0f }, rw::Origin::Center);
        }
        for (const t_SceneObjectInfo* enemy : SceneObjectsOfType("enemy"))
        {
            Spawn(enemy->m_Name, enemy->m_Position, { 40.0f, 40.0f }, rw::Origin::Center, "Enemy");
        }
    }
```

- `SceneObjects()` returns every authored object in the active map.
- `FindSceneObject("name")` looks one up by name.
- `OnSceneLoaded()` also runs when the editor starts playback, but not on
  ordinary `OnStart`; spawn scene-derived actors there.
- Scene objects are read-only data. The engine still draws the sprite.

## Inspector-tunable values

```cpp
void OnStart() override
{
    m_Slime = Spawn("Slime", { 400.0f, 300.0f }, { 64.0f, 64.0f });
    m_Slime->Expose("Speed", &m_Speed, 0.0f, 500.0f);
    m_Slime->Expose("Health", &m_Health, 0, 100);
    m_Slime->Expose("Flying", &m_bFlying);
    m_Slime->Expose("Patrol", &m_PatrolRange);
}

float m_Speed = 120.0f;
int m_Health = 10;
bool m_bFlying = false;
Vector2 m_PatrolRange{ 64.0f, 0.0f };
```

Select the entity in the Scene panel and edit the fields under **Properties**.
Values survive hot reloads through `StateBag`; they reset on app restart. Call
`Expose` in `OnStart` or at spawn time so the editor sees the field.

## Named actions instead of key combos

```cpp
class PlayLevel : public rw::Level
{
protected:
    void OnStart() override
    {
        m_Actions.Bind("left", { KEY_LEFT, KEY_A });
        m_Actions.Bind("right", { KEY_RIGHT, KEY_D });
        m_Actions.Bind("jump", { KEY_SPACE, KEY_W });
    }

    void OnUpdate(float dt) override
    {
        if (m_Actions.b_Down("left")) { m_Player->position.x -= 160.0f * dt; }
        if (m_Actions.b_Pressed("jump")) { m_JumpRequested = true; }
    }

private:
    rw::ActionMap m_Actions;
};

// Or define the bindings once in a small shared helper:
struct Actions
{
    static rw::ActionMap& Map()
    {
        static rw::ActionMap map{};
        return map;
    }
};
```

`ActionMap` queries route through `rw::IsKeyDown` / `IsKeyPressed` /
`IsKeyReleased`, so named actions work under `rwcli run --input`. Raw raylib key
globals are not replayed.

## Audio and fonts with reload-safe lifetime

```cpp
class PlayLevel : public rw::Level
{
protected:
    void OnStart() override
    {
        m_Jump.Load("jump.wav");
        m_Music.Load("music/theme.ogg");
        m_Music.Play();
        m_Font.Load("fonts/ui.ttf", 24);
    }

    void OnUpdate(float dt) override
    {
        m_Music.Update(); // pump the stream every frame
        m_ScoreTimer.Update(dt);
        if (m_ScoreTimer.b_IsDone()) { m_ScoreTimer.Start(1.0f); }
    }

    void OnDrawUI() override
    {
        DrawTextEx(m_Font.Get(), TextFormat("Score: %d", m_Score), { 20.0f, 20.0f },
            static_cast<float>(m_Font.Size()), 1.0f, WHITE);
    }

private:
    rw::Sound m_Jump;
    rw::Music m_Music;
    rw::Font m_Font;
    rw::Timer m_ScoreTimer{ 1.0f };
    int m_Score = 0;
};
```

`rw::Sound`, `rw::Music`, and `rw::Font` are move-only RAII wrappers. They
resolve paths through `AssetResolver`, unload the previous handle on `Load`, and
unload again in the destructor. `OnStart` runs on restart and hot reload, so
reload assets there. Audio wrappers stay inert when the device is not ready.

## Sprite animation

```cpp
class Player
{
public:
    void Start()
    {
        m_Sheet.Load("player.png", { 32.0f, 32.0f });
        m_Animator.AddClip("idle", 0, 4, 8.0f);
        m_Animator.AddClip("run", 4, 6, 12.0f);
    }

    void Update(float dt)
    {
        m_Animator.Play(b_Moving() ? "run" : "idle");
        m_Animator.Update(dt);
    }

    void Draw()
    {
        const Rectangle src = m_Animator.FrameRect(m_Sheet);
        DrawTexturePro(m_Sheet.GetTexture().Get(), src, m_Dest, { 0.0f, 0.0f }, 0.0f, WHITE);
    }

private:
    rw::SpriteSheet m_Sheet;
    rw::Animator m_Animator;
    Rectangle m_Dest{ 100.0f, 100.0f, 64.0f, 64.0f };
};
```

`Animator` only does frame timing, so you can use it in a menu, a cutscene, or a
headless test. Frames are numbered left to right, top to bottom. A non-looping
clip holds its last frame and stops; `Play("same")` keeps progress, while
switching clips restarts the clock.

## Timers and tweens

```cpp
class Spawner : public rw::Level
{
protected:
    void OnUpdate(float dt) override
    {
        m_SpawnTimer.Update(dt);
        if (m_SpawnTimer.b_IsDone()) { SpawnEnemy(); }

        m_FadeTween.Update(dt);
    }

private:
    rw::Timer m_SpawnTimer{ 0.5f, true };
    float m_Fade = 1.0f;
    rw::Tween m_FadeTween{ &m_Fade, 1.0f, 0.0f, 0.4f, rw::Easing::SmoothStep };
};
```

A one-shot `Timer` stays done; a repeating timer wraps and keeps running.
`Tween` drives one float and is complete when it reaches the target. The float
must outlive the tween. Use multiple tweens for a `Vector2` or a color.

## Replayable menu widgets

```cpp
class MenuLevel : public rw::Level
{
protected:
    void OnDrawUI() override
    {
        m_UI.Panel({ 20.0f, 20.0f, 260.0f, 180.0f });
        if (m_UI.Button("Play", { 40.0f, 50.0f, 220.0f, 44.0f })) { RequestGotoMap("Play"); }
        m_UI.Checkbox("Mute", { 40.0f, 110.0f }, m_bMuted);
    }

private:
    rw::UI m_UI;
    bool m_bMuted = false;
};
```

`rw::UI` reads the mouse through `rw::GetMousePosition` and `rw::IsMouseButton*`,
so buttons and checkboxes work in `rwcli run --input` without a human. A click
requires press and release on the same widget; dragging out before releasing
cancels it.

## Keep state across hot reload

Override `OnSave`/`OnLoad` for values `rw::Level` does not already carry
(entity transforms and exposed properties are automatic).

```cpp
protected:
    void OnSave(StateBag& out) const override
    {
        out.SetInt("score", m_Score);
        out.SetBool("paused", m_bPaused);
    }

    void OnLoad(const StateBag& in) override
    {
        m_Score = in.GetInt("score", m_Score);
        m_bPaused = in.GetBool("paused", m_bPaused);
    }
```

## Switch levels

```cpp
RequestGotoMap("Menu");   // safe default: deferred to the next frame
```

`RequestGotoMap` is the safe default; it never destroys the calling map while
its `Initialize`/`Update`/`Draw` frame is still running. From your `rw::Game`
object (outside level callbacks), `GoTo("Menu")` performs the swap immediately
and returns `false` when the id is not registered.

## Faster rebuilds

A generated project builds `GameLogic` as `Release` by default. Three optional
knobs shorten the edit/build/hot-reload loop; none is required and none changes
the game you ship.

- **Build configuration.** While iterating, open **Editor Preferences** and
  pick `Debug` under **Build Configuration**; switch back to `Release` before
  measuring frame times. The editor passes the choice to CMake as
  `-DCMAKE_BUILD_TYPE=...`.
- **Compiler cache.** When `ccache` or `sccache` is on `PATH`, the generated
  build routes compilation through it automatically (ccache is preferred).
  Nothing is downloaded, and a machine without either tool builds as before.
  Cache tools have to launch the compiler themselves, so the launcher is not
  applied when the compiler is a wrapper script such as the Windows Zig
  `.bat` files. Turn it off with `-DRAYWAVES_PROJECT_COMPILER_CACHE=OFF`.
- **Precompiled header.** On Linux, `rw.h` — raylib plus the STL that every
  gameplay file already includes — is precompiled once per build with
  `target_precompile_headers` for GCC/Clang. It is skipped on Windows because
  the engine's Win32 sources intentionally control raylib's `Rectangle`/
  `CloseWindow`/`ShowCursor` symbol order around `<windows.h>`. The X11 window
  helper (`Engine/X11WindowType.cpp`) also opts out on Linux: it must include
  Xlib without raylib, and both define the global name `Font`. The artifact
  lives in the build tree, not next to the engine headers. Turn it off with
  `-DRAYWAVES_PROJECT_PCH=OFF`. ccache only caches the header itself when its
  `pch_defines,time_macros` sloppiness is on; the objects built from it are
  cached either way.

Pass those `-D` overrides on a CMake configure for the project in `.raywaves/`.
CMake remembers them in the project's cache under `.raywaves/build*/`, so one
configure is enough. Do not edit the generated `.raywaves/CMakeLists.txt`: it is
rewritten whenever the project is opened.

Engine contributors can cache the engine build itself; it is off by default:

```sh
cmake --preset zig-release -DRAYWAVES_USE_CCACHE=ON
```

## Agent workflow

```text
rwcli new-level MyGame BossLevel --json
rwcli build MyGame --json
rwcli run MyGame --frames 120 --screenshot shot.png --state state.json --json
rwcli lsp MyGame --check --json
```

`--json` keeps stdout machine-readable; engine logs go to stderr.
