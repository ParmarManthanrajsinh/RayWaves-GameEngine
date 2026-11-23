#pragma once
#include <cstddef>
#include <cstdint>
#include <raylib.h>
#include <string>
#include <string_view>
#include <vector>

// Current scene file format. Readers accept older and missing versions and
// reject anything newer instead of silently dropping fields they do not know.
inline constexpr int c_SCENE_VERSION = 1;

// A single editor-authored sprite. The scene is deliberately a flat list of
// plain values: no ECS, no reflection, no gameplay coupling. GameEngine draws
// the list after the map draws, so existing game code never has to know it
// exists.
struct t_SceneObject
{
    uint32_t m_Id = 0;
    std::string m_Name;
    // Free-form gameplay tag. The engine only draws the sprite; game code can
    // read it through GameMap::GetSceneObjects() to find spawn points or decor.
    std::string m_Type;
    std::string m_TexturePath;
    Texture2D m_Texture{};
    Vector2 m_Position{ 0.0f, 0.0f };
    float m_Rotation = 0.0f;
    Vector2 m_Scale{ 1.0f, 1.0f };
    Rectangle m_Source{ 0.0f, 0.0f, 0.0f, 0.0f };
    Color m_Tint = WHITE;
    int m_Layer = 0;
    bool m_bVisible = true;

    // Set after a failed load so a missing texture does not retry every frame.
    // Runtime-only; never serialized.
    bool m_bTextureMissing = false;
};

class Scene
{
public:
    t_SceneObject& AddObject(std::string_view name);
    t_SceneObject* DuplicateObject(uint32_t id);
    bool b_RemoveObject(uint32_t id);

    // Undo helpers: reinsert a previously removed/copied object without losing
    // its id or position in the list. The GPU handle is always dropped so the
    // scene keeps ownership of textures; the next Draw reloads it lazily.
    t_SceneObject& InsertObject(t_SceneObject object);
    t_SceneObject& InsertObjectAt(std::size_t index, t_SceneObject object);
    std::size_t IndexOfObject(uint32_t id) const;

    t_SceneObject* FindObject(uint32_t id);
    const t_SceneObject* FindObject(uint32_t id) const;

    // Structural edits must go through the mutators above so the cached draw
    // order is invalidated. After editing m_Layer or m_bVisible in place, call
    // MarkDirty().
    std::vector<t_SceneObject>& GetObjects() { return m_Objects; }
    const std::vector<t_SceneObject>& GetObjects() const { return m_Objects; }

    // Object ids in draw order: visible objects only, ascending layer, ties in
    // list order. The order is cached and rebuilt lazily when the scene changes,
    // so the reference stays valid until the next mutation.
    const std::vector<uint32_t>& GetDrawOrder() const;

    void Draw();
    void Clear();

    // Reload a single texture after the inspector changes its path.
    void ReloadTexture(uint32_t id);

    bool b_SaveToFile(std::string_view path);
    bool b_LoadFromFile(std::string_view path);

    bool b_IsDirty() const { return m_bDirty; }
    void MarkDirty()
    {
        m_bDirty = true;
        InvalidateDrawOrder();
    }
    void ClearDirty() { m_bDirty = false; }

    // Map ids become file names under Assets/. Keep them path-safe so a map
    // id with slashes or spaces can never escape the asset root.
    static std::string ToFileKey(std::string_view key);

private:
    static bool b_EnsureTexture(t_SceneObject& object);
    static void UnloadTexture(t_SceneObject& object);

    void InvalidateDrawOrder() { m_bDrawOrderDirty = true; }
    // Rebuilds m_DrawIndices and m_DrawOrder from m_Objects. Only dirty scenes
    // pay the sort cost, and the vectors keep their capacity across rebuilds.
    void RebuildDrawOrder() const;

    std::vector<t_SceneObject> m_Objects;

    // Draw order cache. m_DrawIndices is what Draw iterates (indices into
    // m_Objects); m_DrawOrder holds the matching ids for inspection and tests.
    // Both are mutable so the const accessor can rebuild lazily.
    mutable std::vector<uint32_t> m_DrawOrder;
    mutable std::vector<uint32_t> m_DrawIndices;
    mutable bool m_bDrawOrderDirty = true;

    uint32_t m_NextId = 1;
    bool m_bDirty = false;
};
