#include "GameEditor.h"
#include "../Engine/AssetResolver.h"
#include "../Engine/BuildDiagnostics.h"
#include "../Engine/MapManager.h"
#include "../Engine/PlatformInfo.h"
#include "../Engine/PlatformNames.h"
#include "../Engine/Profiler.h"
#include "../Engine/ProjectManager.h"
#include "AgentLock.h"
#include "Autosave.h"
#include "DllLoader.h"
#include "EditorUtils.h"
#include "ProcessRunner.h"
#include "ThemeService.h"
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <imgui/imgui_stdlib.h>
#include <imgui_internal.h>
#include <iostream>
#include <iterator>
using Clock = std::chrono::steady_clock;

#include "EditorPreferences.h"
#include "PanelRegistry.h"
#include "Panels/EditorPreferencesPanel.h"
#include "Panels/ExportPanel.h"
#include "Panels/FileSystemPanel.h"
#include "Panels/InspectorPanel.h"
#include "Panels/MainMenuBar.h"
#include "Panels/MessageLogPanel.h"
#include "Panels/PerformanceOverlay.h"
#include "Panels/SceneHierarchyPanel.h"
#include "Panels/SceneSettingsPanel.h"
#include "Panels/SceneWindow.h"
#include "Panels/TerminalPanel.h"
#include <cstdlib>
#include <memory>
#include <thread>
#include <vector>

static std::string s_LayoutPath;

bool g_bNeedsTextureRecreate = false;

namespace
{
// One-line summaries for the New Project wizard. Keep in sync with the
// template READMEs; unknown folders simply show no description.
const char* s_TemplateDescription(std::string_view name)
{
    if (name == "Empty2D")
    {
        return "Blank multi-level starter: rw::Game, one empty level, no assets.";
    }
    if (name == "Simple2D")
    {
        return "Recommended start: rw::Level, one sprite, follow camera.";
    }
    if (name == "Platformer2D")
    {
        return "2D platformer: movement, jumping, collisions, follow camera.";
    }
    if (name == "ThirdPerson3D")
    {
        return "3D starter: orbit camera, capsule character, box collision.";
    }
    if (name == "SlimeQuest")
    {
        return "Full demo: multi-map, enemies, audio, parallax, hot reload.";
    }
    return "";
}

// Core panels in draw/dock order. Built fresh per call so repeated
// GameEditor construction (and tests) never double-register.
std::vector<FPanelFactory> s_CorePanelFactories()
{
    return {
        &s_fMakePanel<MainMenuBar>,
        &s_fMakePanel<FileSystemPanel>,
        &s_fMakePanel<SceneHierarchyPanel>,
        &s_fMakePanel<InspectorPanel>,
        &s_fMakePanel<ExportPanel>,
        &s_fMakePanel<SceneSettingsPanel>,
        &s_fMakePanel<SceneWindow>,
        &s_fMakePanel<PerformanceOverlay>,
        &s_fMakePanel<MessageLogPanel>,
        &s_fMakePanel<EditorPreferencesPanel>,
        &s_fMakePanel<TerminalPanel>,
    };
}
} // namespace

GameEditor::GameEditor()
    : m_Viewport(nullptr)
    , m_RaylibTexture({ 0 })
    , m_DisplayTexture({ 0 })
    , m_SourceTexture({ 0, 0 })
    , b_IsPlaying(false)
    , b_IsCompiling(false)
    , m_LogicLoader(m_GameEngine)
    ,

    m_OpaqueShader({ 0 })
    ,

    m_FrameOffset(0)
{
    m_Terminal.InitCapture();

    // A scene write from rwcli must win over a stale in-memory editor copy.
    // Skip the save instead of overwriting the agent's file.
    m_GameEngine.SetSceneSaveGuard(
        []()
        {
            if (!ProjectManager::b_HasOpenProject())
            {
                return true;
            }
            AgentLock::FInfo lock_info;
            return !AgentLock::b_Query(ProjectManager::GetCurrent().m_RootPath, lock_info);
        });

    if (ProjectManager::b_HasOpenProject())
    {
        m_Terminal.SetWorkingDirectory(ProjectManager::GetCurrent().m_RootPath);
    }

    // Route loader diagnostics into the editor terminal (stderr is invisible
    // in the GUI app) and wire DLL exit requests to window close.
    m_LogicLoader.SetLogSink([this](std::string_view message, bool is_error)
        { m_Terminal.add_text(message, is_error ? term::Severity::Error : term::Severity::Debug); });
    m_LogicLoader.SetNewMapCallback(
        [this](GameMap* new_map)
        {
            if (new_map != nullptr)
            {
                new_map->SetExitCallback([this]() { m_bCloseRequested = true; });
            }
        });

    m_Panels.reserve(11);
    for (FPanelFactory factory : s_CorePanelFactories())
    {
        m_Panels.push_back(factory());
    }
    for (FPanelFactory factory : s_ExtensionPanels())
    {
        m_Panels.push_back(factory());
    }
}

GameEditor::~GameEditor()
{
    // Close is idempotent and owns the correct resource teardown order.
    if (!m_bClosed)
    {
        Close();
    }
}

bool GameEditor::Init(int width, int height, std::string_view title)
{
    if (!m_GameEngine.LaunchWindow(width, height, title.data(), true))
    {
        return false;
    }

    // Set window icon
    Image icon = LoadImage(ThemeService::GetEngineContentPath("icon.png").c_str());
    if (icon.data != nullptr)
    {
        SetWindowIcon(icon);
        UnloadImage(icon);
        std::cout << "Window icon loaded successfully from Assets / icon.png\n";
    }
    else
    {
        std::cout << "Failed to load icon from Assets/icon.png\n";
    }

    rlImGuiSetup(true);

    // Load Editor Preferences
    EditorPreferences::GetInstance().m_bLoadFromFile();
    ThemeService::RebakeNow();

    // Layout persistence
    std::filesystem::path dir = std::filesystem::path(EditorPreferences::GetInstance().GetConfigPath()).parent_path();
    s_LayoutPath = (dir / "editor_layout.ini").string();

    if (ProjectManager::b_HasOpenProject())
    {
        std::filesystem::path proj_dir = ProjectManager::GetCurrent().m_RootPath;
        s_LayoutPath = (proj_dir / ".raywaves" / "layout.ini").string();
    }

    if (std::filesystem::exists(s_LayoutPath))
    {
        ImGui::GetIO().IniFilename = s_LayoutPath.c_str();
        ImGui::LoadIniSettingsFromDisk(s_LayoutPath.c_str());
    }
    else
    {
        LoadEditorDefaultIni();
        ImGui::GetIO().IniFilename = s_LayoutPath.c_str();
    }

    // Prefer the config shipped next to the editor. Launching from a desktop
    // shortcut or another working directory must not silently fall back to
    // defaults just because CWD has no config.ini.
    const fs::path exe_config = PlatformInfo::GetExecutablePath().parent_path() / "config.ini";
    const bool b_config_loaded = GameConfig::GetInstance().m_bLoadFromFile(exe_config.string()) ||
        GameConfig::GetInstance().m_bLoadFromFile("config.ini");
    if (b_config_loaded)
    {
        const auto& CONFIG = GameConfig::GetInstance().GetWindowConfig();
        m_SceneSettings.m_SceneWidth = CONFIG.scene_width;
        m_SceneSettings.m_SceneHeight = CONFIG.scene_height;
        m_SceneSettings.m_TargetFPS = CONFIG.scene_fps;
    }

    if (ProjectManager::b_HasOpenProject())
    {
        const auto& prj = ProjectManager::GetCurrent();
        m_SceneSettings.m_SceneWidth = prj.m_SceneWidth;
        m_SceneSettings.m_SceneHeight = prj.m_SceneHeight;
        m_SceneSettings.m_TargetFPS = prj.m_TargetFPS;
    }

    SetTargetFPS(m_SceneSettings.m_TargetFPS);

    m_Viewport = ImGui::GetMainViewport();

    m_RaylibTexture = LoadRenderTexture(m_SceneSettings.m_SceneWidth, m_SceneSettings.m_SceneHeight);
    m_DisplayTexture = LoadRenderTexture(m_SceneSettings.m_SceneWidth, m_SceneSettings.m_SceneHeight);

    SetTextureFilter(m_RaylibTexture.texture, TEXTURE_FILTER_BILINEAR);
    SetTextureFilter(m_DisplayTexture.texture, TEXTURE_FILTER_BILINEAR);

    m_OpaqueShader = LoadOpaqueShader();

    if (!rw::platform::b_IsWindows && PlatformInfo::FindExecutable("zenity").empty() &&
        PlatformInfo::FindExecutable("kdialog").empty())
    {
        m_Terminal.add_text("WARNING: install zenity or kdialog to enable native file dialogs.", term::Severity::Warn);
    }
    return true;
}

