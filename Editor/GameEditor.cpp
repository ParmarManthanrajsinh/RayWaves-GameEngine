#include "../Engine/MapManager.h"
#include "../Game/DllLoader.h"
#include "GameEditor.h"
#include "ProcessRunner.h"
#include <imgui/imgui_stdlib.h>
using Clock = std::chrono::steady_clock;

// Export Utility functions
static void s_fAppendLogLine
(
	std::vector<std::string>& logs, 
	std::mutex& mtx, 
	const std::string& line
);

static bool s_bfValidateExportFolder
(
	const std::string& out_dir, 
	std::vector<std::string>& logs, 
	std::mutex& mtx
);

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
    // Initialize standard output capture
    m_Terminal.InitCapture();
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
	rlImGuiReloadFonts();

	SetEngineTheme();
	LoadEditorDefaultIni();

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

		rlImGuiBegin();

		ImGui::DockSpaceOverViewport(0, m_Viewport);

		DrawMapSelectionUI();
        DrawExportPanel();
		DrawSceneSettingsPanel();
		DrawSceneWindow();
        DrawTerminal();
		DrawPerformanceOverlay();

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
	CloseWindow();
}

void GameEditor::DrawToolbarBackground()
{
	// Get current window's draw list
	ImDrawList* draw_list = ImGui::GetWindowDrawList();

	// Determine position and size of the toolbar
	ImVec2 toolbar_pos = ImGui::GetCursorScreenPos();
	ImVec2 toolbar_size = ImVec2
	(
		ImGui::GetContentRegionAvail().x, 40.0f
	); 

	// Define gradient colors
	ImU32 toolbar_color_top = IM_COL32(50, 50, 55, 255);
	ImU32 toolbar_color_bottom = IM_COL32(40, 40, 45, 255);

	// Draw gradient-filled rectangle
	draw_list->AddRectFilledMultiColor
	(
		toolbar_pos,
		ImVec2
		(
			toolbar_pos.x + toolbar_size.x,
			toolbar_pos.y + toolbar_size.y
		),
		toolbar_color_top,
		toolbar_color_top,
		toolbar_color_bottom,
		toolbar_color_bottom
	);
}

static void s_fDrawSpinner
(
	float radius, float thickness, const ImU32& color
)
{
	// Group all float variables together for cache locality
	alignas(16) struct 
	{
		float time;
		float start;
		float a_min;
		float a_max;
		float centre_x;
		float centre_y;
		float time_x8;
		float inv_num_segments;
		float angle_range;
	} vars{};

	ImVec2 pos = ImGui::GetCursorScreenPos();
	ImGui::Dummy(ImVec2(radius * 2, radius * 2));

	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	DrawList->PathClear();

	vars.time = static_cast<float>(ImGui::GetTime());
	constexpr int c_NUM_SEGMENTS = 30;
	vars.start = fabsf(sinf(vars.time * 1.8f) * (c_NUM_SEGMENTS - 5));

	// Precompute invariant values
	vars.inv_num_segments = 1.0f / static_cast<float>(c_NUM_SEGMENTS);
	vars.time_x8 = vars.time * 8.0f;

	vars.a_min = 3.14159f * 2.0f * vars.start * vars.inv_num_segments;
	vars.a_max = 3.14159f * 2.0f * static_cast<float>(c_NUM_SEGMENTS - 3) * vars.inv_num_segments;
	vars.angle_range = vars.a_max - vars.a_min;

	const ImVec2 CENTER = ImVec2(pos.x + radius, pos.y + radius);
	vars.centre_x = CENTER.x;
	vars.centre_y = CENTER.y;

	// Pre-allocate path memory
	for (int i = 0; i < c_NUM_SEGMENTS; ++i)
	{
		const float A = vars.a_min + (static_cast<float>(i) * vars.inv_num_segments) * vars.angle_range;

		const float ANGLE = A + vars.time_x8;

		DrawList->PathLineTo
		(
			ImVec2
			(
				vars.centre_x + cosf(ANGLE) * radius,
				vars.centre_y + sinf(ANGLE) * radius
			)
		);
	}

	DrawList->PathStroke(color, false, thickness);
}

static bool s_bIconButton
(
	std::string_view label, 
	std::string_view icon, 
	const ImVec2& size, 
	std::string_view tooltip
)
{
    ImGui::PushID(label.data());
    
    // Use FontAwesome font for the button
    // It should have been merged into the default font
    
    bool b_Clicked = ImGui::Button(icon.data(), size);
    
    if (ImGui::IsItemHovered())
    {
		ImGui::SetTooltip
		(
			"%.*s",
			static_cast<int>(tooltip.size()),
			tooltip.data()
		);
    }

    ImGui::PopID();
    return b_Clicked;
}

