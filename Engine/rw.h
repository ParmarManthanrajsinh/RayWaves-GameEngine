#pragma once
#include "AssetResolver.h"
#include "GameLogicExports.h"
#include "GameMap.h"
#include "MapManager.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <raylib.h>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

// RayWaves' small game-facing API.
//
// Everything here is a thin layer over plain raylib and the existing GameMap
// contract: no ECS, no component registry, no retained scene graph. A level
// owns a flat list of entities, draws them with normal raylib calls, and the
// editor sees them automatically.
//
// Minimal game (GameLogic/Game.cpp):
//
//     #include "rw.h"
//
//     class MyLevel : public rw::Level
//     {
//     public:
//         MyLevel() : rw::Level("MyLevel") {}
//
//     protected:
//         void OnStart() override
//         {
//             m_PlayerTexture.Load("player.png");
//             m_Player = Spawn("Player", { 100, 300 }, { 48, 64 }, rw::Origin::Center);
//             Cam().SetZoom(2.0f);
//             Cam().Snap(m_Player->position);
//         }
//
//         void OnUpdate(float dt) override
//         {
//             if (IsKeyDown(KEY_RIGHT)) { m_Player->position.x += 200.0f * dt; }
//             Cam().Follow(m_Player->position, dt);
//         }
//
//         void OnDraw() override
//         {
//             rw::DrawSprite(m_PlayerTexture, *m_Player);
//         }
//
//     private:
//         rw::Texture m_PlayerTexture;
//         rw::Entity* m_Player = nullptr;
//     };
//
//     RW_GAME(MyLevel)
namespace rw
{
// Where Entity::position sits relative to the drawn rectangle.
enum class Origin
{
    TopLeft,
    Center
};

// Deterministic input replay for agent runs.
//
// Default behavior is plain raylib. When RAYWAVES_AGENT_INPUT points at a
// script, keys and mouse are driven from that file instead. The script lives
// inside the GameLogic module, so the host only has to set the environment
// variable before loading the DLL.
//
// Format (frame,KEY_NAME,down|up):
//
//     0,KEY_RIGHT,down
//     30,KEY_RIGHT,up
//
// Mouse position: 0,mouse,640,360
namespace input_detail
{
struct FInputEvent
{
    int m_Frame = 0;
    int m_Key = 0;
    bool m_bDown = false;
};

struct FMouseEvent
{
    int m_Frame = 0;
    Vector2 m_Position{ 0.0f, 0.0f };
};

struct FState
{
    bool m_bLoaded = false;
    bool m_bEnabled = false;
    int m_Frame = -1;
    std::vector<FInputEvent> m_Events;
    std::vector<FMouseEvent> m_MouseEvents;
    size_t m_EventCursor = 0;
    size_t m_MouseCursor = 0;
    std::unordered_map<int, bool> m_Down;
    std::unordered_set<int> m_Pressed;
    std::unordered_set<int> m_Released;
    Vector2 m_MousePosition{ 0.0f, 0.0f };
    bool m_bHasMousePosition = false;
};

inline std::string_view TrimInput(std::string_view value)
{
    const size_t start = value.find_first_not_of(" \t\r\n");
    if (start == std::string_view::npos)
    {
        return {};
    }
    const size_t end = value.find_last_not_of(" \t\r\n");
    return value.substr(start, (end - start) + 1);
}

inline int ParseKeyName(std::string_view raw)
{
    std::string name(raw);
    for (char& character : name)
    {
        character = static_cast<char>(std::toupper(static_cast<unsigned char>(character)));
    }

    if (name.size() > 1 && name.front() >= '0' && name.front() <= '9' &&
        std::all_of(name.begin(), name.end(), [](char c) { return c >= '0' && c <= '9'; }))
    {
        return std::atoi(name.c_str());
    }

    if (name.rfind("KEY_", 0) == 0)
    {
        name.erase(0, 4);
    }

    static const std::pair<const char*, int> c_NAMES[] = {
        { "A", KEY_A },
        { "B", KEY_B },
        { "C", KEY_C },
        { "D", KEY_D },
        { "E", KEY_E },
        { "F", KEY_F },
        { "G", KEY_G },
        { "H", KEY_H },
        { "I", KEY_I },
        { "J", KEY_J },
        { "K", KEY_K },
        { "L", KEY_L },
        { "M", KEY_M },
        { "N", KEY_N },
        { "O", KEY_O },
        { "P", KEY_P },
        { "Q", KEY_Q },
        { "R", KEY_R },
        { "S", KEY_S },
        { "T", KEY_T },
        { "U", KEY_U },
        { "V", KEY_V },
        { "W", KEY_W },
        { "X", KEY_X },
        { "Y", KEY_Y },
        { "Z", KEY_Z },
        { "0", KEY_ZERO },
        { "1", KEY_ONE },
        { "2", KEY_TWO },
        { "3", KEY_THREE },
        { "4", KEY_FOUR },
        { "5", KEY_FIVE },
        { "6", KEY_SIX },
        { "7", KEY_SEVEN },
        { "8", KEY_EIGHT },
        { "9", KEY_NINE },
        { "LEFT", KEY_LEFT },
        { "RIGHT", KEY_RIGHT },
        { "UP", KEY_UP },
        { "DOWN", KEY_DOWN },
        { "SPACE", KEY_SPACE },
        { "ENTER", KEY_ENTER },
        { "ESCAPE", KEY_ESCAPE },
        { "TAB", KEY_TAB },
        { "BACKSPACE", KEY_BACKSPACE },
        { "INSERT", KEY_INSERT },
        { "DELETE", KEY_DELETE },
        { "HOME", KEY_HOME },
        { "END", KEY_END },
        { "PAGE_UP", KEY_PAGE_UP },
        { "PAGE_DOWN", KEY_PAGE_DOWN },
        { "LEFT_SHIFT", KEY_LEFT_SHIFT },
        { "RIGHT_SHIFT", KEY_RIGHT_SHIFT },
        { "LEFT_CONTROL", KEY_LEFT_CONTROL },
        { "RIGHT_CONTROL", KEY_RIGHT_CONTROL },
        { "LEFT_ALT", KEY_LEFT_ALT },
        { "RIGHT_ALT", KEY_RIGHT_ALT },
        { "F1", KEY_F1 },
        { "F2", KEY_F2 },
        { "F3", KEY_F3 },
        { "F4", KEY_F4 },
        { "F5", KEY_F5 },
        { "F6", KEY_F6 },
        { "F7", KEY_F7 },
        { "F8", KEY_F8 },
        { "F9", KEY_F9 },
        { "F10", KEY_F10 },
        { "F11", KEY_F11 },
        { "F12", KEY_F12 },
        { "MOUSE_LEFT", MOUSE_BUTTON_LEFT },
        { "MOUSE_RIGHT", MOUSE_BUTTON_RIGHT },
        { "MOUSE_MIDDLE", MOUSE_BUTTON_MIDDLE },
    };

    for (const auto& [entry_name, key] : c_NAMES)
    {
        if (name == entry_name)
        {
            return key;
        }
    }
    return -1;
}

inline void LoadScript(FState& state)
{
    state.m_bLoaded = true;

    const char* path = std::getenv("RAYWAVES_AGENT_INPUT");
    if (path == nullptr || path[0] == '\0')
    {
        return;
    }

    std::ifstream file(path);
    if (!file.is_open())
    {
        return;
    }

    std::string line;
    while (std::getline(file, line))
    {
        const std::string_view view = TrimInput(line);
        if (view.empty() || view.front() == '#')
        {
            continue;
        }

        std::vector<std::string_view> parts;
        size_t start = 0;
        while (start <= view.size())
        {
            const size_t comma = view.find(',', start);
            const size_t end = (comma == std::string_view::npos) ? view.size() : comma;
            parts.push_back(TrimInput(view.substr(start, end - start)));
            if (comma == std::string_view::npos)
            {
                break;
            }
            start = comma + 1;
        }

        if (parts.size() < 3)
        {
            continue;
        }

        const int frame = std::atoi(std::string(parts[0]).c_str());
        if (parts[1] == "mouse" || parts[1] == "MOUSE")
        {
            if (parts.size() >= 4)
            {
                state.m_MouseEvents.push_back({ frame,
                    { static_cast<float>(std::atof(std::string(parts[2]).c_str())),
                        static_cast<float>(std::atof(std::string(parts[3]).c_str())) } });
            }
            continue;
        }

        const int key = ParseKeyName(parts[1]);
        if (key < 0)
        {
            continue;
        }

        std::string action(parts[2]);
        for (char& character : action)
        {
            character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
        }
        const bool b_down = (action == "down" || action == "1" || action == "true");

        state.m_Events.push_back({ frame, key, b_down });
    }

    std::stable_sort(state.m_Events.begin(), state.m_Events.end(),
        [](const FInputEvent& left, const FInputEvent& right) { return left.m_Frame < right.m_Frame; });
    std::stable_sort(state.m_MouseEvents.begin(), state.m_MouseEvents.end(),
        [](const FMouseEvent& left, const FMouseEvent& right) { return left.m_Frame < right.m_Frame; });

    state.m_bEnabled = !state.m_Events.empty() || !state.m_MouseEvents.empty();
}

inline FState& State()
{
    static FState state;
    return state;
}

inline void Reset()
{
    FState& state = State();
    if (!state.m_bLoaded)
    {
        return;
    }
    state.m_Frame = -1;
    state.m_EventCursor = 0;
    state.m_MouseCursor = 0;
    state.m_Down.clear();
    state.m_Pressed.clear();
    state.m_Released.clear();
    state.m_bHasMousePosition = false;
}

inline void BeginFrame()
{
    FState& state = State();
    if (!state.m_bLoaded)
    {
        LoadScript(state);
    }
    if (!state.m_bEnabled)
    {
        return;
    }

    ++state.m_Frame;
    state.m_Pressed.clear();
    state.m_Released.clear();

    while (state.m_EventCursor < state.m_Events.size() && state.m_Events[state.m_EventCursor].m_Frame <= state.m_Frame)
    {
        const FInputEvent& event = state.m_Events[state.m_EventCursor];
        const bool b_was_down = state.m_Down[event.m_Key];
        state.m_Down[event.m_Key] = event.m_bDown;
        if (event.m_bDown && !b_was_down)
        {
            state.m_Pressed.insert(event.m_Key);
        }
        else if (!event.m_bDown && b_was_down)
        {
            state.m_Released.insert(event.m_Key);
        }
        ++state.m_EventCursor;
    }

    while (state.m_MouseCursor < state.m_MouseEvents.size() &&
        state.m_MouseEvents[state.m_MouseCursor].m_Frame <= state.m_Frame)
    {
        state.m_MousePosition = state.m_MouseEvents[state.m_MouseCursor].m_Position;
        state.m_bHasMousePosition = true;
        ++state.m_MouseCursor;
    }
}

inline bool b_AgentMode()
{
    FState& state = State();
    if (!state.m_bLoaded)
    {
        LoadScript(state);
    }
    return state.m_bEnabled;
}
} // namespace input_detail

inline bool IsKeyDown(int key)
{
    if (input_detail::b_AgentMode())
    {
        const auto found = input_detail::State().m_Down.find(key);
        return found != input_detail::State().m_Down.end() && found->second;
    }
    return ::IsKeyDown(key);
}

inline bool IsKeyUp(int key)
{
    if (input_detail::b_AgentMode())
    {
        const auto found = input_detail::State().m_Down.find(key);
        return found == input_detail::State().m_Down.end() || !found->second;
    }
    return ::IsKeyUp(key);
}

inline bool IsKeyPressed(int key)
{
    if (input_detail::b_AgentMode())
    {
        return input_detail::State().m_Pressed.contains(key);
    }
    return ::IsKeyPressed(key);
}

inline bool IsKeyReleased(int key)
{
    if (input_detail::b_AgentMode())
    {
        return input_detail::State().m_Released.contains(key);
    }
    return ::IsKeyReleased(key);
}

inline bool IsMouseButtonDown(int button)
{
    if (input_detail::b_AgentMode())
    {
        const auto found = input_detail::State().m_Down.find(button);
        return found != input_detail::State().m_Down.end() && found->second;
    }
    return ::IsMouseButtonDown(button);
}

inline bool IsMouseButtonPressed(int button)
{
    if (input_detail::b_AgentMode())
    {
        return input_detail::State().m_Pressed.contains(button);
    }
    return ::IsMouseButtonPressed(button);
}

inline Vector2 GetMousePosition()
{
    if (input_detail::b_AgentMode() && input_detail::State().m_bHasMousePosition)
    {
        return input_detail::State().m_MousePosition;
    }
    return ::GetMousePosition();
}

inline float GetMouseX()
{
    return GetMousePosition().x;
}

inline float GetMouseY()
{
    return GetMousePosition().y;
}

// Owning raylib texture. Paths go through AssetResolver, so relative asset
// names work in both the editor and the standalone game.
class Texture
{
public:
    Texture() = default;
    explicit Texture(std::string_view path) { Load(path); }
    ~Texture() { Unload(); }

    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    Texture(Texture&& other) noexcept : m_Texture(other.m_Texture) { other.m_Texture = {}; }

