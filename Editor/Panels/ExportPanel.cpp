#include "ExportPanel.h"
#include "../GameEditor.h"
#include "../EditorUtils.h"
#include "../ExportService.h"
#include "../../Engine/GameConfig.h"
#include "../../Engine/ProjectManager.h"
#include <imgui.h>
#include <imgui_stdlib.h>
#include <rlImGui.h>
#include <tinyfiledialogs.h>
#include <algorithm>
#include <filesystem>
#include <mutex>
#include <thread>

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
        ImGui::PopItemWidth();
        
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
        ImGui::Text("%s", editor->m_ExportState.m_GameName.c_str());
        ImGui::PopStyleColor();

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Game Icon:");

        ImGui::TableSetColumnIndex(1);
        std::string current_icon = ProjectManager::GetCurrent().m_IconPath;
        if (current_icon.empty()) current_icon = "Default (RayWaves Icon)";
        
        ImGui::TextUnformatted(current_icon.c_str());
        ImGui::SameLine();
        if (ImGui::Button("Browse##icon", ImVec2(80.0f, 0)))
        {
            const char* filters[] = { "*.png" };
            const char* selected = tinyfd_openFileDialog("Select Icon", nullptr, 1, filters, "Icon Files (*.png)", 0);
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
        ImGui::PopItemWidth();
        
        ImGui::SameLine();
        ImGui::Text("x");
        ImGui::SameLine();
        
        ImGui::PushItemWidth(80.0f);
        ImGui::InputInt("##height", &editor->m_ExportState.m_WindowHeight, 0, 0);
        ImGui::PopItemWidth();
        
        ImGui::SameLine();
        ImGui::PushItemWidth(150.0f);
        if (ImGui::BeginCombo("##resolution_presets", "Presets"))
        {
            if (ImGui::Selectable("1920x1080 (Full HD)")) { editor->m_ExportState.m_WindowWidth = 1920; editor->m_ExportState.m_WindowHeight = 1080; }
            if (ImGui::Selectable("1600x900 (HD+)")) { editor->m_ExportState.m_WindowWidth = 1600; editor->m_ExportState.m_WindowHeight = 900; }
            if (ImGui::Selectable("1280x720 (HD)")) { editor->m_ExportState.m_WindowWidth = 1280; editor->m_ExportState.m_WindowHeight = 720; }
            if (ImGui::Selectable("1024x768 (4:3)")) { editor->m_ExportState.m_WindowWidth = 1024; editor->m_ExportState.m_WindowHeight = 768; }
            if (ImGui::Selectable("800x600 (SVGA)")) { editor->m_ExportState.m_WindowWidth = 800; editor->m_ExportState.m_WindowHeight = 600; }
            ImGui::EndCombo();
        }
        ImGui::PopItemWidth();
        
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Window Mode:");

        ImGui::TableSetColumnIndex(1);
        ImGui::Checkbox("Fullscreen", &editor->m_ExportState.m_bFullscreen);
        ImGui::SameLine();
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 20.0f);
        ImGui::Checkbox("Resizable", &editor->m_ExportState.m_bResizable);

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
        ImGui::Checkbox("##b_Vsync", &editor->m_ExportState.m_bVSync);
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
        if (editor->m_ExportState.m_bVSync) ImGui::BeginDisabled();
        
        ImGui::PushItemWidth(80.0f);
        ImGui::InputInt("##target_fps", &editor->m_ExportState.m_TargetFPS, 0, 0);
        ImGui::PopItemWidth();
        
        ImGui::SameLine();
        ImGui::PushItemWidth(100.0f);
        if (ImGui::BeginCombo("##fps_presets", "Presets"))
        {
            if (ImGui::Selectable("30 FPS")) editor->m_ExportState.m_TargetFPS = 30;
            if (ImGui::Selectable("60 FPS")) editor->m_ExportState.m_TargetFPS = 60;
            if (ImGui::Selectable("120 FPS")) editor->m_ExportState.m_TargetFPS = 120;
            if (ImGui::Selectable("144 FPS")) editor->m_ExportState.m_TargetFPS = 144;
            if (ImGui::Selectable("240 FPS")) editor->m_ExportState.m_TargetFPS = 240;
            if (ImGui::Selectable("Unlimited")) editor->m_ExportState.m_TargetFPS = 0;
            ImGui::EndCombo();
        }
        ImGui::PopItemWidth();
        
        if (editor->m_ExportState.m_bVSync) ImGui::EndDisabled();
        
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
        if (current_path.empty()) current_path = "export";

        ImGui::TextUnformatted(current_path.c_str());

        ImGui::SameLine();
        if (ImGui::Button("Browse", ImVec2(80.0f, 0)))
        {
            const char* selected_path = tinyfd_selectFolderDialog("Select Export Folder", nullptr);
            if (selected_path != nullptr)
            {
                fs::path parent_path = fs::path(selected_path);
                editor->m_ExportState.m_ExportPath = parent_path.string();
            }
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
        ImGui::TextWrapped("Note: Close the editor before exporting to avoid file conflicts.");
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
        if (!bCanExport) ImGui::BeginDisabled();
        
        if (ImGui::Button(ICON_FA_PLAY " Start Export", ImVec2(button_width, 40.0f)))
        {
            editor->m_ExportState.m_bIsExporting = true;

            editor->m_ExportState.m_bExportSuccess = false;
            editor->m_ExportState.m_ExportLogs.clear();

            FExportSettings settings;
            settings.m_GameName = editor->m_ExportState.m_GameName;
            settings.m_ExportPath = editor->m_ExportState.m_ExportPath;
            settings.m_WindowWidth = editor->m_ExportState.m_WindowWidth;
            settings.m_WindowHeight = editor->m_ExportState.m_WindowHeight;
            settings.m_TargetFPS = editor->m_ExportState.m_TargetFPS;
            settings.m_bFullscreen = editor->m_ExportState.m_bFullscreen;
            settings.m_bResizable = editor->m_ExportState.m_bResizable;
            settings.m_bVSync = editor->m_ExportState.m_bVSync;
            ExportService::ClampSettings(settings);

            auto cancel = editor->GetThreadCancelFlag();
            editor->m_ExportState.m_ExportThread = std::thread([editor, cancel, settings]()
            {
                if (cancel->load())
                {
                    editor->m_ExportState.m_bIsExporting = false;
                    return;
                }

                FExportResult result = ExportService::RunExport(settings, cancel,
                    [editor](std::string_view line)
                    {
                        std::scoped_lock lk(editor->m_ExportState.m_ExportLogMutex);
                        editor->m_ExportState.m_ExportLogs.emplace_back(line);
                    });

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
        float window_width = ImGui::GetContentRegionAvail().x;
        
        ImGui::SetCursorPosX((window_width - 200.0f) * 0.5f);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 0.7f, 1.0f, 1.0f));
        ImGui::Text("Export in progress...");
        ImGui::PopStyleColor();
        
        ImGui::Spacing();
        

    }

    ImGui::Spacing();

    if (editor->m_ExportState.m_bExportSuccess)
    {
        float window_width = ImGui::GetContentRegionAvail().x;
        ImGui::SetCursorPosX((window_width - ImGui::CalcTextSize("Export Complete!").x) * 0.5f);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 0.8f, 0.2f, 1.0f));
        ImGui::Text("Export Complete!");
        ImGui::PopStyleColor();

        ImGui::Spacing();
        const char* btn_text = ICON_FA_FOLDER_OPEN " Open Output Folder";
        float btn_width = ImGui::CalcTextSize(btn_text).x + (ImGui::GetStyle().FramePadding.x * 2.0f);
        ImGui::SetCursorPosX((window_width - btn_width) * 0.5f);
        if (ImGui::Button(btn_text))
        {
            if (!EditorUtils::OpenInExplorer(editor->m_ExportState.m_ExportPath))
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
                else if (LINE.contains("Completed") || 
                         LINE.contains("SUCCESS")	||
                         LINE.contains("Copied"))
                {
                    text_color = ImVec4(0.3f, 1.0f, 0.3f, 1.0f); 
                }
                else if (LINE.contains("Building")  ||
                         LINE.contains("Creating")  ||
                         LINE.contains("Starting"))
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
