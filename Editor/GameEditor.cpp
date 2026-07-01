#include "../Engine/MapManager.h"
#include "../Engine/ProjectManager.h"
#include "../Engine/AssetResolver.h"
#include "../Game/DllLoader.h"
#include "GameEditor.h"
#include "ProcessRunner.h"
#include <imgui/imgui_stdlib.h>
#include <imgui_internal.h>
#include <filesystem>
#include <cstdio>
using Clock = std::chrono::steady_clock;

static std::string GetEngineContentPath(std::string_view sub_path)
{
    return std::string(GetApplicationDirectory()) + "Assets/EngineContent/" + std::string(sub_path);
}

#include "Panels/MainMenuBar.h"
#include "Panels/SceneWindow.h"
#include "Panels/MapSelectionPanel.h"
#include "Panels/ExportPanel.h"
#include "Panels/SceneSettingsPanel.h"
#include "Panels/PerformanceOverlay.h"
#include "Panels/MessageLogPanel.h"
#include "Panels/EditorPreferencesPanel.h"
#include "EditorPreferences.h"
#include <memory>
#include <cstdlib>

bool g_bNeedsTextureRecreate = false;

GameEditor::GameEditor()
	: m_Viewport(nullptr),
	  m_RaylibTexture({ 0 }),
	  m_DisplayTexture({ 0 }),
	  m_SourceTexture({ 0 , 0 }),
	  b_IsPlaying(false),
	  b_IsCompiling(false),
	  m_GameLogicDll{},
	  m_CreateGameMap(nullptr),
	  m_DestroyGameMap(nullptr),
	  m_OpaqueShader({ 0 }),
	  m_MapManager(nullptr),
	  m_bShowPerformanceStats(false),
	  m_FrameTimes{},
	  m_FrameOffset(0)
{
    m_Terminal.InitCapture();
    
	m_Panels.reserve(7);
    m_Panels.push_back(std::make_unique<MainMenuBar>());
    m_Panels.push_back(std::make_unique<MapSelectionPanel>());
    m_Panels.push_back(std::make_unique<ExportPanel>());
    m_Panels.push_back(std::make_unique<SceneSettingsPanel>());
    m_Panels.push_back(std::make_unique<SceneWindow>());
    m_Panels.push_back(std::make_unique<PerformanceOverlay>());
    m_Panels.push_back(std::make_unique<MessageLogPanel>());
    m_Panels.push_back(std::make_unique<EditorPreferencesPanel>());
}

GameEditor::~GameEditor()
{
	/* 
		Ensure any GameMap instance(potentially from the DLL) is destroyed
		BEFORE unloading the DLL, otherwise vtable/function code may be gone
		when the map's destructor runs.
	*/
	m_MapManager = nullptr; 
	m_GameEngine.SetMap(nullptr);
	m_GameEngine.SetMapManager(nullptr);

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

	if (m_GameLogicDll.handle)
	{
		if (m_DestroyGameMap && m_MapManager)
		{
			m_DestroyGameMap(m_MapManager);
		}
		UnloadDll(m_GameLogicDll);
		m_GameLogicDll = {};
		m_CreateGameMap = nullptr;
		m_DestroyGameMap = nullptr;
	}

	// Save editor preferences on exit
	EditorPreferences::GetInstance().m_bSaveToFile();
}