    Texture& operator=(Texture&& other) noexcept
    {
        if (this != &other)
        {
            Unload();
            m_Texture = other.m_Texture;
            other.m_Texture = {};
        }
        return *this;
    }

    // Reload-safe: loading twice unloads the previous handle first.
    bool Load(std::string_view path)
    {
        Unload();
        if (path.empty())
        {
            return false;
        }

        const std::string resolved = AssetResolver::Resolve(path);
        m_Texture = ::LoadTexture(resolved.c_str());
        if (!::IsTextureValid(m_Texture))
        {
            m_Texture = {};
            return false;
        }
        return true;
    }

    void Unload()
    {
        if (::IsTextureValid(m_Texture))
        {
            ::UnloadTexture(m_Texture);
        }
        m_Texture = {};
    }

    bool IsValid() const { return ::IsTextureValid(m_Texture); }
    int Width() const { return m_Texture.width; }
    int Height() const { return m_Texture.height; }
    Vector2 Size() const { return { static_cast<float>(m_Texture.width), static_cast<float>(m_Texture.height) }; }
    const Texture2D& Get() const { return m_Texture; }
    Texture2D& GetMutable() { return m_Texture; }

private:
    Texture2D m_Texture{};
};

// Follow camera with optional level bounds. rw::Level keeps one and exposes it
// through Cam(); it is also what the editor pans and zooms while stopped.
class Camera
{
public:
    Camera() { UpdateOffset(); }

