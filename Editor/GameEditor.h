#pragma once

#include <chrono>
#include <filesystem>
#include <system_error>
#include <imgui.h>
#include <raylib.h>
#include <rlImGui.h>
#include <thread>
#include <tinyfiledialogs.h>
#include <atomic>
#include <mutex>
#include <vector>
#include <array>
#include <cstdio>
#include <sstream>
#include <fstream>
#include <string>
#include <print>

#include "DllLoader.h"
#include "GameEditorLayout.h"
#include "GameEditorTheme.h"
#include "GameEngine.h"
namespace fs = std::filesystem;

class GameEditor
{
public:
    GameEditor();
    ~GameEditor();

    bool b_IsPlaying;
    std::atomic<bool> b_IsCompiling;
    void Init(int width, int height, std::string_view title);
    void LoadMap(std::unique_ptr<GameMap>& game_map);

    // Load the game logic DLL and create/set a new GameMap from it
    bool b_LoadGameLogic(std::string_view dll_path);

    // Unload and reload the DLL, then recreate the GameMap
    bool b_ReloadGameLogic();

    void Run();
private:
    void Close() const;

    GameEngine m_GameEngine;
    ImGuiViewport* m_Viewport;

    RenderTexture2D m_RaylibTexture;
    RenderTexture2D m_DisplayTexture;
    Texture2D m_SourceTexture;

    void DrawSceneWindow();
    void DrawMapSelectionUI();
    void DrawExportPanel();
    void DrawSceneSettingsPanel();
    void DrawToolbarBackground();

    // Hot-reload state
    DllHandle m_GameLogicDll;
    using CreateGameMapFunc = GameMap * (*)();
    CreateGameMapFunc m_CreateGameMap = nullptr;

    std::string m_GameLogicPath;
    fs::file_time_type m_LastLogicWriteTime{};

    float m_ReloadCheckAccum = 0.0f;
    Shader m_OpaqueShader;
    bool m_bUseOpaquePass = true;

    // Map selection UI
    MapManager* m_MapManager = nullptr;
    std::string m_SelectedMapId;

    bool b_ResolutionChanged = false;
    bool b_FPSChanged = false;
    
    struct m_tSceneSettings
    {
        int m_SceneWidth = 1280;
        int m_SceneHeight = 720;
		int m_TargetFPS = 60;
    } m_SceneSettings;
    
    // Export UI state
    struct m_tExportState 
    {
        std::atomic<bool> m_bIsExporting{false};
        std::atomic<bool> m_bCancelExport{false};
        std::string m_ExportPath = "Export";
        std::string m_GameName = "MyGame";

        // Window configuration options
        int m_WindowWidth = 1280;
        int m_WindowHeight = 720;
        int m_TargetFPS = 60;
        bool m_bFullscreen = false;
        bool m_bResizable = true;
        bool m_bVSync = true;

        std::vector<std::string> m_ExportLogs;
        std::mutex m_ExportLogMutex;
        std::thread m_ExportThread;
        bool m_bExportSuccess = false;
    } m_ExportState;

    // Performance Overlay
    bool m_bShowPerformanceStats = false;
    std::array<float, 120> m_FrameTimes{};
    size_t m_FrameOffset;

    void DrawPerformanceOverlay();
    void UpdatePerformanceMetrics();
};