void GameEditor::RunBrowser()
{
    Texture2D logo = LoadTexture(ThemeService::GetEngineContentPath("icon.png").c_str());

    char newProjectName[128] = "MyNewGame";
    char newProjectLocation[512] = "";
    int selectedTemplateIdx = 0;
    std::vector<std::string> templates;
    std::string browser_error;

    while (!WindowShouldClose())
    {
        if (ProjectManager::b_HasOpenProject())
        {
            break;
        }

        BeginDrawing();
        ClearBackground(Color{ 21, 24, 30, 255 });

        rlImGuiBegin();

        ImGuiViewport* viewport = ImGui::GetMainViewport();
        float margin = 48.0f;
        ImGui::SetNextWindowPos(ImVec2(margin, margin), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(viewport->Size.x - (margin * 2.0f), viewport->Size.y - (margin * 2.0f)));
        ImGui::SetNextWindowViewport(viewport->ID);

        ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
            ImGuiWindowFlags_NoNavFocus;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(40.0f, 40.0f));
        ImGui::Begin("Project Browser", nullptr, window_flags);

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImGuiIO& io = ImGui::GetIO();
        float contentLeft = ImGui::GetCursorScreenPos().x;
        float contentWidth = ImGui::GetContentRegionAvail().x;

        // ── Header ─────────────────────────────────────────────────────
        if (logo.id != 0)
        {
            float logoY = ImGui::GetCursorPosY() + 6.0f;
            ImGui::SetCursorPosY(logoY);
            auto lw = static_cast<float>(logo.width);
            auto lh = static_cast<float>(logo.height);
            float maxDim = 96.0f;
            float scale = (lw > lh) ? maxDim / lw : maxDim / lh;
            float displayH = lh * scale;
            rlImGuiImageSize(&logo, static_cast<int>(lw * scale), static_cast<int>(displayH));
            ImGui::SameLine();
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 12.0f);
            float textGroupH =
                (ImGui::GetTextLineHeight() * 1.5f) + ImGui::GetStyle().ItemSpacing.y + ImGui::GetTextLineHeight();
            ImGui::SetCursorPosY(logoY + ((displayH - textGroupH) * 0.5f));
        }

        // Title and version stacked next to logo
        ImGui::BeginGroup();
        ImGui::SetWindowFontScale(1.5f);
        ImGui::Text("RayWaves");
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
        ImGui::Text("Version %s", version.c_str());
        ImGui::PopStyleColor();
        ImGui::EndGroup();

        // Subtle separator line under header
        float lineY = ImGui::GetCursorScreenPos().y + 12.0f;
        drawList->AddRectFilled(ImVec2(contentLeft, lineY), ImVec2(contentLeft + contentWidth, lineY + 1.0f),
            ImGui::GetColorU32(ImGuiCol_Separator));
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 24.0f);

        // ── Two columns ────────────────────────────────────────────────
        ImGui::Columns(2, "BrowserColumns", false);
        ImGui::SetColumnWidth(0, contentWidth * 0.6f);

        // ── Left Column — Recent Projects ──────────────────────────────
        ImGui::PushFont(io.Fonts->Fonts[Font_Large]);
        ImGui::Text("Recent Projects");
        ImGui::PopFont();
        ImGui::Spacing();

        auto recent = ProjectManager::GetRecent();
        ImGui::BeginChild("RecentProjects", ImVec2(0, 0), 1);

        if (recent.empty())
        {
            float childH = ImGui::GetContentRegionAvail().y;
            float childW = ImGui::GetContentRegionAvail().x;
            ImGui::SetCursorPos(ImVec2(childW * 0.1f, childH * 0.35f));
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
            ImGui::TextWrapped("No recent projects - create or open one to get started");
            ImGui::PopStyleColor();
        }
        else
        {
            for (size_t i = 0; i < recent.size(); ++i)
            {
                const auto& path = recent[i];
                ImGui::PushID(static_cast<int>(i));

                std::filesystem::path fs_path(path);
                std::filesystem::path manifest_path = fs_path / "project.raywaves";
                bool exists = std::filesystem::exists(manifest_path);
                std::string displayName = fs_path.filename().string();

                if (exists)
                {
                    t_Project proj;
                    if (proj.m_bLoadFromFile(manifest_path.string()))
                    {
                        displayName = proj.m_Name;
                    }
                }

                float rowHeight = 48.0f;
                float availW = ImGui::GetContentRegionAvail().x;
                ImVec2 rowPos = ImGui::GetCursorScreenPos();

                // Leave a gutter for the trash button. Drawing the button
                // overlapping the Selectable race: it unmounts/mounts under
                // the mouse and can never receive a click.
                float btnW = 36.0f;
                float rowW = availW - btnW - 8.0f;

                if (!exists)
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
                    displayName += " (missing)";
                }

                // Clickable row
                if (ImGui::Selectable(
                        "##recent_proj", false, exists ? 0 : ImGuiSelectableFlags_Disabled, ImVec2(rowW, rowHeight)))
                {
                    if (exists)
                    {
                        browser_error.clear();
                        if (!OpenProject(path))
                        {
                            browser_error = "Could not open project: " + path + " (see Terminal for details).";
                        }
                    }
                }

                // Folder icon
                ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
                ImVec2 iconPos(rowPos.x + 12.0f, rowPos.y + ((rowHeight - 12.0f) * 0.5f));
                ImGui::SetCursorScreenPos(iconPos);
                ImGui::Text(ICON_FA_FOLDER);
                ImGui::PopStyleColor();

                // Project name + path, clipped to the row so long strings
                // never render underneath the trash button.
                ImGui::PushClipRect(ImVec2(rowPos.x, rowPos.y), ImVec2(rowPos.x + rowW, rowPos.y + rowHeight), true);
                ImVec2 namePos(rowPos.x + 40.0f, rowPos.y + 5.0f);
                ImGui::SetCursorScreenPos(namePos);
                ImGui::Text("%s", displayName.c_str());

                ImVec2 pathPos(rowPos.x + 40.0f, rowPos.y + 25.0f);
                ImGui::SetCursorScreenPos(pathPos);
                ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
                ImGui::Text("%s", path.c_str());
                ImGui::PopStyleColor();
                ImGui::PopClipRect();

                if (!exists)
                    ImGui::PopStyleColor();

                // Trash button: always mounted next to the row, never
                // overlapping it, so the mouse can always hover/click it.
                ImGui::SetCursorScreenPos(ImVec2(rowPos.x + availW - btnW, rowPos.y + ((rowHeight - 24.0f) * 0.5f)));
                if (ImGui::Button(ICON_FA_TRASH_CAN "##del"))
                {
                    ImGui::OpenPopup("RemoveRecentPopup");
                }

                if (ImGui::BeginPopup("RemoveRecentPopup"))
                {
                    ImGui::Text("Remove from list?");
                    if (ImGui::Button("Yes", ImVec2(60, 0)))
                    {
                        ProjectManager::RemoveRecent(path);
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("No", ImVec2(60, 0)))
                    {
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::EndPopup();
                }

                ImGui::SetCursorScreenPos(ImVec2(rowPos.x, rowPos.y + rowHeight));
                ImGui::PopID();
            }
        }
        ImGui::EndChild();

        ImGui::NextColumn();

        // ── Right Column — Actions ─────────────────────────────────────
        ImGui::PushFont(io.Fonts->Fonts[Font_Large]);
        ImGui::Text("Actions");
        ImGui::PopFont();
        ImGui::Spacing();

        // New Project — primary button
        ImGui::PushStyleColor(ImGuiCol_Button, GetAccentColor());
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, GetAccentHoverColor());
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, GetAccentActiveColor());
        if (ImGui::Button(ICON_FA_PLUS "  New Project", ImVec2(-1, 44.0f)))
        {
            browser_error.clear();
            templates = ProjectManager::GetAvailableTemplates();
            selectedTemplateIdx = 0;
            for (int index = 0; index < static_cast<int>(templates.size()); ++index)
            {
                if (templates[index] == "Simple2D")
                {
                    selectedTemplateIdx = index;
                    break;
                }
            }
            ImGui::OpenPopup("New Project Wizard");
        }
        ImGui::PopStyleColor(3);

        ImGui::Spacing();

        // Open Existing — secondary
        if (ImGui::Button(ICON_FA_FOLDER_OPEN "  Open Existing Project", ImVec2(-1, 44.0f)))
        {
            const char* path = tinyfd_selectFolderDialog("Open Project", nullptr);
            if (path != nullptr)
            {
                browser_error.clear();
                if (!OpenProject(path))
                {
                    browser_error = std::string("Could not open project: ") + path + " (see Terminal for details).";
                }
            }
        }

        if (!browser_error.empty())
        {
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.35f, 0.35f, 1.0f));
            ImGui::PushTextWrapPos(0.0f);
            ImGui::TextWrapped("%s", browser_error.c_str());
            ImGui::PopTextWrapPos();
            ImGui::PopStyleColor();
        }

        // GitHub / Docs — link-style third action
        ImGui::Spacing();
        ImGui::PushFont(io.Fonts->Fonts[Font_Default]);
        if (ImGui::Button(ICON_FA_BOOK "  Open GitHub / Docs", ImVec2(-1, 32.0f)))
        {
            EditorUtils::OpenURL("https://github.com/ParmarManthanrajsinh/RayWaves-GameEngine");
        }
        ImGui::PopFont();

        // ── New Project Wizard ─────────────────────────────────────────
        ImGui::SetNextWindowSizeConstraints(ImVec2(480, 0), ImVec2(FLT_MAX, FLT_MAX));
        if (ImGui::BeginPopupModal("New Project Wizard", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::Text("Project Name");
            ImGui::InputText("##new_name", newProjectName, sizeof(newProjectName));

            std::string sanitized = ProjectManager::SanitizeCMakeProjectName(newProjectName);
            if (sanitized != newProjectName && strlen(newProjectName) > 0)
            {
                ImGui::TextDisabled("Will be created as: %s", sanitized.c_str());
            }

            ImGui::Spacing();

            ImGui::Text("Location");
            ImGui::InputText("##new_location", newProjectLocation, sizeof(newProjectLocation));
            ImGui::SameLine();
            if (ImGui::Button("Browse..."))
            {
                const char* folder = tinyfd_selectFolderDialog("Select Project Location", nullptr);
                if (folder != nullptr)
                {
                    strncpy(newProjectLocation, folder, sizeof(newProjectLocation) - 1);
                    newProjectLocation[sizeof(newProjectLocation) - 1] = '\0';
                }
            }

            ImGui::Spacing();

            if (!templates.empty())
            {
                if (selectedTemplateIdx < 0 || selectedTemplateIdx >= static_cast<int>(templates.size()))
                {
                    selectedTemplateIdx = 0;
                }

                if (ImGui::BeginCombo("Template", templates[selectedTemplateIdx].c_str()))
                {
                    for (int i = 0; i < templates.size(); ++i)
                    {
                        const bool is_selected = (selectedTemplateIdx == i);
                        if (ImGui::Selectable(templates[i].c_str(), is_selected))
                            selectedTemplateIdx = i;
                        if (is_selected)
                            ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }

                ImGui::PushTextWrapPos(0.0f);
                ImGui::TextDisabled("%s", s_TemplateDescription(templates[selectedTemplateIdx]));
                ImGui::PopTextWrapPos();
            }
            else
            {
                ImGui::TextColored(ImVec4(1, 0, 0, 1), "No templates found in templates/");
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            // Keep the wizard open on failure so the author can correct the
            // name/location instead of retyping everything.
            if (!browser_error.empty())
            {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.35f, 0.35f, 1.0f));
                ImGui::PushTextWrapPos(0.0f);
                ImGui::TextWrapped("%s", browser_error.c_str());
                ImGui::PopTextWrapPos();
                ImGui::PopStyleColor();
                ImGui::Spacing();
            }

            if (ImGui::Button("Create", ImVec2(120, 0)))
            {
                if (strlen(newProjectName) > 0 && strlen(newProjectLocation) > 0 && !templates.empty())
                {
                    fs::path fullPath = fs::path(newProjectLocation) / newProjectName;
                    std::string error;
                    if (ProjectManager::b_CreateProject(fullPath.string(), templates[selectedTemplateIdx], error))
                    {
                        browser_error.clear();
                        if (OpenProject(fullPath.string()))
                        {
                            ImGui::CloseCurrentPopup();
                        }
                        else
                        {
                            // Creation succeeded, so leave the folder in place and
                            // report the open failure in the browser.
                            browser_error = "Project created but could not be opened. See Terminal for details.";
                            ImGui::CloseCurrentPopup();
                        }
                    }
                    else
                    {
                        browser_error = error.empty() ? "Failed to create project." : error;
                    }
                }
                else
                {
                    browser_error = "Project name, location, and a template are required.";
                }
            }
            ImGui::SetItemDefaultFocus();
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0)))
            {
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }

        ImGui::Columns(1);

        ImGui::End();
        ImGui::PopStyleVar();

        rlImGuiEnd();
        EndDrawing();
    }

    if (logo.id != 0)
        UnloadTexture(logo);
}

