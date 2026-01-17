#pragma once
#include <iostream>
#include <print>
#include <raylib.h>
#include <string>
#include <string_view>
#include <functional>

class GameMap
{
protected:
    std::string m_MapName;
    float m_SceneWidth = 0.0f;   
    float m_SceneHeight = 0.0f;  
	int m_TargetFPS = 60;

    // Transition callback to request a map change via the manager
    std::function<void(std::string_view, bool)> m_TransitionCallback;

public:
    GameMap(); 
    GameMap(const std::string& map_name);
    virtual ~GameMap() = default;  

    virtual void Initialize();
    virtual void Update(float delta_time);
    virtual void Draw();
    
    void SetMapName(const std::string& map_name);
    std::string GetMapName() const;
    void SetSceneBounds(float width, float height);
	Vector2 GetSceneBounds() const;
	void SetTargetFPS(int fps);
	int GetTargetFPS() const;

    // Hook for MapManager: injects a function that executes a map transition.
    // Maps call RequestGotoMap to trigger transitions safely (no global/static).
    void SetTransitionCallback
    (
        std::function<void(std::string_view, bool)> cb
    );

protected:
    // Helper maps can call to request a transition (executes callback if provided)
    void RequestGotoMap(std::string_view map_id, bool force_reload = false);
};