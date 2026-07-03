#include "SceneSettingsPanel.h"
#include "../GameEditor.h"
#include "../../Engine/MapManager.h"
#include "../../Engine/GameEngine.h"
#include <imgui.h>
#include <rlImGui.h>
#include "../../Engine/Profiler.h"

void SceneSettingsPanel::Draw(GameEditor* editor)
{
	SCOPED_TIMER("panel_scene_settings");
	if (!editor->m_bShowSceneSettings)
	{
		return;
	}
    
    ImGuiStyle& style = ImGui::GetStyle();
    ImGuiDir old_menu_pos = style.WindowMenuButtonPosition;
    style.WindowMenuButtonPosition = ImGuiDir_None;

    bool b_begin = ImGui::Begin(ICON_FA_GEARS " Scene Settings", &editor->m_bShowSceneSettings);

    style.WindowMenuButtonPosition = old_menu_pos;

    if (!b_begin)
    {
        ImGui::End();
        return;
    }

    static int s_PrevWidth = editor->m_SceneSettings.m_SceneWidth;
    static int s_PrevHeight = editor->m_SceneSettings.m_SceneHeight;
    static int s_PrevTargetFPS = editor->m_SceneSettings.m_TargetFPS;

    ImGui::Spacing();
    ImGui::SeparatorText("Display Settings");
    ImGui::Spacing();

    if (ImGui::BeginTable("##scene_display_props", 2, ImGuiTableFlags_None))
    {
        ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 140.0f);
        ImGui::TableSetupColumn("Widget", ImGuiTableColumnFlags_WidthStretch);

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Scene Resolution:");

        ImGui::TableSetColumnIndex(1);
        ImGui::PushItemWidth(80.0f);
        ImGui::InputInt("##scene_width", &editor->m_SceneSettings.m_SceneWidth, 0, 0);
        ImGui::PopItemWidth();
        
        ImGui::SameLine();
        ImGui::Text("x");
        ImGui::SameLine();
        
        ImGui::PushItemWidth(80.0f);
        ImGui::InputInt("##scene_height", &editor->m_SceneSettings.m_SceneHeight, 0, 0);
        ImGui::PopItemWidth();
        
        ImGui::SameLine();
        ImGui::PushItemWidth(150.0f);
        if (ImGui::BeginCombo("##scene_resolution_presets", "Presets"))
        {
            if (ImGui::Selectable("1920x1080 (Full HD)")) { editor->m_SceneSettings.m_SceneWidth = 1920; editor->m_SceneSettings.m_SceneHeight = 1080; }
            if (ImGui::Selectable("1600x900 (HD+)")) { editor->m_SceneSettings.m_SceneWidth = 1600; editor->m_SceneSettings.m_SceneHeight = 900; }
            if (ImGui::Selectable("1280x720 (HD)")) { editor->m_SceneSettings.m_SceneWidth = 1280; editor->m_SceneSettings.m_SceneHeight = 720; }
            if (ImGui::Selectable("1024x768 (4:3)")) { editor->m_SceneSettings.m_SceneWidth = 1024; editor->m_SceneSettings.m_SceneHeight = 768; }
            if (ImGui::Selectable("800x600 (SVGA)")) { editor->m_SceneSettings.m_SceneWidth = 800; editor->m_SceneSettings.m_SceneHeight = 600; }
            ImGui::EndCombo();
        }
        ImGui::PopItemWidth();
        
        ImGui::EndTable();
    }

    if (editor->m_SceneSettings.m_SceneWidth < 320) editor->m_SceneSettings.m_SceneWidth = 320;
    if (editor->m_SceneSettings.m_SceneHeight < 240) editor->m_SceneSettings.m_SceneHeight = 240;
    if (editor->m_SceneSettings.m_SceneWidth > 7680) editor->m_SceneSettings.m_SceneWidth = 7680;
    if (editor->m_SceneSettings.m_SceneHeight > 4320) editor->m_SceneSettings.m_SceneHeight = 4320;

    bool b_ResolutionChanged = (s_PrevWidth != editor->m_SceneSettings.m_SceneWidth || s_PrevHeight != editor->m_SceneSettings.m_SceneHeight);

    extern bool g_bNeedsTextureRecreate;
    if (b_ResolutionChanged)
    {
        g_bNeedsTextureRecreate = true;

        if (editor->GetMapManager())
        {
            editor->GetMapManager()->SetSceneBounds(static_cast<float>(editor->m_SceneSettings.m_SceneWidth), static_cast<float>(editor->m_SceneSettings.m_SceneHeight));
        }
        else if (editor->GetGameEngine().GetMapManager())
        {
            editor->GetGameEngine().GetMapManager()->SetSceneBounds(static_cast<float>(editor->m_SceneSettings.m_SceneWidth), static_cast<float>(editor->m_SceneSettings.m_SceneHeight));
        }

        s_PrevWidth = editor->m_SceneSettings.m_SceneWidth;
        s_PrevHeight = editor->m_SceneSettings.m_SceneHeight;
    }

    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::SeparatorText("Performance Settings");
    ImGui::Spacing();

    if (ImGui::BeginTable("##scene_perf_props", 2, ImGuiTableFlags_None))
    {
        ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 140.0f);
        ImGui::TableSetupColumn("Widget", ImGuiTableColumnFlags_WidthStretch);

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Target FPS:");

        ImGui::TableSetColumnIndex(1);
        ImGui::PushItemWidth(100.0f);
        ImGui::InputInt("##target_fps", &editor->m_SceneSettings.m_TargetFPS, 0, 0);
        ImGui::PopItemWidth();

        ImGui::SameLine();
        ImGui::PushItemWidth(150.0f);
        if (ImGui::BeginCombo("##fps_presets", "Presets"))
        {
            if (ImGui::Selectable("30 FPS")) editor->m_SceneSettings.m_TargetFPS = 30;
            if (ImGui::Selectable("60 FPS")) editor->m_SceneSettings.m_TargetFPS = 60;
            if (ImGui::Selectable("120 FPS")) editor->m_SceneSettings.m_TargetFPS = 120;
            if (ImGui::Selectable("144 FPS")) editor->m_SceneSettings.m_TargetFPS = 144;
            if (ImGui::Selectable("240 FPS")) editor->m_SceneSettings.m_TargetFPS = 240;
            if (ImGui::Selectable("Unlimited")) editor->m_SceneSettings.m_TargetFPS = 0;
            ImGui::EndCombo();
        }
        ImGui::PopItemWidth();
        
        ImGui::EndTable();
    }

    editor->b_FPSChanged = (s_PrevTargetFPS != editor->m_SceneSettings.m_TargetFPS);

    if (editor->b_FPSChanged)
    {
        if (editor->GetMapManager())
        {
            editor->GetMapManager()->SetTargetFPS(editor->m_SceneSettings.m_TargetFPS);
        }
        else if (editor->GetGameEngine().GetMapManager())
        {
            editor->GetGameEngine().GetMapManager()->SetTargetFPS(editor->m_SceneSettings.m_TargetFPS);
        }
        s_PrevTargetFPS = editor->m_SceneSettings.m_TargetFPS;
    }

    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::SeparatorText("Hot Reload Settings");
    ImGui::Spacing();
    ImGui::Checkbox("Preserve state on reload", &editor->m_bPreserveStateOnReload);

    ImGui::End();
}
