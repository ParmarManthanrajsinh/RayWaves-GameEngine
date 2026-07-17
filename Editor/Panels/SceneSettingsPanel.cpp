#include "SceneSettingsPanel.h"
#include "../GameEditor.h"
#include "../../Engine/MapManager.h"
#include "../../Engine/GameEngine.h"
#include <imgui.h>
#include <rlImGui.h>
#include "../../Engine/Profiler.h"
#include "../../Engine/ProjectManager.h"
#include <imgui_stdlib.h>

#include <algorithm>

void SceneSettingsPanel::Draw(GameEditor* editor)
{
	SCOPED_TIMER("panel_scene_settings");
    if (!ProjectManager::b_HasOpenProject())
    {
        editor->m_bShowSceneSettings = false;
        return;
    }

    bool b_now_visible = editor->m_bShowSceneSettings;
    if (b_now_visible && !m_bPanelVisibleLastFrame)
    {
        m_PrevWidth = editor->m_SceneSettings.m_SceneWidth;
        m_PrevHeight = editor->m_SceneSettings.m_SceneHeight;
        m_PrevTargetFPS = editor->m_SceneSettings.m_TargetFPS;
    }
    m_bPanelVisibleLastFrame = b_now_visible;
    if (!b_now_visible)
    {
        return;
    }
    
    ImGuiStyle& style = ImGui::GetStyle();
    ImGuiDir old_menu_pos = style.WindowMenuButtonPosition;
    style.WindowMenuButtonPosition = ImGuiDir_None;

    bool b_begin = ImGui::Begin(ICON_FA_GEARS " Project Settings", &editor->m_bShowSceneSettings);

    style.WindowMenuButtonPosition = old_menu_pos;

    if (!b_begin)
    {
        ImGui::End();
        return;
    }

    ImGui::Spacing();
    ImGui::SeparatorText("Project Settings");
    ImGui::Spacing();

    static std::string s_TempProjectName;
    static std::string s_LastProjectPath;
    if ((b_now_visible && !m_bPanelVisibleLastFrame) || s_LastProjectPath != ProjectManager::GetCurrent().m_RootPath)
    {
        s_TempProjectName = ProjectManager::GetCurrent().m_Name;
        s_LastProjectPath = ProjectManager::GetCurrent().m_RootPath;
    }

    if (ImGui::BeginTable("##scene_project_props", 2, ImGuiTableFlags_None))
    {
        ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 140.0f);
        ImGui::TableSetupColumn("Widget", ImGuiTableColumnFlags_WidthStretch);

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Project Name:");

        ImGui::TableSetColumnIndex(1);
        ImGui::PushItemWidth(-120.0f);
        ImGui::InputText("##project_name", &s_TempProjectName);
        ImGui::PopItemWidth();
        
        ImGui::SameLine();
        if (ImGui::Button("Rename", ImVec2(100, 0)))
        {
            ProjectManager::GetCurrent().m_Name = s_TempProjectName;
            ProjectManager::b_SaveCurrentProject();
            ProjectManager::GenerateCMakeLists();
        }
        ImGui::EndTable();
    }

    ImGui::Spacing();
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

    editor->m_SceneSettings.m_SceneWidth = std::max(editor->m_SceneSettings.m_SceneWidth, 320);
    editor->m_SceneSettings.m_SceneHeight = std::max(editor->m_SceneSettings.m_SceneHeight, 240);
    editor->m_SceneSettings.m_SceneWidth = std::min(editor->m_SceneSettings.m_SceneWidth, 7680);
    editor->m_SceneSettings.m_SceneHeight = std::min(editor->m_SceneSettings.m_SceneHeight, 4320);

    bool b_ResolutionChanged = (m_PrevWidth != editor->m_SceneSettings.m_SceneWidth || m_PrevHeight != editor->m_SceneSettings.m_SceneHeight);

    extern bool g_bNeedsTextureRecreate;
    if (b_ResolutionChanged)
    {
        g_bNeedsTextureRecreate = true;

        if (editor->GetMapManager() != nullptr)
        {
            editor->GetMapManager()->SetSceneBounds(static_cast<float>(editor->m_SceneSettings.m_SceneWidth), static_cast<float>(editor->m_SceneSettings.m_SceneHeight));
        }
        else if (editor->GetGameEngine().GetMapManager() != nullptr)
        {
            editor->GetGameEngine().GetMapManager()->SetSceneBounds(static_cast<float>(editor->m_SceneSettings.m_SceneWidth), static_cast<float>(editor->m_SceneSettings.m_SceneHeight));
        }

        m_PrevWidth = editor->m_SceneSettings.m_SceneWidth;
        m_PrevHeight = editor->m_SceneSettings.m_SceneHeight;
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

    editor->b_FPSChanged = (m_PrevTargetFPS != editor->m_SceneSettings.m_TargetFPS);

    if (editor->b_FPSChanged)
    {
        if (editor->GetMapManager() != nullptr)
        {
            editor->GetMapManager()->SetTargetFPS(editor->m_SceneSettings.m_TargetFPS);
        }
        else if (editor->GetGameEngine().GetMapManager() != nullptr)
        {
            editor->GetGameEngine().GetMapManager()->SetTargetFPS(editor->m_SceneSettings.m_TargetFPS);
        }
        m_PrevTargetFPS = editor->m_SceneSettings.m_TargetFPS;
    }

    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::SeparatorText("Hot Reload Settings");
    ImGui::Spacing();
    ImGui::Checkbox("Preserve state on reload", &editor->m_bPreserveStateOnReload);

    ImGui::End();
}
