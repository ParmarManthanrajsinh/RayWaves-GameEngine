#include <iostream>
#include "GameEngine.h"
#include "MapManager.h"
#include "AssetResolver.h"
#include "Profiler.h"

#define Rectangle WinAPIRectangle
#define CloseWindow WinAPICloseWindow
#define ShowCursor  WinAPIShowCursor
#include <windows.h>
#include <dwmapi.h>
#undef Rectangle
#undef CloseWindow
#undef ShowCursor

#include <shellapi.h>
#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "Dwmapi.lib")

GameEngine::GameEngine()
{
	m_WindowWidth = 1280;
	m_WindowHeight = 720;
	m_WindowTitle = "Game Window";
}
GameEngine::~GameEngine() = default;

void GameEngine::SetViewportSize(int width, int height)
{
	m_ViewportWidth = width;
	m_ViewportHeight = height;
}

int GameEngine::GetViewportWidth() const
{
	return m_ViewportWidth;
}

int GameEngine::GetViewportHeight() const
{
	return m_ViewportHeight;
}

void GameEngine::LaunchWindow(int width, int height, std::string_view title)
{
	m_WindowWidth = width;
	m_WindowHeight = height;
	m_WindowTitle = title;
	std::cout << "Window initialized: " << title << " (" << width << "x" << height << ")\n";

	InitWindow(width, height, title.data());
	InitAudioDevice();

	HWND hwnd = static_cast<HWND>(GetWindowHandle());
	BOOL value = TRUE;

	if (hwnd == nullptr) 
	{
		return;
	}

	// Windows 10 (attribute 19)
	DwmSetWindowAttribute(hwnd, 19, &value, sizeof(value));

	// Windows 11 (attribute 20)
	DwmSetWindowAttribute(hwnd, 20, &value, sizeof(value));

	// Extract and set icon from executable
	char exePath[MAX_PATH];
	GetModuleFileNameA(nullptr, exePath, MAX_PATH);
	HICON hIcon = ExtractIconA(GetModuleHandle(nullptr), exePath, 0);
	if (hIcon != nullptr && hIcon != (HICON)1)
	{
		SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
		SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
	}

	m_bIsRunning = true;
}


void GameEngine::LaunchWindow(const t_WindowConfig& config)
{
	m_WindowWidth = config.width;
	m_WindowHeight = config.height;
	m_WindowTitle = config.title;

	std::cout << "Window initialized from config: " << config.title << " (" << config.width << "x" << config.height << ") " << (config.b_Fullscreen ? "Fullscreen" : "Windowed") << "\n";

	// Set window flags before initialization
	unsigned int flags = 0;
	if (config.b_Resizable) flags |= FLAG_WINDOW_RESIZABLE;
	if (config.b_Vsync) flags |= FLAG_VSYNC_HINT;

	if (flags != 0) 
	{
		SetConfigFlags(flags);
	}

	InitWindow(config.width, config.height, config.title.c_str());
	InitAudioDevice();

	HWND hwnd = static_cast<HWND>(GetWindowHandle());
	if (hwnd != nullptr)
	{
		BOOL value = TRUE;
		DwmSetWindowAttribute(hwnd, 19, &value, sizeof(value));
		DwmSetWindowAttribute(hwnd, 20, &value, sizeof(value));

		char exePath[MAX_PATH];
		GetModuleFileNameA(nullptr, exePath, MAX_PATH);
		HICON hIcon = ExtractIconA(GetModuleHandle(nullptr), exePath, 0);
		if (hIcon != nullptr && hIcon != (HICON)1)
		{
			SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
			SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
		}
	}

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
		std::cout << "Switched to fullscreen mode\n";
	}
	else
	{
		std::cout << "Switched to windowed mode\n";
	}
}

void GameEngine::SetWindowMode(bool fullscreen)
{
	bool b_IsCurrentlyFullscreen = IsWindowFullscreen();
	if (fullscreen && !b_IsCurrentlyFullscreen)
	{
		::ToggleFullscreen();
		std::cout << "Switched to fullscreen mode\n";
	}
	else if (!fullscreen && b_IsCurrentlyFullscreen)
	{
		::ToggleFullscreen();
		std::cout << "Switched to windowed mode\n";
	}
}

void GameEngine::SetMap(GameMap* game_map)
{
	m_GameMap = game_map;
	if (m_GameMap != nullptr)
	{
		m_GameMap->SetSceneBounds
		(
			static_cast<float>(m_WindowWidth), 
			static_cast<float>(m_WindowHeight)
		);
		m_GameMap->SetProjectAssetPath(AssetResolver::GetProjectAssetPath());
		m_GameMap->Initialize();
	}
}

void GameEngine::DrawMap()
{
	SCOPED_TIMER("game_draw");
	// First check if we have a MapManager
	// Otherwise, use the regular GameMap
	if (m_MapManager != nullptr)
	{
		m_MapManager->Draw();
	}
	else if (m_GameMap != nullptr)
	{
		m_GameMap->Draw();
	}
}

void GameEngine::UpdateMap(float dt)
{
	SCOPED_TIMER("game_update");
	if (m_MapManager != nullptr)
	{
		m_MapManager->SetSceneBounds(static_cast<float>(m_ViewportWidth), static_cast<float>(m_ViewportHeight));
		m_MapManager->Update(dt);
	}
	else if (m_GameMap != nullptr)
	{
		m_GameMap->SetSceneBounds(static_cast<float>(m_ViewportWidth), static_cast<float>(m_ViewportHeight));
		m_GameMap->Update(dt);
	}
}

void GameEngine::ResetMap()
{
	if (m_MapManager != nullptr)
	{
		m_MapManager->Initialize();
	}
	else if (m_GameMap != nullptr)
	{
		m_GameMap->Initialize();
	}
}

void GameEngine::SetMapManager(MapManager* map_manager)
{
	m_MapManager = map_manager;
	if (m_MapManager != nullptr)
	{
		m_MapManager->SetSceneBounds
		(
			static_cast<float>(m_WindowWidth), 
			static_cast<float>(m_WindowHeight)
		);
		m_MapManager->SetProjectAssetPath(AssetResolver::GetProjectAssetPath());
		m_MapManager->Initialize();
	}
}

MapManager* GameEngine::GetMapManager()
{
	return m_MapManager;
}

const MapManager* GameEngine::GetMapManager() const
{
	return m_MapManager;
}

bool GameEngine::b_HasMapManager() const
{
	return m_MapManager != nullptr;
}
