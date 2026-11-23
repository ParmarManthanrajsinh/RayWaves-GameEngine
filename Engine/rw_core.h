#pragma once
#include "AssetResolver.h"
#include "GameLogicExports.h"
#include "GameMap.h"
#include "MapManager.h"
#include "rw_input.h"
#include <algorithm>
#include <cstdint>
#include <memory>
#include <raylib.h>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>
namespace rw
{
// Where Entity::position sits relative to the drawn rectangle.
enum class Origin
{
    TopLeft,
    Center
};

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

// A gameplay field exposed to the Inspector. Points into the entity's owning
// game object; the editor writes through m_Data.
struct Property
{
    std::string m_Name;
    EEntityPropertyType m_Type = EEntityPropertyType::Float;
    void* m_Data = nullptr;
    float m_Min = 0.0f;
    float m_Max = 0.0f;
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

    // Expose a gameplay value to the editor Inspector. Call during OnStart, or
    // at spawn time; values are also carried across hot reloads.
    void Expose(std::string_view property_name, float* value, float min = 0.0f, float max = 0.0f)
    {
        ExposeProperty(property_name, EEntityPropertyType::Float, value, min, max);
    }
    void Expose(std::string_view property_name, int* value, int min = 0, int max = 0)
    {
        ExposeProperty(
            property_name, EEntityPropertyType::Int, value, static_cast<float>(min), static_cast<float>(max));
    }
    void Expose(std::string_view property_name, bool* value)
    {
        ExposeProperty(property_name, EEntityPropertyType::Bool, value, 0.0f, 0.0f);
    }
    void Expose(std::string_view property_name, Vector2* value)
    {
        ExposeProperty(property_name, EEntityPropertyType::Vector2, value, 0.0f, 0.0f);
    }

    // Stable property storage (unique_ptr keeps addresses valid when the list
    // grows) and the contiguous views handed to the editor.
    std::vector<std::unique_ptr<Property>> m_Properties;
    std::vector<t_EntityProperty> m_PropertyViews;
    t_EditorEntity* m_EditorEntity = nullptr;
    bool m_bPropertiesDirty = false;

    void ExposeProperty(std::string_view property_name, EEntityPropertyType type, void* data, float min, float max)
    {
        if (data == nullptr)
        {
            return;
        }
        auto property = std::make_unique<Property>();
        property->m_Name = std::string(property_name);
        property->m_Type = type;
        property->m_Data = data;
        property->m_Min = min;
        property->m_Max = max;
        m_Properties.push_back(std::move(property));
        m_bPropertiesDirty = true;
    }
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

        t_EditorEntity* registered = AddEntity(raw->name, &raw->position, &raw->size, options);
        raw->m_EditorEntity = registered;
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

    // Editor-authored scene objects. Read-only: the engine still draws the
    // sprites, and gameplay can read tags/markers to spawn its own actors.
    const std::vector<t_SceneObjectInfo>& SceneObjects() const { return GetSceneObjects(); }
    std::vector<const t_SceneObjectInfo*> SceneObjectsOfType(std::string_view type) const
    {
        return FindSceneObjectsOfType(type);
    }

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
        RefreshProperties();
    }

