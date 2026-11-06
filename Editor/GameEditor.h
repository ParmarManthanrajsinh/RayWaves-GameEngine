#pragma once

#include <chrono>
#include <filesystem>
#include <system_error>
#include <imgui.h>
#include <raylib.h>
#include "../Engine/EngineVersion.h"
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

#include "GameLogicLoader.h"
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

    // Load the game logic DLL and create/set a new GameMap from it.
    // Forwards to GameLogicLoader; the exit-request callback is attached
    // via the loader's new-map callback (wired in the constructor).
    bool b_LoadGameLogic(std::string_view dll_path);

    // Unload and reload the DLL, then recreate the GameMap.
    // Pauses playback around the reload; forwards the swap to GameLogicLoader.
    bool b_ReloadGameLogic();

    void RunBrowser();
    void Run();
    
    // Starts a build. If one is already running it is cancelled first, so
    // only one build exists at a time.
    void CompileGameLogic();
    // Requests cancellation of the running build; the worker kills the
    // build's process tree and exits shortly after.
    void CancelBuild();
    void OpenProject(std::string_view folderPath);
    void CloseProject();
    void CleanupProject();
    GameEngine& GetGameEngine() { return m_GameEngine; }
    MapManager* GetMapManager() { return m_LogicLoader.GetMapManager(); }
    GameLogicLoader& GetLogicLoader() { return m_LogicLoader; }
    std::shared_ptr<std::atomic<bool>> GetThreadCancelFlag() const { return m_ThreadCancelFlag; }
    term::Terminal& GetTerminal() { return m_Terminal; }

    std::string version = std::string("RayWaves v") + RAYWAVES_VERSION;

    RenderTexture2D m_RaylibTexture;
    RenderTexture2D m_DisplayTexture;
    Texture2D m_SourceTexture;

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
    std::thread m_BuildThread;
    // Per-build cancel flag. Replaced on every CompileGameLogic(), so a
    // cancelled build cannot cancel its successor.
    std::shared_ptr<std::atomic<bool>> m_BuildCancel = std::make_shared<std::atomic<bool>>(false);
    bool m_bClosed = false;
    std::array<float, 120> m_FrameTimes{};
    size_t m_FrameOffset;
    
    std::atomic<EBuildStatus> BuildStatus = EBuildStatus::None;
    std::atomic<float> NotificationTimer = 0.0f;
    std::vector<FBuildMessage> BuildMessages;
    std::mutex BuildMessagesMutex;
    // Written from the build worker thread on failure, read on the UI
    // thread. Must be atomic to avoid a data race. ImGui wants a bool*,
    // so call sites proxy through a local bool.
    std::atomic<bool> bShowMessageLog{false};
    void ParseBuildLine(std::string_view line);
    
    bool m_bShowTerminal = true;
    bool m_bShowSceneSettings = false;
    bool m_bShowExport = false;
    bool m_bShowEditorPreferences = false;
    bool m_bNeedsThemeRebake = false;
    bool m_bNeedsLayoutReset = false;
    bool m_bNeedsIniLoad = false;
    
    bool m_bUseOpaquePass = true;

    std::string m_SelectedMapId;

private:
    void Close();

    GameEngine m_GameEngine;
    ImGuiViewport* m_Viewport;

    // Hot-reload lifecycle (DLL handles, timestamp watching, state-preserving
    // swaps). Owned here so its lifetime matches the engine's; panels reach
    // it through the forwarding methods above.
    GameLogicLoader m_LogicLoader;

    Shader m_OpaqueShader;

    void UpdatePerformanceMetrics();

    // Terminal
    term::Terminal m_Terminal;

    // UI Panels
    std::vector<std::unique_ptr<class IEditorPanel>> m_Panels;

    bool m_bCloseRequested = false;
    std::shared_ptr<std::atomic<bool>> m_ThreadCancelFlag = std::make_shared<std::atomic<bool>>(false);
};