void GameEditor::DrawSceneWindow()
{
	ImGui::Begin("Scene");
	DrawToolbarBackground();

	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 4));
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 2));
	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);

	float toolbar_height = 40.0f;
	float button_height = 32.0f;
	float vertical_offset = (toolbar_height - button_height) / 2.0f;
	ImGui::SetCursorPosY(ImGui::GetCursorPosY() + vertical_offset);
	ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 5.0f);

    // Play/Pause
    if (b_IsPlaying)
	{
		if (s_bIconButton("pause_btn", ICON_FA_PAUSE, ImVec2(32, 32), "Pause"))
		{
			b_IsPlaying = false;
		}
	}
	else
	{
		if (s_bIconButton("play_btn", ICON_FA_PLAY, ImVec2(32, 32), "Play"))
		{
			b_IsPlaying = true;
		}
	}
	
	ImGui::SameLine();
	
	// Restart
	if 
	(
		s_bIconButton
		(
			"restart_btn", 
			ICON_FA_ARROW_ROTATE_RIGHT, 
			ImVec2(32, 32), 
			"Restart"
		) || IsWindowResized()
	)
	{
		b_IsPlaying = false;
		m_MapManager->b_ReloadCurrentMap();
	}

	ImGui::SameLine();
	ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 12);

    // Status Text
	const bool b_IS_PLAYING = b_IsPlaying;

	const ImVec4 COLOR = b_IsPlaying
		? ImVec4(0.2f, 0.8f, 0.2f, 1.0f)
		: ImVec4(0.8f, 0.2f, 0.2f, 1.0f);

	const std::string &ICON = b_IsPlaying ? ICON_FA_PLAY : ICON_FA_STOP;
	const std::string &LABEL = b_IsPlaying ? " PLAYING" : " STOPPED";

	float text_y_offset = 
	(
		(toolbar_height - ImGui::GetTextLineHeight()) * 0.5f
	) - vertical_offset - 2.0f;
	float base_cursor_y = ImGui::GetCursorPosY();

	// Icon
	ImGui::SetCursorPosY(base_cursor_y + text_y_offset);
	ImGui::TextColored(COLOR, "%s", ICON.c_str());
	ImGui::SameLine();
	
	ImGui::SetCursorPosY(base_cursor_y + text_y_offset - 1.0f);
	ImGui::TextColored(COLOR, "%s", LABEL.c_str());

	ImGui::SameLine();
	ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 12.0f);

    
    // Restore
	if 
	(
		s_bIconButton
		(
			"restore_btn", 
			ICON_FA_ARROW_ROTATE_LEFT, 
			ImVec2(32, 32), 
			"Reset Game"
		)
	)
	{
		b_IsPlaying = false;
		if (!b_ReloadGameLogic())
		{
			m_GameEngine.ResetMap();
		}
	}

	ImGui::SameLine();
	ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 5);

    // Clean
	if 
	(
		s_bIconButton
		(
			"clean_btn", 
			ICON_FA_TRASH_CAN, 
			ImVec2(32, 32), 
			"Delete Build Folder"
		)
	)
	{
		if (fs::exists("build"))
		{
			fs::remove_all("build");
		}
	}

	ImGui::SameLine();
	ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 5);

	// Performance Toggle
	const bool b_SHOW_STATS = m_bShowPerformanceStats;
	const ImVec4 STATS_COLOR = b_SHOW_STATS 
		? ImVec4(1.0f, 1.0f, 1.0f, 1.0f) 
		: ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
	ImGui::PushStyleColor(ImGuiCol_Text, STATS_COLOR);

	if 
	(
		s_bIconButton
		(
			"perf_btn", 
			ICON_FA_CHART_LINE, 
			ImVec2(32, 32), 
			"Performance Overlay"
		)
	)
	{
		m_bShowPerformanceStats = !m_bShowPerformanceStats;
	}

	ImGui::PopStyleColor();
	ImGui::SameLine();	
    
    // Terminal Toggle
    const bool b_SHOW_TERM = m_bShowTerminal;
    const ImVec4 TERM_COLOR = b_SHOW_TERM 
		? ImVec4(1.0f, 1.0f, 1.0f, 1.0f) 
		: ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, TERM_COLOR);
    
    if
	(
		s_bIconButton
		(
			"term_btn", 
			ICON_FA_TERMINAL, 
			ImVec2(32, 32), 
			"Debug Console"
		)
	) 
	{
        m_bShowTerminal = !m_bShowTerminal;
    }
    ImGui::PopStyleColor();
    ImGui::SameLine();

	// Compile
	float button_sz = 32.0f + ImGui::GetStyle().FramePadding.x * 2.0f;
	float status_sz = 0.0f;

	if (b_IsCompiling)
	{
		status_sz = 20.0f + ImGui::GetStyle().ItemSpacing.x + ImGui::CalcTextSize("Compiling...").x + ImGui::GetStyle().ItemSpacing.x;
	}

	float avail = ImGui::GetContentRegionAvail().x;
	float pos_x = ImGui::GetCursorPosX() + avail - button_sz - status_sz;

	if (pos_x > ImGui::GetCursorPosX())
	{
		ImGui::SetCursorPosX(pos_x);
	}

	if (b_IsCompiling)
	{
		float spinner_height = 20.0f;
		float spinner_y_offset = (toolbar_height - spinner_height) / 2.0f;
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() - vertical_offset + spinner_y_offset);

		s_fDrawSpinner(10.0f, 2.0f, ImGui::GetColorU32(ImVec4(0.2f, 0.8f, 0.2f, 1.0f)));
		
		ImGui::SameLine();
		float text_compile_y_offset = (toolbar_height - ImGui::GetTextLineHeight()) / 2.0f;
		ImGui::SetCursorPosY
		(
			ImGui::GetCursorPosY() - vertical_offset - 5 + text_compile_y_offset
		);
		ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "Compiling...");
		ImGui::SameLine();
	}

	bool b_Disabled = b_IsCompiling.load();
	if (b_Disabled)
	{
		ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
	}

	if (s_bIconButton("compile_btn", ICON_FA_HAMMER, ImVec2(32, 32), "Recompile"))
	{
		if (!b_Disabled)
		{
			b_IsCompiling = true;
			b_IsPlaying = false;
			if (!b_ReloadGameLogic()) m_GameEngine.ResetMap();

			m_bShowTerminal = true;
			m_Terminal.add_text("Starting build process...", term::Severity::Debug);

			ProcessRunner::RunBuildCommand
			(
				"build_gamelogic.bat nopause",
				[this](const std::string& line, bool isError)
				{
					m_Terminal.add_text
					(
						line, 
						isError ? term::Severity::Error : term::Severity::Debug
					);
				},
				[this](bool success)
				{
					if (success)
					{
						m_Terminal.add_text("Build Successful.", term::Severity::Debug);
					}
					else
					{
						m_Terminal.add_text("Build Failed.", term::Severity::Error);
					}
					b_IsCompiling = false;
				}
			);
		}
	}

	if (b_Disabled)
	{
		ImGui::PopStyleVar();
	}

	ImGui::PopStyleVar(3);

	rlImGuiImageRenderTextureFit
	(
		m_bUseOpaquePass ? &m_DisplayTexture : &m_RaylibTexture, 
		true
	);

	ImGui::End();
}