    void SetViewport(float width, float height)
    {
        if (width <= 0.0f || height <= 0.0f || (width == m_Viewport.x && height == m_Viewport.y))
        {
            return;
        }
        m_Viewport = { width, height };
        UpdateOffset();
    }

    Vector2 GetViewport() const { return m_Viewport; }

    void Snap(Vector2 target)
    {
        m_Camera.target = target;
        UpdateOffset();
        ClampToBounds();
    }

    void Follow(Vector2 target, float delta_time, float speed = 8.0f)
    {
        UpdateOffset();
        const float amount = std::clamp(speed * delta_time, 0.0f, 1.0f);
        m_Camera.target.x += (target.x - m_Camera.target.x) * amount;
        m_Camera.target.y += (target.y - m_Camera.target.y) * amount;
        m_Camera.zoom = std::max(m_Camera.zoom, m_MinZoom);
        ClampToBounds();
    }

    void SetZoom(float zoom)
    {
        m_Camera.zoom = std::max(zoom, m_MinZoom);
        UpdateOffset();
    }

    float GetZoom() const { return m_Camera.zoom; }

    void SetMinZoom(float min_zoom) { m_MinZoom = std::max(min_zoom, 0.01f); }
    float GetMinZoom() const { return m_MinZoom; }

    void SetBounds(float left, float right, float top, float bottom)
    {
        m_BoundsLeft = left;
        m_BoundsRight = right;
        m_BoundsTop = top;
        m_BoundsBottom = bottom;
        m_bHasBounds = true;
        ClampToBounds();
    }

