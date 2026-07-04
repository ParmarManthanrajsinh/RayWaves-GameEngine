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
#include "terminal/terminal.h"
#include "../Engine/GameState.h"
namespace fs = std::filesystem;

enum class EBuildStatus { None, Compiling, Success, Failed };

struct FBuildMessage {
    enum class ESeverity { Info, Warning, Error };
    ESeverity Severity = ESeverity::Info;
    std::string File;
    int Line = 0;
    std::string Text;
};

class GameEditor
{
public:
    GameEditor();
    ~GameEditor();

    bool b_IsPlaying;
    std::atomic<bool> b_IsCompiling;
    void Init(int width, int height, std::string_view title);
    void LoadMap(GameMap* game_map);

    // Load the game logic DLL and create/set a new GameMap from it
    bool b_LoadGameLogic(std::string_view dll_path);

    // Unload and reload the DLL, then recreate the GameMap
    bool b_ReloadGameLogic();

    void RunBrowser();
    void Run();
    
    void CompileGameLogic();
    void OpenProject(std::string_view folderPath);
    void CloseProject();
    void CleanupProject();
    GameEngine& GetGameEngine() { return m_GameEngine; }
    MapManager* GetMapManager() { return m_MapManager; }
    std::shared_ptr<std::atomic<bool>> GetThreadCancelFlag() const { return m_ThreadCancelFlag; }
    term::Terminal& GetTerminal() { return m_Terminal; }
    
    bool IsWindowResized() const { return b_ResolutionChanged; } // Or ImGui function if needed

    std::string version = "RayWaves v0.6.0";

    RenderTexture2D m_RaylibTexture;
    RenderTexture2D m_DisplayTexture;
    Texture2D m_SourceTexture;
    
    bool b_ResolutionChanged = false;
    bool b_FPSChanged = false;
    
    struct m_tSceneSettings
    {
        int m_SceneWidth = 1280;
        int m_SceneHeight = 720;
		int m_TargetFPS = 60;
    } m_SceneSettings;
    
    struct m_tExportState 
    {
        std::atomic<bool> m_bIsExporting{false};
        std::string m_ExportPath = "";
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

    bool m_bShowPerformanceStats = false;
    std::array<float, 120> m_FrameTimes{};
    size_t m_FrameOffset;
    
    std::atomic<EBuildStatus> BuildStatus = EBuildStatus::None;
    std::atomic<float> NotificationTimer = 0.0f;
    std::vector<FBuildMessage> BuildMessages;
    std::mutex BuildMessagesMutex;
    bool bShowMessageLog = false;
    void ParseBuildLine(std::string_view line);
    
    bool m_bShowTerminal = true;
    bool m_bShowSceneSettings = false;
    bool m_bShowExport = false;
    bool m_bShowEditorPreferences = false;
    bool m_bNeedsThemeRebake = false;
    bool m_bNeedsLayoutReset = false;
    bool m_bNeedsIniLoad = false;
    
    bool m_bUseOpaquePass = true;
    bool m_bPreserveStateOnReload = true;
    
    std::string m_SelectedMapId;

private:
    void Close();

    GameEngine m_GameEngine;
    ImGuiViewport* m_Viewport;

    // Hot-reload state
    DllHandle m_GameLogicDll;
    using CreateGameMapFunc = GameMap * (*)();
    using DestroyGameMapFunc = void (*)(GameMap*);
    
    CreateGameMapFunc m_CreateGameMap = nullptr;
    DestroyGameMapFunc m_DestroyGameMap = nullptr;
    
    std::string m_GameLogicPath;
    fs::file_time_type m_LastLogicWriteTime{};
    std::atomic<bool> m_bNeedsReload = false;

    float m_ReloadCheckAccum = 0.0f;
    std::chrono::steady_clock::time_point m_LastReloadCheckTime = std::chrono::steady_clock::now();
    Shader m_OpaqueShader;

    // Map selection UI
    MapManager* m_MapManager = nullptr;

    void UpdatePerformanceMetrics();

    // Terminal
    term::Terminal m_Terminal;

    // UI Panels
    std::vector<std::unique_ptr<class IEditorPanel>> m_Panels;

    bool m_bCloseRequested = false;
    std::shared_ptr<std::atomic<bool>> m_ThreadCancelFlag = std::make_shared<std::atomic<bool>>(false);
};