bool GameEditor::OpenProject(std::string_view folderPath)
{
    // Never swap the session out from under an export: the worker holds a
    // project snapshot and paths that teardown would invalidate.
    if (m_ExportState.m_bIsExporting.load())
    {
        LogSessionError("Cannot switch projects while an export is running.");
        return false;
    }

    // Validate first so a bad folder leaves the current session and any
    // in-flight build untouched.
    t_Project validated_project;
    std::string error;
    if (!ProjectManager::b_ValidateProject(folderPath, validated_project, error))
    {
        LogSessionError("Cannot open project: " + error);
        return false;
    }

    // A build worker captures the project root and DLL paths. Stop it before
    // teardown so it cannot write into the old or new project.
    CancelBuild();
    if (m_BuildThread.joinable())
    {
        m_BuildThread.join();
    }

    // 1. Unload old DLL and reset map state (single teardown path in the loader)
    m_GameEngine.UnloadScene();
    m_LogicLoader.Unload();
    ClearSelection();
    m_CommandStack.Clear();
    m_EditorEntities.clear();
    m_LastActiveMap = nullptr;
    m_bHasSavedViewCamera = false;
    m_bWasPlaying = b_IsPlaying;
    m_AutoSaveElapsedSeconds = 0.0f;

    // 2. StateBag is scoped to hot-reloads (local in the loader). Game state lives
    //    inside the DLL's MapManager, which was destroyed and unloaded in step 1.

    // 3. Open project metadata
    if (!ProjectManager::b_OpenProject(folderPath, true, error))
    {
        LogSessionError("Cannot open project: " + error);
        // b_OpenProject flips the open flag before generating the project CMake
        // files. Drop that half-open state so the editor does not run against a
        // project whose setup never finished.
        ProjectManager::CloseProject();
        return false;
    }

    CleanupStaleShadowCopies(folderPath);

    // Set Window Title
    std::string windowTitle = "RayWaves — " + ProjectManager::GetCurrent().m_Name;
    SetWindowTitle(windowTitle.c_str());

    // 4. Set DLL path
    m_LogicLoader.SetGameLogicPath(ProjectManager::GetCurrent().m_DllPath);

    // 5. Update AssetResolver
    AssetResolver::SetProjectAssetPath(ProjectManager::GetCurrent().m_AssetPath);

    // 6. Compile async (DLL will be loaded when the loader drains its reload request)
    CompileGameLogic();

    // 7. Config sync
    auto& prj = ProjectManager::GetCurrent();
    m_SceneSettings.m_SceneWidth = prj.m_SceneWidth;
    m_SceneSettings.m_SceneHeight = prj.m_SceneHeight;
    m_SceneSettings.m_TargetFPS = prj.m_TargetFPS;
    SetTargetFPS(m_SceneSettings.m_TargetFPS);

    // 8. Update workspace layout path and load it
    if (ImGui::GetIO().IniFilename != nullptr)
    {
        ImGui::SaveIniSettingsToDisk(ImGui::GetIO().IniFilename);
    }

    std::filesystem::path proj_dir = ProjectManager::GetCurrent().m_RootPath;
    m_Terminal.SetWorkingDirectory(proj_dir.string());
    s_LayoutPath = (proj_dir / ".raywaves" / "layout.ini").string();
    ImGui::GetIO().IniFilename = s_LayoutPath.c_str();

    if (std::filesystem::exists(s_LayoutPath))
    {
        ImGui::LoadIniSettingsFromDisk(s_LayoutPath.c_str());

        // Layouts saved before the Scene/Viewport split leave the Inspector
        // floating or hidden. Reset once so upgrades land in the default dock.
        std::ifstream layout_file(s_LayoutPath);
        const std::string layout((std::istreambuf_iterator<char>(layout_file)), std::istreambuf_iterator<char>());
        if (layout.find("Viewport") == std::string::npos)
        {
            LoadEditorDefaultIni();
            ImGui::GetIO().IniFilename = s_LayoutPath.c_str();
        }
    }
    else
    {
        LoadEditorDefaultIni();
        ImGui::GetIO().IniFilename = s_LayoutPath.c_str();
    }

    return true;
}