    void ClearBounds() { m_bHasBounds = false; }
    bool HasBounds() const { return m_bHasBounds; }

    void ClampToBounds()
    {
        if (!m_bHasBounds)
        {
            return;
        }

        const float half_width = (m_Viewport.x / m_Camera.zoom) * 0.5f;
        const float half_height = (m_Viewport.y / m_Camera.zoom) * 0.5f;

        const float min_x = m_BoundsLeft + half_width;
        const float max_x = m_BoundsRight - half_width;
        m_Camera.target.x =
            (min_x <= max_x) ? std::clamp(m_Camera.target.x, min_x, max_x) : (m_BoundsLeft + m_BoundsRight) * 0.5f;

        const float min_y = m_BoundsTop + half_height;
        const float max_y = m_BoundsBottom - half_height;
        m_Camera.target.y =
            (min_y <= max_y) ? std::clamp(m_Camera.target.y, min_y, max_y) : (m_BoundsTop + m_BoundsBottom) * 0.5f;
    }

    void Begin() const { BeginMode2D(m_Camera); }
    static void End() { EndMode2D(); }

    Vector2 GetTarget() const { return m_Camera.target; }
    Vector2 ScreenToWorld(Vector2 screen) const { return GetScreenToWorld2D(screen, m_Camera); }
    Vector2 WorldToScreen(Vector2 world) const { return GetWorldToScreen2D(world, m_Camera); }

