#pragma once
#include <cstdint>
#include <raylib.h>
#include <string>
#include <string_view>
#include <functional>
#include "GameState.h"

// GameLogic DLL ABI version. GameLogic DLLs must export
// GetGameLogicAbiVersion() returning this value. The editor/runtime reject
// DLLs with a different (or missing) version instead of risking heap
// corruption across the DLL boundary.
inline constexpr uint32_t RAYWAVES_GAMELOGIC_ABI_VERSION = 1;

class GameMap
{
protected:
    std::string m_MapName;
    float m_SceneWidth = 0.0f;   
    float m_SceneHeight = 0.0f;  
	int m_TargetFPS = 60;
    std::string m_ProjectAssetPath;

    // Transition callback to request a map change via the manager
    std::function<void(std::string_view, bool)> m_TransitionCallback;

    // Exit callback so DLL can request shutdown without calling CloseWindow() directly
    std::function<void()> m_ExitCallback;

public:
    GameMap(); 
    GameMap(std::string_view map_name);
    virtual ~GameMap() = default;  

    virtual void Initialize();
    virtual void Update(float delta_time);
    virtual void Draw();
    
    virtual void SetProjectAssetPath(const std::string& path);
    
    virtual void SaveState(StateBag& out) const {}
    virtual void LoadState(const StateBag& in) {}

    // Type query across the DLL boundary. RTTI/typeid cannot be trusted
    // between separately linked modules, so MapManager identifies itself
    // via this virtual instead. Never guess by map name.
    virtual bool b_IsMapManager() const { return false; }
    
    void SetMapName(std::string_view map_name);
    std::string GetMapName() const;
    void SetSceneBounds(float width, float height);
	Vector2 GetSceneBounds() const;
	void SetTargetFPS(int fps);
	int GetTargetFPS() const;

    // Hook for MapManager: injects a function that executes a map transition.
    // Maps call RequestGotoMap to trigger transitions safely (no global/static).
    void SetTransitionCallback
    (
        std::function<void(std::string_view, bool)> callback
    );

    void SetExitCallback(std::function<void()> callback);

protected:
    // Helper maps can call to request a transition (executes callback if provided)
    void RequestGotoMap(std::string_view map_id, bool force_reload = false) const;

    // Helper maps can call to request shutdown (executes callback if provided)
    void RequestExit();
};
