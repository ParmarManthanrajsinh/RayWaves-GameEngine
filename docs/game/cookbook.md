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
struct MoveResult
{
    bool b_HitX = false;
    bool b_HitY = false;
    bool b_Grounded = false;
};

MoveResult MoveAndCollide(Rectangle& body, Vector2 velocity, float delta_time, const std::vector<Rectangle>& solids)
{
    MoveResult result;
    body.x += velocity.x * delta_time;
    for (const Rectangle& solid : solids)
    {
        if (!CheckCollisionRecs(body, solid))
        {
            continue;
        }
        body.x = (velocity.x > 0.0f) ? solid.x - body.width : solid.x + solid.width;
        result.b_HitX = true;
        break;
    }

    body.y += velocity.y * delta_time;
    for (const Rectangle& solid : solids)
    {
        if (!CheckCollisionRecs(body, solid))
        {
            continue;
        }
        if (velocity.y > 0.0f)
        {
            body.y = solid.y - body.height;
            result.b_Grounded = true;
        }
        else
        {
            body.y = solid.y + solid.height;
        }
        result.b_HitY = true;
        break;
    }
    return result;
}
```

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
struct Actions
{
    static bool b_Left() { return rw::IsKeyDown(KEY_LEFT) || rw::IsKeyDown(KEY_A); }
    static bool b_Right() { return rw::IsKeyDown(KEY_RIGHT) || rw::IsKeyDown(KEY_D); }
    static bool b_Jump() { return rw::IsKeyPressed(KEY_SPACE) || rw::IsKeyPressed(KEY_W); }
};
```

Use `rw::IsKeyDown` / `rw::IsKeyPressed` / `rw::IsMouseButtonPressed` instead
of the raylib globals so `rwcli run --input` can replay the game.

## Audio with reload-safe lifetime

```cpp
class PlayLevel : public rw::Level
{
protected:
    void OnStart() override
    {
        UnloadAudio();
        m_Jump = LoadSound(AssetResolver::Resolve("jump.wav").c_str());
    }

    ~PlayLevel() override { UnloadAudio(); }

private:
    void UnloadAudio()
    {
        if (IsSoundValid(m_Jump))
        {
            UnloadSound(m_Jump);
        }
        m_Jump = {};
    }

    Sound m_Jump{};
};
```

`OnStart` runs again on restart and hot reload, so reload assets there.

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
  `CloseWindow`/`ShowCursor` symbol order around `<windows.h>`. The artifact
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
