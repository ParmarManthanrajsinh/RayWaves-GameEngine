#define IMGUI_DEFINE_MATH_OPERATORS
#include "SceneWindow.h"
#include "../GameEditor.h"
#include "../../Engine/MapManager.h"
#include "../../Engine/ProjectManager.h"
#include "../../Engine/GameEngine.h"
#include "../ProcessRunner.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <rlImGui.h>
#include <cmath>
#include <numbers>
#include "../../Engine/Profiler.h"

void SceneWindow::DrawToolbarBackground()
{
	ImDrawList* draw_list = ImGui::GetWindowDrawList();
	ImVec2 toolbar_pos = ImGui::GetCursorScreenPos();
	ImVec2 toolbar_size = ImVec2(ImGui::GetContentRegionAvail().x, 40.0f);

	ImU32 toolbar_color_top = ImGui::GetColorU32(ImGuiCol_MenuBarBg);
	ImU32 toolbar_color_bottom = ImGui::GetColorU32(ImGuiCol_WindowBg);

	draw_list->AddRectFilledMultiColor
	(
		toolbar_pos,
		ImVec2(toolbar_pos.x + toolbar_size.x, toolbar_pos.y + toolbar_size.y),
		toolbar_color_top, toolbar_color_top,
		toolbar_color_bottom, toolbar_color_bottom
	);
}

void SceneWindow::s_fDrawSpinner(float radius, float thickness, const unsigned int& color)
{
	alignas(16) struct
	{
		float time; float start; float a_min; float a_max; float centre_x; float centre_y; float time_x8; float inv_num_segments; float angle_range;
	} vars{};

	ImVec2 pos = ImGui::GetCursorScreenPos();
	ImGui::Dummy(ImVec2(radius * 2, radius * 2));
	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	DrawList->PathClear();

	vars.time = static_cast<float>(ImGui::GetTime());
	constexpr int c_NUM_SEGMENTS = 30;
	vars.start = fabsf(sinf(vars.time * 1.8f) * (c_NUM_SEGMENTS - 5));

	vars.inv_num_segments = 1.0f / static_cast<float>(c_NUM_SEGMENTS);
	vars.time_x8 = vars.time * 8.0f;

	vars.a_min = std::numbers::pi_v<float> * 2.0f * vars.start * vars.inv_num_segments;
	vars.a_max = std::numbers::pi_v<float> * 2.0f * static_cast<float>(c_NUM_SEGMENTS - 3) * vars.inv_num_segments;
	vars.angle_range = vars.a_max - vars.a_min;

	const ImVec2 CENTER = ImVec2(pos.x + radius, pos.y + radius);
	vars.centre_x = CENTER.x; vars.centre_y = CENTER.y;

	for (int i = 0; i < c_NUM_SEGMENTS; ++i)
	{
		const float A = vars.a_min + ((static_cast<float>(i) * vars.inv_num_segments) * vars.angle_range);
		const float ANGLE = A + vars.time_x8;
		DrawList->PathLineTo(ImVec2(vars.centre_x + (cosf(ANGLE) * radius), vars.centre_y + (sinf(ANGLE) * radius)));
	}
	DrawList->PathStroke(color, 0, thickness);
}

bool SceneWindow::s_bIconButton(std::string_view label, std::string_view icon, const ImVec2& size, std::string_view tooltip)
{
	ImGui::PushID(label.data());
	bool b_Clicked = ImGui::Button(icon.data(), size);

	if (ImGui::IsItemHovered()) 
	{
		ImGui::SetTooltip("%.*s", static_cast<int>(tooltip.size()), tooltip.data());
	}
	ImGui::PopID();
	return b_Clicked;
}

