#include <iostream>
#include "../Engine/MapManager.h" // IWYU pragma: keep
#include "../Engine/ProjectManager.h"
#include "../Engine/Profiler.h"
#include "../Engine/AssetResolver.h"
#include "GameEditor.h"
#include "ThemeService.h"
#include "EditorUtils.h"
#include "ProcessRunner.h"
#include "../Game/DllLoader.h"
#include <imgui/imgui_stdlib.h>
#include <imgui_internal.h>
#include <filesystem>
#include <map>
#include <cstdio>
using Clock = std::chrono::steady_clock;

#include "Panels/MainMenuBar.h"
#include "Panels/SceneWindow.h"
#include "Panels/MapSelectionPanel.h"
#include "Panels/ExportPanel.h"
#include "Panels/SceneSettingsPanel.h"
#include "Panels/PerformanceOverlay.h"
#include "Panels/MessageLogPanel.h"
#include "Panels/EditorPreferencesPanel.h"
#include "EditorPreferences.h"
#include "PanelRegistry.h"
#include <memory>
#include <cstdlib>

static std::string s_LayoutPath;

bool g_bNeedsTextureRecreate = false;

namespace
{
    // Core panels in draw/dock order. Built fresh per call so repeated
    // GameEditor construction (and tests) never double-register.
    std::vector<FPanelFactory> s_CorePanelFactories()
    {
        return 
        {
            &s_fMakePanel<MainMenuBar>,
            &s_fMakePanel<MapSelectionPanel>,
            &s_fMakePanel<ExportPanel>,
            &s_fMakePanel<SceneSettingsPanel>,
            &s_fMakePanel<SceneWindow>,
            &s_fMakePanel<PerformanceOverlay>,
            &s_fMakePanel<MessageLogPanel>,
            &s_fMakePanel<EditorPreferencesPanel>,
        };
    }
}

GameEditor::GameEditor()
	:
	  b_IsPlaying(false),
	  b_IsCompiling(false),
	  m_RaylibTexture({}),
	  m_DisplayTexture({}),
	  m_SourceTexture({}),
	  m_FrameOffset(0),
	  m_Viewport(nullptr),
	  m_LogicLoader(m_GameEngine),
	  m_OpaqueShader({})
{
    m_Terminal.InitCapture();

    // Route loader diagnostics into the editor terminal (stderr is invisible
    // in the GUI app) and wire DLL exit requests to window close.
    m_LogicLoader.SetLogSink([this](std::string_view message, bool is_error)
    {
        m_Terminal.add_text(message, is_error ? term::Severity::Error : term::Severity::Debug);
    });
    m_LogicLoader.SetNewMapCallback([this](GameMap* new_map)
    {
        if (new_map != nullptr)
        {
            new_map->SetExitCallback([this]() { m_bCloseRequested = true; });
        }
    });

	m_Panels.reserve(8);
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
	m_ThreadCancelFlag->store(true);

	// Join the build thread first: cancel flag suppresses its callbacks,
	// joining guarantees no callback can touch this object afterwards.
	if (m_BuildThread.joinable())
	{
		m_BuildThread.join();
	}

	// Join export thread before destroying its state
	if (m_ExportState.m_ExportThread.joinable())
	{
		m_ExportState.m_ExportThread.join();
	}

	/*
		Ensure any GameMap instance (potentially from the DLL) is destroyed
		BEFORE unloading the DLL, otherwise vtable/function code may be gone
		when the map's destructor runs. GameLogicLoader::Unload() owns that
		ordering; its own destructor is the final backstop.
	*/
	m_LogicLoader.Unload();

	m_GameEngine.SetMap(nullptr);
	m_GameEngine.SetMapManager(nullptr);

	m_Terminal.add_text("Shutting down...", term::Severity::Debug);

	// Save configurations on exit
	EditorPreferences::GetInstance().m_bSaveToFile();

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

	if (m_OpaqueShader.id != 0)
	{
		UnloadShader(m_OpaqueShader);
		m_OpaqueShader.id = 0;
	}
}

