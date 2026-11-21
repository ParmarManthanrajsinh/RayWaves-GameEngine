#include "GameEngine.h"
#include "AssetResolver.h"
#include "MapManager.h"
#include "Profiler.h"
#include "WindowUtils.h"
#include <iostream>
#include <utility>

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

bool GameEngine::LaunchWindow(int width, int height, std::string_view title, bool b_EditorWindow)
{
    m_WindowWidth = width;
    m_WindowHeight = height;
    m_WindowTitle = title;
    std::cerr << "Window initialized: " << title << " (" << width << "x" << height << ")\n";

    // The editor creates a hidden, resizable window first so it can advertise
    // a normal X11 window type and sane size bounds before mapping; tiling
    // compositors such as Hyprland otherwise float untyped XWayland windows.
    // Game windows keep their usual behavior.
    unsigned int flags = FLAG_WINDOW_RESIZABLE;
    if (b_EditorWindow)
    {
        flags |= FLAG_WINDOW_HIDDEN;
    }
    SetConfigFlags(flags);

    InitWindow(width, height, title.data());
    if (!IsWindowReady())
    {
        std::cerr << "Failed to create the RayWaves window. Check DISPLAY/X11 and the OpenGL driver.\n";
        return false;
    }

    if (b_EditorWindow)
    {
        // A fixed 1280x720 editor is small on high-DPI or large monitors. Size
        // it while hidden, then let the window manager tile it.
        const int monitor = GetCurrentMonitor();
        const int monitor_width = GetMonitorWidth(monitor);
        const int monitor_height = GetMonitorHeight(monitor);
        if (monitor_width > 0 && monitor_height > 0)
        {
            SetWindowSize(monitor_width * 9 / 10, monitor_height * 9 / 10);
        }

        WindowUtils::SetNormalWindowType();
        ClearWindowState(FLAG_WINDOW_HIDDEN);
    }

    InitAudioDevice();

    // Title-bar/icon tweaks are best effort. A platform without native window
    // decoration support must still be considered running.
    WindowUtils::SetupNativeWindow();
    m_bIsRunning = true;
    return true;
}

bool GameEngine::LaunchWindow(const t_WindowConfig& config)
{
    m_WindowWidth = config.width;
    m_WindowHeight = config.height;
    m_WindowTitle = config.title;

    std::cerr << "Window initialized from config: " << config.title << " (" << config.width << "x" << config.height
              << ") " << (config.b_Fullscreen ? "Fullscreen" : "Windowed") << "\n";

    // Set window flags before initialization
    unsigned int flags = 0;
    if (config.b_Resizable)
        flags |= FLAG_WINDOW_RESIZABLE;
    if (config.b_Vsync)
        flags |= FLAG_VSYNC_HINT;
    if (config.b_Hidden)
        flags |= FLAG_WINDOW_HIDDEN;

    if (flags != 0)
    {
        SetConfigFlags(flags);
    }

    InitWindow(config.width, config.height, config.title.c_str());
    if (!IsWindowReady())
    {
        std::cerr << "Failed to create the game window. Check DISPLAY/X11 and the OpenGL driver.\n";
        return false;
    }

    // Agent runs default to no audio: CI hosts have no device, and the game
    // logic does not need it to produce a screenshot.
    if (!config.b_NoAudio)
    {
        InitAudioDevice();
    }

    WindowUtils::SetupNativeWindow();
    m_bIsRunning = true;

    // Set fullscreen after window creation if needed
    if (config.b_Fullscreen)
    {
        ToggleFullscreen();
    }
    return true;
}

void GameEngine::ToggleFullscreen()
{
    ::ToggleFullscreen();
    if (IsWindowFullscreen())
    {
        std::cerr << "Switched to fullscreen mode\n";
    }
    else
    {
        std::cerr << "Switched to windowed mode\n";
    }
}