    Camera2D& Get() { return m_Camera; }
    const Camera2D& Get() const { return m_Camera; }

private:
    void UpdateOffset() { m_Camera.offset = { m_Viewport.x * 0.5f, m_Viewport.y * 0.5f }; }

    Camera2D m_Camera{ { 0.0f, 0.0f }, { 0.0f, 0.0f }, 0.0f, 1.0f };
    Vector2 m_Viewport{ 1280.0f, 720.0f };
    float m_MinZoom = 0.1f;

    float m_BoundsLeft = 0.0f;
    float m_BoundsRight = 0.0f;
    float m_BoundsTop = 0.0f;
    float m_BoundsBottom = 0.0f;
    bool m_bHasBounds = false;
};

// A drawable object the editor can select. Game code owns behaviour; the
// editor owns selection. Fields are public on purpose: mutate them directly.
struct Entity
{
    uint64_t id = 0; // editor id, 0 when not registered
    std::string name;
    std::string type;
    Vector2 position{ 0.0f, 0.0f };
    Vector2 size{ 32.0f, 32.0f };
    Vector2 origin{ 0.0f, 0.0f }; // position -> draw rect top-left
    float rotation = 0.0f;
    bool visible = true;
};

inline void DrawSprite(const Texture2D& texture, const Entity& entity, Color tint = WHITE)
{
    if (!entity.visible || (texture.id == 0))
    {
        return;
    }

    const Rectangle source{ 0.0f, 0.0f, static_cast<float>(texture.width), static_cast<float>(texture.height) };
    const Rectangle destination{ entity.position.x + entity.origin.x, entity.position.y + entity.origin.y,
        entity.size.x, entity.size.y };
    DrawTexturePro(texture, source, destination, { 0.0f, 0.0f }, entity.rotation, tint);
}

inline void DrawSprite(const Texture& texture, const Entity& entity, Color tint = WHITE)
{
    DrawSprite(texture.Get(), entity, tint);
}

// Selection-sized outline, useful while prototyping without a sprite.
inline void DrawEntityBox(const Entity& entity, Color color = LIME)
{
    const Rectangle rect{ entity.position.x + entity.origin.x, entity.position.y + entity.origin.y, entity.size.x,
        entity.size.y };
    DrawRectangleLinesEx(rect, 1.0f, color);
}

// A single game map with entities, a camera, and raylib drawing.
//
// Lifecycle: Initialize() runs OnStart() from a clean slate. The engine calls
// Initialize() on load, map switch, restart, and hot reload, so OnStart() must
// be able to build the whole level from scratch.
class Level : public GameMap
{
public:
    explicit Level(std::string_view name) : GameMap(name) { m_Camera.SetViewport(ViewportWidth(), ViewportHeight()); }

