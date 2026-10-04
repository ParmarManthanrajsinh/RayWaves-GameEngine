#include <iostream>
#include "GameEngine.h"
#include "MapManager.h"
#include "AssetResolver.h"
#include "Profiler.h"
#include "WindowUtils.h"

GameEngine::GameEngine()
{
	m_WindowWidth = 1280;
	m_WindowHeight = 720;
	m_WindowTitle = "Game Window";
}
GameEngine::~GameEngine() = default;

void GameEngine::SetViewportSize(int width, int height)
{
	if ((width == m_ViewportWidth) && (height == m_ViewportHeight))
	{
		return;
	}
	m_ViewportWidth = width;
	m_ViewportHeight = height;

	// Propagate bounds only when the size actually changed; UpdateMap() used
	// to push the same values every frame.
	if (m_MapManager != nullptr)
	{
		m_MapManager->SetSceneBounds(static_cast<float>(width), static_cast<float>(height));
	}
	else if (m_GameMap != nullptr)
	{
		m_GameMap->SetSceneBounds(static_cast<float>(width), static_cast<float>(height));
	}
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

	if (!WindowUtils::SetupNativeWindow())
	{
		return;
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

	WindowUtils::SetupNativeWindow();

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
		// Prefer the last viewport size when one was set. UpdateMap() no
		// longer pushes bounds every frame, so attach must converge to the
		// same steady state the per-frame push used to enforce.
		const int b_w = (m_ViewportWidth > 0) ? m_ViewportWidth : m_WindowWidth;
		const int b_h = (m_ViewportHeight > 0) ? m_ViewportHeight : m_WindowHeight;
		m_GameMap->SetSceneBounds
		(
			static_cast<float>(b_w),
			static_cast<float>(b_h)
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
		m_MapManager->Update(dt);
	}
	else if (m_GameMap != nullptr)
	{
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
		const int b_w = (m_ViewportWidth > 0) ? m_ViewportWidth : m_WindowWidth;
		const int b_h = (m_ViewportHeight > 0) ? m_ViewportHeight : m_WindowHeight;
		m_MapManager->SetSceneBounds
		(
			static_cast<float>(b_w),
			static_cast<float>(b_h)
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