void GameEditor::DrawExportPanel()
{
    ImGui::Begin("Export", nullptr, ImGuiWindowFlags_NoCollapse);

    // Join previous worker if it finished to avoid accumulating threads
    if (!m_ExportState.m_bIsExporting && 
		 m_ExportState.m_ExportThread.joinable())
    {
        m_ExportState.m_ExportThread.join();
    }

    ImGui::Text("Export standalone game");
    ImGui::Separator();
    
    // Game Configuration
    ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);
    ImGui::Text("Game Configuration");
    ImGui::PopFont();
    ImGui::Separator();
    ImGui::Spacing();

    // Game Name
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Game Name:");
    ImGui::SameLine();
    ImGui::SetCursorPosX(120.0f);
	ImGui::InputText("##game_name", &m_ExportState.m_GameName);
    
    ImGui::SameLine();
    ImGui::TextDisabled("%s.exe", m_ExportState.m_GameName.c_str());

    ImGui::Spacing();
    ImGui::Spacing();

    ImGui::Text("Display Settings");
    ImGui::Separator();
    ImGui::Spacing();

    // Resolution Section
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Resolution:");
    ImGui::SameLine();
    ImGui::SetCursorPosX(120.0f);
    
    ImGui::PushItemWidth(80.0f);
    ImGui::InputInt("##width", &m_ExportState.m_WindowWidth, 0, 0);
    ImGui::PopItemWidth();
    
    ImGui::SameLine();
    ImGui::Text("Ã—");
    ImGui::SameLine();
    
    ImGui::PushItemWidth(80.0f);
    ImGui::InputInt("##height", &m_ExportState.m_WindowHeight, 0, 0);
    ImGui::PopItemWidth();
    
    ImGui::SameLine();
    ImGui::PushItemWidth(150.0f);
    if (ImGui::BeginCombo("##resolution_presets", "Presets"))
    {
        if (ImGui::Selectable("1920Ã—1080 (Full HD)")) 
		{
            m_ExportState.m_WindowWidth = 1920;
            m_ExportState.m_WindowHeight = 1080;
        }
        if (ImGui::Selectable("1600Ã—900 (HD+)")) 
		{
            m_ExportState.m_WindowWidth = 1600;
            m_ExportState.m_WindowHeight = 900;
        }
        if (ImGui::Selectable("1280Ã—720 (HD)")) 
		{
            m_ExportState.m_WindowWidth = 1280;
            m_ExportState.m_WindowHeight = 720;
        }
        if (ImGui::Selectable("1024Ã—768 (4:3)")) 
		{
            m_ExportState.m_WindowWidth = 1024;
            m_ExportState.m_WindowHeight = 768;
        }
        if (ImGui::Selectable("800Ã—600 (SVGA)")) 
		{
            m_ExportState.m_WindowWidth = 800;
            m_ExportState.m_WindowHeight = 600;
        }
        ImGui::EndCombo();
    }
    ImGui::PopItemWidth();
    
    ImGui::Spacing();

    // Window Options
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Window Mode:");
    ImGui::SameLine();
    ImGui::SetCursorPosX(130.0f);
    
    ImGui::Checkbox("Fullscreen", &m_ExportState.m_bFullscreen);
    ImGui::SameLine();
    ImGui::SetCursorPosX(260.0f);
    ImGui::Checkbox("Resizable", &m_ExportState.m_bResizable);

    ImGui::Spacing();

    ImGui::Text("Performance Settings");
    ImGui::Separator();
    ImGui::Spacing();

    // VSync
    ImGui::AlignTextToFramePadding();
    ImGui::Text("V-Sync:");
    ImGui::SameLine();
    ImGui::SetCursorPosX(120.0f);
    ImGui::Checkbox("##b_Vsync", &m_ExportState.m_bVSync);
    if (m_ExportState.m_bVSync) 
	{
        ImGui::SameLine();
        ImGui::TextDisabled("(Locks FPS to display refresh rate)");
    }

    // FPS Settings
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Target FPS:");
    ImGui::SameLine();
    ImGui::SetCursorPosX(120.0f);
    
    if (m_ExportState.m_bVSync) 
	{
        ImGui::BeginDisabled();
    }
    
    ImGui::PushItemWidth(80.0f);
    ImGui::InputInt("##target_fps", &m_ExportState.m_TargetFPS, 0, 0);
    ImGui::PopItemWidth();
    
    ImGui::SameLine();
    ImGui::PushItemWidth(100.0f);
    if (ImGui::BeginCombo("##fps_presets", "Presets"))
    {
        if (ImGui::Selectable("30 FPS")) m_ExportState.m_TargetFPS = 30;
        if (ImGui::Selectable("60 FPS")) m_ExportState.m_TargetFPS = 60;
        if (ImGui::Selectable("120 FPS")) m_ExportState.m_TargetFPS = 120;
        if (ImGui::Selectable("144 FPS")) m_ExportState.m_TargetFPS = 144;
        if (ImGui::Selectable("240 FPS")) m_ExportState.m_TargetFPS = 240;
        if (ImGui::Selectable("Unlimited")) m_ExportState.m_TargetFPS = 0;
        ImGui::EndCombo();
    }
    ImGui::PopItemWidth();
    
    if (m_ExportState.m_bVSync) 
	{
        ImGui::EndDisabled();
    }
    
    if (m_ExportState.m_TargetFPS == 0 && !m_ExportState.m_bVSync) 
	{
        ImGui::SameLine();
        ImGui::TextDisabled("(Unlimited)");
    }

    ImGui::Spacing();
    ImGui::Spacing();

    // Validation
	if (m_ExportState.m_WindowWidth < 320)
	{
		m_ExportState.m_WindowWidth = 320;
	}

	if (m_ExportState.m_WindowHeight < 240)
	{
		m_ExportState.m_WindowHeight = 240;
	}

	if (m_ExportState.m_WindowWidth > 7680)
	{
		m_ExportState.m_WindowWidth = 7680;
	}

	if (m_ExportState.m_WindowHeight > 4320)
	{
		m_ExportState.m_WindowHeight = 4320;
	}

	if (m_ExportState.m_TargetFPS < 0)
	{
		m_ExportState.m_TargetFPS = 0;
	}

	if (m_ExportState.m_TargetFPS > 1000)
	{
		m_ExportState.m_TargetFPS = 1000;
	}

    ImGui::Text("Export Settings");
    ImGui::Separator();
    ImGui::Spacing();

    // Export Location
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Output Folder:");
    ImGui::SameLine();
    ImGui::SetCursorPosX(120.0f);

	std::string& current_path = m_ExportState.m_ExportPath;
	if (current_path.empty())
	{
		current_path = "export";
	}

	ImGui::PushItemWidth(300.0f);
	ImGui::InputText
	(
		"##export_path",
		current_path.data(),
		current_path.capacity() + 1,
		ImGuiInputTextFlags_ReadOnly
	);
	ImGui::PopItemWidth();

    ImGui::SameLine();
    if (ImGui::Button("Browse", ImVec2(80.0f, 0)))
    {
        const char* selected_path = 
		tinyfd_saveFileDialog
		(
			"Select Export Folder",
			fs::current_path().string().c_str(), 
			0, 
			nullptr, 
			nullptr
		);

		fs::path parent_path = selected_path ? 
		fs::path(selected_path).parent_path() : fs::current_path();
		m_ExportState.m_ExportPath = parent_path.string();
    }

    ImGui::Spacing();
    ImGui::Spacing();
    
    // Warning message
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.2f, 1.0f));
    ImGui::TextWrapped
	(
		"Note: Close the editor before exporting to avoid file conflicts."
	);
    ImGui::PopStyleColor();
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Export Logic
    if (!m_ExportState.m_bIsExporting)
    {
        // Center the export button and make it prominent
        float button_width = 200.0f;
        float window_width = ImGui::GetContentRegionAvail().x;
        ImGui::SetCursorPosX
		(
			(window_width - button_width) * 0.5f
		);
        
        ImGui::PushStyleColor
		(
			ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 1.0f)
		);
        ImGui::PushStyleColor
		(
			ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.8f, 0.3f, 1.0f)
		);
        ImGui::PushStyleColor
		(
			ImGuiCol_ButtonActive, ImVec4(0.1f, 0.6f, 0.1f, 1.0f)
		);
        
        if (ImGui::Button("Start Export", ImVec2(button_width, 40.0f)))
        {
            m_ExportState.m_bIsExporting = true;
            m_ExportState.m_bCancelExport = false;
            m_ExportState.m_bExportSuccess = false;
            m_ExportState.m_ExportLogs.clear();

            // m_ExportState.m_ExportPath is now set directly from the UI

            m_ExportState.m_ExportThread = std::thread
			(
				[this]() 
				{
					// Create export directory if it doesn't exist
					fs::create_directories(m_ExportState.m_ExportPath);
					
					s_fAppendLogLine
					(
						m_ExportState.m_ExportLogs,
						m_ExportState.m_ExportLogMutex, 
						"Starting export process..."
					);
                
					fs::path current_path = fs::current_path();
                
					// Check if we're in a distribution (has app.exe but not full build system)
					bool b_IsDistribution = 
					fs::exists
					(
						current_path / "game.exe"
					) && 
					!fs::exists
					(
						current_path / "Game" / "game.cpp"
					);
                
					if (b_IsDistribution) 
					{
						s_fAppendLogLine
						(
							m_ExportState.m_ExportLogs,
							m_ExportState.m_ExportLogMutex, 
							"Distribution environment detected - using direct file copy..."
						);
                    
						// In distribution, just copy the existing runtime files
						fs::path app_exe = current_path / "game.exe";
						fs::path game_logic_dll = current_path / "GameLogic.dll";
						fs::path raylib_dll = current_path / "raylib.dll";
                    
                    if (!fs::exists(app_exe)) 
					{
                        s_fAppendLogLine
						(
							m_ExportState.m_ExportLogs,
							m_ExportState.m_ExportLogMutex, 
							"ERROR: app.exe not found in distribution!"
						);
                        m_ExportState.m_bExportSuccess = false;
                        m_ExportState.m_bIsExporting = false;
                        return;
                    }
                    
                    if (!fs::exists(game_logic_dll)) 
					{
                        s_fAppendLogLine
						(
							m_ExportState.m_ExportLogs,
							m_ExportState.m_ExportLogMutex, 
							"ERROR: GameLogic.dll not found in distribution!"
						);
                        m_ExportState.m_bExportSuccess = false;
                        m_ExportState.m_bIsExporting = false;
                        return;
                    }
                    
                    if (!fs::exists(raylib_dll)) 
					{
                        s_fAppendLogLine
						(
							m_ExportState.m_ExportLogs ,
							m_ExportState.m_ExportLogMutex, 
							"ERROR: raylib.dll not found in distribution!"
						);
                        m_ExportState.m_bExportSuccess = false;
                        m_ExportState.m_bIsExporting = false;
                        return;
                    }
                    
                    try 
					{
                        fs::path export_dir = 
							current_path / m_ExportState.m_ExportPath;

                        fs::create_directories(export_dir);
                        
                        // Use custom game name for the executable
                        std::string game_exe_name = 
							m_ExportState.m_GameName + ".exe";
                        s_fAppendLogLine
						(
							m_ExportState.m_ExportLogs,
							m_ExportState.m_ExportLogMutex, 
							"Creating game executable: " + game_exe_name
						);
                        fs::copy_file
						(
							app_exe, 
							export_dir / game_exe_name, 
							fs::copy_options::overwrite_existing
						);
                        
                        // Create game configuration file
                        s_fAppendLogLine
						(
							m_ExportState.m_ExportLogs,
							m_ExportState.m_ExportLogMutex, 
							"Creating game configuration..."
						);
                        
                        fs::path config_path = export_dir / "config.ini";
                        std::ofstream config_file(config_path.string());
						if (config_file.is_open())
						{
							GameConfig::GetInstance().ApplyExportSettings
							(
								m_ExportState.m_WindowWidth,
								m_ExportState.m_WindowHeight,
								m_ExportState.m_bFullscreen,
								m_ExportState.m_bResizable,
								m_ExportState.m_bVSync,
								m_ExportState.m_TargetFPS
							);

							config_file << GameConfig::GetInstance().GenerateConfigString();
							config_file.close();
						}
                        
                        s_fAppendLogLine
						(
							m_ExportState.m_ExportLogs,
							m_ExportState.m_ExportLogMutex, 
							"Copying GameLogic.dll..."
						);
                        fs::copy_file
						(
							game_logic_dll, 
							export_dir / "GameLogic.dll", 
							fs::copy_options::overwrite_existing
						);
                        
                        s_fAppendLogLine
						(
							m_ExportState.m_ExportLogs,
							m_ExportState.m_ExportLogMutex, 
							"Copying raylib.dll..."
						);
                        fs::copy_file
						(
							raylib_dll, 
							export_dir / "raylib.dll", 
							fs::copy_options::overwrite_existing
						);
                        
                        // Copy game assets (excluding EngineContent)
                        fs::path assets_dir = current_path / "Assets";
                        if (fs::exists(assets_dir)) 
                        {
                            s_fAppendLogLine
                            (
                                m_ExportState.m_ExportLogs,
                                m_ExportState.m_ExportLogMutex, 
                                "Copying game assets..."
                            );
                            
                            fs::path export_assets_dir = export_dir / "Assets";
                            fs::create_directories(export_assets_dir);
                            
                            for 
							(
								const auto& ENTRY : fs::directory_iterator(assets_dir)
							)
                            {
                                if (ENTRY.is_directory() && 
									ENTRY.path().filename()!="EngineContent")
                                {
                                    fs::path dest = export_assets_dir / ENTRY.path().filename();
                                    fs::copy
									(
										ENTRY.path(), 
										dest, 
                                        fs::copy_options::recursive | 
                                        fs::copy_options::overwrite_existing
									);
                                    
                                    s_fAppendLogLine
                                    (
                                        m_ExportState.m_ExportLogs,
                                        m_ExportState.m_ExportLogMutex, 
                                        "Copied asset folder: " + 
										ENTRY.path().filename().string()
                                    );
                                }
                                else if (ENTRY.is_regular_file())
                                {
                                    fs::path dest = export_assets_dir / ENTRY.path().filename();
                                    fs::copy_file(ENTRY.path(), dest, 
                                        fs::copy_options::overwrite_existing);
                                    
                                    s_fAppendLogLine
                                    (
                                        m_ExportState.m_ExportLogs,
                                        m_ExportState.m_ExportLogMutex, 
                                        "Copied asset file: " + 
										ENTRY.path().filename().string()
                                    );
                                }
                            }
                        }
                        else 
                        {
                            s_fAppendLogLine
                            (
                                m_ExportState.m_ExportLogs,
                                m_ExportState.m_ExportLogMutex, 
                                "No Assets folder found - skipping asset copy"
                            );
                        }
                        
                        s_fAppendLogLine
						(
							m_ExportState.m_ExportLogs,
							m_ExportState.m_ExportLogMutex, 
							"Export completed successfully!"
						);
                        m_ExportState.m_bExportSuccess = true;
                    } 
					catch (const std::exception& e) 
					{
                        s_fAppendLogLine
						(
							m_ExportState.m_ExportLogs,
							m_ExportState.m_ExportLogMutex, 
							std::string("ERROR: ") + e.what()
						);
                        m_ExportState.m_bExportSuccess = false;
                    }
                    
                    m_ExportState.m_bIsExporting = false;
                    return;
                }
                
                // Source environment - check for running processes and build
                s_fAppendLogLine
				(
					m_ExportState.m_ExportLogs,
					m_ExportState.m_ExportLogMutex, 
					"Source environment detected - checking for m_bIsExporting processes..."
				);
                
                // Check for running main.exe process
                std::stringstream check_cmd;
                check_cmd << "powershell -Command \"Get-Process -Name 'main' -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Path\"";
                
                FILE* check_pipe = _popen(check_cmd.str().c_str(), "r");
                if (check_pipe) 
				{
					std::array<char, 1024> buffer{};
                    bool b_FoundRunningProcess = false;
                    while 
					(
						fgets(buffer.data(), sizeof(buffer), check_pipe) != nullptr
					) 
					{
                        if(std::string_view{ buffer.data() }.contains("main.exe"))
						{
                            b_FoundRunningProcess = true;
                            s_fAppendLogLine
							(
								m_ExportState.m_ExportLogs, 
								m_ExportState.m_ExportLogMutex, 
								"WARNING: main.exe is currently m_bIsExporting. Export may fail."
							);
                            s_fAppendLogLine
							(
								m_ExportState.m_ExportLogs, 
								m_ExportState.m_ExportLogMutex, 
								"Please close the game editor before exporting for best results."
							);
                            break;
                        }
                    }

                    _pclose(check_pipe);
                    if (!b_FoundRunningProcess) 
					{
                        s_fAppendLogLine
						(
							m_ExportState.m_ExportLogs, 
							m_ExportState.m_ExportLogMutex, 
							"No conflicting processes found. Proceeding with build..."
						);
                    }
                }
                
                // Use simple export script that builds from source
                s_fAppendLogLine
				(
					m_ExportState.m_ExportLogs, 
					m_ExportState.m_ExportLogMutex, 
					"Building game runtime from source..."
				);

                // Add final log message about process completion
                s_fAppendLogLine
				(
					m_ExportState.m_ExportLogs, 
					m_ExportState.m_ExportLogMutex, 
					std::string
					(
						"Process completed. Validating export folder: "
					) + m_ExportState.m_ExportPath
				);
                
                // Validate
                bool b_Ok = s_bfValidateExportFolder
				(
					m_ExportState.m_ExportPath, 
					m_ExportState.m_ExportLogs, 
					m_ExportState.m_ExportLogMutex
				);
                m_ExportState.m_bExportSuccess = b_Ok;
                
                if (!b_Ok) 
				{
                    s_fAppendLogLine
					(
						m_ExportState.m_ExportLogs, 
						m_ExportState.m_ExportLogMutex, 
						"Export validation failed - check export folder contents"
					);
                }
                
                m_ExportState.m_bIsExporting = false;
            });
        }
        ImGui::PopStyleColor(3);
    }
    else
    {
        // Export in progress - show centered status
        float window_width = ImGui::GetContentRegionAvail().x;
        
        // Progress indicator
        ImGui::SetCursorPosX((window_width - 200.0f) * 0.5f);
        ImGui::PushStyleColor
		(
			ImGuiCol_Text, ImVec4(0.3f, 0.7f, 1.0f, 1.0f)
		);
        ImGui::Text("Export in progress...");
        ImGui::PopStyleColor();
        
        ImGui::Spacing();
        
        // Cancel button (smaller, centered)
        float cancel_width = 100.0f;
        ImGui::SetCursorPosX((window_width - cancel_width) * 0.5f);
        
        ImGui::PushStyleColor
		(
			ImGuiCol_Button, ImVec4(0.7f, 0.3f, 0.3f, 1.0f)
		);
        ImGui::PushStyleColor
		(
			ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.4f, 0.4f, 1.0f)
		);
        ImGui::PushStyleColor
		(
			ImGuiCol_ButtonActive, ImVec4(0.6f, 0.2f, 0.2f, 1.0f)
		);
        
        if (ImGui::Button("Cancel", ImVec2(cancel_width, 30.0f)))
        {
            m_ExportState.m_bCancelExport = true; 
        }
        ImGui::PopStyleColor(3);
    }

    ImGui::Spacing();

    // Status indicator (centered and more prominent)
    if (m_ExportState.m_bExportSuccess)
    {
        float window_width = ImGui::GetContentRegionAvail().x;
        ImGui::SetCursorPosX
		(
			(
				window_width - ImGui::CalcTextSize("Export Complete!").x
			) * 0.5f
		);
        ImGui::PushStyleColor
		(
			ImGuiCol_Text, ImVec4(0.2f, 0.8f, 0.2f, 1.0f)
		);
        ImGui::Text("Export Complete!");
        ImGui::PopStyleColor();
    }
    else if 
	(
		!m_ExportState.m_bIsExporting && 
		!m_ExportState.m_ExportLogs.empty() && 
		!m_ExportState.m_bIsExporting
	)
    {
        float window_width = ImGui::GetContentRegionAvail().x;
        ImGui::SetCursorPosX
		(
			(
				window_width - ImGui::CalcTextSize("Export Failed").x
			) * 0.5f
		);
        ImGui::PushStyleColor
		(
			ImGuiCol_Text, ImVec4(0.8f, 0.2f, 0.2f, 1.0f)
		);
        ImGui::Text("Export Failed");
        ImGui::PopStyleColor();
    }

    // === EXPORT LOG ===
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::Text("Export Log");
    ImGui::Separator();
    
    // Log container with better styling
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 5.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.1f, 0.1f, 0.1f, 0.8f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.4f, 0.4f, 0.4f, 0.5f));
    
    if (ImGui::BeginChild("export_log", ImVec2(0, 200), true))
    {
        std::scoped_lock lk(m_ExportState.m_ExportLogMutex);
        
        if (m_ExportState.m_ExportLogs.empty())
        {
            ImGui::PushStyleColor
			(
				ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f)
			);
            ImGui::Text("Export log will appear here...");
            ImGui::PopStyleColor();
        }
        else
        {
            for (const auto& LINE : m_ExportState.m_ExportLogs)
            {
                // Color code log messages ( Default white )
                ImVec4 text_color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f); 
                
                if (LINE.contains("ERROR:"))
                {	
					// Red
                    text_color = ImVec4(1.0f, 0.3f, 0.3f, 1.0f); 
                }
                else if (LINE.contains("WARNING:"))
                {
					// Yellow
                    text_color = ImVec4(1.0f, 0.8f, 0.3f, 1.0f); 
                }
                else if (LINE.contains("Completed") || 
                         LINE.contains("SUCCESS")	||
                         LINE.contains("Copied"))
                {
					// Green
                    text_color = ImVec4(0.3f, 1.0f, 0.3f, 1.0f); 
                }
                else if (LINE.contains("Building")  ||
                         LINE.contains("Creating")  ||
                         LINE.contains("Starting"))
                {
					// Blue
                    text_color = ImVec4(0.3f, 0.8f, 1.0f, 1.0f); 
                }
                
                ImGui::PushStyleColor(ImGuiCol_Text, text_color);
                ImGui::TextUnformatted(LINE.c_str());
                ImGui::PopStyleColor();
            }
            
            // Auto-scroll to bottom during export
            if (m_ExportState.m_bIsExporting)
            {
                ImGui::SetScrollHereY(1.0f);
            }
        }
    }
    ImGui::EndChild();
    
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);

    // Cleanup worker if finished
    if (!m_ExportState.m_bIsExporting &&
		 m_ExportState.m_ExportThread.joinable())
    {
        m_ExportState.m_ExportThread.join();
    }

    ImGui::End();
}