void SceneWindow::Draw(GameEditor* editor)
{
	SCOPED_TIMER("panel_scene_window");
	ImGuiStyle& style = ImGui::GetStyle();
	style.WindowMenuButtonPosition = ImGuiDir_Left;

	ImGui::Begin(ICON_FA_IMAGE " Scene", nullptr, ImGuiWindowFlags_NoTitleBar);

	ImGuiDockNode* node = ImGui::DockBuilderGetNode(0x00000001);
	if (node != nullptr)
	{
		node->LocalFlags |= ImGuiDockNodeFlags_HiddenTabBar;
	}

	DrawToolbarBackground();

	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 4));
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 2));
	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);

	float toolbar_height = 40.0f;
	float button_height = 32.0f;
	float vertical_offset = (toolbar_height - button_height) / 2.0f;
	ImGui::SetCursorPosY(ImGui::GetCursorPosY() + vertical_offset);
	const std::string center_icon = editor->b_IsPlaying ? ICON_FA_PLAY : ICON_FA_STOP;
	const std::string center_label = editor->b_IsPlaying ? " PLAYING" : " STOPPED";
	float status_w = ImGui::CalcTextSize((center_icon + center_label).c_str()).x + 32.0f;
	float spacing_x = ImGui::GetStyle().ItemSpacing.x;
	float total_center_w = 226.0f + status_w + (6.0f * spacing_x);
	
	ImGui::SetCursorPosX((ImGui::GetWindowWidth() - total_center_w) * 0.5f);

	// Play/Pause
	if (editor->b_IsPlaying)
	{
		if (s_bIconButton("pause_btn", ICON_FA_PAUSE, ImVec2(32, 32), "Pause")) editor->b_IsPlaying = false;
	}
	else
	{
		if (s_bIconButton("play_btn", ICON_FA_PLAY, ImVec2(32, 32), "Play")) editor->b_IsPlaying = true;
	}

	ImGui::SameLine();

	// Restart
	if (s_bIconButton("restart_btn", ICON_FA_ARROW_ROTATE_RIGHT, ImVec2(32, 32), "Restart") || editor->IsWindowResized())
	{
		editor->b_IsPlaying = false;
		editor->GetMapManager()->b_ReloadCurrentMap();
	}

	ImGui::SameLine();
	ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 12);

	// Status Text
	const ImVec4 COLOR = editor->b_IsPlaying ? ImVec4(0.2f, 0.8f, 0.2f, 1.0f) : ImVec4(0.8f, 0.2f, 0.2f, 1.0f);
	std::string_view ICON = editor->b_IsPlaying ? ICON_FA_PLAY : ICON_FA_STOP;
	std::string_view LABEL = editor->b_IsPlaying ? " PLAYING" : " STOPPED";

	ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(16.0f, 4.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 12.0f);

	ImVec4 bgColor = editor->b_IsPlaying ? ImVec4(0.15f, 0.6f, 0.15f, 1.0f) : ImVec4(0.7f, 0.15f, 0.15f, 1.0f);
	ImVec4 textColor = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);

	ImGui::PushStyleColor(ImGuiCol_Button, bgColor);
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, bgColor);
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, bgColor);
	ImGui::PushStyleColor(ImGuiCol_Text, textColor);

	std::string fullLabel;
	fullLabel.reserve(ICON.size() + LABEL.size());
	fullLabel.append(ICON);
	fullLabel.append(LABEL);
	ImGui::Button(fullLabel.c_str(), ImVec2(0, 32.0f));

	ImGui::PopStyleVar(3);
	ImGui::PopStyleColor(4);

	ImGui::SameLine();
	ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 12.0f);

	// Restore
	if (s_bIconButton("restore_btn", ICON_FA_ARROW_ROTATE_LEFT, ImVec2(32, 32), "Reset Game"))
	{
		editor->b_IsPlaying = false;
		if (!editor->b_ReloadGameLogic()) editor->GetGameEngine().ResetMap();
	}

	ImGui::SameLine();
	ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 5);

	// Clean
	if (s_bIconButton("clean_btn", ICON_FA_TRASH_CAN, ImVec2(32, 32), "Delete Build Folder"))
	{
		if (ProjectManager::b_HasOpenProject())
		{
			auto raywaves_build = std::filesystem::path(ProjectManager::GetCurrent().m_RootPath) / ".raywaves" / "build";
			if (std::filesystem::exists(raywaves_build)) std::filesystem::remove_all(raywaves_build);
		}
	}

	ImGui::SameLine();
	ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 5);

	// Performance Toggle
	const ImVec4 STATS_COLOR = editor->m_bShowPerformanceStats ? ImGui::GetStyle().Colors[ImGuiCol_Text] : ImGui::GetStyle().Colors[ImGuiCol_TextDisabled];
	ImGui::PushStyleColor(ImGuiCol_Text, STATS_COLOR);

	if (s_bIconButton("perf_btn", ICON_FA_CHART_LINE, ImVec2(32, 32), "Performance Overlay")) editor->m_bShowPerformanceStats = !editor->m_bShowPerformanceStats;
	ImGui::PopStyleColor();
	ImGui::SameLine();

	// Terminal Toggle
	const ImVec4 TERM_COLOR = editor->m_bShowTerminal ? ImGui::GetStyle().Colors[ImGuiCol_Text] : ImGui::GetStyle().Colors[ImGuiCol_TextDisabled];
	ImGui::PushStyleColor(ImGuiCol_Text, TERM_COLOR);
	if (s_bIconButton("term_btn", ICON_FA_TERMINAL, ImVec2(32, 32), "Debug Console")) editor->m_bShowTerminal = !editor->m_bShowTerminal;
	ImGui::PopStyleColor();
	ImGui::SameLine();

	// Compile
	float button_sz = 32.0f + (ImGui::GetStyle().FramePadding.x * 2.0f);
	float status_sz = 0.0f;

	if (editor->b_IsCompiling) status_sz = 20.0f + ImGui::GetStyle().ItemSpacing.x + ImGui::CalcTextSize("Compiling...").x + ImGui::GetStyle().ItemSpacing.x;

	float avail = ImGui::GetContentRegionAvail().x;
	float pos_x = ImGui::GetCursorPosX() + avail - button_sz - status_sz;

	if (pos_x > ImGui::GetCursorPosX()) ImGui::SetCursorPosX(pos_x);

	if (editor->b_IsCompiling)
	{
		float spinner_height = 20.0f;
		float spinner_y_offset = (toolbar_height - spinner_height) / 2.0f;
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() - vertical_offset + spinner_y_offset);

		s_fDrawSpinner(10.0f, 2.0f, ImGui::GetColorU32(ImVec4(0.2f, 0.8f, 0.2f, 1.0f)));

		ImGui::SameLine();
		float text_compile_y_offset = (toolbar_height - ImGui::GetTextLineHeight()) / 2.0f;
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() - vertical_offset - 5 + text_compile_y_offset);
		ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "Compiling...");
		ImGui::SameLine();
	}

	bool b_Disabled = editor->b_IsCompiling.load();
	if (b_Disabled) ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);

	if (s_bIconButton("compile_btn", ICON_FA_HAMMER, ImVec2(32, 32), "Recompile"))
	{
		if (!b_Disabled)
		{
			editor->CompileGameLogic();
		}
	}

	if (b_Disabled) ImGui::PopStyleVar();
	ImGui::PopStyleVar(3);

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
	ImGui::BeginChild("ViewportArea", ImVec2(0, 0), 0, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

	rlImGuiImageRenderTextureFit(editor->m_bUseOpaquePass ? &editor->m_DisplayTexture : &editor->m_RaylibTexture, true);

	ImGui::EndChild();
	ImGui::PopStyleVar();

	ImGui::End();
}
