#include "MapManager.h"

MapManager::MapManager()
    : m_CurrentMap(nullptr)
    , m_CurrentMapId("")
    , m_bUsingDefaultMap(false)
{
    std::println("[MapManager] Initialized - ready for map registration");
}

MapManager::~MapManager()
{
    std::println("[MapManager] Destroyed - all maps cleaned up");
}

void MapManager::Initialize()
{
    std::println("[MapManager] MapManager initialized - waiting for map registration");
    if (m_CurrentMap)
    {
        // Make sure the map has proper scene bounds
        Vector2 bounds = GameMap::GetSceneBounds();
        m_CurrentMap->SetSceneBounds(bounds.x, bounds.y);

        // Inject transition callback so the map can request transitions
        m_CurrentMap->SetTransitionCallback
        (
            [this](std::string_view id, bool force)
            {
                this->b_GotoMap(std::string(id), force);
            }
        );

        m_CurrentMap->Initialize();
        std::println("[MapManager] Successfully initialized with map: {}'", m_CurrentMapId);
    }
    else if (!m_MapRegistry.empty())
    {
        std::println
        (
            "[MapManager] Maps registered but none loaded yet. Use GotoMap() to load a map."
        );

        std::print("[MapManager] Registered maps: ");
        for (const auto& PAIR : m_MapRegistry)
        {
            std::println("'{}' ", PAIR.first);
        }
    }
    else
    {
        std::println
        (
            "[MapManager] No maps registered yet. Register maps using RegisterMap<YourMap>()"
        );
    }
}

void MapManager::Update(float delta_time)
{
    // Update the current map if we have one
    if (m_CurrentMap)
    {
        m_CurrentMap->Update(delta_time);
    }
}

void MapManager::Draw()
{
    // Draw the current map if we have one
    if (m_CurrentMap)
    {
        m_CurrentMap->Draw();
    }
    else
    {
        // Check if the window is ready before drawing
        if (GetWindowHandle() != nullptr)
        {
            DrawRectangle
            (
                0, 
                0, 
                static_cast<int>(GetSceneBounds().x), 
                static_cast<int>(GetSceneBounds().y), 
                DARKGRAY
            );

            DrawText
            (
                "No map loaded - use GotoMap() to load a map", 
                50, 
                50, 
                20, 
                RED
            );

            DrawText
            (
                "Register maps using RegisterMap<YourMap>(\"MAP_ID\")",
                50, 
                100, 
                16, 
                WHITE
            );
        }
    }
}

void MapManager::SetSceneBounds(float width, float height)
{
    // Update our own bounds first
    GameMap::SetSceneBounds(width, height);
    
    // Forward to current map if it exists
    if (m_CurrentMap)
    {
        m_CurrentMap->SetSceneBounds(width, height);
    }
}

Vector2 MapManager::GetSceneBounds() const
{
    // If we have a current map, get bounds from it
    if (m_CurrentMap)
    {
        return m_CurrentMap->GetSceneBounds();
    }
    
    return GameMap::GetSceneBounds();
}

