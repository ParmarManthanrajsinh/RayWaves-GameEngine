#include "ExportPanel.h"
#include "../../Engine/GameConfig.h"
#include "../../Engine/PlatformNames.h"
#include "../../Engine/ProjectManager.h"
#include "../EditorUtils.h"
#include "../ExportService.h"
#include "../GameEditor.h"
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <imgui.h>
#include <imgui_stdlib.h>
#include <mutex>
#include <rlImGui.h>
#include <thread>
#include <tinyfiledialogs.h>

namespace fs = std::filesystem;

void ExportPanel::Draw(GameEditor* editor)
{
    if (!editor->m_bShowExport)
    {
        return;
    }

    ImGuiStyle& style = ImGui::GetStyle();
    ImGuiDir old_menu_pos = style.WindowMenuButtonPosition;
    style.WindowMenuButtonPosition = ImGuiDir_None;

    bool b_begin = ImGui::Begin(ICON_FA_FILE_EXPORT " Export", &editor->m_bShowExport);

    style.WindowMenuButtonPosition = old_menu_pos;

    if (!b_begin)
    {
        ImGui::End();
        return;
    }

    if (!editor->m_ExportState.m_bIsExporting && editor->m_ExportState.m_ExportThread.joinable())
    {
        editor->m_ExportState.m_ExportThread.join();
    }

    // The persisted export settings live in the project manifest. Reload them
    // when the open project changes; edits are written back only on commit so
    // dragging a widget never hits the disk.
    if (ProjectManager::b_HasOpenProject())
    {
        const t_Project& project = ProjectManager::GetCurrent();
        if (m_LoadedProjectRoot != project.m_RootPath)
        {
            m_LoadedProjectRoot = project.m_RootPath;
            editor->m_ExportState.m_GameName = project.m_ExportName.empty() ? project.m_Name : project.m_ExportName;
            editor->m_ExportState.m_ExportPath = project.m_ExportPath;
            editor->m_ExportState.m_WindowWidth = std::clamp(project.m_ExportWidth, 320, 7680);
            editor->m_ExportState.m_WindowHeight = std::clamp(project.m_ExportHeight, 240, 4320);
            editor->m_ExportState.m_TargetFPS = std::clamp(project.m_ExportFPS, 0, 1000);
            editor->m_ExportState.m_bFullscreen = project.m_bExportFullscreen;
            editor->m_ExportState.m_bResizable = project.m_bExportResizable;
            editor->m_ExportState.m_bVSync = project.m_bExportVSync;
        }
    }
    else
    {
        // Reopening the same folder must reload, so forget the tracked root
        // while no project is open.
        m_LoadedProjectRoot.clear();
    }

    // Dimensions are clamped while persisting so the manifest never stores a
    // value the widgets would correct on the next frame.
    auto sync_export_settings = [editor]()
    {
        if (!ProjectManager::b_HasOpenProject())
            return;

        t_Project& project = ProjectManager::GetCurrent();
        const auto& state = editor->m_ExportState;
        project.m_ExportName = state.m_GameName;
        project.m_ExportPath = state.m_ExportPath;
        project.m_ExportWidth = std::clamp(state.m_WindowWidth, 320, 7680);
        project.m_ExportHeight = std::clamp(state.m_WindowHeight, 240, 4320);
        project.m_ExportFPS = std::clamp(state.m_TargetFPS, 0, 1000);
        project.m_bExportFullscreen = state.m_bFullscreen;
        project.m_bExportResizable = state.m_bResizable;
        project.m_bExportVSync = state.m_bVSync;
        ProjectManager::b_SaveCurrentProject();
    };

    ImGui::Spacing();
    ImGui::SeparatorText("Game Configuration");
    ImGui::Spacing();

    if (ImGui::BeginTable("##export_config_props", 2, ImGuiTableFlags_None))
    {
        ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 140.0f);
        ImGui::TableSetupColumn("Widget", ImGuiTableColumnFlags_WidthStretch);

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Game Name:");

        ImGui::TableSetColumnIndex(1);
        ImGui::PushItemWidth(250.0f);
        ImGui::InputText("##game_name", &editor->m_ExportState.m_GameName);
        if (ImGui::IsItemDeactivatedAfterEdit())
            sync_export_settings();
        ImGui::PopItemWidth();

        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
        const std::string executable_name = rw::platform::ExecutableName(editor->m_ExportState.m_GameName);
        ImGui::TextUnformatted(executable_name.c_str());
        ImGui::PopStyleColor();

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Game Icon:");

        ImGui::TableSetColumnIndex(1);
        std::string current_icon = ProjectManager::GetCurrent().m_IconPath;
        if (current_icon.empty())
            current_icon = "Default (RayWaves Icon)";

        ImGui::TextUnformatted(current_icon.c_str());
        ImGui::SameLine();
        if (ImGui::Button("Browse##icon", ImVec2(80.0f, 0)))
        {
            const char* filters[] = { "*.ico" };
            const char* selected = tinyfd_openFileDialog("Select Icon", nullptr, 1, filters, "Icon Files (*.ico)", 0);
            if (selected != nullptr)
            {
                ProjectManager::GetCurrent().m_IconPath = selected;
                ProjectManager::b_SaveCurrentProject();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Clear##icon", ImVec2(80.0f, 0)))
        {
            ProjectManager::GetCurrent().m_IconPath = "";
            ProjectManager::b_SaveCurrentProject();
        }

        ImGui::EndTable();
    }

    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::SeparatorText("Display Settings");
    ImGui::Spacing();

    if (ImGui::BeginTable("##export_display_props", 2, ImGuiTableFlags_None))
    {
        ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 140.0f);
        ImGui::TableSetupColumn("Widget", ImGuiTableColumnFlags_WidthStretch);

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Resolution:");

        ImGui::TableSetColumnIndex(1);
        ImGui::PushItemWidth(80.0f);
        ImGui::InputInt("##width", &editor->m_ExportState.m_WindowWidth, 0, 0);
        if (ImGui::IsItemDeactivatedAfterEdit())
            sync_export_settings();
        ImGui::PopItemWidth();

        ImGui::SameLine();
        ImGui::Text("x");
        ImGui::SameLine();

        ImGui::PushItemWidth(80.0f);
        ImGui::InputInt("##height", &editor->m_ExportState.m_WindowHeight, 0, 0);
        if (ImGui::IsItemDeactivatedAfterEdit())
            sync_export_settings();
        ImGui::PopItemWidth();

        ImGui::SameLine();
        ImGui::PushItemWidth(150.0f);
        bool b_resolution_preset_changed = false;
        if (ImGui::BeginCombo("##resolution_presets", "Presets"))
        {
            if (ImGui::Selectable("1920x1080 (Full HD)"))
            {
                editor->m_ExportState.m_WindowWidth = 1920;
                editor->m_ExportState.m_WindowHeight = 1080;
                b_resolution_preset_changed = true;
            }
            if (ImGui::Selectable("1600x900 (HD+)"))
            {
                editor->m_ExportState.m_WindowWidth = 1600;
                editor->m_ExportState.m_WindowHeight = 900;
                b_resolution_preset_changed = true;
            }
            if (ImGui::Selectable("1280x720 (HD)"))
            {
                editor->m_ExportState.m_WindowWidth = 1280;
                editor->m_ExportState.m_WindowHeight = 720;
                b_resolution_preset_changed = true;
            }
            if (ImGui::Selectable("1024x768 (4:3)"))
            {
                editor->m_ExportState.m_WindowWidth = 1024;
                editor->m_ExportState.m_WindowHeight = 768;
                b_resolution_preset_changed = true;
            }
            if (ImGui::Selectable("800x600 (SVGA)"))
            {
                editor->m_ExportState.m_WindowWidth = 800;
                editor->m_ExportState.m_WindowHeight = 600;
                b_resolution_preset_changed = true;
            }
            ImGui::EndCombo();
        }
        if (b_resolution_preset_changed)
            sync_export_settings();
        ImGui::PopItemWidth();

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Window Mode:");

        ImGui::TableSetColumnIndex(1);
        if (ImGui::Checkbox("Fullscreen", &editor->m_ExportState.m_bFullscreen))
            sync_export_settings();
        ImGui::SameLine();
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 20.0f);
        if (ImGui::Checkbox("Resizable", &editor->m_ExportState.m_bResizable))
            sync_export_settings();

        ImGui::EndTable();
    }

    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::SeparatorText("Performance Settings");
    ImGui::Spacing();

    if (ImGui::BeginTable("##export_perf_props", 2, ImGuiTableFlags_None))
    {
        ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 140.0f);
        ImGui::TableSetupColumn("Widget", ImGuiTableColumnFlags_WidthStretch);

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::AlignTextToFramePadding();
        ImGui::Text("V-Sync:");

        ImGui::TableSetColumnIndex(1);
        if (ImGui::Checkbox("##b_Vsync", &editor->m_ExportState.m_bVSync))
            sync_export_settings();
        if (editor->m_ExportState.m_bVSync)
        {
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
            ImGui::Text("(Locks FPS to display refresh rate)");
            ImGui::PopStyleColor();
        }

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Target FPS:");

        ImGui::TableSetColumnIndex(1);
        if (editor->m_ExportState.m_bVSync)
            ImGui::BeginDisabled();

        ImGui::PushItemWidth(80.0f);
        ImGui::InputInt("##target_fps", &editor->m_ExportState.m_TargetFPS, 0, 0);
        if (ImGui::IsItemDeactivatedAfterEdit())
            sync_export_settings();
        ImGui::PopItemWidth();

        ImGui::SameLine();
        ImGui::PushItemWidth(100.0f);
        bool b_fps_preset_changed = false;
        if (ImGui::BeginCombo("##fps_presets", "Presets"))
        {
            if (ImGui::Selectable("30 FPS"))
            {
                editor->m_ExportState.m_TargetFPS = 30;
                b_fps_preset_changed = true;
            }
            if (ImGui::Selectable("60 FPS"))
            {
                editor->m_ExportState.m_TargetFPS = 60;
                b_fps_preset_changed = true;
            }
            if (ImGui::Selectable("120 FPS"))
            {
                editor->m_ExportState.m_TargetFPS = 120;
                b_fps_preset_changed = true;
            }
            if (ImGui::Selectable("144 FPS"))
            {
                editor->m_ExportState.m_TargetFPS = 144;
                b_fps_preset_changed = true;
            }
            if (ImGui::Selectable("240 FPS"))
            {
                editor->m_ExportState.m_TargetFPS = 240;
                b_fps_preset_changed = true;
            }
            if (ImGui::Selectable("Unlimited"))
            {
                editor->m_ExportState.m_TargetFPS = 0;
                b_fps_preset_changed = true;
            }
            ImGui::EndCombo();
        }
        if (b_fps_preset_changed)
            sync_export_settings();
        ImGui::PopItemWidth();

        if (editor->m_ExportState.m_bVSync)
            ImGui::EndDisabled();

        if (editor->m_ExportState.m_TargetFPS == 0 && !editor->m_ExportState.m_bVSync)
        {
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
            ImGui::Text("(Unlimited)");
            ImGui::PopStyleColor();
        }

        ImGui::EndTable();
    }

    editor->m_ExportState.m_WindowWidth = std::max(editor->m_ExportState.m_WindowWidth, 320);
    editor->m_ExportState.m_WindowHeight = std::max(editor->m_ExportState.m_WindowHeight, 240);
    editor->m_ExportState.m_WindowWidth = std::min(editor->m_ExportState.m_WindowWidth, 7680);
    editor->m_ExportState.m_WindowHeight = std::min(editor->m_ExportState.m_WindowHeight, 4320);
    editor->m_ExportState.m_TargetFPS = std::max(editor->m_ExportState.m_TargetFPS, 0);
    editor->m_ExportState.m_TargetFPS = std::min(editor->m_ExportState.m_TargetFPS, 1000);

    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::SeparatorText("Export Settings");
    ImGui::Spacing();

    if (ImGui::BeginTable("##export_folder_props", 2, ImGuiTableFlags_None))
    {
        ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 140.0f);
        ImGui::TableSetupColumn("Widget", ImGuiTableColumnFlags_WidthStretch);

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Output Folder:");

        ImGui::TableSetColumnIndex(1);
        std::string current_path = std::string(editor->m_ExportState.m_ExportPath);
        if (current_path.empty())
            current_path = "export";

        ImGui::TextUnformatted(current_path.c_str());

        ImGui::SameLine();
        if (ImGui::Button("Browse", ImVec2(80.0f, 0)))
        {
            const char* selected_path = tinyfd_selectFolderDialog("Select Export Folder", nullptr);
            if (selected_path != nullptr)
            {
                fs::path parent_path = fs::path(selected_path);
                editor->m_ExportState.m_ExportPath = parent_path.string();
                sync_export_settings();
            }
        }

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Clean Output:");

        ImGui::TableSetColumnIndex(1);
        // Session-only option; there is no clean field in the manifest.
        ImGui::Checkbox("Clean output folder before export##clean_output", &m_bCleanOutput);
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Deletes the existing export folder before copying. "
                              "The project root is never deleted.");
        }

        ImGui::EndTable();
    }

    ImGui::Spacing();

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::GetStyle().Colors[ImGuiCol_FrameBgHovered]);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 4.0f);
    if (ImGui::BeginChild("export_info_box", ImVec2(0, 50), 0))
    {
        ImGui::SetCursorPos(ImVec2(10, 10));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.6f, 1.0f, 1.0f));
        ImGui::Text(ICON_FA_CIRCLE_INFO);
        ImGui::PopStyleColor();

        ImGui::SameLine();
        ImGui::SetCursorPosY(10);
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
        ImGui::TextWrapped(
            "Export builds GameLogic into .raywaves/build while holding the project build lock. Builds and saves "
            "wait until it finishes.");
        ImGui::PopStyleColor();
    }
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    ImGui::Spacing();
    ImGui::Spacing();

    if (!editor->m_ExportState.m_bIsExporting)
    {
        float button_width = 200.0f;
        float window_width = ImGui::GetContentRegionAvail().x;
        ImGui::SetCursorPosX((window_width - button_width) * 0.5f);

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.839f, 0.188f, 0.192f, 0.8f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.839f, 0.188f, 0.192f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.6f, 0.1f, 0.1f, 1.0f));

        bool bCanExport = !editor->m_ExportState.m_ExportPath.empty();
        if (!bCanExport)
            ImGui::BeginDisabled();

        if (ImGui::Button(ICON_FA_PLAY " Start Export", ImVec2(button_width, 40.0f)))
        {
            editor->m_ExportState.m_bIsExporting = true;

            editor->m_ExportState.m_bExportSuccess = false;
            editor->m_ExportState.m_ExportLogs.clear();

            m_ExportProgress.store(0.0f);
            {
                std::scoped_lock lk(m_ExportStatusMutex);
                m_ExportStage.clear();
                m_OutputPath.clear();
            }

            FExportSettings settings;
            settings.m_GameName = editor->m_ExportState.m_GameName;
            settings.m_ExportPath = editor->m_ExportState.m_ExportPath;
            settings.m_WindowWidth = editor->m_ExportState.m_WindowWidth;
            settings.m_WindowHeight = editor->m_ExportState.m_WindowHeight;
            settings.m_TargetFPS = editor->m_ExportState.m_TargetFPS;
            settings.m_bFullscreen = editor->m_ExportState.m_bFullscreen;
            settings.m_bResizable = editor->m_ExportState.m_bResizable;
            settings.m_bVSync = editor->m_ExportState.m_bVSync;
            settings.m_bCleanOutput = m_bCleanOutput;
            ExportService::ClampSettings(settings);

            // Share the editor shutdown flag so closing the window stops the
            // worker too. Clear it first: a previous cancel leaves it set.
            auto cancel = editor->GetThreadCancelFlag();
            cancel->store(false);
            m_ExportCancel = cancel;

            editor->m_ExportState.m_ExportThread = std::thread(
                [this, editor, cancel, settings]()
                {
                    if (cancel->load())
                    {
                        editor->m_ExportState.m_bIsExporting = false;
                        return;
                    }

                    auto progress = [this](float value, std::string_view stage)
                    {
                        m_ExportProgress.store(value);
                        std::scoped_lock lk(m_ExportStatusMutex);
                        m_ExportStage.assign(stage);
                    };

                    FExportResult result = ExportService::RunExport(
                        settings, cancel,
                        [editor](std::string_view line)
                        {
                            std::scoped_lock lk(editor->m_ExportState.m_ExportLogMutex);
                            editor->m_ExportState.m_ExportLogs.emplace_back(line);
                        },
                        progress);

                    {
                        std::scoped_lock lk(m_ExportStatusMutex);
                        m_OutputPath = result.m_OutputPath;
                    }

                    editor->m_ExportState.m_bExportSuccess = result.m_bSuccess;
                    editor->m_ExportState.m_bIsExporting = false;
                });
        }

        if (!bCanExport)
        {
            ImGui::EndDisabled();
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            {
                ImGui::SetTooltip("Please select an export folder first.");
            }
        }

        ImGui::PopStyleColor(3);
    }
    else
    {
        const float progress_value = m_ExportProgress.load();
        std::string stage;
        {
            std::scoped_lock lk(m_ExportStatusMutex);
            stage = m_ExportStage;
        }

        const float window_width = ImGui::GetContentRegionAvail().x;

        ImGui::SetCursorPosX((window_width - ImGui::CalcTextSize("Export in progress...").x) * 0.5f);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 0.7f, 1.0f, 1.0f));
        ImGui::Text("Export in progress...");
        ImGui::PopStyleColor();

        ImGui::Spacing();

        // No milestone yet means the pipeline is still starting (or the host
        // passed no progress sink), so animate an indeterminate bar.
        if (progress_value <= 0.0f && stage.empty())
        {
            ImGui::ProgressBar(-static_cast<float>(ImGui::GetTime()), ImVec2(-1.0f, 0.0f), "Starting...");
        }
        else
        {
            char overlay[256];
            if (stage.empty())
            {
                std::snprintf(overlay, sizeof(overlay), "%.0f%%", progress_value * 100.0f);
            }
            else
            {
                std::snprintf(overlay, sizeof(overlay), "%s (%.0f%%)", stage.c_str(), progress_value * 100.0f);
            }
            ImGui::ProgressBar(progress_value, ImVec2(-1.0f, 0.0f), overlay);
        }

        ImGui::Spacing();

        const char* cancel_text = ICON_FA_STOP " Cancel Export";
        const float cancel_width = ImGui::CalcTextSize(cancel_text).x + (ImGui::GetStyle().FramePadding.x * 2.0f);
        ImGui::SetCursorPosX((window_width - cancel_width) * 0.5f);
        if (ImGui::Button(cancel_text) && m_ExportCancel)
        {
            m_ExportCancel->store(true);
        }

        ImGui::Spacing();
    }

    ImGui::Spacing();

    if (editor->m_ExportState.m_bExportSuccess)
    {
        std::string output_path;
        {
            std::scoped_lock lk(m_ExportStatusMutex);
            output_path = m_OutputPath;
        }
        if (output_path.empty())
        {
            output_path = editor->m_ExportState.m_ExportPath;
        }

        float window_width = ImGui::GetContentRegionAvail().x;
        ImGui::SetCursorPosX((window_width - ImGui::CalcTextSize("Export Complete!").x) * 0.5f);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 0.8f, 0.2f, 1.0f));
        ImGui::Text("Export Complete!");
        ImGui::PopStyleColor();

        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
        ImGui::TextWrapped("Output: %s", output_path.c_str());
        ImGui::PopStyleColor();

        ImGui::Spacing();
        const char* btn_text = ICON_FA_FOLDER_OPEN " Open Output Folder";
        float btn_width = ImGui::CalcTextSize(btn_text).x + (ImGui::GetStyle().FramePadding.x * 2.0f);
        ImGui::SetCursorPosX((window_width - btn_width) * 0.5f);
        if (ImGui::Button(btn_text))
        {
            if (output_path.empty() || !EditorUtils::OpenInExplorer(fs::path(output_path)))
            {
                std::scoped_lock lk(editor->m_ExportState.m_ExportLogMutex);
                editor->m_ExportState.m_ExportLogs.emplace_back("ERROR: Failed to open output folder.");
            }
        }
    }
    else if (!editor->m_ExportState.m_bIsExporting && !editor->m_ExportState.m_ExportLogs.empty())
    {
        float window_width = ImGui::GetContentRegionAvail().x;
        ImGui::SetCursorPosX((window_width - ImGui::CalcTextSize("Export Failed").x) * 0.5f);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
        ImGui::Text("Export Failed");
        ImGui::PopStyleColor();
    }

    ImGui::Separator();
    ImGui::Spacing();
    ImGui::Text("Export Log");
    ImGui::Separator();

    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 5.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::GetStyle().Colors[ImGuiCol_FrameBg]);
    ImGui::PushStyleColor(ImGuiCol_Border, ImGui::GetStyle().Colors[ImGuiCol_Border]);

    if (ImGui::BeginChild("export_log", ImVec2(0, 200), 1))
    {
        std::scoped_lock lk(editor->m_ExportState.m_ExportLogMutex);

        if (editor->m_ExportState.m_ExportLogs.empty())
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
            ImGui::Text("Export log will appear here...");
            ImGui::PopStyleColor();
        }
        else
        {
            for (const auto& LINE : editor->m_ExportState.m_ExportLogs)
            {
                ImVec4 text_color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);

                if (LINE.contains("ERROR:"))
                {
                    text_color = ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
                }
                else if (LINE.contains("WARNING:"))
                {
                    text_color = ImVec4(1.0f, 0.8f, 0.3f, 1.0f);
                }
                else if (LINE.contains("Completed") || LINE.contains("SUCCESS") || LINE.contains("Copied"))
                {
                    text_color = ImVec4(0.3f, 1.0f, 0.3f, 1.0f);
                }
                else if (LINE.contains("Building") || LINE.contains("Creating") || LINE.contains("Starting"))
                {
                    text_color = ImVec4(0.3f, 0.8f, 1.0f, 1.0f);
                }

                ImGui::PushStyleColor(ImGuiCol_Text, text_color);
                ImGui::TextUnformatted(LINE.c_str());
                ImGui::PopStyleColor();
            }

            if (editor->m_ExportState.m_bIsExporting)
            {
                ImGui::SetScrollHereY(1.0f);
            }
        }
    }
    ImGui::EndChild();

    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);

    if (!editor->m_ExportState.m_bIsExporting && editor->m_ExportState.m_ExportThread.joinable())
    {
        editor->m_ExportState.m_ExportThread.join();
    }

    ImGui::End();
}