void GameEditor::CleanupProject()
{
    m_GameEngine.UnloadScene();
    m_LogicLoader.Unload();
    m_LogicLoader.SetGameLogicPath("");
    ClearSelection();
    m_CommandStack.Clear();
    m_EditorEntities.clear();
    m_LastActiveMap = nullptr;
    m_bHasSavedViewCamera = false;
    m_bWasPlaying = false;
    m_EditorCamera3D.Invalidate();
    m_bEditorCamera3DReady = false;
    m_EditorCameraMapKey.clear();
}

void GameEditor::LogSessionError(std::string_view message)
{
    m_Terminal.add_text(message, term::Severity::Error);

    FBuildMessage build_message;
    build_message.Severity = FBuildMessage::ESeverity::Error;
    build_message.Text = std::string(message);
    std::scoped_lock lock(BuildMessagesMutex);
    BuildMessages.push_back(std::move(build_message));
}

bool GameEditor::b_RestoreAutosave()
{
    if (!ProjectManager::b_HasOpenProject())
    {
        LogSessionError("Cannot restore autosave: no project is open.");
        return false;
    }

    const std::filesystem::path project_root = ProjectManager::GetCurrent().m_RootPath;
    Autosave::FSnapshot snapshot;
    if (!Autosave::b_Query(project_root, snapshot))
    {
        LogSessionError("Cannot restore autosave: no snapshot exists for this project.");
        return false;
    }

    // A snapshot from another map would silently replace unrelated content, so
    // refuse instead of guessing which scene the author meant.
    const std::string active_map_id = m_GameEngine.GetActiveSceneKey();
    if (active_map_id != snapshot.m_MapId)
    {
        LogSessionError("Cannot restore autosave: snapshot belongs to map '" + snapshot.m_MapId +
            "', current map is '" + active_map_id + "'.");
        return false;
    }

    std::string error;
    Autosave::FSnapshot loaded_snapshot;
    if (!Autosave::b_Load(project_root, GetScene(), loaded_snapshot, error))
    {
        LogSessionError("Cannot restore autosave: " + (error.empty() ? std::string("unknown error") : error));
        return false;
    }

    // Recovery is an unsaved edit: the author still has to commit it with Save.
    GetScene().MarkDirty();
    m_GameEngine.RefreshSceneObjects();
    m_Terminal.add_text("Restored autosave for map '" + loaded_snapshot.m_MapId + "'.", term::Severity::Debug);
    return true;
}

GameMap* GameEditor::GetActiveMap()
{
    if (MapManager* manager = GetMapManager())
    {
        return manager->GetCurrentMap();
    }
    return m_GameEngine.GetMap();
}

Camera2D GameEditor::GetActiveViewCamera()
{
    if (GameMap* active_map = GetActiveMap())
    {
        return active_map->GetViewCamera();
    }
    return Camera2D{ { 0.0f, 0.0f }, { 0.0f, 0.0f }, 0.0f, 1.0f };
}