    ~Level() override
    {
        // The editor must not keep pointers into entities that are about to die.
        ClearEditorEntities();
    }

    Level(const Level&) = delete;
    Level& operator=(const Level&) = delete;
    Level(Level&&) = delete;
    Level& operator=(Level&&) = delete;

    // Creates an entity and registers it with the editor in one call.
    Entity* Spawn(
        std::string_view name, Vector2 position, Vector2 size = { 32.0f, 32.0f }, Origin origin = Origin::TopLeft)
    {
        return Spawn(name, position, size, origin, name);
    }

    // Same, with a type separate from the display name (e.g. "Slime 2" / "Enemy").
    Entity* Spawn(std::string_view name, Vector2 position, Vector2 size, Origin origin, std::string_view type)
    {
        auto entity = std::make_unique<Entity>();
        entity->name = std::string(name);
        entity->type = type.empty() ? entity->name : std::string(type);
        entity->position = position;
        entity->size = size;
        entity->origin = (origin == Origin::Center) ? Vector2{ -size.x * 0.5f, -size.y * 0.5f } : Vector2{ 0.0f, 0.0f };

        Entity* raw = entity.get();
        m_Entities.push_back(std::move(entity));

        t_EntityOptions options;
        options.Origin = raw->origin;
        options.Type = raw->type;
        options.Rotation = &raw->rotation;
        options.Visible = &raw->visible;

        const t_EditorEntity* registered = AddEntity(raw->name, &raw->position, &raw->size, options);
        raw->id = (registered != nullptr) ? registered->m_Id : 0;
        return raw;
    }

    // Removes one entity and its editor registration.
    void Destroy(Entity* entity)
    {
        if (entity == nullptr)
        {
            return;
        }
        if (entity->id != 0)
        {
            UnregisterEditorEntity(entity->id);
        }
        std::erase_if(m_Entities, [entity](const std::unique_ptr<Entity>& item) { return item.get() == entity; });
    }

    // Removes every entity and editor registration.
    void ClearEntities()
    {
        ClearEditorEntities();
        m_Entities.clear();
    }

    using EntityList = std::vector<std::unique_ptr<Entity>>;
    const EntityList& Entities() const { return m_Entities; }

    Camera& Cam() { return m_Camera; }
    const Camera& Cam() const { return m_Camera; }

    void SetBackground(Color color) { m_Background = color; }
    Color GetBackground() const { return m_Background; }

    float ViewportWidth() const { return (m_SceneWidth > 0.0f) ? m_SceneWidth : 1280.0f; }
    float ViewportHeight() const { return (m_SceneHeight > 0.0f) ? m_SceneHeight : 720.0f; }

    // GameMap interface. Subclasses override the On* hooks instead.
    void Initialize() final
    {
        ClearEntities();
        input_detail::Reset();
        m_Camera.SetViewport(ViewportWidth(), ViewportHeight());
        OnStart();
    }

    void Update(float delta_time) final
    {
        input_detail::BeginFrame();
        m_Camera.SetViewport(ViewportWidth(), ViewportHeight());
        OnUpdate(delta_time);
    }

    void Draw() final
    {
        ClearBackground(m_Background);
        m_Camera.Begin();
        OnDraw();
        EndMode2D();
        OnDrawUI();
    }

    Camera2D GetViewCamera() const final { return m_Camera.Get(); }
    Camera2D* GetMutableViewCamera() final { return &m_Camera.Get(); }

