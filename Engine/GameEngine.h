#pragma once

#include "GameMap.h"
#include "GameConfig.h"
#include <string>
class MapManager;

class GameEngine
{
    bool m_bIsRunning = false;
    bool b_IsRunning() const { return m_bIsRunning; }

	// The window dimensions
    int m_WindowWidth;
	int m_WindowHeight;
	std::string m_WindowTitle;
	GameMap* m_GameMap = nullptr;
	
	// MapManager instance for advanced map management
	MapManager* m_MapManager = nullptr;
	
public:
    GameEngine();
    ~GameEngine();

	void LaunchWindow(int width, int height, std::string_view title);
	void LaunchWindow(const t_WindowConfig& config);
	void ToggleFullscreen();
	void SetWindowMode(bool fullscreen);
	void SetMap(GameMap* game_map);
	GameMap* GetMap() const { return m_GameMap; }
	void DrawMap() const;
	void UpdateMap(float delta_time) const;
	void ResetMap();
	
	// MapManager integration methods
	void SetMapManager(MapManager* map_manager);
	MapManager* GetMapManager() const;
	bool b_HasMapManager() const;
};
