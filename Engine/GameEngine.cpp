#include "GameEngine.h"
#include "MapManager.h"

#define CloseWindow WinAPICloseWindow
#define ShowCursor  WinAPIShowCursor
#include <windows.h>
#include <dwmapi.h>
#undef CloseWindow
#undef ShowCursor

#pragma comment(lib, "Dwmapi.lib")

GameEngine::GameEngine()
{
	m_WindowWidth = 1280;
	m_WindowHeight = 720;
	m_WindowTitle = "Game Window";
}
GameEngine::~GameEngine() = default;

void GameEngine::LaunchWindow(int width, int height, std::string_view title)
{
	m_WindowWidth = width;
	m_WindowHeight = height;
	m_WindowTitle = title;
	std::println("Window initialized: {} ({}x{})", title, width, height);

	InitWindow(width, height, title.data());

	HWND hwnd = GetActiveWindow();
	BOOL value = TRUE;

	if (!hwnd) 
	{
		return;
	}

	// Windows 10 (attribute 19)
	DwmSetWindowAttribute(hwnd, 19, &value, sizeof(value));

	// Windows 11 (attribute 20)
	DwmSetWindowAttribute(hwnd, 20, &value, sizeof(value));
}


void GameEngine::LaunchWindow(const t_WindowConfig& config)
{
	m_WindowWidth = config.width;
	m_WindowHeight = config.height;
	m_WindowTitle = config.title;

	std::println
	(
		"Window initialized from config: {} ({}x{}) {}",
		config.title,
		config.width,
		config.height,
		config.b_Fullscreen ? "Fullscreen" : "Windowed"
	);

	// Set window flags before initialization
	unsigned int flags = 0;
	if (config.b_Resizable) flags |= FLAG_WINDOW_RESIZABLE;
	if (config.b_Vsync) flags |= FLAG_VSYNC_HINT;

	if (flags != 0) 
	{
		SetConfigFlags(flags);
	}

	InitWindow(config.width, config.height, config.title.c_str());

	// Set fullscreen after window creation if needed
	if (config.b_Fullscreen)
	{
		ToggleFullscreen();
	}
}

void GameEngine::ToggleFullscreen()
{
	::ToggleFullscreen();
	if (IsWindowFullscreen())
	{
		std::println("Switched to fullscreen mode");
	}
	else
	{
		std::println("Switched to windowed mode");
	}
}

void GameEngine::SetWindowMode(bool fullscreen)
{
	bool b_IsCurrentlyFullscreen = IsWindowFullscreen();
	if (fullscreen && !b_IsCurrentlyFullscreen)
	{
		::ToggleFullscreen();
		std::println("Switched to fullscreen mode");
	}
	else if (!fullscreen && b_IsCurrentlyFullscreen)
	{
		::ToggleFullscreen();
		std::println("Switched to windowed mode");
	}
}

void GameEngine::SetMap(std::unique_ptr<GameMap> game_map)
{
	m_GameMap = std::move(game_map);
	if (m_GameMap)
	{
		m_GameMap->SetSceneBounds
		(
			static_cast<float>(m_WindowWidth), 
			static_cast<float>(m_WindowHeight)
		);
		m_GameMap->Initialize();
	}
}

void GameEngine::DrawMap() const
{
	// First check if we have a MapManager
	// Otherwise, use the regular GameMap
	if (m_MapManager)
	{
		m_MapManager->Draw();
	}
	else if (m_GameMap)
	{
		m_GameMap->Draw();
	}
}

void GameEngine::UpdateMap(float dt) const
{
	if (m_MapManager)
	{
		m_MapManager->Update(dt);
	}
	else if (m_GameMap)
	{
		m_GameMap->Update(dt);
	}
}

void GameEngine::ResetMap()
{
    if (m_GameMap)
    {
        m_GameMap->Initialize();
    }
}

void GameEngine::SetMapManager(std::unique_ptr<MapManager> map_manager)
{
	m_MapManager = std::move(map_manager);
	if (m_MapManager)
	{
		m_MapManager->SetSceneBounds
		(
			static_cast<float>(m_WindowWidth), 
			static_cast<float>(m_WindowHeight)
		);
		m_MapManager->Initialize();
	}
}

MapManager* GameEngine::GetMapManager() const
{
	return m_MapManager.get();
}

bool GameEngine::b_HasMapManager() const
{
	return m_MapManager != nullptr;
}