void GameEditor::DrawSceneSettingsPanel()
{
    ImGui::Begin("Scene Settings", nullptr, ImGuiWindowFlags_NoCollapse);

    ImGui::Text("Scene Resolution Settings");
    ImGui::Separator();
    ImGui::Spacing();

    // Store previous values to detect changes
    static int s_PrevWidth = m_SceneSettings.m_SceneWidth;
    static int s_PrevHeight = m_SceneSettings.m_SceneHeight;
	static int s_PrevTargetFPS = m_SceneSettings.m_TargetFPS;

    // Resolution Section
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Scene Resolution:");
    ImGui::SameLine();
    ImGui::SetCursorPosX(155.0f);
    
    ImGui::PushItemWidth(80.0f);
    ImGui::InputInt("##scene_width", &m_SceneSettings.m_SceneWidth, 0, 0);
    ImGui::PopItemWidth();
    
    ImGui::SameLine();
    ImGui::Text("Ã—");
    ImGui::SameLine();
    
    ImGui::PushItemWidth(80.0f);
    ImGui::InputInt("##scene_height", &m_SceneSettings.m_SceneHeight, 0, 0);
    ImGui::PopItemWidth();
    
    ImGui::SameLine();
    ImGui::PushItemWidth(150.0f);
    if (ImGui::BeginCombo("##scene_resolution_presets", "Presets"))
    {
        if (ImGui::Selectable("1920Ã—1080 (Full HD)")) 
        {
            m_SceneSettings.m_SceneWidth = 1920;
            m_SceneSettings.m_SceneHeight = 1080;
        }
        if (ImGui::Selectable("1600Ã—900 (HD+)")) 
        {
            m_SceneSettings.m_SceneWidth = 1600;
            m_SceneSettings.m_SceneHeight = 900;
        }
        if (ImGui::Selectable("1280Ã—720 (HD)")) 
        {
            m_SceneSettings.m_SceneWidth = 1280;
            m_SceneSettings.m_SceneHeight = 720;
        }
        if (ImGui::Selectable("1024Ã—768 (4:3)")) 
        {
            m_SceneSettings.m_SceneWidth = 1024;
            m_SceneSettings.m_SceneHeight = 768;
        }
        if (ImGui::Selectable("800Ã—600 (SVGA)")) 
        {
            m_SceneSettings.m_SceneWidth = 800;
            m_SceneSettings.m_SceneHeight = 600;
        }
        ImGui::EndCombo();
    }
    ImGui::PopItemWidth();

    ImGui::Spacing();

    // Validation
    if (m_SceneSettings.m_SceneWidth < 320)
    {
        m_SceneSettings.m_SceneWidth = 320;
    }
    if (m_SceneSettings.m_SceneHeight < 240)
    {
        m_SceneSettings.m_SceneHeight = 240;
    }
    if (m_SceneSettings.m_SceneWidth > 7680)
    {
        m_SceneSettings.m_SceneWidth = 7680;
    }
    if (m_SceneSettings.m_SceneHeight > 4320)
    {
        m_SceneSettings.m_SceneHeight = 4320;
    }

    // Check if resolution changed
    b_ResolutionChanged = 
	(
		s_PrevWidth != m_SceneSettings.m_SceneWidth || 
        s_PrevHeight != m_SceneSettings.m_SceneHeight
	);

    if (b_ResolutionChanged)
    {
        // Update render textures with new resolution
        UnloadRenderTexture(m_RaylibTexture);
        UnloadRenderTexture(m_DisplayTexture);

        m_RaylibTexture = LoadRenderTexture
		(
			m_SceneSettings.m_SceneWidth, m_SceneSettings.m_SceneHeight
		);
        m_DisplayTexture = LoadRenderTexture
		(
			m_SceneSettings.m_SceneWidth, m_SceneSettings.m_SceneHeight
		);

        SetTextureFilter(m_RaylibTexture.texture, TEXTURE_FILTER_BILINEAR);
        SetTextureFilter(m_DisplayTexture.texture, TEXTURE_FILTER_BILINEAR);

        // Update scene bounds for game map/manager
        if (m_MapManager)
        {
            m_MapManager->SetSceneBounds
			(
                static_cast<float>(m_SceneSettings.m_SceneWidth), 
                static_cast<float>(m_SceneSettings.m_SceneHeight)
            );
        }
        else if (m_GameEngine.GetMapManager())
        {
            m_GameEngine.GetMapManager()->SetSceneBounds
			(
                static_cast<float>(m_SceneSettings.m_SceneWidth), 
                static_cast<float>(m_SceneSettings.m_SceneHeight)
            );
        }

        // Update previous values
        s_PrevWidth = m_SceneSettings.m_SceneWidth;
        s_PrevHeight = m_SceneSettings.m_SceneHeight;
    }

	ImGui::Spacing();

	// FPS Settings
	ImGui::AlignTextToFramePadding();
	ImGui::Text("Target FPS:");
	ImGui::SameLine();
	ImGui::SetCursorPosX(120.0f);

	ImGui::SameLine();
	ImGui::PushItemWidth(100.0f);
	ImGui::InputInt("##target_fps", &m_SceneSettings.m_TargetFPS, 0, 0);
	ImGui::PopItemWidth();

	ImGui::SameLine();
	ImGui::PushItemWidth(150.0f);
	if (ImGui::BeginCombo("##fps_presets", "Presets"))
	{
		if (ImGui::Selectable("30 FPS")) m_SceneSettings.m_TargetFPS = 30;
		if (ImGui::Selectable("60 FPS")) m_SceneSettings.m_TargetFPS = 60;
		if (ImGui::Selectable("120 FPS")) m_SceneSettings.m_TargetFPS = 120;
		if (ImGui::Selectable("144 FPS")) m_SceneSettings.m_TargetFPS = 144;
		if (ImGui::Selectable("240 FPS")) m_SceneSettings.m_TargetFPS = 240;
		if (ImGui::Selectable("Unlimited")) m_SceneSettings.m_TargetFPS = 0;
		ImGui::EndCombo();
	}
	ImGui::PopItemWidth();

	// Check if FPS changed
	b_FPSChanged =
	(
		s_PrevTargetFPS != m_SceneSettings.m_TargetFPS
	);

	if (b_FPSChanged)
	{
		if (m_MapManager)
		{
			m_MapManager->SetTargetFPS(m_SceneSettings.m_TargetFPS);
		}
		else if (m_GameEngine.GetMapManager())
		{
			m_GameEngine.GetMapManager()->SetTargetFPS(m_SceneSettings.m_TargetFPS);
		}
		s_PrevTargetFPS = m_SceneSettings.m_TargetFPS;
	}

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
    ImGui::TextWrapped
	(
		"This sets the resolution of the scene viewport that your game will use during development. "

        "The export resolution can be set separately in the Export panel."
	);
    ImGui::PopStyleColor();

    // Sync button to copy scene resolution to export settings
    ImGui::Spacing();
    if (ImGui::Button("Copy to Export Settings", ImVec2(200.0f, 30.0f)))
    {
        m_ExportState.m_WindowWidth = m_SceneSettings.m_SceneWidth;
        m_ExportState.m_WindowHeight = m_SceneSettings.m_SceneHeight;
    }

    ImGui::End();
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

static void s_fAppendLogLine
(
	std::vector<std::string>& logs, 
	std::mutex& mtx, 
	const std::string& line
)
{
	std::scoped_lock lk(mtx);
	logs.push_back(line);
}

static bool s_bfValidateExportFolder
(
	const std::string& out_dir, 
	std::vector<std::string>& logs, 
	std::mutex& mtx
)
{
	bool b_Ok = true;
	
	// Log current working directory for debugging
	s_fAppendLogLine
	(
		logs, 
		mtx, 
		std::string
		(
			"Validation working directory: "
		) + fs::current_path().string()
	);
	s_fAppendLogLine
	(
		logs, 
		mtx, 
		std::string
		(
			"Checking export directory: "
		) + out_dir
	);
	
	auto require = [&](const fs::path& p)
	{
		bool b_Exists = fs::exists(p);

		s_fAppendLogLine
		(
			logs, 
			mtx, 
			std::string("Checking: ") + p.string() + " - " + 
			(
				b_Exists ? "EXISTS" : "MISSING"
			)
		);
		if (!b_Exists) b_Ok = false;
	};
	// For validation, we need to check for "MyGame.exe" since we don't   have access to the actual game name here
	
	// This is a limitation - 
	// we'll just check for any .exe file in the export directory
	bool b_FoundGameExe = false;
	std::error_code ec;
	if (fs::exists(out_dir, ec) && !ec) 
	{
		for (const auto& ENTRY : fs::directory_iterator(out_dir, ec)) 
		{
			if 
			(
				!ec && ENTRY.is_regular_file() && 
				ENTRY.path().extension() == ".exe"
			) 
			{
				b_FoundGameExe = true;
				s_fAppendLogLine
				(
					logs, 
					mtx, 
					std::string
					(
						"Found game executable: "
					) + ENTRY.path().filename().string()
				);
				break;
			}
		}
	}
	if (!b_FoundGameExe) 
	{
		s_fAppendLogLine(logs, mtx, "Missing: Game executable (.exe file)");
		b_Ok = false;
	}
	
	require(fs::path(out_dir) / "GameLogic.dll");
	require(fs::path(out_dir) / "raylib.dll");
	
	// Check for Assets folder (optional - may not exist if no game assets)
	fs::path assets_path = fs::path(out_dir) / "Assets";
	if (fs::exists(assets_path)) 
	{
		s_fAppendLogLine(logs, mtx, "Found Assets folder in export");
	}
	else 
	{
		s_fAppendLogLine
		(
			logs, 
			mtx, 
			"No Assets folder found - this is OK if game has no assets"
		);
	}
	
	return b_Ok;
}

void GameEditor::DrawMapSelectionUI()
{
	// Only show map selection when game is paused and we have a MapManager
	if (!m_MapManager) 
	{
		return;
	}

	ImGui::Begin("Map Selection", nullptr, ImGuiWindowFlags_NoCollapse);
	ImGui::Text("Current Map: %s", m_MapManager->GetCurrentMapId().c_str());
	ImGui::Separator();
	ImGui::Spacing();

	// Get available maps
	std::vector<std::string> available_maps = m_MapManager->GetAvailableMaps();

	if (available_maps.empty())
	{
		ImGui::TextColored
		(
			ImVec4(1.0f, 0.6f, 0.6f, 1.0f), 
			"No maps registered in MapManager"
		);
		ImGui::Text("Register maps using RegisterMap<YourMap>(\"MAP_ID\")");
	}
	else
	{
		ImGui::Text("Available Maps:");
		ImGui::Spacing();

		// Create a combo box for map selection
		static int s_SelectedIndex = 0;
		std::string curr_map_id = m_MapManager->GetCurrentMapId();

		// Find current map index
		for (int i = 0; i < available_maps.size(); i++)
		{
			if (available_maps[i] == curr_map_id)
			{
				s_SelectedIndex = i;
				break;
			}
		}

		// Create combo box
		if
		(
			ImGui::BeginCombo
			(
				"Select Map", 
				curr_map_id.empty() ? 
				"No map loaded" : curr_map_id.c_str()
			)
		)
		{
			for (int i = 0; i < available_maps.size(); ++i)
			{
				bool b_IsSelected = (s_SelectedIndex == i);
				bool b_IsCurrent = (available_maps[i] == curr_map_id);

				// Highlight current map in red
				if (b_IsCurrent)
				{
					ImGui::PushStyleColor
					(
						// Red color
						ImGuiCol_Text, ImVec4(1.0f, 0.2f, 0.2f, 1.0f)
					);
				}
				// Highlight MainMap (index 0) in a special way
				else if (i == 0)
				{
					ImGui::PushStyleColor
					(
						// Yellow/gold color
						ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.2f, 1.0f) 
					);
				}

				if
				(
					ImGui::Selectable
					(
						available_maps[i].c_str(), b_IsSelected
					)
				)
				{
					s_SelectedIndex = i;
					m_SelectedMapId = available_maps[i];

					// Switch to selected map
					if (m_SelectedMapId != curr_map_id)
					{
						m_MapManager->b_GotoMap(m_SelectedMapId);
					}
				}

				// If the item is focused, set it as the default
				if (b_IsSelected)
				{
					ImGui::SetItemDefaultFocus();
				}

				// Pop style color if we pushed it
				if (b_IsCurrent || i == 0)
				{
					ImGui::PopStyleColor();
				}
			}
			ImGui::EndCombo();
		}

		ImGui::Spacing();

		// Quick access buttons for each map
		ImGui::Text("Quick Access:");
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		for (int i = 0; i < available_maps.size(); ++i)
		{
			const auto& MAP_ID = available_maps[i];
			bool b_IsCurrent = (MAP_ID == curr_map_id);

			// Style current map button with red color
			if (b_IsCurrent)
			{
				ImGui::PushStyleColor
				(
					// Dark red
					ImGuiCol_Button, ImVec4(0.7f, 0.2f, 0.2f, 0.6f)
				);

				ImGui::PushStyleColor
				(
					// Light red
					ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 0.8f)
				);

				ImGui::PushStyleColor
				(
					// Darker red
					ImGuiCol_ButtonActive, ImVec4(0.5f, 0.1f, 0.1f, 1.0f) 
				);
			}
			// Style MainMap (index 0) button differently to make it stand out
			else if (i == 0)
			{
				ImGui::PushStyleColor
				(
					// Dark gold
					ImGuiCol_Button, ImVec4(0.8f, 0.6f, 0.0f, 0.7f) 
				);

				ImGui::PushStyleColor
				(
					// Light gold
					ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.8f, 0.2f, 0.9f) 
				);

				ImGui::PushStyleColor
				(
					// Darker gold
					ImGuiCol_ButtonActive, ImVec4(0.6f, 0.4f, 0.0f, 1.0f) 
				);
			}

			// Add a special label for MainMap
			std::string button_label = MAP_ID;
			if (i == 0)
			{
				button_label += " (Main)";
			}

			if (ImGui::Button(button_label.c_str(), ImVec2(-1, 0)))
			{
				if (MAP_ID != curr_map_id)
				{
					m_MapManager->b_GotoMap(MAP_ID);
				}
			}

			// Pop style colors if we pushed them
			if (b_IsCurrent || i == 0)
			{
				ImGui::PopStyleColor(3);
			}
		}

		ImGui::Spacing();
		ImGui::Separator();
	}

	ImGui::End();
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

