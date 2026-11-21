#include "GameMap.h"
#include "AssetResolver.h"
#include <algorithm>
#include <iostream>

GameMap::GameMap() : m_MapName("DefaultMap")
{
}

GameMap::GameMap(std::string_view map_name) : m_MapName(map_name)
{
}

void GameMap::Initialize()
{
    // Initialization code for the game map
}

void GameMap::Update(float delta_time)
{
    // Update logic for the game map
}

void GameMap::Draw()
{
    // Drawing logic for the game map
}

void GameMap::SetProjectAssetPath(const std::string& path)
{
    m_ProjectAssetPath = path;
    AssetResolver::SetProjectAssetPath(path);
}

void GameMap::SetMapName(std::string_view map_name)
{
    m_MapName = map_name;
}

std::string GameMap::GetMapName() const
{
    return m_MapName;
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
void GameMap::SetSceneBounds(float width, float height)
{
    m_SceneWidth = width;
    m_SceneHeight = height;
}

Vector2 GameMap::GetSceneBounds() const
{
    return Vector2{ m_SceneWidth, m_SceneHeight };
}

void GameMap::SetEditorViewCamera3D(const Camera3D& camera)
{
    m_EditorViewCamera3D = camera;
    m_bHasEditorViewCamera3D = true;
}

void GameMap::ClearEditorViewCamera3D()
{
    m_bHasEditorViewCamera3D = false;
}

Camera3D GameMap::GetRenderCamera3D() const
{
    return m_bHasEditorViewCamera3D ? m_EditorViewCamera3D : GetViewCamera3D();
}

void GameMap::BeginView3D() const
{
    BeginMode3D(GetRenderCamera3D());
}

void GameMap::EndView3D()
{
    EndMode3D();
}

void GameMap::SetTargetFPS(int fps)
{
    ::SetTargetFPS(fps);
    m_TargetFPS = fps;
}

int GameMap::GetTargetFPS() const
{
    return m_TargetFPS;
}

t_EditorEntity* GameMap::RegisterEditorEntity(std::string_view name, Vector2* position, Vector2* size, uint64_t id)
{
    t_EditorEntity entity;
    entity.m_Id = id;
    entity.m_Name = std::string(name);
    entity.m_Type = entity.m_Name;
    entity.m_Position = position;
    entity.m_Size = size;
    return RegisterEditorEntity(std::move(entity));
}

t_EditorEntity* GameMap::AddEntity(
    std::string_view name, Vector2* position, Vector2* size, const t_EntityOptions& options)
{
    t_EditorEntity entity;
    entity.m_Id = options.Id;
    entity.m_Name = std::string(name);
    entity.m_Type = options.Type.empty() ? entity.m_Name : std::string(options.Type);
    entity.m_Position = position;
    entity.m_Size = size;
    entity.m_Origin = options.Origin;
    entity.m_Rotation = options.Rotation;
    entity.m_Visible = options.Visible;
    return RegisterEditorEntity(std::move(entity));
}

t_EditorEntity* GameMap::AddEntity(
    std::string_view name, Vector3* position, Vector3* size, const t_EntityOptions& options)
{
    t_EditorEntity entity;
    entity.m_Id = options.Id;
    entity.m_Name = std::string(name);
    entity.m_Type = options.Type.empty() ? entity.m_Name : std::string(options.Type);
    entity.m_Position3D = position;
    entity.m_Size3D = size;
    entity.m_Origin3D = options.Origin3D;
    entity.m_Rotation = options.Rotation;
    entity.m_Visible = options.Visible;
    return RegisterEditorEntity(std::move(entity));
}

t_EditorEntity* GameMap::RegisterEditorEntity(t_EditorEntity entity)
{
    if ((entity.m_Position == nullptr) && (entity.m_Position3D == nullptr))
    {
        std::cerr << "[GameMap] RegisterEditorEntity rejected: null position for '" << entity.m_Name << "'\n";
        return nullptr;
    }

    const auto id_exists = [this](uint64_t id)
    { return std::ranges::any_of(m_EditorEntities, [id](const t_EditorEntity& item) { return item.m_Id == id; }); };

    if (entity.m_Id != 0 && !id_exists(entity.m_Id))
    {
        m_NextEditorEntityId = std::max(m_NextEditorEntityId, entity.m_Id + 1);
    }
    else
    {
        entity.m_Id = m_NextEditorEntityId++;
        while (id_exists(entity.m_Id))
        {
            entity.m_Id = m_NextEditorEntityId++;
        }
    }

    if (entity.m_Name.empty())
    {
        entity.m_Name = "Entity " + std::to_string(entity.m_Id);
    }
    if (entity.m_Type.empty())
    {
        entity.m_Type = entity.m_Name;
    }

    m_EditorEntities.push_back(std::move(entity));
    return &m_EditorEntities.back();
}

bool GameMap::UnregisterEditorEntity(uint64_t id)
{
    const auto entity =
        std::ranges::find_if(m_EditorEntities, [id](const t_EditorEntity& item) { return item.m_Id == id; });
    if (entity == m_EditorEntities.end())
    {
        return false;
    }

    m_EditorEntities.erase(entity);
    return true;
}

void GameMap::ClearEditorEntities()
{
    m_EditorEntities.clear();
    m_NextEditorEntityId = 1;
}

const GameMap::t_EditorEntityList& GameMap::GetEditorEntities() const
{
    return m_EditorEntities;
}

GameMap::t_EditorEntityList& GameMap::GetEditorEntities()
{
    return m_EditorEntities;
}

void GameMap::SetSceneObjects(const t_SceneObjectView* objects, size_t count)
{
    m_SceneObjects.clear();
    if ((objects != nullptr) && (count > 0))
    {
        m_SceneObjects.reserve(count);
        for (size_t index = 0; index < count; ++index)
        {
            const t_SceneObjectView& view = objects[index];
            t_SceneObjectInfo info;
            info.m_Id = view.m_Id;
            if (view.m_Name != nullptr)
            {
                info.m_Name = view.m_Name;
            }
            if (view.m_Type != nullptr)
            {
                info.m_Type = view.m_Type;
            }
            if (view.m_TexturePath != nullptr)
            {
                info.m_TexturePath = view.m_TexturePath;
            }
            info.m_Position = view.m_Position;
            info.m_Rotation = view.m_Rotation;
            info.m_Scale = view.m_Scale;
            info.m_Source = view.m_Source;
            info.m_Tint = view.m_Tint;
            info.m_Layer = view.m_Layer;
            info.m_bVisible = view.m_bVisible;
            m_SceneObjects.push_back(std::move(info));
        }
    }

    OnSceneChanged();
}

const t_SceneObjectInfo* GameMap::FindSceneObject(std::string_view name) const
{
    const auto found =
        std::ranges::find_if(m_SceneObjects, [name](const t_SceneObjectInfo& object) { return object.m_Name == name; });
    return (found == m_SceneObjects.end()) ? nullptr : &*found;
}

std::vector<const t_SceneObjectInfo*> GameMap::FindSceneObjectsOfType(std::string_view type) const
{
    std::vector<const t_SceneObjectInfo*> matches;
    for (const t_SceneObjectInfo& object : m_SceneObjects)
    {
        if (object.m_Type == type)
        {
            matches.push_back(&object);
        }
    }
    return matches;
}

void GameMap::SetStandaloneMode(bool b_Standalone)
{
    m_bStandalone = b_Standalone;
}

bool GameMap::b_IsStandalone() const
{
    return m_bStandalone;
}

void GameMap::SetTransitionCallback(std::function<void(std::string_view, bool)> callback)
{
    m_TransitionCallback = std::move(callback);
}

void GameMap::SetExitCallback(std::function<void()> callback)
{
    m_ExitCallback = std::move(callback);
}

void GameMap::RequestExit()
{
    if (m_ExitCallback)
    {
        m_ExitCallback();
    }
    else
    {
        std::cerr << "Exit callback not set!\n";
    }
}

void GameMap::RequestGotoMap(std::string_view map_id, bool force_reload) const
{
    if (m_TransitionCallback)
    {
        m_TransitionCallback(map_id, force_reload);
    }
    else
    {
        std::cerr << "Transition callback not set!\n";
    }
}
