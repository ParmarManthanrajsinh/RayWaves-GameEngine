#include "../Engine/MapManager.h"
#include "../Game/DllLoader.h"
#include "GameEditor.h"
#include "ProcessRunner.h"
#include <imgui/imgui_stdlib.h>
#include <imgui_internal.h>
#include <filesystem>
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
#include <memory>

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
		UnloadDll(m_GameLogicDll);
		m_GameLogicDll = {};
		m_CreateGameMap = nullptr;
	}

	// Save editor preferences on exit
	EditorPreferences::GetInstance().m_bSaveToFile();
}

void GameEditor::Init(int width, int height, std::string_view title)
{
	m_GameEngine.LaunchWindow(width, height, title.data());
	SetWindowState(FLAG_WINDOW_RESIZABLE);
	
	// Set window icon
	Image icon = LoadImage("Assets/EngineContent/icon.png");
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

	std::string base_font = "Assets/EngineContent/Roboto-Regular.ttf";
	std::string mono_font = "Assets/EngineContent/Consolas-Regular.ttf";
	if (prefs.FontFamily == "Consolas")
	{
		base_font = mono_font;
	}

	SetEngineTheme(*selected_preset, prefs.GuiScale, base_font, mono_font);

    // Layout persistence
	static std::string s_LayoutPath;
	s_LayoutPath = EditorPreferences::GetInstance().GetConfigPath();
	std::filesystem::path dir = std::filesystem::path(s_LayoutPath).parent_path();
	std::filesystem::path layout_file = dir / "editor_layout.ini";

	if (std::filesystem::exists(layout_file))
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

void GameEditor::Run()
{
	while (!WindowShouldClose())
	{
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
			
			static std::string s_LayoutPath = EditorPreferences::GetInstance().GetConfigPath();
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



void GameEditor::LoadMap(std::unique_ptr<GameMap>& game_map)
{
    if (game_map)
    {
        // Check if the loaded map is a MapManager
        MapManager* map_manager = static_cast<MapManager*>(game_map.get());
        if (map_manager)
        {
            // If it's a MapManager, set it using the dedicated method
            std::unique_ptr<MapManager> owned_map_manager
			(
				static_cast<MapManager*>(game_map.release())
			);

            m_GameEngine.SetMapManager(std::move(owned_map_manager));
            
            // Store reference for map selection UI
            m_MapManager = m_GameEngine.GetMapManager();
        }
        else
        {
            // Otherwise, use the regular SetMap method
            m_GameEngine.SetMap(std::move(game_map));
            m_MapManager = nullptr; // No MapManager available
        }
    }
    else
    {
        m_GameEngine.SetMap(std::make_unique<GameMap>());
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

	if (!new_factory)
	{
		std::cerr << "Failed to get CreateGameMap from DLL" << "\n";
		UnloadDll(new_dll);
		return false;
	}

	// 3) Create the new map before disturbing current state
	std::unique_ptr<GameMap> new_map(new_factory());
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
		UnloadDll(m_GameLogicDll);
		m_GameLogicDll = {};
		m_CreateGameMap = nullptr;
	}

	// 6) Swap in new DLL and map
	m_GameLogicDll = new_dll;
	m_CreateGameMap = new_factory;
	
	// Check if the loaded map is a MapManager
	MapManager* map_manager = static_cast<MapManager*>(new_map.get());
	if (map_manager)
	{
		// If it's a MapManager, set it using the dedicated method
		std::unique_ptr<MapManager> owned_map_manager
		(
			static_cast<MapManager*>(new_map.release())
		);
		m_GameEngine.SetMapManager(std::move(owned_map_manager));
		
		// Store reference for map selection UI
		m_MapManager = m_GameEngine.GetMapManager();
	}
	else
	{
		// Otherwise, use the regular SetMap method
		m_GameEngine.SetMap(std::move(new_map));
		m_MapManager = nullptr; // No MapManager available
	}

	// Update watched timestamp 
	// (watch the original DLL path, not the shadow)
	std::error_code ec;

	m_LastLogicWriteTime =
	fs::last_write_time
	(
		fs::path(m_GameLogicPath),	
		ec
	);

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

    std::string appDir = GetApplicationDirectory();
    std::string buildCmd;
    
    // Check if we are running the distributed version (where build_gamelogic.bat exists in root)
    if (std::filesystem::exists(appDir + "/build_gamelogic.bat"))
    {
        // Enclose in extra quotes so cmd.exe /C handles spaces correctly
        buildCmd = "\"\"" + appDir + "/build_gamelogic.bat\" nopause\"";
    }
    else
    {
        buildCmd = "\"\"cmake\" --build \"" + appDir + "\" --target GameLogic\"";
    }
    
    ProcessRunner::RunBuildCommand
    (
        buildCmd.c_str(),
        [this](const std::string_view line, bool isError) 
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