void GameEditor::DrawPerformanceOverlay()
{
	if (!m_bShowPerformanceStats)
	{
		return;
	}

	// Semi-Transparent background
	ImGui::SetNextWindowBgAlpha(0.7f);

	ImGuiWindowFlags window_flags =
		ImGuiWindowFlags_NoDecoration |
		ImGuiWindowFlags_AlwaysAutoResize |
		ImGuiWindowFlags_NoFocusOnAppearing |
		ImGuiWindowFlags_NoDocking |
		ImGuiWindowFlags_NoNav;



	if (ImGui::Begin("Performance Overlay", &m_bShowPerformanceStats, window_flags))
	{
		// Calculate stats
		float avg_frame_time = 0.0f;
		float max_frame_time = 0.0f;

		for (float t : m_FrameTimes)
		{
			avg_frame_time += t;
			if (t > max_frame_time) max_frame_time = t;
		}
		avg_frame_time /= m_FrameTimes.size();

		// Avoid division by zero
		float fps = 1000.0f / (avg_frame_time > 0.001f ? avg_frame_time : 16.66f);

		// Header Text
		ImGui::PushFont
		(
			ImGui::GetIO().Fonts->Fonts[ImGui::GetIO().Fonts->Fonts.size() > 1 ? 1 : 0]
		); // Bigger font if available
		ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "%.0f FPS", fps);
		ImGui::PopFont();

		ImGui::Separator();
		ImGui::Text("Avg: %.2f ms", avg_frame_time);
		ImGui::Text("Max: %.2f ms", max_frame_time);

		ImGui::Spacing();

		// Plot lines
		// Use the offset to make the graph look like a rolling buffer
		ImGui::PlotLines
		(
			"##FrameTimes",
			m_FrameTimes.data(),
			static_cast<int>(m_FrameTimes.size()),
			static_cast<int>(m_FrameOffset),
			"Frame Time (ms)",
			0.0f,
			33.0f, // 0-33ms range covers up to 30fps dips
			ImVec2(200, 60)
		);
	}
	ImGui::End();
}

void GameEditor::DrawTerminal()
{
    if (m_bShowTerminal) 
    {
        m_Terminal.show("Debug Console", &m_bShowTerminal);
    }
}