    // Hot reload keeps entity transforms (keyed by spawn order) and forwards to
    // the game hooks for everything else.
    void SaveState(StateBag& out) const final
    {
        out.SetInt("rw.entity.count", static_cast<int>(m_Entities.size()));
        for (size_t index = 0; index < m_Entities.size(); ++index)
        {
            const Entity& entity = *m_Entities[index];
            const std::string base = "rw.entity." + std::to_string(index);
            out.SetVector2(base + ".position", entity.position);
            out.SetVector2(base + ".size", entity.size);
            out.SetFloat(base + ".rotation", entity.rotation);
            out.SetBool(base + ".visible", entity.visible);
        }
        OnSave(out);
    }

    void LoadState(const StateBag& in) final
    {
        const int count = in.GetInt("rw.entity.count", 0);
        for (size_t index = 0; index < m_Entities.size() && static_cast<int>(index) < count; ++index)
        {
            Entity& entity = *m_Entities[index];
            const std::string base = "rw.entity." + std::to_string(index);
            entity.position = in.GetVector2(base + ".position", entity.position);
            entity.size = in.GetVector2(base + ".size", entity.size);
            entity.rotation = in.GetFloat(base + ".rotation", entity.rotation);
            entity.visible = in.GetBool(base + ".visible", entity.visible);
        }
        OnLoad(in);
    }

protected:
    virtual void OnStart() {}
    virtual void OnUpdate(float delta_time) { (void)delta_time; }
    virtual void OnDraw() {}
    virtual void OnDrawUI() {}
    virtual void OnSave(StateBag& out) const { (void)out; }
    virtual void OnLoad(const StateBag& in) { (void)in; }

private:
    EntityList m_Entities;
    Camera m_Camera;
    Color m_Background{ 24, 24, 24, 255 };
};

// Multi-level game: thin sugar over MapManager.
//
//     class MyGame : public rw::Game
//     {
//     public:
//         MyGame()
//         {
//             AddLevel<MenuLevel>("menu");
//             AddLevel<PlayLevel>("play");
//             Start("menu");
//         }
//     };
//     RW_GAME(MyGame)
class Game : public MapManager
{
public:
    template <typename LevelType>
    Game& AddLevel(std::string_view id, std::string_view description = "")
    {
        static_assert(std::is_base_of_v<GameMap, LevelType>, "rw::Game::AddLevel requires a GameMap subclass");
        RegisterMap<LevelType>(id, description);
        return *this;
    }

    void Start(std::string_view level_id) { SetInitialMap(level_id); }
    bool GoTo(std::string_view level_id, bool force_reload = false) { return b_GotoMap(level_id, force_reload); }
};

namespace detail
{
// One instance per DLL module, destroyed by DestroyGameMap. Function-local
// statics survive hot reload of the same module but are reset on unload.
template <typename GameType>
GameMap*& InstanceSlot()
{
    static GameMap* slot = nullptr;
    return slot;
}
} // namespace detail
} // namespace rw

// Defines the three exports the editor and runtime load from the GameLogic
// module. Use exactly once per game module, in a .cpp file.
// clang-format off
#define RW_GAME(GameType)                                                                        \
    RW_GAMELOGIC_EXPORT uint32_t GetGameLogicAbiVersion()                                       \
    {                                                                                            \
        return RAYWAVES_GAMELOGIC_ABI_VERSION;                                                   \
    }                                                                                            \
    RW_GAMELOGIC_EXPORT GameMap* CreateGameMap()                                                 \
    {                                                                                            \
        GameMap*& slot = rw::detail::InstanceSlot<GameType>();                                   \
        if (slot == nullptr)                                                                     \
        {                                                                                        \
            slot = new GameType();                                                               \
        }                                                                                        \
        return slot;                                                                             \
    }                                                                                            \
    RW_GAMELOGIC_EXPORT void DestroyGameMap(GameMap* map)                                        \
    {                                                                                            \
        if (map == nullptr)                                                                      \
        {                                                                                        \
            return;                                                                              \
        }                                                                                        \
        GameMap*& slot = rw::detail::InstanceSlot<GameType>();                                   \
        if (slot == map)                                                                         \
        {                                                                                        \
            slot = nullptr;                                                                      \
        }                                                                                        \
        delete map;                                                                              \
    }
// clang-format on
