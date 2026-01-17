#pragma once
#include "GameMap.h"
#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>
#include <sstream>
#include <print>

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
    std::unordered_map<std::string, 
    std::function<std::unique_ptr<GameMap>()>> m_MapRegistry;
    std::string m_CurrentMapId;
    
    // Map metadata for better developer experience
    struct t_MapInfo 
    {
        std::string description;
        bool b_IsLoaded = false;
    };

    std::unordered_map<std::string, t_MapInfo> m_MapInfo;
    bool m_bUsingDefaultMap;

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
    
    void SetSceneBounds(float width, float height);
    Vector2 GetSceneBounds() const;
 
    template<typename T>
    void RegisterMap
    (
        const std::string& map_id, 
        const std::string& description = ""
    );
    bool b_GotoMap(const std::string& map_id, bool force_reload = false);
    bool b_IsCurrentMap(const std::string& map_id) const;
    bool b_IsMapRegistered(const std::string& map_id) const;
    bool b_ReloadCurrentMap();

    const std::string& GetCurrentMapId() const { return m_CurrentMapId; }
    std::vector<std::string> GetAvailableMaps() const;

    void UnloadCurrentMap();
    std::string GetDebugInfo() const;

    
private:

    void LoadDefaultMap();
};

/*
+--------------------------------------------------------+
|                   UTILITY TEMPLATES                    |
+--------------------------------------------------------+
*/

template<typename T>
void MapManager::RegisterMap
(
    const std::string& map_id, 
    const std::string& description
)
{
    static_assert
    (
        std::is_base_of_v<GameMap, T>, 
        "Map type must inherit from GameMap"
    );
    
    // Create factory function that creates instances of type T
    m_MapRegistry[map_id] = []() -> std::unique_ptr<GameMap> 
    {
        return std::make_unique<T>();
    };
    
    // Store metadata
    m_MapInfo[map_id] = 
    { 
        description.empty() ? "No description" : description, false 
    };

    std::println("[MapManager] Registered map: {} - {}", map_id, description);
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
inline void RegisterMap
(
    Manager& manager, 
    std::string_view name = typeid(MapClass).name()
) 
{
    manager.template RegisterMap<MapClass>(std::string(name));
}

template <typename MapClass, typename Manager>
inline void RegisterMapAs
(
    Manager& manager, 
    int map_id, 
    const std::string& desc
)
{
    manager.template RegisterMap<MapClass>(map_id, desc);
}