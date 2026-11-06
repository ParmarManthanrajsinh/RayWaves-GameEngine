#include <iostream> // load-bearing: RegisterMap<T>() below logs via std::cout
#pragma once
#include "GameMap.h"
#include <functional>
#include <map>
#include <memory>
#include <vector>
/**
 * @brief Developer-friendly MapManager for easy game map management
 * 
 * This class provides a simple interface for game developers to:
 * - Register maps with a single line of code
 * - Switch between maps easily
 * - Handle map transitions smoothly
 * - Get helpful error messages and debugging info
 * 
 * Example Usage:
 * @code
 * // In your map registration (usually in Initialize):
 * manager.RegisterMap<YourMapClass>("map_id");
 * 
 * // Switch maps anywhere in your code:
 * manager.b_GotoMap("map_id");
 * 
 * // Check current map:
 * if (manager.b_IsCurrentMap("map_id")) 
 * {
 *     // Do something specific to that map
 * }
 * @endcode
 */
class MapManager : public GameMap
{
private:
    std::unique_ptr<GameMap> m_CurrentMap;

    // Registry of available maps with their factory functions
    std::map<std::string, std::function<std::unique_ptr<GameMap>()>, std::less<>> m_MapRegistry;
    std::string m_CurrentMapId;
    std::string m_InitialMapId;

    // Map metadata for better developer experience
    struct t_MapInfo
    {
        std::string description;
        bool b_IsLoaded = false;
    };

    std::map<std::string, t_MapInfo, std::less<>> m_MapInfo;
    mutable std::vector<std::string> m_AvailableMapsCache;
    mutable bool m_bMapsCacheDirty = true;
    bool m_bUsingDefaultMap;

    // Deferred transition queue. A map that calls RequestGotoMap from
    // inside Initialize/Update/Draw cannot be destroyed synchronously:
    // b_GotoMap would delete the map whose stack frame is still running
    // (use-after-free). Callbacks stash the request here; Update()/Draw()
    // drain it once the caller has returned.
    std::string m_PendingMapId;
    bool m_bPendingForce = false;
    bool m_bPendingTransition = false;
    bool m_bInTransition = false;

public:
    MapManager();
    ~MapManager() override;

    // Explicitly declare move constructor and assignment operator
    MapManager(MapManager&&) noexcept = default;
    MapManager& operator=(MapManager&&) noexcept = default;

    // Delete copy constructor and assignment operator
    MapManager(const MapManager&) = delete;
    MapManager& operator=(const MapManager&) = delete;

    void Initialize() override;
    void Update(float delta_time) override;
    void Draw() override;

    void SaveState(StateBag& out) const override;
    void LoadState(const StateBag& in) override;
    bool b_IsMapManager() const override { return true; }

    void SetSceneBounds(float width, float height);
    Vector2 GetSceneBounds() const;

    template <typename T>
    void RegisterMap(std::string_view map_id, std::string_view description = "");
    // Returns true when the swap happened, or when it was queued because a
    // swap is already in progress (applied by the next Update()/Draw()).
    bool b_GotoMap(std::string_view map_id, bool force_reload = false);
    bool b_IsCurrentMap(std::string_view map_id) const;
    bool b_IsMapRegistered(std::string_view map_id) const;
    bool b_ReloadCurrentMap();

    void SetInitialMap(std::string_view map_id) { m_InitialMapId = std::string(map_id); }

    std::string_view GetCurrentMapId() const { return m_CurrentMapId; }
    const std::vector<std::string>& GetAvailableMaps() const;

    void UnloadCurrentMap();
    std::string GetDebugInfo() const;

    // Upper bound on map swaps applied from one drain. Stops two maps that
    // request each other from Initialize() from looping forever.
    static constexpr int k_MaxTransitionsPerDrain = 8;

private:
    // Queue a transition requested through a map callback. Deferred so the
    // requesting map is never destroyed while its own code is running.
    void QueueTransition(std::string_view map_id, bool force_reload = false);
    // Applies queued transitions. Only called from the top-level entry
    // points (Initialize, Update, Draw), never from inside b_GotoMap.
    void ProcessPendingTransition();

    static void LoadDefaultMap();
};

/*
+--------------------------------------------------------+
|                   UTILITY TEMPLATES                    |
+--------------------------------------------------------+
*/

template <typename T>
void MapManager::RegisterMap(std::string_view map_id, std::string_view description)
{
    static_assert(std::is_base_of_v<GameMap, T>, "Map type must inherit from GameMap");

    // Create factory function that creates instances of type T
    m_MapRegistry[std::string(map_id)] = []() -> std::unique_ptr<GameMap> { return std::make_unique<T>(); };

    // Store metadata
    m_MapInfo[std::string(map_id)] = { description.empty() ? "No description" : std::string(description), false };

    m_bMapsCacheDirty = true;
    std::cout << "[MapManager] Registered map: " << map_id << "::" << description << std::endl;
}
/*
+----------------------------------------------------------------+
|                    HELPER TEMPLATE FUNCTIONS                   |
+----------------------------------------------------------------+
*/

/*
Disadvantages of macros:

No type safety (compiler won’t catch mismatches).

Harder to debug (the debugger doesn’t “see” macros).

Error messages can be confusing.

Can accidentally evaluate arguments multiple times.

*/

//#define REGISTER_MAP(manager, MapClass) \
//    (manager).RegisterMap<MapClass>(#MapClass)

template <typename MapClass, typename Manager>
inline void RegisterMap(Manager& manager, std::string_view name = typeid(MapClass).name())
{
    manager.template RegisterMap<MapClass>(std::string(name));
}
