#include "MainMenuBar.h"
#include "../GameEditor.h"
#include "../../Engine/ProjectManager.h"
#include <imgui.h>
#include <rlImGui.h>
#include <tinyfiledialogs.h>

void MainMenuBar::Draw(GameEditor* editor)
{
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem(ICON_FA_FLOPPY_DISK " Save Project", "Ctrl+S"))
            {
                ProjectManager::b_SaveCurrentProject();
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
                ProjectManager::CloseProject();
            }
            ImGui::EndMenu();
        }

        // Global Ctrl+S shortcut (works even when menu is not focused)
        if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_S))
        {
            ProjectManager::b_SaveCurrentProject();
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

        ImGui::EndMainMenuBar();
    }
}
