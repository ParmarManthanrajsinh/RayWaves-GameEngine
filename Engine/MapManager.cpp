#include "MapManager.h"
#include "Log.h"
#include <sstream>

MapManager::MapManager() : m_CurrentMap(nullptr), m_bUsingDefaultMap(false)
{
    m_MapName = "MapManager";
    Log::Info("MapManager", "Initialized - ready for map registration");
}

// NOLINTNEXTLINE(bugprone-exception-escape)
MapManager::~MapManager()
{
    if (m_CurrentMap)
    {
        m_CurrentMap->SetTransitionCallback(nullptr);
        m_CurrentMap->SetExitCallback(nullptr);
    }
    Log::Info("MapManager", "Destroyed - all maps cleaned up");
}

void MapManager::Initialize()
{
    Log::Info("MapManager", "MapManager initialized - waiting for map registration");
    if (m_CurrentMap)
    {
        // Make sure the map has proper scene bounds
        Vector2 bounds = GameMap::GetSceneBounds();
        m_CurrentMap->SetSceneBounds(bounds.x, bounds.y);
        m_CurrentMap->SetProjectAssetPath(m_ProjectAssetPath);
        m_CurrentMap->SetStandaloneMode(b_IsStandalone());

        // Inject transition callback so the map can request transitions.
        // Deferred via QueueTransition: never destroys the calling map
        // from inside its own Initialize/Update/Draw (use-after-free).
        m_CurrentMap->SetTransitionCallback(
            [this](std::string_view map_id, bool force) { this->QueueTransition(map_id, force); });

        m_CurrentMap->SetExitCallback([this]() { this->RequestExit(); });

        m_CurrentMap->ClearEditorEntities();
        m_CurrentMap->Initialize();
        Log::Info("MapManager", "Successfully initialized with map: '" + m_CurrentMapId + "'");
        // The current map requested a transition from its own
        // Initialize(); its frame has returned, so swapping is safe.
        ProcessPendingTransition();
    }
    else if (!m_MapRegistry.empty())
    {
        if (!m_InitialMapId.empty() && b_IsMapRegistered(m_InitialMapId))
        {
            Log::Info("MapManager", "Auto-loading initial map: " + m_InitialMapId);
            b_GotoMap(m_InitialMapId);
        }
        else
        {
            Log::Info("MapManager", "Maps registered but none loaded yet. Use GotoMap() to load a map.");

            std::ostringstream registered_maps;
            registered_maps << "Registered maps:";
            for (const auto& PAIR : m_MapRegistry)
            {
                registered_maps << " '" << PAIR.first << "'";
            }
            Log::Info("MapManager", registered_maps.str());
        }
    }
    else
    {
        Log::Info("MapManager", "No maps registered yet. Register maps using RegisterMap<YourMap>()");
    }
}

void MapManager::Update(float delta_time)
{
    // Update the current map if we have one
    if (m_CurrentMap)
    {
        m_CurrentMap->Update(delta_time);
    }

    // Apply transitions requested from inside Update(): the old map's
    // frame has fully returned, so destroying it is safe now.
    ProcessPendingTransition();
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
            DrawRectangle(0, 0, static_cast<int>(GetSceneBounds().x), static_cast<int>(GetSceneBounds().y), DARKGRAY);

            DrawText("No map loaded - use GotoMap() to load a map", 50, 50, 20, RED);

            DrawText("Register maps using RegisterMap<YourMap>(\"MAP_ID\")", 50, 100, 16, WHITE);
        }
    }

    // Apply transitions requested from inside Draw() for the same reason
    // as in Update(). No-op when nothing is queued.
    ProcessPendingTransition();
}

void MapManager::SaveState(StateBag& out) const
{
    out.SetString("__mapmanager_current_id", m_CurrentMapId);
    if (m_CurrentMap)
    {
        m_CurrentMap->SaveState(out);
    }
}