void GameEngine::SetWindowMode(bool fullscreen)
{
    bool b_IsCurrentlyFullscreen = IsWindowFullscreen();
    if (fullscreen && !b_IsCurrentlyFullscreen)
    {
        ::ToggleFullscreen();
        std::cerr << "Switched to fullscreen mode\n";
    }
    else if (!fullscreen && b_IsCurrentlyFullscreen)
    {
        ::ToggleFullscreen();
        std::cerr << "Switched to windowed mode\n";
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
        m_GameMap->SetSceneBounds(static_cast<float>(b_w), static_cast<float>(b_h));
        m_GameMap->SetProjectAssetPath(AssetResolver::GetProjectAssetPath());
        m_GameMap->ClearEditorEntities();
        m_GameMap->Initialize();
        SyncSceneWithMap();
    }
    else
    {
        UnloadScene();
    }
}

std::string GameEngine::GetActiveSceneKey() const
{
    if (m_MapManager != nullptr)
    {
        return std::string(m_MapManager->GetCurrentMapId());
    }
    if (m_GameMap != nullptr)
    {
        return m_GameMap->GetMapName();
    }
    return {};
}

Camera2D GameEngine::GetActiveViewCamera() const
{
    if (m_MapManager != nullptr)
    {
        if (const GameMap* current_map = m_MapManager->GetCurrentMap())
        {
            return current_map->GetViewCamera();
        }
    }
    else if (m_GameMap != nullptr)
    {
        return m_GameMap->GetViewCamera();
    }

    return Camera2D{ { 0.0f, 0.0f }, { 0.0f, 0.0f }, 0.0f, 1.0f };
}

bool GameEngine::b_LoadActiveScene()
{
    const std::string key = Scene::ToFileKey(GetActiveSceneKey());
    m_Scene.Clear();
    m_LoadedSceneKey = key;

    if (key.empty() || AssetResolver::GetProjectAssetPath().empty())
    {
        return false;
    }

    return m_Scene.b_LoadFromFile(AssetResolver::Resolve(key + ".scene"));
}

bool GameEngine::b_SaveActiveScene()
{
    if (m_SceneSaveGuard && !m_SceneSaveGuard())
    {
        return false;
    }

    const std::string key = m_LoadedSceneKey.empty() ? Scene::ToFileKey(GetActiveSceneKey()) : m_LoadedSceneKey;
    if (key.empty() || AssetResolver::GetProjectAssetPath().empty())
    {
        return false;
    }

    return m_Scene.b_SaveToFile(AssetResolver::Resolve(key + ".scene"));
}

void GameEngine::UnloadScene()
{
    if (!m_LoadedSceneKey.empty() && m_Scene.b_IsDirty())
    {
        b_SaveActiveScene();
    }
    m_Scene.Clear();
    m_LoadedSceneKey.clear();
}

void GameEngine::SetSceneSaveGuard(SceneSaveGuard guard)
{
    m_SceneSaveGuard = std::move(guard);
}

void GameEngine::SyncSceneWithMap()
{
    const std::string key = Scene::ToFileKey(GetActiveSceneKey());
    if (key == m_LoadedSceneKey)
    {
        return;
    }

    if (!m_LoadedSceneKey.empty() && m_Scene.b_IsDirty())
    {
        b_SaveActiveScene();
    }

    if (key.empty())
    {
        m_Scene.Clear();
        m_LoadedSceneKey.clear();
        return;
    }

    b_LoadActiveScene();
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

    // Load lazily so map transitions requested from gameplay are picked up
    // without changing GameMap or MapManager.
    SyncSceneWithMap();

    // Scene props are world-space, consistent with gameplay entities and
    // editor viewport pan/zoom.
    const Camera2D view_camera = GetActiveViewCamera();
    BeginMode2D(view_camera);
    m_Scene.Draw();
    EndMode2D();
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
        m_GameMap->ClearEditorEntities();
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
        m_MapManager->SetSceneBounds(static_cast<float>(b_w), static_cast<float>(b_h));
        m_MapManager->SetProjectAssetPath(AssetResolver::GetProjectAssetPath());
        m_MapManager->Initialize();
        SyncSceneWithMap();
    }
    else
    {
        UnloadScene();
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