Camera2D* GameEditor::GetMutableActiveViewCamera()
{
    if (GameMap* active_map = GetActiveMap())
    {
        return active_map->GetMutableViewCamera();
    }
    return nullptr;
}

Camera3D GameEditor::GetActiveViewCamera3D()
{
    if (!b_IsPlaying && m_bEditorCamera3DReady && b_ActiveMapUses3DView())
    {
        return m_EditorCamera3D.GetCamera();
    }

    if (GameMap* active_map = GetActiveMap())
    {
        return active_map->GetViewCamera3D();
    }
    return Camera3D{ { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f }, { 0.0f, 1.0f, 0.0f }, 60.0f, CAMERA_PERSPECTIVE };
}

void GameEditor::SyncEditorCamera3D()
{
    GameMap* active_map = GetActiveMap();
    if ((active_map == nullptr) || !active_map->b_Uses3DView())
    {
        m_EditorCamera3D.Invalidate();
        m_bEditorCamera3DReady = false;
        m_EditorCameraMapKey.clear();
        return;
    }

    const std::string key = m_GameEngine.GetActiveSceneKey();
    if (!m_bEditorCamera3DReady || (key != m_EditorCameraMapKey))
    {
        m_EditorCamera3D.FromCamera(active_map->GetViewCamera3D());
        m_EditorCameraMapKey = key;
        m_bEditorCamera3DReady = true;
    }
}

void GameEditor::ApplyEditorViewCamera3D()
{
    GameMap* active_map = GetActiveMap();
    if (active_map == nullptr)
    {
        return;
    }

    if (!b_IsPlaying && active_map->b_Uses3DView() && m_bEditorCamera3DReady)
    {
        active_map->SetEditorViewCamera3D(m_EditorCamera3D.GetCamera());
    }
    else
    {
        active_map->ClearEditorViewCamera3D();
    }
}

bool GameEditor::b_ActiveMapUses3DView()
{
    GameMap* active_map = GetActiveMap();
    return (active_map != nullptr) && active_map->b_Uses3DView();
}

Vector2 GameEditor::GetViewportCenterWorld()
{
    const Camera2D camera = GetActiveViewCamera();
    return GetScreenToWorld2D({ static_cast<float>(m_SceneSettings.m_SceneWidth) * 0.5f,
                                  static_cast<float>(m_SceneSettings.m_SceneHeight) * 0.5f },
        camera);
}

void GameEditor::UpdatePlayStateCamera()
{
    if (b_IsPlaying == m_bWasPlaying)
    {
        return;
    }

    Camera2D* camera = GetMutableActiveViewCamera();
    if (b_IsPlaying)
    {
        if (camera != nullptr)
        {
            m_SavedViewCamera = *camera;
            m_bHasSavedViewCamera = true;
        }
    }
    else if (camera != nullptr && m_bHasSavedViewCamera)
    {
        *camera = m_SavedViewCamera;
        m_bHasSavedViewCamera = false;
    }

    m_bWasPlaying = b_IsPlaying;
}

bool GameEditor::b_IsSceneObjectSelected(uint32_t id) const
{
    return std::ranges::find(m_SelectedSceneObjectIds, id) != m_SelectedSceneObjectIds.end();
}

bool GameEditor::b_IsEditorEntitySelected(uint64_t id) const
{
    return std::ranges::find(m_SelectedEditorEntityIds, id) != m_SelectedEditorEntityIds.end();
}

std::size_t GameEditor::GetSelectionCount() const
{
    if (m_SelectionKind == EEditorSelectionKind::SceneObject)
    {
        return m_SelectedSceneObjectIds.size();
    }
    if (m_SelectionKind == EEditorSelectionKind::GameplayEntity)
    {
        return m_SelectedEditorEntityIds.size();
    }
    return 0;
}

void GameEditor::SelectSceneObject(uint32_t id)
{
    m_SelectionKind = EEditorSelectionKind::SceneObject;
    m_SelectedSceneObjectIds.clear();
    if (id != 0)
    {
        m_SelectedSceneObjectIds.push_back(id);
    }
    m_SelectedSceneObjectId = id;
    m_SelectedEditorEntityId = 0;
    m_SelectedEditorEntityIds.clear();
}

void GameEditor::ToggleSceneObjectSelection(uint32_t id)
{
    if (id == 0)
    {
        return;
    }

    if (m_SelectionKind != EEditorSelectionKind::SceneObject)
    {
        SelectSceneObject(id);
        return;
    }

    const auto found = std::ranges::find(m_SelectedSceneObjectIds, id);
    if (found != m_SelectedSceneObjectIds.end())
    {
        m_SelectedSceneObjectIds.erase(found);
        if (m_SelectedSceneObjectIds.empty())
        {
            ClearSelection();
            return;
        }
        if (m_SelectedSceneObjectId == id)
        {
            m_SelectedSceneObjectId = m_SelectedSceneObjectIds.back();
        }
        return;
    }

    m_SelectedSceneObjectIds.push_back(id);
    m_SelectedSceneObjectId = id;
}

void GameEditor::AddSceneObjectToSelection(uint32_t id)
{
    if (id == 0)
    {
        return;
    }
    if (m_SelectionKind != EEditorSelectionKind::SceneObject)
    {
        SelectSceneObject(id);
        return;
    }
    if (!b_IsSceneObjectSelected(id))
    {
        m_SelectedSceneObjectIds.push_back(id);
    }
    m_SelectedSceneObjectId = id;
}

void GameEditor::SelectSceneObjects(const std::vector<uint32_t>& ids)
{
    if (ids.empty())
    {
        ClearSelection();
        return;
    }

    m_SelectionKind = EEditorSelectionKind::SceneObject;
    m_SelectedSceneObjectIds = ids;
    m_SelectedSceneObjectId = ids.back();
    m_SelectedEditorEntityId = 0;
    m_SelectedEditorEntityIds.clear();
}

void GameEditor::RemoveSceneObjectFromSelection(uint32_t id)
{
    const auto found = std::ranges::find(m_SelectedSceneObjectIds, id);
    if (found == m_SelectedSceneObjectIds.end())
    {
        return;
    }

    m_SelectedSceneObjectIds.erase(found);
    if (m_SelectedSceneObjectIds.empty())
    {
        ClearSelection();
    }
    else if (m_SelectedSceneObjectId == id)
    {
        m_SelectedSceneObjectId = m_SelectedSceneObjectIds.back();
    }
}

void GameEditor::SelectEditorEntity(uint64_t id)
{
    m_SelectionKind = EEditorSelectionKind::GameplayEntity;
    m_SelectedEditorEntityIds.clear();
    if (id != 0)
    {
        m_SelectedEditorEntityIds.push_back(id);
    }
    m_SelectedEditorEntityId = id;
    m_SelectedSceneObjectId = 0;
    m_SelectedSceneObjectIds.clear();
}

void GameEditor::ToggleEditorEntitySelection(uint64_t id)
{
    if (id == 0)
    {
        return;
    }
    if (m_SelectionKind != EEditorSelectionKind::GameplayEntity)
    {
        SelectEditorEntity(id);
        return;
    }

    const auto found = std::ranges::find(m_SelectedEditorEntityIds, id);
    if (found != m_SelectedEditorEntityIds.end())
    {
        m_SelectedEditorEntityIds.erase(found);
        if (m_SelectedEditorEntityIds.empty())
        {
            ClearSelection();
            return;
        }
        if (m_SelectedEditorEntityId == id)
        {
            m_SelectedEditorEntityId = m_SelectedEditorEntityIds.back();
        }
        return;
    }

    m_SelectedEditorEntityIds.push_back(id);
    m_SelectedEditorEntityId = id;
}