    void Update(float delta_time) final
    {
        input_detail::BeginFrame();
        m_Camera.SetViewport(ViewportWidth(), ViewportHeight());
        OnUpdate(delta_time);
        RefreshPropertiesIfDirty();
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

    // Scene snapshots arrive after the initial map has initialized. Levels
    // override OnSceneLoaded() to spawn actors from authored markers.
    void OnSceneChanged() final { OnSceneLoaded(); }

    // Hot reload keeps entity transforms and exposed property values (keyed by
    // spawn order) and forwards to the game hooks for everything else.
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

            const std::string properties_base = base + ".props";
            out.SetInt(properties_base + ".count", static_cast<int>(entity.m_Properties.size()));
            for (size_t property_index = 0; property_index < entity.m_Properties.size(); ++property_index)
            {
                const Property& property = *entity.m_Properties[property_index];
                if (property.m_Data == nullptr)
                {
                    continue;
                }
                const std::string property_base = properties_base + "." + std::to_string(property_index);
                out.SetString(property_base + ".name", property.m_Name);
                switch (property.m_Type)
                {
                    case EEntityPropertyType::Float:
                        out.SetFloat(property_base + ".value", *static_cast<const float*>(property.m_Data));
                        break;
                    case EEntityPropertyType::Int:
                        out.SetInt(property_base + ".value", *static_cast<const int*>(property.m_Data));
                        break;
                    case EEntityPropertyType::Bool:
                        out.SetBool(property_base + ".value", *static_cast<const bool*>(property.m_Data));
                        break;
                    case EEntityPropertyType::Vector2:
                        out.SetVector2(property_base + ".value", *static_cast<const Vector2*>(property.m_Data));
                        break;
                }
            }
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

            const std::string properties_base = base + ".props";
            const int property_count = in.GetInt(properties_base + ".count", 0);
            for (int property_index = 0; property_index < property_count; ++property_index)
            {
                const std::string property_base = properties_base + "." + std::to_string(property_index);
                const std::string saved_name = in.GetString(property_base + ".name", "");
                if (saved_name.empty())
                {
                    continue;
                }

                const auto match = std::find_if(entity.m_Properties.begin(), entity.m_Properties.end(),
                    [&saved_name](const std::unique_ptr<Property>& property)
                    { return property->m_Name == saved_name; });
                if (match == entity.m_Properties.end())
                {
                    continue;
                }

                Property& property = **match;
                if (property.m_Data == nullptr)
                {
                    continue;
                }
                switch (property.m_Type)
                {
                    case EEntityPropertyType::Float:
                        *static_cast<float*>(property.m_Data) = in.GetFloat(property_base + ".value", 0.0f);
                        break;
                    case EEntityPropertyType::Int:
                        *static_cast<int*>(property.m_Data) = in.GetInt(property_base + ".value", 0);
                        break;
                    case EEntityPropertyType::Bool:
                        *static_cast<bool*>(property.m_Data) = in.GetBool(property_base + ".value", false);
                        break;
                    case EEntityPropertyType::Vector2:
                        *static_cast<Vector2*>(property.m_Data) =
                            in.GetVector2(property_base + ".value", Vector2{ 0.0f, 0.0f });
                        break;
                }
            }
        }
        OnLoad(in);
        RefreshProperties();
    }

protected:
    virtual void OnStart() {}
    virtual void OnSceneLoaded() {}
    virtual void OnUpdate(float delta_time) { (void)delta_time; }
    virtual void OnDraw() {}
    virtual void OnDrawUI() {}
    virtual void OnSave(StateBag& out) const { (void)out; }
    virtual void OnLoad(const StateBag& in) { (void)in; }

private:
    // Rebuilds the contiguous property views the editor reads. Property nodes
    // are unique_ptr, so the names and values they point at stay stable.
    void RefreshProperties()
    {
        for (const std::unique_ptr<Entity>& entity : m_Entities)
        {
            if (entity->m_EditorEntity == nullptr)
            {
                continue;
            }

            entity->m_PropertyViews.clear();
            entity->m_PropertyViews.reserve(entity->m_Properties.size());
            for (const std::unique_ptr<Property>& property : entity->m_Properties)
            {
                if (property->m_Data == nullptr)
                {
                    continue;
                }
                t_EntityProperty view;
                view.m_Name = property->m_Name.c_str();
                view.m_Type = property->m_Type;
                view.m_Data = property->m_Data;
                view.m_Min = property->m_Min;
                view.m_Max = property->m_Max;
                entity->m_PropertyViews.push_back(view);
            }

            entity->m_EditorEntity->m_Properties =
                entity->m_PropertyViews.empty() ? nullptr : entity->m_PropertyViews.data();
            entity->m_EditorEntity->m_PropertyCount = static_cast<uint32_t>(entity->m_PropertyViews.size());
            entity->m_bPropertiesDirty = false;
        }
    }

    void RefreshPropertiesIfDirty()
    {
        for (const std::unique_ptr<Entity>& entity : m_Entities)
        {
            if (entity->m_bPropertiesDirty)
            {
                RefreshProperties();
                return;
            }
        }
    }

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
