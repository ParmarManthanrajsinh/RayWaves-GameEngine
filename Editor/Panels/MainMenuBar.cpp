#include "MainMenuBar.h"
#include "../../Engine/Profiler.h"
#include "../../Engine/ProjectManager.h"
#include "../Autosave.h"
#include "../EditorUtils.h"
#include "../FileAssociation.h"
#include "../GameEditor.h"
#include <imgui.h>
#include <rlImGui.h>
#include <tinyfiledialogs.h>

static void SaveProjectWithSceneSettings(GameEditor* editor)
{
    if (!ProjectManager::b_HasOpenProject())
        return;
    editor->GetGameEngine().b_SaveActiveScene();
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
            if (ImGui::MenuItem(ICON_FA_FILE_LINES " Save Scene", "Ctrl+S"))
            {
                editor->GetGameEngine().b_SaveActiveScene();
            }

            bool b_has_autosave = false;
            if (ProjectManager::b_HasOpenProject())
            {
                Autosave::FSnapshot snapshot;
                b_has_autosave = Autosave::b_Query(ProjectManager::GetCurrent().m_RootPath, snapshot);
            }
            const bool b_can_restore_autosave = b_has_autosave && !editor->GetGameEngine().GetActiveSceneKey().empty();
            if (ImGui::MenuItem(ICON_FA_ROTATE_LEFT " Restore Autosave", nullptr, false, b_can_restore_autosave))
            {
                editor->b_RestoreAutosave();
            }
            if (ImGui::MenuItem(ICON_FA_TRASH_CAN " Clear Autosave", nullptr, false, b_has_autosave))
            {
                if (Autosave::b_Clear(ProjectManager::GetCurrent().m_RootPath))
                {
                    editor->GetTerminal().add_text("Cleared autosave snapshot.", term::Severity::Debug);
                }
                else
                {
                    editor->GetTerminal().add_text("Failed to clear autosave snapshot.", term::Severity::Error);
                }
            }

            ImGui::Separator();
            if (ImGui::MenuItem(ICON_FA_FOLDER_OPEN " Switch Project..."))
            {
                const char* path = tinyfd_selectFolderDialog("Switch Project", nullptr);
                if (path != nullptr)
                {
                    editor->OpenProject(path);
                }
            }
            if (ImGui::MenuItem(ICON_FA_XMARK " Close Project"))
            {
                editor->CloseProject();
            }
            ImGui::Separator();
            if (ImGui::MenuItem(
                    ICON_FA_FOLDER_OPEN " Open Project Folder", nullptr, false, ProjectManager::b_HasOpenProject()))
            {
                EditorUtils::OpenInExplorer(ProjectManager::GetCurrent().m_RootPath);
            }
            ImGui::EndMenu();
        }

        // Global Ctrl+S shortcut (works even when menu is not focused).
        // Do not steal it from a focused terminal shell.
        if (!editor->GetTerminal().b_IsKeyboardCaptured() && ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_S))
        {
            SaveProjectWithSceneSettings(editor);
        }

        if (ImGui::BeginMenu("View"))
        {
            ImGui::MenuItem(ICON_FA_FOLDER_TREE " FileSystem", nullptr, &editor->m_bShowFileSystem);
            ImGui::MenuItem(ICON_FA_SITEMAP " Scene", nullptr, &editor->m_bShowHierarchy);
            ImGui::MenuItem(ICON_FA_SLIDERS " Inspector", nullptr, &editor->m_bShowInspector);
            ImGui::Separator();
            ImGui::MenuItem(ICON_FA_TERMINAL " Terminal", nullptr, &editor->m_bShowTerminal);
            ImGui::MenuItem(ICON_FA_CHART_LINE " Performance Stats", nullptr, &editor->m_bShowPerformanceStats);
            // bShowMessageLog is atomic (written from the build worker);
            // proxy through a local bool for ImGui.
            bool show_msg_log = editor->bShowMessageLog.load();
            if (ImGui::MenuItem(ICON_FA_LIST " Message Log", nullptr, &show_msg_log))
            {
                editor->bShowMessageLog.store(show_msg_log);
            }
            ImGui::Separator();
            if (ImGui::MenuItem(ICON_FA_ARROW_ROTATE_LEFT " Reset Layout"))
            {
                editor->m_bNeedsLayoutReset = true;
            }
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

            bool bRegistered = IsRayWavesFileAssociationRegistered();
            if (ImGui::MenuItem(ICON_FA_LINK " Register .raywaves file association", nullptr, false, !bRegistered))
            {
                if (RegisterRayWavesFileAssociation())
                {
                    editor->GetTerminal().add_text(
                        ".raywaves file association registered successfully.", term::Severity::Debug);
                }
                else
                {
                    editor->GetTerminal().add_text(
                        "Failed to register .raywaves file association.", term::Severity::Error);
                }
            }
            if (bRegistered)
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
            std::string projName = ProjectManager::GetCurrent().m_Name;
            float textWidth = ImGui::CalcTextSize(projName.c_str()).x;
            ImGui::SameLine(ImGui::GetWindowWidth() - textWidth - 20.0f);
            ImGui::TextDisabled("%s", projName.c_str());
        }

        ImGui::EndMainMenuBar();
    }
}