void GameEditor::AddEditorEntityToSelection(uint64_t id)
{
    if (id == 0)
    {
        return;
    }
    if (m_SelectionKind != EEditorSelectionKind::GameplayEntity)
    {
        SelectEditorEntity(id);
        return;
    }
    if (!b_IsEditorEntitySelected(id))
    {
        m_SelectedEditorEntityIds.push_back(id);
    }
    m_SelectedEditorEntityId = id;
}

void GameEditor::SelectEditorEntities(const std::vector<uint64_t>& ids)
{
    if (ids.empty())
    {
        ClearSelection();
        return;
    }

    m_SelectionKind = EEditorSelectionKind::GameplayEntity;
    m_SelectedEditorEntityIds = ids;
    m_SelectedEditorEntityId = ids.back();
    m_SelectedSceneObjectId = 0;
    m_SelectedSceneObjectIds.clear();
}

void GameEditor::RemoveEditorEntityFromSelection(uint64_t id)
{
    const auto found = std::ranges::find(m_SelectedEditorEntityIds, id);
    if (found == m_SelectedEditorEntityIds.end())
    {
        return;
    }

    m_SelectedEditorEntityIds.erase(found);
    if (m_SelectedEditorEntityIds.empty())
    {
        ClearSelection();
    }
    else if (m_SelectedEditorEntityId == id)
    {
        m_SelectedEditorEntityId = m_SelectedEditorEntityIds.back();
    }
}

void GameEditor::ClearSelection()
{
    m_SelectionKind = EEditorSelectionKind::None;
    m_SelectedSceneObjectId = 0;
    m_SelectedEditorEntityId = 0;
    m_SelectedSceneObjectIds.clear();
    m_SelectedEditorEntityIds.clear();
}

void GameEditor::RefreshEditorEntities()
{
    GameMap* active_map = GetActiveMap();
    if (active_map != m_LastActiveMap)
    {
        m_LastActiveMap = active_map;
        m_bHasSavedViewCamera = false;
        m_bWasPlaying = b_IsPlaying;
        ClearSelection();
        // Commands hold Scene references and gameplay pointers; a map swap
        // makes both stale.
        m_CommandStack.Clear();
    }

    m_EditorEntities.clear();
    if (active_map == nullptr)
    {
        return;
    }

    const auto& registry = active_map->GetEditorEntities();
    m_EditorEntities.reserve(registry.size());
    for (const t_EditorEntity& entity : registry)
    {
        if ((entity.m_Position == nullptr) && (entity.m_Position3D == nullptr))
        {
            continue;
        }

        FEditorEntityRef ref;
        ref.m_Id = entity.m_Id;
        ref.m_Name = entity.m_Name;
        ref.m_Type = entity.m_Type;
        ref.m_Position = entity.m_Position;
        ref.m_Size = entity.m_Size;
        ref.m_Origin = entity.m_Origin;
        ref.m_Rotation = entity.m_Rotation;
        ref.m_Visible = entity.m_Visible;
        ref.m_Position3D = entity.m_Position3D;
        ref.m_Size3D = entity.m_Size3D;
        ref.m_Origin3D = entity.m_Origin3D;
        ref.m_Properties = entity.m_Properties;
        ref.m_PropertyCount = entity.m_PropertyCount;
        m_EditorEntities.push_back(std::move(ref));
    }

    // Drop ids the refreshed map no longer owns. Stop at the first invalid
    // primary so the selection always has a live Inspector target.
    if (m_SelectionKind == EEditorSelectionKind::SceneObject)
    {
        std::erase_if(m_SelectedSceneObjectIds,
            [this](uint32_t id) { return m_GameEngine.GetScene().FindObject(id) == nullptr; });
        if (m_SelectedSceneObjectIds.empty())
        {
            ClearSelection();
        }
        else if (!b_IsSceneObjectSelected(m_SelectedSceneObjectId))
        {
            m_SelectedSceneObjectId = m_SelectedSceneObjectIds.back();
        }
    }
    else if (m_SelectionKind == EEditorSelectionKind::GameplayEntity)
    {
        std::erase_if(m_SelectedEditorEntityIds,
            [this](uint64_t id)
            {
                return std::ranges::find_if(m_EditorEntities, [id](const FEditorEntityRef& entity)
                           { return entity.m_Id == id; }) == m_EditorEntities.end();
            });
        if (m_SelectedEditorEntityIds.empty())
        {
            ClearSelection();
        }
        else if (!b_IsEditorEntitySelected(m_SelectedEditorEntityId))
        {
            m_SelectedEditorEntityId = m_SelectedEditorEntityIds.back();
        }
    }
}

void GameEditor::InvalidateEditorEntities()
{
    m_EditorEntities.clear();
    m_LastActiveMap = nullptr;
    ClearSelection();
    m_CommandStack.Clear();
}

void GameEditor::CloseProject()
{
    // The export worker reads project paths and the GameLogic DLL; tearing the
    // session down first would leave it writing into a half-destroyed project.
    if (m_ExportState.m_bIsExporting.load())
    {
        LogSessionError("Cannot close the project while an export is running.");
        return;
    }

    CancelBuild();
    if (m_BuildThread.joinable())
    {
        m_BuildThread.join();
    }

    m_AutoSaveElapsedSeconds = 0.0f;
    CleanupProject();
    ProjectManager::CloseProject();
    m_SceneSettings.m_SceneWidth = 1280;
    m_SceneSettings.m_SceneHeight = 720;
    m_SceneSettings.m_TargetFPS = 60;
    SetWindowTitle("RayWaves");
}

