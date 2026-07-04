#include "MainMenuBar.h"
#include "../GameEditor.h"
#include "../../Engine/ProjectManager.h"
#include <imgui.h>
#include <rlImGui.h>
#include <tinyfiledialogs.h>
#include "../EditorUtils.h"
#include "../../Engine/Profiler.h"

static void SaveProjectWithSceneSettings(GameEditor* editor)
{
    if (!ProjectManager::b_HasOpenProject()) return;
    auto& prj = ProjectManager::GetCurrent();
    prj.m_SceneWidth = editor->m_SceneSettings.m_SceneWidth;
    prj.m_SceneHeight = editor->m_SceneSettings.m_SceneHeight;
    prj.m_TargetFPS = editor->m_SceneSettings.m_TargetFPS;
    ProjectManager::b_SaveCurrentProject();
}

void MainMenuBar::Draw(GameEditor* editor)
{
	SCOPED_TIMER("panel_menu_bar");
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem(ICON_FA_FLOPPY_DISK " Save Project", "Ctrl+S"))
            {
                SaveProjectWithSceneSettings(editor);
            }
            ImGui::Separator();
            if (ImGui::MenuItem(ICON_FA_FOLDER_OPEN " Switch Project..."))
            {
                const char* path = tinyfd_selectFolderDialog("Switch Project", nullptr);
                if (path)
                {
                    editor->OpenProject(path);
                }
            }
            if (ImGui::MenuItem(ICON_FA_XMARK " Close Project"))
            {
                editor->CloseProject();
            }
            ImGui::Separator();
            if (ImGui::MenuItem(ICON_FA_FOLDER_OPEN " Open Project Folder", nullptr, false, ProjectManager::b_HasOpenProject()))
            {
                EditorUtils::OpenInExplorer(ProjectManager::GetCurrent().m_RootPath);
            }
            ImGui::EndMenu();
        }

        // Global Ctrl+S shortcut (works even when menu is not focused)
        if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_S))
        {
            SaveProjectWithSceneSettings(editor);
        }

        if (ImGui::BeginMenu("View"))
        {
            ImGui::MenuItem(ICON_FA_TERMINAL " Console", nullptr, &editor->m_bShowTerminal);
            ImGui::MenuItem(ICON_FA_CHART_LINE " Performance Stats", nullptr, &editor->m_bShowPerformanceStats);
            ImGui::MenuItem(ICON_FA_LIST " Message Log", nullptr, &editor->bShowMessageLog);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Settings"))
        {
            ImGui::MenuItem(ICON_FA_GEARS " Scene Settings", nullptr, &editor->m_bShowSceneSettings);
            ImGui::MenuItem(ICON_FA_SLIDERS " Editor Preferences", nullptr, &editor->m_bShowEditorPreferences);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Tools"))
        {            
            if (ImGui::MenuItem(ICON_FA_HAMMER " Force Recompile"))
            {
                editor->CompileGameLogic();
            }

            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu("Export"))
        {
            ImGui::MenuItem(ICON_FA_FILE_EXPORT " Export Game", nullptr, &editor->m_bShowExport);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Help"))
        {
            if (ImGui::MenuItem(ICON_FA_GLOBE " GitHub Repository"))
            {
                EditorUtils::OpenURL("https://github.com/ParmarManthanrajsinh/RayWaves-GameEngine");
            }
            if (ImGui::MenuItem(ICON_FA_INFO " About"))
            {
                // TODO: Add about window
            }
            ImGui::EndMenu();
        }

        if (ProjectManager::b_HasOpenProject())
        {
            std::string projName = ProjectManager::GetCurrent().m_Name;
            float textWidth = ImGui::CalcTextSize(projName.c_str()).x;
            ImGui::SameLine(ImGui::GetWindowWidth() - textWidth - 20.0f);
            ImGui::TextDisabled("%s", projName.c_str());
        }

        ImGui::EndMainMenuBar();
    }
}