void MapManager::LoadState(const StateBag& in_state)
{
    std::string map_id = in_state.GetString("__mapmanager_current_id", "");
    if (!map_id.empty() && b_IsMapRegistered(map_id) && !b_IsCurrentMap(map_id))
    {
        b_GotoMap(map_id);
    }

    if (m_CurrentMap)
    {
        m_CurrentMap->LoadState(in_state);
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

void MapManager::SetStandaloneMode(bool b_Standalone)
{
    GameMap::SetStandaloneMode(b_Standalone);

    if (m_CurrentMap)
    {
        m_CurrentMap->SetStandaloneMode(b_Standalone);
    }
}

bool MapManager::b_Uses3DView() const
{
    return (m_CurrentMap != nullptr) && m_CurrentMap->b_Uses3DView();
}

Camera3D MapManager::GetViewCamera3D() const
{
    if (m_CurrentMap != nullptr)
    {
        return m_CurrentMap->GetViewCamera3D();
    }
    return GameMap::GetViewCamera3D();
}

void MapManager::QueueTransition(std::string_view map_id, bool force_reload)
{
    // Last request wins; at most one deferred swap is pending at a time.
    m_PendingMapId = std::string(map_id);
    m_bPendingForce = force_reload;
    m_bPendingTransition = true;
}

void MapManager::ProcessPendingTransition()
{
    // A swap can queue another transition from the new map's Initialize(),
    // so keep going until the queue is empty. Capped: without the cap, two
    // maps requesting each other would loop here forever.
    int applied = 0;
    while (m_bPendingTransition)
    {
        if (applied >= k_MaxTransitionsPerDrain)
        {
            const std::string drop_message = "Transition limit reached (" + std::to_string(k_MaxTransitionsPerDrain) +
                " per frame); dropping request for '" + m_PendingMapId + "'";
            Log::Warn("MapManager", drop_message);
            m_bPendingTransition = false;
            m_PendingMapId.clear();
            m_bPendingForce = false;
            break;
        }
        ++applied;

        std::string next_id = m_PendingMapId;
        bool force = m_bPendingForce;
        m_bPendingTransition = false;
        m_PendingMapId.clear();
        m_bPendingForce = false;

        b_GotoMap(next_id, force);
    }
}

bool MapManager::b_GotoMap(std::string_view map_id, bool force_reload)
{
    // Re-entrancy guard: a map asked us to switch while we were already
    // mid-swap (e.g. from its own Initialize()). Queue instead of
    // recursing: the caller is still on the stack.
    if (m_bInTransition)
    {
        QueueTransition(map_id, force_reload);
        return true;
    }

    // Check if map is registered
    if (!b_IsMapRegistered(map_id))
    {
        Log::Error("MapManager", "Error: Map '" + std::string(map_id) + "' is not registered!");

        std::ostringstream available_maps;
        available_maps << "Available maps:";
        for (const auto& available_map : GetAvailableMaps())
        {
            available_maps << " '" << available_map << "'";
        }
        Log::Error("MapManager", available_maps.str());
        return false;
    }

    // If it's the same map and we don't want to force reload, just return true
    if (m_CurrentMapId == map_id && !force_reload)
    {
        return true;
    }

    Log::Info("MapManager", "Switching to map: '" + std::string(map_id) + "'");

    // Set the guard for the rest of this function. Cleared on every exit
    // path below so a queued transition can be applied afterwards.
    m_bInTransition = true;

    try
    {
        // Create the new map
        auto reg_it = m_MapRegistry.find(map_id);
        if (reg_it == m_MapRegistry.end())
        {
            m_bInTransition = false;
            return false;
        }
        auto new_map = reg_it->second();

        if (!new_map)
        {
            Log::Error("MapManager", "Error: Factory for map '" + std::string(map_id) + "' returned null!");
            m_bInTransition = false;
            return false;
        }

        // Created new map
        m_CurrentMap = std::move(new_map);
        m_CurrentMapId = map_id;
        auto info_it = m_MapInfo.find(map_id);
        if (info_it != m_MapInfo.end())
            info_it->second.b_IsLoaded = true;
        m_bUsingDefaultMap = false;

        // Set up the new map with current scene bounds
        Vector2 bounds = GameMap::GetSceneBounds();
        if (m_CurrentMap)
        {
            m_CurrentMap->SetSceneBounds(bounds.x, bounds.y);
            m_CurrentMap->SetProjectAssetPath(m_ProjectAssetPath);
            m_CurrentMap->SetStandaloneMode(b_IsStandalone());

            // Inject transition callback for map-driven transitions.
            // Deferred: destroying the current map from inside its own
            // Initialize() would be a use-after-free.
            m_CurrentMap->SetTransitionCallback(
                [this](std::string_view request_id, bool force) { this->QueueTransition(request_id, force); });

            m_CurrentMap->SetExitCallback([this]() { this->RequestExit(); });

            m_CurrentMap->ClearEditorEntities();
            m_CurrentMap->Initialize();
        }

        m_bInTransition = false;

        Log::Info("MapManager", "Successfully loaded map: '" + std::string(map_id) + "'");

        Log::Info("MapManager", "Current map Id: '" + m_CurrentMapId + "'");

        // A transition requested from the new map's Initialize() is applied
        // by the next Update()/Draw(), not here. Draining here would nest
        // swaps inside this call and recurse without bound.
        return true;
    }
    catch (const std::exception& e)
    {
        m_bInTransition = false;
        Log::Error("MapManager", "Error creating map '" + std::string(map_id) + "': " + e.what());
        return false;
    }
    catch (...)
    {
        m_bInTransition = false;
        Log::Error("MapManager", "Unknown error creating map '" + std::string(map_id) + "'");
        return false;
    }
}

bool MapManager::b_IsCurrentMap(std::string_view map_id) const
{
    return m_CurrentMapId == map_id && m_CurrentMap != nullptr;
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
        Log::Info("MapManager", "Unloading map '" + m_CurrentMapId + "'");

        // Mark as not loaded in metadata
        auto info_it = m_MapInfo.find(m_CurrentMapId);
        if (info_it != m_MapInfo.end())
        {
            info_it->second.b_IsLoaded = false;
        }

        m_CurrentMap.reset();
        m_CurrentMapId = "";
        m_bUsingDefaultMap = false;
        m_bPendingTransition = false;
        m_PendingMapId.clear();
        m_bPendingForce = false;
    }
    else
    {
        Log::Warn("MapManager", "No map to unload");
    }
}

bool MapManager::b_ReloadCurrentMap()
{
    if (m_CurrentMapId.empty())
    {
        Log::Error("MapManager", "No current map to reload");
        return false;
    }

    std::string map_to_reload = m_CurrentMapId;
    Log::Info("MapManager", "Reloading map: '" + map_to_reload + "'");

    return b_GotoMap(map_to_reload, true);
}

std::string MapManager::GetDebugInfo() const
{
    std::stringstream debug_info;
    debug_info << "=== MapManager Debug Info ===\n";

    debug_info << "Current Map: " << (m_CurrentMapId.empty() ? "None" : m_CurrentMapId) << "\n";

    debug_info << "Using Default Map: " << (m_bUsingDefaultMap ? "Yes" : "No") << "\n";

    debug_info << "Registered Maps (" << m_MapRegistry.size() << "):";

    for (const auto& PAIR : m_MapInfo)
    {
        std::string_view MAP_ID = PAIR.first;
        const t_MapInfo& INFO = PAIR.second;
        debug_info << "\n  - '" << MAP_ID << "': " << INFO.description;
        debug_info << " [" << (INFO.b_IsLoaded ? "LOADED" : "NOT LOADED") << "]";
    }

    if (m_MapRegistry.empty())
    {
        debug_info << "\n  (No maps registered - call RegisterMap<YourMap>() to register maps)";
    }

    return debug_info.str();
}

void MapManager::LoadDefaultMap()
{
    Log::Warn("MapManager", "No default map available in Engine library");
    Log::Warn("MapManager", "Register and load your own maps using RegisterMap<YourMap>()");
}
