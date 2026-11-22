#include "MainMenuBar.h"
#include "../../Engine/MapManager.h"
#include "../../Engine/Profiler.h"
#include "../../Engine/ProjectManager.h"
#include "../Autosave.h"
#include "../EditorUtils.h"
#include "../FileAssociation.h"
#include "../GameEditor.h"
#include <imgui.h>
#include <rlImGui.h>
#include <tinyfiledialogs.h>

static void TogglePlayPause(GameEditor* editor)
{
    switch (editor->GetPlayState())
    {
        case GameEditor::EPlayState::Stopped:
            editor->SetPlayState(GameEditor::EPlayState::Playing);
            break;
        case GameEditor::EPlayState::Playing:
            editor->SetPlayState(GameEditor::EPlayState::Paused);
            break;
        case GameEditor::EPlayState::Paused:
            editor->SetPlayState(GameEditor::EPlayState::Playing);
            break;
    }
}

static void RestartPlayback(GameEditor* editor)
{
    editor->SetPlayState(GameEditor::EPlayState::Stopped);
    editor->InvalidateEditorEntities();
    if (MapManager* map_manager = editor->GetMapManager())
    {
        map_manager->b_ReloadCurrentMap();
    }
}

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

        // Undo/redo are global, but a focused text field still owns Ctrl+Z for
        // its own edit history. Check redo first: Ctrl+Shift+Z also satisfies
        // the Ctrl+Z chord.
        const bool b_keyboard_free = !editor->GetTerminal().b_IsKeyboardCaptured() && !ImGui::GetIO().WantTextInput;
        if (b_keyboard_free &&
            (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_Z) ||
                ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_Y)))
        {
            editor->GetCommandStack().Redo();
        }
        else if (b_keyboard_free && ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_Z))
        {
            editor->GetCommandStack().Undo();
        }

        // Playback shortcuts mirror the Play menu. They use the same text-field
        // guard so typing in the terminal or a field never triggers them.
        if (b_keyboard_free)
        {
            if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_F5))
            {
                if (editor->GetMapManager() != nullptr)
                {
                    RestartPlayback(editor);
                }
            }
            else if (ImGui::IsKeyChordPressed(ImGuiMod_Shift | ImGuiKey_F5))
            {
                editor->SetPlayState(GameEditor::EPlayState::Stopped);
            }
            else if (ImGui::IsKeyChordPressed(ImGuiKey_F5))
            {
                TogglePlayPause(editor);
            }
            else if (ImGui::IsKeyChordPressed(ImGuiKey_F7) && editor->GetPlayState() != GameEditor::EPlayState::Playing)
            {
                editor->StepOnce();
            }
        }

        if (ImGui::BeginMenu("Play"))
        {
            const GameEditor::EPlayState play_state = editor->GetPlayState();
            const char* play_icon = ICON_FA_PLAY;
            const char* play_label = " Play";
            if (play_state == GameEditor::EPlayState::Playing)
            {
                play_icon = ICON_FA_PAUSE;
                play_label = " Pause";
            }
            else if (play_state == GameEditor::EPlayState::Paused)
            {
                play_icon = ICON_FA_PLAY;
                play_label = " Resume";
            }

            if (ImGui::MenuItem((std::string(play_icon) + play_label).c_str(), "F5"))
            {
                TogglePlayPause(editor);
            }
            if (ImGui::MenuItem(ICON_FA_STOP " Stop", "Shift+F5", false, play_state != GameEditor::EPlayState::Stopped))
            {
                editor->SetPlayState(GameEditor::EPlayState::Stopped);
            }
            if (ImGui::MenuItem(
                    ICON_FA_FORWARD_STEP " Step", "F7", false, play_state != GameEditor::EPlayState::Playing))
            {
                editor->StepOnce();
            }
            if (ImGui::MenuItem(
                    ICON_FA_ARROW_ROTATE_RIGHT " Restart", "Ctrl+F5", false, editor->GetMapManager() != nullptr))
            {
                RestartPlayback(editor);
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Edit"))
        {
            CommandStack& commands = editor->GetCommandStack();
            const IEditorCommand* undo = commands.PeekUndo();
            const IEditorCommand* redo = commands.PeekRedo();

            const std::string undo_label = undo != nullptr ? "Undo " + std::string(undo->Name()) : std::string("Undo");
            const std::string redo_label = redo != nullptr ? "Redo " + std::string(redo->Name()) : std::string("Redo");

            if (ImGui::MenuItem(
                    (std::string(ICON_FA_ROTATE_LEFT) + " " + undo_label).c_str(), "Ctrl+Z", false, undo != nullptr))
            {
                commands.Undo();
            }
            if (ImGui::MenuItem(
                    (std::string(ICON_FA_ROTATE_RIGHT) + " " + redo_label).c_str(), "Ctrl+Y", false, redo != nullptr))
            {
                commands.Redo();
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View"))
        {
            ImGui::MenuItem(ICON_FA_FOLDER_TREE " FileSystem", nullptr, &editor->m_bShowFileSystem);
            ImGui::MenuItem(ICON_FA_SITEMAP " Scene", nullptr, &editor->m_bShowHierarchy);
            ImGui::MenuItem(ICON_FA_SLIDERS " Inspector", nullptr, &editor->m_bShowInspector);
            ImGui::Separator();
            ImGui::MenuItem(ICON_FA_TERMINAL " Terminal", nullptr, &editor->m_bShowTerminal);
            ImGui::MenuItem(ICON_FA_LIST " Log", nullptr, &editor->m_bShowLog);
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
