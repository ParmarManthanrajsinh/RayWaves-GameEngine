#include <iostream>
#include <sstream>
#include "MapManager.h"

MapManager::MapManager()
    : m_CurrentMap(nullptr)
    , m_CurrentMapId("")
    , m_bUsingDefaultMap(false)
{
    m_MapName = "_RAYWAVES_MAP_MANAGER_";
    std::cout << "[MapManager] Initialized - ready for map registration\n";
}

MapManager::~MapManager()
{
    std::cout << "[MapManager] Destroyed - all maps cleaned up\n";
}

void MapManager::Initialize()
{
    std::cout << "[MapManager] MapManager initialized - waiting for map registration\n";
    if (m_CurrentMap)
    {
        // Make sure the map has proper scene bounds
        Vector2 bounds = GameMap::GetSceneBounds();
        m_CurrentMap->SetSceneBounds(bounds.x, bounds.y);
        m_CurrentMap->SetProjectAssetPath(m_ProjectAssetPath);

        // Inject transition callback so the map can request transitions
        m_CurrentMap->SetTransitionCallback
        (
            [this](std::string_view id, bool force)
            {
                this->b_GotoMap(std::string(id), force);
            }
        );

        m_CurrentMap->SetExitCallback
        (
            [this]()
            {
                this->RequestExit();
            }
        );

        m_CurrentMap->Initialize();
        std::cout << "[MapManager] Successfully initialized with map: '" << m_CurrentMapId << "'" << "\n";
    }
    else if (!m_MapRegistry.empty())
    {
        if (!m_InitialMapId.empty() && b_IsMapRegistered(m_InitialMapId))
        {
            std::cout << "[MapManager] Auto-loading initial map: " << m_InitialMapId << "\n";
            b_GotoMap(m_InitialMapId);
        }
        else
        {
            std::cout << "[MapManager] Maps registered but none loaded yet. Use GotoMap() to load a map." << "\n";

            std::cout << "[MapManager] Registered maps: ";
            for (const auto& PAIR : m_MapRegistry)
            {
                std::cout << "'" << PAIR.first << "' " << "\n";
            }
        }
    }
    else
    {
        std::cout << "[MapManager] No maps registered yet. Register maps using RegisterMap<YourMap>()" << "\n";
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

void MapManager::SaveState(StateBag& out) const
{
    out.SetString("__mapmanager_current_id", m_CurrentMapId);
    if (m_CurrentMap)
    {
        m_CurrentMap->SaveState(out);
    }
}

void MapManager::LoadState(const StateBag& in)
{
    std::string map_id = in.GetString("__mapmanager_current_id", "");
    if (!map_id.empty() && b_IsMapRegistered(map_id) && !b_IsCurrentMap(map_id))
    {
        b_GotoMap(map_id);
    }
    
    if (m_CurrentMap)
    {
        m_CurrentMap->LoadState(in);
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

bool MapManager::b_GotoMap(std::string_view map_id, bool force_reload)
{
    // Check if map is registered
    if (!b_IsMapRegistered(map_id))
    {
        std::cerr << "[MapManager] Error: Map '" << map_id << "' is not registered!" << "\n";

        auto maps = GetAvailableMaps();
        std::cerr << "[MapManager] Available maps: ";
        for (const auto& available_map : maps)
        {
            std::cerr << "'" << available_map << "' " << "\n";
        }
        std::cerr << "\n";
        return false;
    }

    // If it's the same map and we don't want to force reload, just return true
    if (m_CurrentMapId == map_id && !force_reload)
    {
        std::cout << "[MapManager] Map '" << map_id << "' is already loaded" << "\n";
        return true;
    }

    std::cout << "[MapManager] Switching to map: '" << map_id << "'" << "\n";

    try
    {
        // Create the new map
        auto reg_it = m_MapRegistry.find(map_id);
        if (reg_it == m_MapRegistry.end()) return false;
        auto new_map = reg_it->second();

        if (!new_map)
        {
            std::cerr << "[MapManager] Error: Factory for map '" << map_id << "' returned null!" << "\n";
            return false;
        }

        // Created new map 
        m_CurrentMap = std::move(new_map);
        m_CurrentMapId = map_id;
        auto info_it = m_MapInfo.find(map_id);
        if (info_it != m_MapInfo.end()) info_it->second.b_IsLoaded = true;
        m_bUsingDefaultMap = false;

        // Set up the new map with current scene bounds
        Vector2 bounds = GameMap::GetSceneBounds();
        if (m_CurrentMap)
        {
            m_CurrentMap->SetSceneBounds(bounds.x, bounds.y);
            m_CurrentMap->SetProjectAssetPath(m_ProjectAssetPath);

            // Inject transition callback for map-driven transitions
            m_CurrentMap->SetTransitionCallback
            (
                [this](std::string_view id, bool force)
                {
                    this->b_GotoMap(std::string(id), force);
                }
            );

            m_CurrentMap->SetExitCallback
            (
                [this]()
                {
                    this->RequestExit();
                }
            );

            m_CurrentMap->Initialize();
        }

        std::cout << "[MapManager] Successfully loaded map: '" << map_id << "'" << "\n";

        std::cout << "[MapManager] Current map Id: '" << m_CurrentMapId << "'" << "\n";

        return true;
    }
    catch (const std::exception& e)
    {
        std::cerr << "[MapManager] Error creating map '" << map_id << "': " << e.what() << "\n";
        return false;
    }
    catch (...)
    {
        std::cerr << "[MapManager] Unknown error creating map '" << map_id << "'\n";
        return false;
    }
}


bool MapManager::b_IsCurrentMap(std::string_view map_id) const
{
    return m_CurrentMapId == map_id &&
           m_CurrentMap != nullptr;
}

const std::vector<std::string>& MapManager::GetAvailableMaps() const
{
    if (m_bMapsCacheDirty)
    {
        m_AvailableMapsCache.clear();
        m_AvailableMapsCache.reserve(m_MapRegistry.size());
        for (const auto& pair : m_MapRegistry)
        {
            m_AvailableMapsCache.push_back(pair.first);
        }
        m_bMapsCacheDirty = false;
    }
    return m_AvailableMapsCache;
}

bool MapManager::b_IsMapRegistered(std::string_view map_id) const
{
    return m_MapRegistry.contains(map_id);
}

void MapManager::UnloadCurrentMap()
{
    if (m_CurrentMap)
    {
        std::cout << "[MapManager] Unloading map '" << m_CurrentMapId << "'" << "\n";

        // Mark as not loaded in metadata
        auto info_it = m_MapInfo.find(m_CurrentMapId);
        if (info_it != m_MapInfo.end())
        {
            info_it->second.b_IsLoaded = false;
        }
        
        m_CurrentMap.reset();
        m_CurrentMapId = "";
        m_bUsingDefaultMap = false;
    }
    else
    {
        std::cout << "[MapManager] No map to unload" << "\n";
    }
}

bool MapManager::b_ReloadCurrentMap()
{
    if (m_CurrentMapId.empty())
    {
        std::cout << "[MapManager] No current map to reload" << "\n";
        return false;
    }
    
    std::string map_to_reload = m_CurrentMapId;
    std::cout << "[MapManager] Reloading map: '" << map_to_reload << "'" << "\n";
    
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
    
    for (const auto& PAIR : m_MapInfo)
    {
        std::string_view MAP_ID = PAIR.first;
        const t_MapInfo& INFO = PAIR.second;
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
    std::cout << "[MapManager] No default map available in Engine library" << "\n";
    std::cout << "[MapManager] Register and load your own maps using RegisterMap<YourMap>()" << "\n";
}