void GameEditor::Run()
{
    while (!WindowShouldClose())
    {
        // Clean exit requested from DLL callback
        if (m_bCloseRequested)
            break;

        if (!ProjectManager::b_HasOpenProject())
        {
            // Cleanup current project state before opening browser
            CleanupProject();

            // Show browser
            RunBrowser();

            // If a new project was opened from the browser
            if (ProjectManager::b_HasOpenProject())
            {
                // OpenProject() already triggered CompileGameLogic() async.
                // DLL will be loaded when the loader drains its reload request.
                continue;
            }

            // User closed the window from the browser
            break;
        }

        SCOPED_TIMER("frame_total");

        // DLL change detection runs first so timestamp-triggered reloads apply
        // the same frame; the drain below pauses playback around the swap.
        m_LogicLoader.CheckForChanges();

        // A build completed and the DLL needs reloading (flagged by the
        // CompileGameLogic completion callback, possibly off-thread).
        // The wrapper owns play-state around the swap; this just drains the flag.
        if (m_LogicLoader.PollReloadRequested())
        {
            b_ReloadGameLogic();
        }

        UpdatePerformanceMetrics();

        float delta_time = GetFrameTime();

        // Periodic crash-recovery snapshot. Only unsaved, stopped edits are
        // worth capturing; the snapshot never clears the dirty flag.
        const t_EditorPreferences& preferences = EditorPreferences::GetInstance().GetPreferences();
        const int auto_save_interval = std::clamp(preferences.AutoSaveIntervalSeconds, 10, 3600);
        const bool b_can_autosave = preferences.AutoSaveEnabled && !b_IsPlaying && GetScene().b_IsDirty();
        if (b_can_autosave)
        {
            m_AutoSaveElapsedSeconds += delta_time;
            if (m_AutoSaveElapsedSeconds >= static_cast<float>(auto_save_interval))
            {
                m_AutoSaveElapsedSeconds = 0.0f;
                const std::string active_map_id = m_GameEngine.GetActiveSceneKey();
                if (!active_map_id.empty())
                {
                    std::string autosave_error;
                    if (!Autosave::b_Save(
                            ProjectManager::GetCurrent().m_RootPath, active_map_id, GetScene(), autosave_error))
                    {
                        m_Terminal.add_text("Autosave failed: " + autosave_error, term::Severity::Error);
                    }
                }
            }
        }
        else
        {
            m_AutoSaveElapsedSeconds = 0.0f;
        }

        m_GameEngine.SetViewportSize(m_SceneSettings.m_SceneWidth, m_SceneSettings.m_SceneHeight);
        UpdatePlayStateCamera();
        SyncEditorCamera3D();
        ApplyEditorViewCamera3D();
        if (b_IsPlaying)
        {
            m_GameEngine.UpdateMap(delta_time);
        }

        // Handle deferred texture recreation outside ImGui render loop to avoid OpenGL crashes

        if (g_bNeedsTextureRecreate)
        {
            SCOPED_TIMER("texture_recreate");
            g_bNeedsTextureRecreate = false;
            UnloadRenderTexture(m_RaylibTexture);
            if (m_DisplayTexture.id != 0)
                UnloadRenderTexture(m_DisplayTexture);

            m_RaylibTexture = LoadRenderTexture(m_SceneSettings.m_SceneWidth, m_SceneSettings.m_SceneHeight);
            m_DisplayTexture = LoadRenderTexture(m_SceneSettings.m_SceneWidth, m_SceneSettings.m_SceneHeight);

            SetTextureFilter(m_RaylibTexture.texture, TEXTURE_FILTER_BILINEAR);
            SetTextureFilter(m_DisplayTexture.texture, TEXTURE_FILTER_BILINEAR);
        }

        BeginDrawing();

        BeginTextureMode(m_RaylibTexture);
        ClearBackground(RAYWHITE);

        m_GameEngine.DrawMap();
        EndTextureMode();

        m_SourceTexture = m_RaylibTexture.texture;

        // Opaque pass to strip alpha before presenting via ImGui
        if (m_bUseOpaquePass)
        {
            BeginTextureMode(m_DisplayTexture);
            ClearBackground(BLANK);
            BeginShaderMode(m_OpaqueShader);
            Rectangle src = { 0, 0, static_cast<float>(m_SourceTexture.width),
                -static_cast<float>(m_SourceTexture.height) };
            DrawTextureRec(m_SourceTexture, src, { 0.0f, 0.0f }, WHITE);
            EndShaderMode();
            EndTextureMode();
            m_SourceTexture = m_DisplayTexture.texture;
        }

        if (m_bNeedsThemeRebake)
        {
            m_bNeedsThemeRebake = false;
            ThemeService::RebakeNow();
        }

        if (m_bNeedsLayoutReset)
        {
            m_bNeedsLayoutReset = false;
            LoadEditorDefaultIni();

            if (ProjectManager::b_HasOpenProject())
            {
                std::filesystem::path proj_dir = ProjectManager::GetCurrent().m_RootPath;
                s_LayoutPath = (proj_dir / ".raywaves" / "layout.ini").string();
            }
            else
            {
                std::filesystem::path dir =
                    std::filesystem::path(EditorPreferences::GetInstance().GetConfigPath()).parent_path();
                s_LayoutPath = (dir / "editor_layout.ini").string();
            }

            ImGui::GetIO().IniFilename = s_LayoutPath.c_str();
        }

        Profiler::Get().NextFrame();

        RefreshEditorEntities();

        rlImGuiBegin();

        ImGui::DockSpaceOverViewport(0, m_Viewport);

        for (auto& panel : m_Panels)
        {
            panel->Draw(this);
        }

        rlImGuiEnd();
        EndDrawing();
    }

    Close();
}

void GameEditor::Close()
{
    if (m_bClosed)
    {
        return;
    }
    m_bClosed = true;

    t_WindowConfig& config = GameConfig::GetInstance().GetWindowConfig();
    config.scene_width = m_SceneSettings.m_SceneWidth;
    config.scene_height = m_SceneSettings.m_SceneHeight;
    config.scene_fps = m_SceneSettings.m_TargetFPS;
    // Save beside the executable: the editor may be launched from a desktop
    // shortcut whose working directory is not the install directory.
    const fs::path exe_config = PlatformInfo::GetExecutablePath().parent_path() / "config.ini";
    const std::string config_path = exe_config.empty() ? std::string("config.ini") : exe_config.string();
    GameConfig::GetInstance().m_bSaveToFile(config_path);

    if (ProjectManager::b_HasOpenProject())
    {
        auto& prj = ProjectManager::GetCurrent();
        prj.m_SceneWidth = m_SceneSettings.m_SceneWidth;
        prj.m_SceneHeight = m_SceneSettings.m_SceneHeight;
        prj.m_TargetFPS = m_SceneSettings.m_TargetFPS;
        ProjectManager::b_SaveCurrentProject();
    }

    // Stop workers before destroying objects captured by their callbacks.
    m_ThreadCancelFlag->store(true);
    CancelBuild();
    if (m_BuildThread.joinable())
    {
        m_BuildThread.join();
    }
    if (m_ExportState.m_ExportThread.joinable())
    {
        m_ExportState.m_ExportThread.join();
    }

    // Destroy DLL-owned maps and unload the DLL while the graphics/audio
    // contexts are still alive. Map destructors release textures and sounds.
    m_LogicLoader.Unload();
    m_GameEngine.SetMap(nullptr);
    m_GameEngine.SetMapManager(nullptr);

    if (m_OpaqueShader.id != 0)
    {
        UnloadShader(m_OpaqueShader);
        m_OpaqueShader.id = 0;
    }

    if (m_RaylibTexture.id != 0)
    {
        UnloadRenderTexture(m_RaylibTexture);
        m_RaylibTexture.id = 0;
    }

    if (m_DisplayTexture.id != 0)
    {
        UnloadRenderTexture(m_DisplayTexture);
        m_DisplayTexture.id = 0;
    }

    m_Terminal.Shutdown();

    rlImGuiShutdown();
    CloseAudioDevice();
    CloseWindow();
}

void GameEditor::LoadMap(GameMap* game_map)
{
    m_LogicLoader.AttachMap(game_map);
}

bool GameEditor::b_LoadGameLogic(std::string_view dll_path)
{
    // Exit-request wiring lives in the loader's new-map callback (see constructor).
    return m_LogicLoader.b_LoadGameLogic(dll_path);
}

bool GameEditor::b_ReloadGameLogic()
{
    // The reload destroys the map and its editor-entity storage. Drop cached
    // pointers before the swap; panels may still draw later this frame.
    InvalidateEditorEntities();

    bool b_WasPlaying = b_IsPlaying;
    b_IsPlaying = false;

    bool b_Ok = m_LogicLoader.b_ReloadGameLogic();
    b_IsPlaying = b_WasPlaying;

    return b_Ok;
}

