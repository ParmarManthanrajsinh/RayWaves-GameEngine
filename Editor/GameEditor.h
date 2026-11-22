#pragma once

#include "../Engine/EngineVersion.h"
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <imgui.h>
#include <mutex>
#include <raylib.h>
#include <rlImGui.h>
#include <sstream>
#include <string>
#include <system_error>
#include <thread>
#include <tinyfiledialogs.h>
#include <vector>

#include "../Engine/GameState.h"
#include "CommandStack.h"
#include "EditorCamera3D.h"
#include "GameEditorLayout.h"
#include "GameEditorTheme.h"
#include "GameEngine.h"
#include "GameLogicLoader.h"
#include "LogBuffer.h"
#include "terminal/terminal.h"
namespace fs = std::filesystem;

enum class EBuildStatus
{
    None,
    Compiling,
    Success,
    Failed
};

struct FBuildMessage
{
    enum class ESeverity
    {
        Info,
        Warning,
        Error
    };
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

    // Playback state. b_IsPlaying is kept equal to "not stopped" because panels
    // outside this class only know that flag and use it to disable editing.
    enum class EPlayState
    {
        Stopped,
        Playing,
        Paused
    };

    EPlayState GetPlayState() const { return m_PlayState; }
    bool b_IsPaused() const { return m_PlayState == EPlayState::Paused; }
    void SetPlayState(EPlayState state);
    // Requests a single fixed-size frame. From Stopped this enters Paused first
    // so the gameplay camera is preserved like any other playback session.
    void StepOnce();

    std::atomic<bool> b_IsCompiling;
    bool Init(int width, int height, std::string_view title);
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
    bool OpenProject(std::string_view folderPath);
    void CloseProject();
    void CleanupProject();

    // Replaces the live scene with the project's autosave snapshot. Fails when
    // the snapshot belongs to a different map so recovery never crosses maps.
    bool b_RestoreAutosave();
    GameEngine& GetGameEngine() { return m_GameEngine; }
    MapManager* GetMapManager() { return m_LogicLoader.GetMapManager(); }
    GameLogicLoader& GetLogicLoader() { return m_LogicLoader; }
    std::shared_ptr<std::atomic<bool>> GetThreadCancelFlag() const { return m_ThreadCancelFlag; }
    term::Terminal& GetTerminal() { return m_Terminal; }
    LogBuffer& GetLogBuffer() { return m_LogBuffer; }
    const LogBuffer& GetLogBuffer() const { return m_LogBuffer; }

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
        std::atomic<bool> m_bIsExporting{ false };
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
        std::atomic<bool> m_bExportSuccess{ false };
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
    std::atomic<bool> bShowMessageLog{ false };
    void ParseBuildLine(std::string_view line);

    bool m_bShowTerminal = true;
    bool m_bShowLog = false;
    LogBuffer m_LogBuffer;
    bool m_bShowSceneSettings = false;
    bool m_bShowExport = false;
    bool m_bShowEditorPreferences = false;
    bool m_bShowFileSystem = true;
    bool m_bShowHierarchy = true;
    bool m_bShowInspector = true;
    bool m_bNeedsThemeRebake = false;
    bool m_bNeedsLayoutReset = false;
    bool m_bNeedsIniLoad = false;

    // Selected editor object. Panels share this instead of owning selection.
    enum class EEditorSelectionKind
    {
        None,
        SceneObject,
        GameplayEntity
    };

    struct FEditorEntityRef
    {
        uint64_t m_Id = 0;
        std::string m_Name;
        std::string m_Type;
        Vector2* m_Position = nullptr;
        Vector2* m_Size = nullptr;
        Vector2 m_Origin{ 0.0f, 0.0f };
        float* m_Rotation = nullptr;
        bool* m_Visible = nullptr;
        Vector3* m_Position3D = nullptr;
        Vector3* m_Size3D = nullptr;
        Vector3 m_Origin3D{ 0.0f, 0.0f, 0.0f };
        t_EntityProperty* m_Properties = nullptr;
        uint32_t m_PropertyCount = 0;
    };

    // Primary selection: what the Inspector shows. The id vectors hold the full
    // selection set; the primary id is always one of them.
    EEditorSelectionKind m_SelectionKind = EEditorSelectionKind::None;
    uint32_t m_SelectedSceneObjectId = 0;
    uint64_t m_SelectedEditorEntityId = 0;
    std::vector<uint32_t> m_SelectedSceneObjectIds;
    std::vector<uint64_t> m_SelectedEditorEntityIds;

