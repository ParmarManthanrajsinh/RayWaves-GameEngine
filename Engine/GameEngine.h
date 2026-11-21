#pragma once

#include "GameConfig.h"
#include "GameMap.h"
#include "Scene.h"
#include <functional>
#include <string>
class MapManager;

class GameEngine
{
public:
    using SceneSaveGuard = std::function<bool()>;

private:
    bool m_bIsRunning = false;
    bool b_IsRunning() const { return m_bIsRunning; }

    // The window dimensions
    int m_WindowWidth;
    int m_WindowHeight;
    std::string m_WindowTitle;
    GameMap* m_GameMap = nullptr;

    // MapManager instance for advanced map management
    MapManager* m_MapManager = nullptr;
    // Viewport size for camera logic
    int m_ViewportWidth = 0;
    int m_ViewportHeight = 0;

    // Editor-authored sprite layer. Drawn after the map; game code never
    // needs to know it exists.
    Scene m_Scene;
    std::string m_LoadedSceneKey;
    SceneSaveGuard m_SceneSaveGuard;
    // Map that currently owns a copy of the scene snapshot. Host-owned; never
    // dereferenced after the map is unloaded.
    GameMap* m_SceneMap = nullptr;
    void SyncSceneWithMap();
    GameMap* GetActiveMap();
    void ClearSceneFromMap();
    void PushSceneToMap();

public:
    GameEngine();
    ~GameEngine();

    void SetViewportSize(int width, int height);
    int GetViewportWidth() const;
    int GetViewportHeight() const;

    bool LaunchWindow(int width, int height, std::string_view title, bool b_EditorWindow = false);
    bool LaunchWindow(const t_WindowConfig& config);
    static void ToggleFullscreen();
    static void SetWindowMode(bool fullscreen);
    void SetMap(GameMap* game_map);
    GameMap* GetMap() { return m_GameMap; }
    const GameMap* GetMap() const { return m_GameMap; }
    void DrawMap();
    void UpdateMap(float delta_time);
    void ResetMap();

    // Editor sprite layer. The key is the active map id (MapManager) or map
    // name (plain GameMap); scene data lives in Assets/<key>.scene.
    Scene& GetScene() { return m_Scene; }
    const Scene& GetScene() const { return m_Scene; }
    Camera2D GetActiveViewCamera() const;
    std::string GetActiveSceneKey() const;
    bool b_SaveActiveScene();

    // Optional host guard. Returning false skips writing the scene file, which
    // lets the editor avoid overwriting a scene edited by another process.
    void SetSceneSaveGuard(SceneSaveGuard guard);
    bool b_LoadActiveScene();
    void UnloadScene();

    // Re-push the current scene snapshot to the active map. The editor calls
    // this when playback starts so edits made while stopped reach gameplay.
    void RefreshSceneObjects();

    // MapManager integration methods
    void SetMapManager(MapManager* map_manager);
    MapManager* GetMapManager();
    const MapManager* GetMapManager() const;
    bool b_HasMapManager() const;
};