bool MapManager::b_GotoMap(const std::string& map_id, bool force_reload)
{
    // Check if map is registered
    if (!b_IsMapRegistered(map_id))
    {
        std::println
        (
            std::cerr,
            "[MapManager] Error: Map '{}' is not registered!",
            map_id
        );

        std::print(std::cerr, "[MapManager] Available maps: ");
        for (const auto& available_map : GetAvailableMaps())
        {
            std::print(std::cerr, "'{}' ", available_map);
        }
        std::print(std::cerr, "\n");
        return false;
    }

    // If it's the same map and we don't want to force reload, just return true
    if (m_CurrentMapId == map_id && !force_reload)
    {
        std::println
        (
            "[MapManager] Map '{}' is already loaded",
            map_id
        );
        return true;
    }

    std::println
    (
        "[MapManager] Switching to map: '{}'",
        map_id
    );

    try
    {
        // Create the new map
        auto& factory = m_MapRegistry[map_id];
        auto new_map = factory();

        if (!new_map)
        {
            std::println
            (
                std::cerr,
                "[MapManager] Error: Factory for map '{}' returned null!",
                map_id
            );
            return false;
        }

        // Created new map 
        m_CurrentMap = std::move(new_map);
        m_CurrentMapId = map_id;
        m_MapInfo[map_id].b_IsLoaded = true;
        m_bUsingDefaultMap = false;

        // Set up the new map with current scene bounds
        Vector2 bounds = GameMap::GetSceneBounds();
        if (m_CurrentMap)
        {
            m_CurrentMap->SetSceneBounds(bounds.x, bounds.y);

            // Inject transition callback for map-driven transitions
            m_CurrentMap->SetTransitionCallback
            (
                [this](std::string_view id, bool force)
                {
                    this->b_GotoMap(std::string(id), force);
                }
            );
            m_CurrentMap->Initialize();
        }

        std::println
        (
            "[MapManager] Successfully loaded map: '{}'",
            map_id
        );

        std::println
        (
            "[MapManager] Current map Id: '{}'",
            m_CurrentMapId
        );

        return true;
    }
    catch (const std::exception& e)
    {
        std::println
        (
            std::cerr,
            "[MapManager] Error creating map '{}': {}",
            map_id,
            e.what()
        );
        return false;
    }
    catch (...)
    {
        std::println
        (
            std::cerr,
            "[MapManager] Unknown error creating map '{}'",
            map_id
        );
        return false;
    }
}


bool MapManager::b_IsCurrentMap(const std::string& map_id) const
{
    return m_CurrentMapId == map_id &&
           m_CurrentMap != nullptr;
}

std::vector<std::string> MapManager::GetAvailableMaps() const
{
    std::vector<std::string> maps;
    maps.reserve(m_MapRegistry.size());
    
    for (const auto& pair : m_MapRegistry)
    {
        maps.push_back(pair.first);
    }
    
    return maps;
}

bool MapManager::b_IsMapRegistered(const std::string& map_id) const
{
    return m_MapRegistry.find(map_id) != m_MapRegistry.end();
}

void MapManager::UnloadCurrentMap()
{
    if (m_CurrentMap)
    {
        std::println("[MapManager] Unloading map '{}'", m_CurrentMapId);

        // Mark as not loaded in metadata
        if (m_MapInfo.find(m_CurrentMapId) != m_MapInfo.end())
        {
            m_MapInfo[m_CurrentMapId].b_IsLoaded = false;
        }
        
        m_CurrentMap.reset();
        m_CurrentMapId = "";
        m_bUsingDefaultMap = false;
    }
    else
    {
        std::println("[MapManager] No map to unload");
    }
}

bool MapManager::b_ReloadCurrentMap()
{
    if (m_CurrentMapId.empty())
    {
        std::println("[MapManager] No current map to reload");
        return false;
    }
    
    std::string map_to_reload = m_CurrentMapId;
    std::println("[MapManager] Reloading map: '{}'", map_to_reload);
    
    return b_GotoMap(map_to_reload, true);
}

std::string MapManager::GetDebugInfo() const
{
    std::stringstream ss;
    ss << "=== MapManager Debug Info ===\n";

    ss << "Current Map: " 
       << ( m_CurrentMapId.empty() ? "None" : m_CurrentMapId) 
       << "\n";

    ss << "Using Default Map: " 
       << (m_bUsingDefaultMap ? "Yes" : "No") 
       << "\n";

    ss << "Registered Maps (" << m_MapRegistry.size() << "):";
    
    for (const auto& pair : m_MapInfo)
    {
        const std::string& MAP_ID = pair.first;
        const t_MapInfo& INFO = pair.second;
        ss << "\n  - '" << MAP_ID << "': " << INFO.description;
        ss << " [" << (INFO.b_IsLoaded ? "LOADED" : "NOT LOADED") << "]";
    }
    
    if (m_MapRegistry.empty())
    {
        ss << "\n  (No maps registered - call RegisterMap<YourMap>() to register maps)";
    }
    
    return ss.str();
}

void MapManager::LoadDefaultMap()
{
    std::println("[MapManager] No default map available in Engine library");
    std::println("[MapManager] Register and load your own maps using RegisterMap<YourMap>()");
}