    CommandStack m_CommandStack;

    std::vector<FEditorEntityRef> m_EditorEntities;
    GameMap* m_LastActiveMap = nullptr;

    Camera2D m_SavedViewCamera{};
    bool m_bHasSavedViewCamera = false;
    bool m_bWasPlaying = false;

    // Editor-owned 3D view. Separate from the gameplay camera so orbiting the
    // scene never changes what the game sees.
    EditorCamera3D m_EditorCamera3D;
    std::string m_EditorCameraMapKey;
    bool m_bEditorCamera3DReady = false;

    Scene& GetScene() { return m_GameEngine.GetScene(); }

    GameMap* GetActiveMap();
    Camera2D GetActiveViewCamera();
    Camera2D* GetMutableActiveViewCamera();
    Camera3D GetActiveViewCamera3D();
    bool b_ActiveMapUses3DView();
    Vector2 GetViewportCenterWorld();
    void UpdatePlayStateCamera();
    void RefreshEditorEntities();

    EditorCamera3D& GetEditorCamera3D() { return m_EditorCamera3D; }
    const EditorCamera3D& GetEditorCamera3D() const { return m_EditorCamera3D; }

    // Rebuilds the editor camera when the active map changes, and marks the 3D
    // override for GameMap::GetRenderCamera3D().
    void SyncEditorCamera3D();
    void ApplyEditorViewCamera3D();

    // Drops cached gameplay-entity pointers. Call before/after destroying or
    // reinitializing a map: GameMap::ClearEditorEntities() frees the storage
    // those pointers reference, and panels must not touch them afterwards.
    void InvalidateEditorEntities();

    std::vector<FEditorEntityRef>& GetEditorEntities() { return m_EditorEntities; }
    const std::vector<FEditorEntityRef>& GetEditorEntities() const { return m_EditorEntities; }

    FEditorEntityRef* GetSelectedEditorEntity()
    {
        if (m_SelectionKind != EEditorSelectionKind::GameplayEntity)
        {
            return nullptr;
        }
        for (FEditorEntityRef& entity : m_EditorEntities)
        {
            if (entity.m_Id == m_SelectedEditorEntityId)
            {
                return &entity;
            }
        }
        return nullptr;
    }

    t_SceneObject* GetSelectedSceneObject()
    {
        if (m_SelectionKind != EEditorSelectionKind::SceneObject)
        {
            return nullptr;
        }
        return m_GameEngine.GetScene().FindObject(m_SelectedSceneObjectId);
    }

    CommandStack& GetCommandStack() { return m_CommandStack; }
    const CommandStack& GetCommandStack() const { return m_CommandStack; }

    bool b_IsSceneObjectSelected(uint32_t id) const;
    bool b_IsEditorEntitySelected(uint64_t id) const;

    const std::vector<uint32_t>& GetSelectedSceneObjectIds() const { return m_SelectedSceneObjectIds; }
    const std::vector<uint64_t>& GetSelectedEditorEntityIds() const { return m_SelectedEditorEntityIds; }

    // Number of objects in the current selection set, regardless of kind.
    std::size_t GetSelectionCount() const;

    void SelectSceneObject(uint32_t id);
    void ToggleSceneObjectSelection(uint32_t id);
    void AddSceneObjectToSelection(uint32_t id);
    void SelectSceneObjects(const std::vector<uint32_t>& ids);
    void RemoveSceneObjectFromSelection(uint32_t id);

    void SelectEditorEntity(uint64_t id);
    void ToggleEditorEntitySelection(uint64_t id);
    void AddEditorEntityToSelection(uint64_t id);
    void SelectEditorEntities(const std::vector<uint64_t>& ids);
    void RemoveEditorEntityFromSelection(uint64_t id);

    void ClearSelection();

    bool m_bUseOpaquePass = true;

    std::string m_SelectedMapId;

private:
    void Close();

    // Sends a session failure to both the terminal and the message log so it
    // stays visible after the terminal scrolls.
    void LogSessionError(std::string_view message);

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

    EPlayState m_PlayState = EPlayState::Stopped;
    bool m_bStepRequested = false;
    // Returns true once per request so a step never leaks into the next frame.
    bool b_ConsumeStepRequest();

    // Frame-time accumulator for periodic autosave. Editor-only, UI thread.
    float m_AutoSaveElapsedSeconds = 0.0f;
};
