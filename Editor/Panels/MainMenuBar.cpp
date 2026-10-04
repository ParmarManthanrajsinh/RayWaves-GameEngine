#include "MainMenuBar.h"
#include "../GameEditor.h"
#include "../../Engine/ProjectManager.h"
#include "../FileAssociation.h"
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
                const std::string DIALOG_DIR = EditorUtils::DefaultDialogDir();
                const char* PATH = tinyfd_selectFolderDialog("Switch Project", DIALOG_DIR.c_str());
                if (PATH != nullptr)
                {
                    editor->OpenProject(PATH);
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
            if (ProjectManager::b_HasOpenProject())
            {
                ImGui::MenuItem(ICON_FA_GEARS " Project Settings", nullptr, &editor->m_bShowSceneSettings);
            }
            ImGui::MenuItem(ICON_FA_SLIDERS " Editor Preferences", nullptr, &editor->m_bShowEditorPreferences);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Tools"))
        {            
            if (ImGui::MenuItem(ICON_FA_HAMMER " Force Recompile"))
            {
                editor->CompileGameLogic();
            }

            ImGui::Separator();

            bool b_Registered = IsRayWavesFileAssociationRegistered();
            if (ImGui::MenuItem(ICON_FA_LINK " Register .raywaves file association",
                nullptr, false, !b_Registered))
            {
                if (RegisterRayWavesFileAssociation())
                {
                    editor->GetTerminal().add_text
                    (
                        ".raywaves file association registered successfully.",
                        term::Severity::Debug
                    );
                }
                else
                {
                    editor->GetTerminal().add_text
                    (
                        "Failed to register .raywaves file association.",
                        term::Severity::Error
                    );
                }
            }
            if (b_Registered)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
                ImGui::MenuItem(ICON_FA_CHECK " .raywaves association active", nullptr, nullptr, false);
                ImGui::PopStyleColor();
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
            std::string proj_name = ProjectManager::GetCurrent().m_Name;
            float text_width = ImGui::CalcTextSize(proj_name.c_str()).x;
            ImGui::SameLine(ImGui::GetWindowWidth() - text_width - 20.0f);
            ImGui::TextDisabled("%s", proj_name.c_str());
        }

        ImGui::EndMainMenuBar();
    }
}