void GameEditor::UpdatePerformanceMetrics()
{
    if (m_FrameTimes.empty())
    {
        return;
    }

    m_FrameTimes[m_FrameOffset] = GetFrameTime() * 1000.0f;
    m_FrameOffset = (m_FrameOffset + 1) % m_FrameTimes.size();
}

void GameEditor::ParseBuildLine(std::string_view line)
{
    BuildDiagnostics::t_Diagnostic diagnostic;
    if (!BuildDiagnostics::b_ParseLine(line, diagnostic))
    {
        return;
    }

    FBuildMessage msg;
    msg.Severity = (diagnostic.m_Severity == BuildDiagnostics::ESeverity::Error) ? FBuildMessage::ESeverity::Error
                                                                                 : FBuildMessage::ESeverity::Warning;
    msg.File = diagnostic.m_File;
    msg.Line = diagnostic.m_Line;
    msg.Text = diagnostic.m_Raw;

    std::scoped_lock lock(BuildMessagesMutex);
    BuildMessages.push_back(std::move(msg));
}

void GameEditor::CancelBuild()
{
    if (m_BuildCancel)
    {
        m_BuildCancel->store(true);
    }
}

void GameEditor::CompileGameLogic()
{
    // A running build is cancelled, not waited on. The worker notices within
    // one poll interval and kills its process tree, so this join stays short.
    CancelBuild();
    if (m_BuildThread.joinable())
    {
        m_BuildThread.join();
    }
    m_BuildCancel = std::make_shared<std::atomic<bool>>(false);

    // Both the editor and rwcli link the same GameLogic module. Acquire the
    // cross-process lock instead of only observing it, otherwise a build
    // started here can overlap an agent build that starts a moment later.
    fs::path locked_project_root;
    std::shared_ptr<void> build_lock;
    if (ProjectManager::b_HasOpenProject())
    {
        locked_project_root = ProjectManager::GetCurrent().m_RootPath;
        std::string lock_error;
        if (!AgentLock::b_Acquire(locked_project_root, "editor build", lock_error))
        {
            m_bShowTerminal = true;
            m_Terminal.SelectConsole();
            m_Terminal.add_text(
                "Build skipped: " + lock_error + ". Wait for the other build to finish.", term::Severity::Error);
            bShowMessageLog = true;
            return;
        }

        struct FLockGuard
        {
            fs::path m_Root;
            ~FLockGuard() { AgentLock::Release(m_Root); }
        };
        build_lock = std::shared_ptr<void>(
            new FLockGuard{ locked_project_root }, [](void* pointer) { delete static_cast<FLockGuard*>(pointer); });
    }

    b_IsCompiling = true;
    b_IsPlaying = false;

    m_bShowTerminal = true;
    m_Terminal.SelectConsole();
    m_Terminal.add_text("Starting build process...", term::Severity::Debug);

    BuildStatus = EBuildStatus::Compiling;
    {
        std::scoped_lock lock(BuildMessagesMutex);
        BuildMessages.clear();
    }

    const fs::path app_dir = GetApplicationDirectory();
    const fs::path tools_dir = ProjectManager::GetToolsDirectory();
    fs::path cmake_exe = tools_dir / "cmake" / "bin" / rw::platform::ExecutableName("cmake");
    if (!fs::exists(cmake_exe))
    {
        const fs::path system_cmake = PlatformInfo::FindExecutable("cmake");
        if (!system_cmake.empty())
        {
            cmake_exe = system_cmake;
        }
    }
    const fs::path setup_tool = tools_dir / (rw::platform::b_IsWindows ? "setup_tools.ps1" : "setup_tools.sh");
    const bool b_has_project = ProjectManager::b_HasOpenProject();

    fs::path project_root = app_dir;
    fs::path source_dir = app_dir;
    fs::path build_dir = app_dir;
    if (b_has_project)
    {
        project_root = ProjectManager::GetCurrent().m_RootPath;
        source_dir = project_root / ".raywaves";
        build_dir = source_dir / "build";
    }

    m_Terminal.add_text("Executing: cmake build", term::Severity::Debug);

    auto cancel = m_BuildCancel;
    m_BuildThread = std::thread(
        [this, cancel, build_lock, app_dir, cmake_exe, setup_tool, project_root, source_dir, build_dir, b_has_project]()
        {
            // Keep the cross-process lock alive for the whole build; the
            // shared_ptr releases it on every exit path, including cancel.
            (void)build_lock;
            auto run = [this, cancel](const std::vector<std::string>& argv, const fs::path& working_dir)
            {
                if (cancel->load())
                {
                    return false;
                }
                return ProcessRunner::RunAndWaitCommand(
                    argv, working_dir.string(),
                    [this, cancel](std::string_view line, bool is_error)
                    {
                        if (cancel->load())
                        {
                            return;
                        }
                        ParseBuildLine(line);
                        m_Terminal.add_text(line, is_error ? term::Severity::Error : term::Severity::Debug);
                    },
                    cancel);
            };

            bool b_ok = true;
            if (!fs::exists(cmake_exe))
            {
                m_Terminal.add_text("Downloading CMake (first-time setup, in background)...", term::Severity::Debug);
                std::vector<std::string> setup_argv;
                if (rw::platform::b_IsWindows)
                {
                    setup_argv = { "powershell", "-ExecutionPolicy", "Bypass", "-File", setup_tool.string(), "-SkipZig",
                        "-SkipRcEdit", "-SkipNinja" };
                }
                else
                {
                    setup_argv = { "bash", setup_tool.string(), "--skip-zig", "--skip-rcedit", "--skip-ninja" };
                }
                b_ok = run(setup_argv, project_root);
            }

            if (b_ok && b_has_project)
            {
                // Project folders are portable, but CMake caches contain
                // absolute paths. Try a normal configure, then --fresh.
                b_ok = run(
                    { cmake_exe.string(), "-G", "Ninja", source_dir.string(), "-B", build_dir.string() }, project_root);
                if (!b_ok)
                {
                    b_ok = run(
                        { cmake_exe.string(), "--fresh", "-G", "Ninja", source_dir.string(), "-B", build_dir.string() },
                        project_root);
                }
                if (b_ok)
                {
                    b_ok =
                        run({ cmake_exe.string(), "--build", build_dir.string(), "--config", "Release" }, project_root);
                }
            }
            else if (b_ok)
            {
                // Dev environment fallback: build the bundled GameLogic target.
                b_ok = run({ cmake_exe.string(), "--build", app_dir.string(), "--target", "GameLogic" }, app_dir);
            }

            if (cancel->load())
            {
                b_IsCompiling = false;
                return;
            }
            if (b_ok)
            {
                m_Terminal.add_text("Build Successful.", term::Severity::Debug);
                BuildStatus = EBuildStatus::Success;
                NotificationTimer = 4.0f;
                m_LogicLoader.RequestReload();
            }
            else
            {
                m_Terminal.add_text("Build Failed.", term::Severity::Error);
                BuildStatus = EBuildStatus::Failed;
                bShowMessageLog = true;
                NotificationTimer = 10.0f;
            }
            b_IsCompiling = false;
        });
}