void GameEditor::Init(int width, int height, std::string_view title)
{
	m_GameEngine.LaunchWindow(width, height, title.data());
	SetWindowState(FLAG_WINDOW_RESIZABLE);
	
	// Set window icon
	Image icon = LoadImage(GetEngineContentPath("icon.png").c_str());
	if (icon.data != nullptr)
	{
		SetWindowIcon(icon);
		UnloadImage(icon);
		std::println("Window icon loaded successfully from Assets / icon.png");
	} 
	else
	{
		std::println("Failed to load icon from Assets/icon.png");
	}
	
	rlImGuiSetup(true);
	
	// Load Editor Preferences
	EditorPreferences::GetInstance().m_bLoadFromFile();
	const auto& prefs = EditorPreferences::GetInstance().GetPreferences();

	const FThemePreset* selected_preset = &GetThemePresets()[0];
	for (const auto& preset : GetThemePresets())
	{
		if (preset.Name == prefs.ThemeName)
		{
			selected_preset = &preset;
			break;
		}
	}

	std::string base_font = GetEngineContentPath("Roboto-Regular.ttf");
	std::string mono_font = GetEngineContentPath("Consolas-Regular.ttf");
	if (prefs.FontFamily == "Consolas")
	{
		base_font = mono_font;
	}

	SetEngineTheme(*selected_preset, prefs.GuiScale, base_font, mono_font);

    // Layout persistence
	static std::string s_LayoutPath;
	std::filesystem::path dir = std::filesystem::path(EditorPreferences::GetInstance().GetConfigPath()).parent_path();
	s_LayoutPath = (dir / "editor_layout.ini").string();

	if (std::filesystem::exists(s_LayoutPath))
	{
		ImGui::GetIO().IniFilename = s_LayoutPath.c_str();
		ImGui::LoadIniSettingsFromDisk(s_LayoutPath.c_str());
	}
	else
	{
		LoadEditorDefaultIni();
		// Restore IniFilename since LoadEditorDefaultIni sets it to nullptr, 
		// enabling ImGui's native auto-save layout behavior (Option A)
		ImGui::GetIO().IniFilename = s_LayoutPath.c_str();
	}

	if (GameConfig::GetInstance().m_bLoadFromFile("config.ini"))
	{
		const auto& CONFIG = GameConfig::GetInstance().GetWindowConfig();
		m_SceneSettings.m_SceneWidth = CONFIG.scene_width;
		m_SceneSettings.m_SceneHeight = CONFIG.scene_height;
		m_SceneSettings.m_TargetFPS = CONFIG.scene_fps;
	}

	SetTargetFPS(60);
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
    Texture2D logo = LoadTexture(GetEngineContentPath("logo.png").c_str());

    char newProjectName[128] = "MyNewGame";
    char newProjectLocation[512] = "";
    int selectedTemplateIdx = 0;
    std::vector<std::string> templates;

    while (!WindowShouldClose())
    {
        if (ProjectManager::b_HasOpenProject())
        {
            break;
        }

        BeginDrawing();
        ClearBackground(RAYWHITE);
        
        rlImGuiBegin();

        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);
        
        ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
        
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(40.0f, 40.0f));
        ImGui::Begin("Project Browser", nullptr, window_flags);
        
        // Header
        ImGui::Text("RayWaves Game Engine");
        ImGui::Text("Version: %s", version.c_str());
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        ImGui::Columns(2, "BrowserColumns", false);
        ImGui::SetColumnWidth(0, viewport->WorkSize.x * 0.6f);
        
        // Left Column - Recent Projects
        ImGui::Text("Recent Projects");
        ImGui::Spacing();
        
        ImGui::BeginChild("RecentProjects", ImVec2(0, 0), true);
        auto recent = ProjectManager::GetRecent();
        for (const auto& path : recent)
        {
            if (ImGui::Selectable(path.c_str(), false, 0, ImVec2(0, 30.0f)))
            {
                OpenProject(path);
            }
        }
        ImGui::EndChild();

        
        ImGui::NextColumn();
        
        // Right Column - Actions
        ImGui::Text("Actions");
        ImGui::Spacing();
        
        if (ImGui::Button("Open Existing Project", ImVec2(-1, 40.0f)))
        {
            const char* path = tinyfd_selectFolderDialog("Open Project", nullptr);
            if (path)
            {
                OpenProject(path);
            }
        }
        
        ImGui::Spacing();
        
        if (ImGui::Button("New Project", ImVec2(-1, 40.0f)))
        {
            templates = ProjectManager::GetAvailableTemplates();
            ImGui::OpenPopup("New Project Wizard");
        }
        
        ImGui::Spacing();
        
        // Modal for New Project
        if (ImGui::BeginPopupModal("New Project Wizard", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::InputText("Project Name", newProjectName, sizeof(newProjectName));
            
            ImGui::InputText("Location", newProjectLocation, sizeof(newProjectLocation));
            ImGui::SameLine();
            if (ImGui::Button("Browse..."))
            {
                const char* folder = tinyfd_selectFolderDialog("Select Project Location", nullptr);
                if (folder)
                {
                    strncpy(newProjectLocation, folder, sizeof(newProjectLocation) - 1);
                }
            }
            
            if (!templates.empty())
            {
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
                if (strlen(newProjectName) > 0 && strlen(newProjectLocation) > 0 && !templates.empty())
                {
                    fs::path fullPath = fs::path(newProjectLocation) / newProjectName;
                    if (ProjectManager::b_CreateProject(fullPath.string(), templates[selectedTemplateIdx]))
                    {
                        OpenProject(fullPath.string());
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
        
        if (ImGui::Button("Open GitHub / Docs", ImVec2(-1, 40.0f)))
        {
            std::system("start https://github.com/ParmarManthanrajsinh/RayWaves-GameEngine");
        }
        
        ImGui::Columns(1);
        
        ImGui::End();
        ImGui::PopStyleVar();
        
        rlImGuiEnd();
        EndDrawing();
    }
    
    if (logo.id != 0) UnloadTexture(logo);
}

void GameEditor::OpenProject(std::string_view folderPath)
{
    // 1. Unload old DLL and reset map state
    if (m_GameLogicDll.handle)
    {
        if (m_DestroyGameMap && m_MapManager)
        {
            m_DestroyGameMap(m_MapManager);
        }
        m_MapManager = nullptr;
        m_GameEngine.SetMapManager(nullptr);
        m_GameEngine.SetMap(nullptr);
        UnloadDll(m_GameLogicDll);
        m_GameLogicDll = {};
        m_CreateGameMap = nullptr;
        m_DestroyGameMap = nullptr;
    }
    
    // 2. State is tied to Map/MapManager, so it is cleared by setting them to nullptr above.
    
    // 3. Open project metadata
    if (!ProjectManager::b_OpenProject(folderPath)) return;
    
    // 4. Set DLL path
    m_GameLogicPath = ProjectManager::GetCurrent().m_DllPath;
    
    // 5. Update AssetResolver
    AssetResolver::SetProjectAssetPath(ProjectManager::GetCurrent().m_AssetPath);
    
    // 6. Compile async
    CompileGameLogic();
    
    // 6. Config sync
    auto& prj = ProjectManager::GetCurrent();
    m_SceneSettings.m_SceneWidth = prj.m_SceneWidth;
    m_SceneSettings.m_SceneHeight = prj.m_SceneHeight;
    m_SceneSettings.m_TargetFPS = prj.m_TargetFPS;
}

void GameEditor::Run()
{
	while (!WindowShouldClose())
	{
		if (!ProjectManager::b_HasOpenProject())
		{
			// Cleanup current project state before opening browser
			m_GameEngine.SetMap(nullptr);
			m_GameEngine.SetMapManager(nullptr);
			if (m_DestroyGameMap && m_MapManager)
			{
				m_DestroyGameMap(m_MapManager);
			}
			m_MapManager = nullptr;
			if (m_GameLogicDll.handle)
			{
				UnloadDll(m_GameLogicDll);
				m_GameLogicDll = {};
				m_CreateGameMap = nullptr;
				m_DestroyGameMap = nullptr;
			}
			m_GameLogicPath = "";
			
			// Show browser
			RunBrowser();

			// If a new project was opened from the browser
			if (ProjectManager::b_HasOpenProject())
			{
				b_LoadGameLogic(ProjectManager::GetCurrent().m_DllPath);
				continue;
			}
			else
			{
				// User closed the window from the browser
				break;
			}
		}

		static auto s_LastReloadCheckTime = Clock::now();

		// Periodically check for GameLogic.dll changes (e.g., every 0.5s)
		const auto CURRENT_TIME = Clock::now();
		auto elapsed_time = std::chrono::duration<float>
		(
			CURRENT_TIME - s_LastReloadCheckTime
		).count();

		if (elapsed_time > 0.5f && !m_GameLogicPath.empty())
		{
			s_LastReloadCheckTime = CURRENT_TIME;
			std::error_code ec;

			// cache path once per check
			const fs::path PATH(m_GameLogicPath);

			auto now_write = fs::last_write_time(PATH, ec);

			if (!ec && now_write != m_LastLogicWriteTime)
			{
				if (m_LastLogicWriteTime != fs::file_time_type{})
				{
					b_ReloadGameLogic();
				}
				m_LastLogicWriteTime = now_write;
			}
		}

		UpdatePerformanceMetrics();

		float delta_time = GetFrameTime();
		if (b_IsPlaying)
		{
			m_GameEngine.UpdateMap(delta_time);
		}

		// Handle deferred texture recreation outside ImGui render loop to avoid OpenGL crashes
		extern bool g_bNeedsTextureRecreate;
		if (g_bNeedsTextureRecreate)
		{
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
			const auto& prefs = EditorPreferences::GetInstance().GetPreferences();
			const FThemePreset* selected_preset = &GetThemePresets()[0];
			for (const auto& preset : GetThemePresets())
			{
				if (preset.Name == prefs.ThemeName)
				{
					selected_preset = &preset;
					break;
				}
			}
			
			std::string base_font = "Assets/EngineContent/Roboto-Regular.ttf";
			std::string mono_font = "Assets/EngineContent/Consolas-Regular.ttf";
			if (prefs.FontFamily == "Consolas")
			{
				base_font = mono_font;
			}
			SetEngineTheme(*selected_preset, prefs.GuiScale, base_font, mono_font);
		}

		if (m_bNeedsLayoutReset)
		{
			m_bNeedsLayoutReset = false;
			LoadEditorDefaultIni();
			
			static std::string s_LayoutPath;
			std::filesystem::path dir = std::filesystem::path(EditorPreferences::GetInstance().GetConfigPath()).parent_path();
			s_LayoutPath = (dir / "editor_layout.ini").string();
			ImGui::GetIO().IniFilename = s_LayoutPath.c_str();
		}

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

void GameEditor::Close() const
{
	t_WindowConfig& config = GameConfig::GetInstance().GetWindowConfig();
	config.scene_width = m_SceneSettings.m_SceneWidth;
	config.scene_height = m_SceneSettings.m_SceneHeight;
	config.scene_fps = m_SceneSettings.m_TargetFPS;
	GameConfig::GetInstance().m_bSaveToFile("config.ini");

	UnloadRenderTexture(m_RaylibTexture);

	if (m_DisplayTexture.id != 0)
	{
		UnloadRenderTexture(m_DisplayTexture);
	}

	rlImGuiShutdown();
	CloseAudioDevice();
	CloseWindow();
}



void GameEditor::LoadMap(GameMap* game_map)
{
    if (game_map)
    {
        // Check if the loaded map is a MapManager using its internal name
        if (game_map->GetMapName() == "_RAYWAVES_MAP_MANAGER_")
        {
            MapManager* map_manager = static_cast<MapManager*>(game_map);
            // If it's a MapManager, set it using the dedicated method
            m_GameEngine.SetMapManager(map_manager);
            
            // Store reference for map selection UI
            m_MapManager = m_GameEngine.GetMapManager();
        }
        else
        {
            // Otherwise, use the regular SetMap method
            m_GameEngine.SetMap(game_map);
            m_MapManager = nullptr; // No MapManager available
        }
    }
    else
    {
        m_GameEngine.SetMap(nullptr);
        m_MapManager = nullptr;
    }
}

bool GameEditor::b_LoadGameLogic(std::string_view dll_path)
{
	m_GameLogicPath = dll_path.data() ? dll_path.data() : "";

	DllHandle new_dll = LoadDll(dll_path.data());
	if (!new_dll.handle)
	{
		std::cerr << "Failed to load GameLogic DLL: "
				  << dll_path
				  << "\n";

		return false;
	}

	// 2) Get factory
	CreateGameMapFunc new_factory =
	reinterpret_cast<CreateGameMapFunc>
	(
		GetDllSymbol(new_dll, "CreateGameMap")
	);

	DestroyGameMapFunc new_destroy =
	reinterpret_cast<DestroyGameMapFunc>
	(
		GetDllSymbol(new_dll, "DestroyGameMap")
	);

	if (!new_factory || !new_destroy)
	{
		std::cerr << "Failed to get CreateGameMap/DestroyGameMap from DLL" << "\n";
		UnloadDll(new_dll);
		return false;
	}

	// 3) Create the new map before disturbing current state
	GameMap* new_map = new_factory();
	if (!new_map)
	{
		std::cerr << "CreateGameMap returned null" << "\n";
		UnloadDll(new_dll);
		return false;
	}

	bool b_IsReload = (m_GameLogicDll.handle != nullptr);
	StateBag reload_state;

	if (b_IsReload && m_bPreserveStateOnReload)
	{
		try
		{
			if (m_GameEngine.GetMapManager())
			{
				m_GameEngine.GetMapManager()->SaveState(reload_state);
			}
			else if (m_GameEngine.GetMap())
			{
				m_GameEngine.GetMap()->SaveState(reload_state);
			}
		}
		catch (const std::exception& e)
		{
			m_Terminal.add_text(std::string("SaveState threw an exception: ") + e.what(), term::Severity::Error);
		}
		catch (...)
		{
			m_Terminal.add_text("SaveState threw an unknown exception", term::Severity::Error);
		}
	}

	// 4) Destroy current map to release old DLL code before unloading
	m_GameEngine.SetMap(nullptr);
	m_GameEngine.SetMapManager(nullptr);

	// 5) Unload old DLL (if any)
	if (m_GameLogicDll.handle)
	{
		if (m_DestroyGameMap && m_MapManager)
		{
			m_DestroyGameMap(m_MapManager);
		}
		UnloadDll(m_GameLogicDll);
		m_GameLogicDll = {};
		m_CreateGameMap = nullptr;
		m_DestroyGameMap = nullptr;
	}

	// 6) Swap in new DLL and map
	m_GameLogicDll = new_dll;
	m_CreateGameMap = new_factory;
	m_DestroyGameMap = new_destroy;
	
	// Check if the loaded map is a MapManager using its internal name
	if (new_map->GetMapName() == "_RAYWAVES_MAP_MANAGER_")
	{
		MapManager* map_manager = static_cast<MapManager*>(new_map);
		// If it's a MapManager, set it using the dedicated method
		m_GameEngine.SetMapManager(map_manager);
		
		// Store reference for map selection UI
		m_MapManager = m_GameEngine.GetMapManager();
	}
	else
	{
		// Otherwise, use the regular SetMap method
		m_GameEngine.SetMap(new_map);
		m_MapManager = nullptr; // No MapManager available
	}

	// Update watched timestamp 
	// (watch the original DLL path, not the shadow)
    std::error_code ec;
	m_LastLogicWriteTime = fs::last_write_time(fs::path(m_GameLogicPath), ec);

	if (b_IsReload && m_bPreserveStateOnReload)
	{
		try
		{
			if (m_GameEngine.GetMapManager())
			{
				m_GameEngine.GetMapManager()->LoadState(reload_state);
			}
			else if (m_GameEngine.GetMap())
			{
				m_GameEngine.GetMap()->LoadState(reload_state);
			}
		}
		catch (const std::exception& e)
		{
			m_Terminal.add_text(std::string("LoadState threw an exception: ") + e.what(), term::Severity::Error);
		}
		catch (...)
		{
			m_Terminal.add_text("LoadState threw an unknown exception", term::Severity::Error);
		}
	}

	return true;
}

bool GameEditor::b_ReloadGameLogic()
{
	if (m_GameLogicPath.empty())
	{
		return false;
	}

	bool b_WasPlaying = b_IsPlaying;
	b_IsPlaying = false;

	bool b_Ok = b_LoadGameLogic(m_GameLogicPath.c_str());
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

        std::lock_guard<std::mutex> lock(BuildMessagesMutex);
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
        std::lock_guard<std::mutex> lock(BuildMessagesMutex);
        BuildMessages.clear();
    }

    std::string buildCmd;
    std::string appDir = GetApplicationDirectory();

    if (ProjectManager::b_HasOpenProject())
    {
        const auto& proj = ProjectManager::GetCurrent();
        
        std::string zig_exe = appDir + "Core/Tools/zig/zig.exe";
        std::string zig_cmd = "zig"; // fallback
        if (std::filesystem::exists(zig_exe))
        {
            zig_cmd = "\"" + zig_exe + "\"";
        }

        std::string src_files;
        if (std::filesystem::exists(proj.m_SourcePath))
        {
            for (const auto& entry : std::filesystem::directory_iterator(proj.m_SourcePath))
            {
                if (entry.path().extension() == ".cpp")
                    src_files += "\"" + entry.path().string() + "\" ";
            }
        }
        std::string engine_src = appDir + "Core/Engine";
        if (std::filesystem::exists(engine_src))
        {
            for (const auto& entry : std::filesystem::directory_iterator(engine_src))
            {
                if (entry.path().extension() == ".cpp")
                    src_files += "\"" + entry.path().string() + "\" ";
            }
        }

        std::string includes = "-I\"" + appDir + "Core/Engine\" -I\"" + appDir + "Core/raylib/include\"";
        std::string libs = "-L\"" + appDir + "Core/raylib/lib\" -lraylib -ldwmapi";
        
        buildCmd = "\"\"" + zig_exe + "\" c++ -shared -o \"" + proj.m_DllPath + "\" " + src_files + includes + " " + libs + " -std=c++23 -msse4.2 -O2\"";
    }
    else
    {
        // Dev environment or no project fallback
        if (std::filesystem::exists(appDir + "/build_gamelogic.bat"))
        {
            buildCmd = "\"\"" + appDir + "/build_gamelogic.bat\" nopause\"";
        }
        else
        {
            buildCmd = "\"\"cmake\" --build \"" + appDir + "\" --target GameLogic\"";
        }
    }
    
    ProcessRunner::RunBuildCommand
    (
        buildCmd.c_str(),
        [this](std::string_view line, bool isError) 
        {
            ParseBuildLine(line);
            m_Terminal.add_text(line, isError ? term::Severity::Error : term::Severity::Debug);
        },
        [this](bool success) 
        {
            if (success) 
            {
                m_Terminal.add_text("Build Successful.", term::Severity::Debug);
                BuildStatus = EBuildStatus::Success;
                NotificationTimer = 4.0f;
				m_bNeedsReload = true; // Trigger reload after build completes
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