void GameEditor::Init(int width, int height, std::string_view title)
{
	// Flags must be set BEFORE InitWindow: flipping GLFW_RESIZABLE after the
	// window is mapped makes KWin desync the titlebar (close button missing
	// until the next resize).
	SetConfigFlags(FLAG_WINDOW_RESIZABLE);
	m_GameEngine.LaunchWindow(width, height, title.data());

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

	// Prefer the XDG copy; fall back to the legacy CWD-relative config.ini
	// (old versions scattered one per launch directory).
	{
		const std::string XDG_CFG = EditorUtils::GameConfigPath();
		bool b_Loaded = GameConfig::GetInstance().m_bLoadFromFile(XDG_CFG);
		if (!b_Loaded && XDG_CFG != "config.ini")
		{
			b_Loaded = GameConfig::GetInstance().m_bLoadFromFile("config.ini");
		}
		if (b_Loaded)
		{
			const auto& CONFIG = GameConfig::GetInstance().GetWindowConfig();
			m_SceneSettings.m_SceneWidth = CONFIG.scene_width;
			m_SceneSettings.m_SceneHeight = CONFIG.scene_height;
			m_SceneSettings.m_TargetFPS = CONFIG.scene_fps;
		}
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

	m_RaylibTexture = LoadRenderTexture
	(
		m_SceneSettings.m_SceneWidth,
		m_SceneSettings.m_SceneHeight
	);
	m_DisplayTexture = LoadRenderTexture
	(
		m_SceneSettings.m_SceneWidth,
		m_SceneSettings.m_SceneHeight
	);

	SetTextureFilter
	(
		m_RaylibTexture.texture, TEXTURE_FILTER_BILINEAR
	);
	SetTextureFilter
	(
		m_DisplayTexture.texture, TEXTURE_FILTER_BILINEAR
	);

	m_OpaqueShader = LoadOpaqueShader();
}

void GameEditor::RunBrowser()
{
    Texture2D logo = LoadTexture(ThemeService::GetEngineContentPath("icon.png").c_str());

    char new_project_name[128] = "MyNewGame";
    char new_project_location[512] = "";
    int selected_template_idx = 0;
    std::vector<std::string> templates;

    // Manifest names change only when a project is renamed/removed; parsing
    // every recent manifest each frame spams disk I/O (and used to print).
    std::map<std::string, std::string> recent_display_names;

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
        constexpr float c_MARGIN = 48.0f;
        ImGui::SetNextWindowPos(ImVec2(c_MARGIN, c_MARGIN), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(viewport->Size.x - (c_MARGIN * 2.0f), viewport->Size.y - (c_MARGIN * 2.0f)));
        ImGui::SetNextWindowViewport(viewport->ID);

        ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(40.0f, 40.0f));
        ImGui::Begin("Project Browser", nullptr, window_flags);

        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        ImGuiIO& io = ImGui::GetIO();
        float content_left = ImGui::GetCursorScreenPos().x;
        float content_width = ImGui::GetContentRegionAvail().x;

        // ── Header ─────────────────────────────────────────────────────
        if (logo.id != 0)
        {
            float logo_y = ImGui::GetCursorPosY() + 6.0f;
            ImGui::SetCursorPosY(logo_y);
            auto lw = static_cast<float>(logo.width);
            auto lh = static_cast<float>(logo.height);
            constexpr float c_MAX_DIM = 96.0f;
            float scale = (lw > lh) ? c_MAX_DIM / lw : c_MAX_DIM / lh;
            float display_h = lh * scale;
            rlImGuiImageSize(&logo, static_cast<int>(lw * scale), static_cast<int>(display_h));
            ImGui::SameLine();
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 12.0f);
            float text_group_h = (ImGui::GetTextLineHeight() * 1.5f) + ImGui::GetStyle().ItemSpacing.y + ImGui::GetTextLineHeight();
            ImGui::SetCursorPosY(logo_y + ((display_h - text_group_h) * 0.5f));
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
        float line_y = ImGui::GetCursorScreenPos().y + 12.0f;
        draw_list->AddRectFilled(
            ImVec2(content_left, line_y),
            ImVec2(content_left + content_width, line_y + 1.0f),
            ImGui::GetColorU32(ImGuiCol_Separator)
        );
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 24.0f);

        // ── Two columns ────────────────────────────────────────────────
        ImGui::Columns(2, "BrowserColumns", false);
        ImGui::SetColumnWidth(0, content_width * 0.6f);

        // ── Left Column — Recent Projects ──────────────────────────────
        ImGui::PushFont(io.Fonts->Fonts[Font_Large]);
        ImGui::Text("Recent Projects");
        ImGui::PopFont();
        ImGui::Spacing();

        auto recent = ProjectManager::GetRecent();
        ImGui::BeginChild("RecentProjects", ImVec2(0, 0), 1);

        if (recent.empty())
        {
            float child_h = ImGui::GetContentRegionAvail().y;
            float child_w = ImGui::GetContentRegionAvail().x;
            ImGui::SetCursorPos(ImVec2(child_w * 0.1f, child_h * 0.35f));
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
                std::string display_name = fs_path.filename().string();

                if (exists)
                {
                    auto cached = recent_display_names.find(path);
                    if (cached != recent_display_names.end())
                    {
                        display_name = cached->second;
                    }
                    else
                    {
                        t_Project proj;
                        if (proj.m_bLoadFromFile(manifest_path.string()))
                        {
                            display_name = proj.m_Name;
                        }
                        recent_display_names[path] = display_name;
                    }
                }
                else
                {
                    recent_display_names.erase(path);
                }

                constexpr float c_ROW_HEIGHT = 48.0f;
                float avail_w = ImGui::GetContentRegionAvail().x;
                ImVec2 row_pos = ImGui::GetCursorScreenPos();

                if (!exists)
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
                    display_name += " (missing)";
                }

                // Clickable row
                if (ImGui::Selectable("##recent_proj", false, exists ? 0 : ImGuiSelectableFlags_Disabled, ImVec2(avail_w, c_ROW_HEIGHT)))
                {
                    if (exists) OpenProject(path);
                }

                bool is_hovered = ImGui::IsItemHovered();

                // Folder icon
                ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
                ImVec2 icon_pos(row_pos.x + 12.0f, row_pos.y + ((c_ROW_HEIGHT - 12.0f) * 0.5f));
                ImGui::SetCursorScreenPos(icon_pos);
                ImGui::Text(ICON_FA_FOLDER);
                ImGui::PopStyleColor();

                // Project name
                ImVec2 name_pos(row_pos.x + 40.0f, row_pos.y + 5.0f);
                ImGui::SetCursorScreenPos(name_pos);
                ImGui::Text("%s", display_name.c_str());

                // Path
                ImVec2 path_pos(row_pos.x + 40.0f, row_pos.y + 25.0f);
                ImGui::SetCursorScreenPos(path_pos);
                ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
                ImGui::Text("%s", path.c_str());
                ImGui::PopStyleColor();

                if (!exists) ImGui::PopStyleColor();

                // Trash button on hover
                if (is_hovered)
                {
                    ImGui::SetCursorScreenPos(ImVec2(row_pos.x + avail_w - 36.0f, row_pos.y + ((c_ROW_HEIGHT - 24.0f) * 0.5f)));
                    if (ImGui::Button(ICON_FA_TRASH_CAN))
                    {
                        ImGui::OpenPopup("RemoveRecentPopup");
                    }
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

                ImGui::SetCursorScreenPos(ImVec2(row_pos.x, row_pos.y + c_ROW_HEIGHT));
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
            templates = ProjectManager::GetAvailableTemplates();
            ImGui::OpenPopup("New Project Wizard");
        }
        ImGui::PopStyleColor(3);

        ImGui::Spacing();

        // Open Existing — secondary
        if (ImGui::Button(ICON_FA_FOLDER_OPEN "  Open Existing Project", ImVec2(-1, 44.0f)))
        {
            const std::string DIALOG_DIR = EditorUtils::DefaultDialogDir();
            const char* path = tinyfd_selectFolderDialog("Open Project", DIALOG_DIR.c_str());
            if (path != nullptr)
            {
                OpenProject(path);
            }
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
            ImGui::InputText("##new_name", new_project_name, sizeof(new_project_name));

            std::string sanitized = ProjectManager::SanitizeCMakeProjectName(new_project_name);
            if (sanitized != new_project_name && strlen(new_project_name) > 0)
            {
                ImGui::TextDisabled("Will be created as: %s", sanitized.c_str());
            }

            ImGui::Spacing();

            ImGui::Text("Location");
            ImGui::InputText("##new_location", new_project_location, sizeof(new_project_location));
            ImGui::SameLine();
            if (ImGui::Button("Browse..."))
            {
                const std::string DIALOG_DIR = EditorUtils::DefaultDialogDir();
                const char* folder = tinyfd_selectFolderDialog("Select Project Location", DIALOG_DIR.c_str());
                if (folder != nullptr)
                {
                    strncpy(new_project_location, folder, sizeof(new_project_location) - 1);
                    new_project_location[sizeof(new_project_location) - 1] = '\0';
                }
            }

            ImGui::Spacing();

            if (!templates.empty())
            {
                if (ImGui::BeginCombo("Template", templates[selected_template_idx].c_str()))
                {
                    for (int i = 0; i < static_cast<int>(templates.size()); ++i)
                    {
                        const bool IS_SELECTED = (selected_template_idx == i);
                        if (ImGui::Selectable(templates[i].c_str(), IS_SELECTED))
                            selected_template_idx = i;
                        if (IS_SELECTED)
                            ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
            }
            else
            {
                ImGui::TextColored(ImVec4(1, 0, 0, 1), "No templates found in dist/Templates/");
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            if (ImGui::Button("Create", ImVec2(120, 0)))
            {
                if (strlen(new_project_name) > 0 && strlen(new_project_location) > 0 && !templates.empty())
                {
                    fs::path full_path = fs::path(new_project_location) / new_project_name;
                    if (ProjectManager::b_CreateProject(full_path.string(), templates[selected_template_idx]))
                    {
                        OpenProject(full_path.string());
                        ImGui::CloseCurrentPopup();
                    }
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

    if (logo.id != 0) UnloadTexture(logo);
}

void GameEditor::OpenProject(std::string_view folder_path)
{
    // 1. Unload old DLL and reset map state (single teardown path in the loader)
    m_LogicLoader.Unload();

    // 2. StateBag is scoped to hot-reloads (local in the loader). Game state lives
    //    inside the DLL's MapManager, which was destroyed and unloaded in step 1.

    // 3. Open project metadata
    if (!ProjectManager::b_OpenProject(folder_path)) return;

    // Set Window Title
    std::string window_title = "RayWaves — " + ProjectManager::GetCurrent().m_Name;
    SetWindowTitle(window_title.c_str());

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
    s_LayoutPath = (proj_dir / ".raywaves" / "layout.ini").string();
    ImGui::GetIO().IniFilename = s_LayoutPath.c_str();

    if (std::filesystem::exists(s_LayoutPath))
    {
        ImGui::LoadIniSettingsFromDisk(s_LayoutPath.c_str());
    }
    else
    {
        LoadEditorDefaultIni();
        ImGui::GetIO().IniFilename = s_LayoutPath.c_str();
    }
}

void GameEditor::CleanupProject()
{
	m_LogicLoader.Unload();
	m_LogicLoader.SetGameLogicPath("");
}

void GameEditor::CloseProject()
{
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
		if (m_bCloseRequested) CloseWindow();

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
        m_GameEngine.SetViewportSize(m_SceneSettings.m_SceneWidth, m_SceneSettings.m_SceneHeight);
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
			if (m_DisplayTexture.id != 0) UnloadRenderTexture(m_DisplayTexture);

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
			Rectangle src =
			{
				0,
				0,
				static_cast<float>(m_SourceTexture.width),
				-static_cast<float>(m_SourceTexture.height)
			};
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
				std::filesystem::path dir = std::filesystem::path(EditorPreferences::GetInstance().GetConfigPath()).parent_path();
				s_LayoutPath = (dir / "editor_layout.ini").string();
			}

			ImGui::GetIO().IniFilename = s_LayoutPath.c_str();
		}

		Profiler::Get().NextFrame();

		rlImGuiBegin();

		ImGui::DockSpaceOverViewport(0, m_Viewport);

        for (auto& panel : m_Panels)
        {
            panel->Draw(this);
        }

        if (m_bShowTerminal)
        {
            m_Terminal.show(ICON_FA_TERMINAL " Console", &m_bShowTerminal);
        }

		rlImGuiEnd();
		EndDrawing();
	}

	Close();
}

void GameEditor::Close()
{
	t_WindowConfig& config = GameConfig::GetInstance().GetWindowConfig();
	config.scene_width = m_SceneSettings.m_SceneWidth;
	config.scene_height = m_SceneSettings.m_SceneHeight;
	config.scene_fps = m_SceneSettings.m_TargetFPS;
	{
		const std::string XDG_CFG = EditorUtils::GameConfigPath();
		const std::filesystem::path PARENT_DIR = std::filesystem::path(XDG_CFG).parent_path();
		if (!PARENT_DIR.empty())
		{
			std::error_code ec;
			std::filesystem::create_directories(PARENT_DIR, ec);
		}
		GameConfig::GetInstance().m_bSaveToFile(XDG_CFG);
	}

	if (ProjectManager::b_HasOpenProject())
	{
		auto& prj = ProjectManager::GetCurrent();
		prj.m_SceneWidth = m_SceneSettings.m_SceneWidth;
		prj.m_SceneHeight = m_SceneSettings.m_SceneHeight;
		prj.m_TargetFPS = m_SceneSettings.m_TargetFPS;
		ProjectManager::b_SaveCurrentProject();
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
    auto err_pos = line.find("error:");
    if (err_pos == std::string_view::npos)
	{
		err_pos = line.find("error C");
	}
    if (err_pos == std::string_view::npos)
	{
		err_pos = line.find("FAILED:");
	}

    auto warn_pos = line.find("warning:");
	if (warn_pos == std::string_view::npos)
	{
		warn_pos = line.find("warning C");
	}

    if (err_pos != std::string_view::npos || warn_pos != std::string_view::npos)
    {
        FBuildMessage msg;
        msg.Severity = (err_pos != std::string_view::npos) ? FBuildMessage::ESeverity::Error : FBuildMessage::ESeverity::Warning;
        msg.Text = std::string(line);

        size_t keyword_pos = (err_pos != std::string_view::npos) ? err_pos : warn_pos;
        std::string_view prefix = line.substr(0, keyword_pos);

        size_t last_colon = prefix.find_last_of(':');
        size_t last_paren = prefix.find_last_of(')');

        if (last_paren != std::string_view::npos && last_paren > 0)
        {
            size_t open_paren = prefix.find_last_of('(', last_paren);
            if (open_paren != std::string_view::npos)
            {
                std::string_view line_str = prefix.substr(open_paren + 1, last_paren - open_paren - 1);
                msg.Line = std::atoi(std::string(line_str).c_str());
                msg.File = std::string(prefix.substr(0, open_paren));
            }
        }
        else if (last_colon != std::string_view::npos)
        {
            size_t second_last_colon = prefix.find_last_of(':', last_colon > 0 ? last_colon - 1 : 0);
            if (second_last_colon != std::string_view::npos)
            {
                std::string_view line_str = prefix.substr(second_last_colon + 1, last_colon - second_last_colon - 1);
                msg.Line = std::atoi(std::string(line_str).c_str());
                msg.File = std::string(prefix.substr(0, second_last_colon));
            }
        }

        std::scoped_lock lock(BuildMessagesMutex);
        BuildMessages.push_back(msg);
    }
}

void GameEditor::CompileGameLogic()
{
    b_IsCompiling = true;
    b_IsPlaying = false;

    m_bShowTerminal = true;
    m_Terminal.add_text("Starting build process...", term::Severity::Debug);

    BuildStatus = EBuildStatus::Compiling;
    {
        std::scoped_lock lock(BuildMessagesMutex);
        BuildMessages.clear();
    }

    std::string build_cmd;
    std::string app_dir =
        std::filesystem::path(GetHostExePath()).parent_path().string();

    if (ProjectManager::b_HasOpenProject())
    {
        const auto& proj = ProjectManager::GetCurrent();
        std::filesystem::path raywaves_dir = std::filesystem::path(proj.m_RootPath) / ".raywaves";
        std::string path_str = raywaves_dir.string();
        if (!EditorUtils::IsShellSafe(path_str))
        {
            build_cmd = "echo ERROR: Project path contains unsafe characters.";
        }
        else
        {
            // System cmake from PATH: `sh -c` resolves it.
            // Project folders are portable, but CMake caches contain absolute paths.
            // Try normal configure first, if it fails (e.g. moved project), fallback to --fresh.
            build_cmd = "cd \"" + path_str + "\" && (cmake -G Ninja . -B build || cmake --fresh -G Ninja . -B build) && cmake --build build --config Release";
        }
    }
    else
    {
        // Dev environment or no project fallback: build GameLogic in-tree.
        build_cmd = "cmake --build \"" + app_dir + "\" --target GameLogic";
    }

    m_Terminal.add_text("Executing: " + build_cmd, term::Severity::Debug);

    auto cancel = m_ThreadCancelFlag;
    // A previous build thread (finished or not) must be joined before reuse.
    if (m_BuildThread.joinable())
    {
        m_BuildThread.join();
    }
    m_BuildThread = ProcessRunner::RunBuildCommand
    (
        build_cmd,
        [this, cancel](std::string_view line, bool is_error)
        {
            if (cancel->load()) return;
            ParseBuildLine(line);
            m_Terminal.add_text(line, is_error ? term::Severity::Error : term::Severity::Debug);
        },
        [this, cancel](bool success)
        {
            if (cancel->load()) return;
            if (success)
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
        }
    );
}
