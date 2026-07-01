#include "ExportPanel.h"
#include "../GameEditor.h"
#include "../../Engine/GameConfig.h"
#include <imgui.h>
#include <imgui_stdlib.h>
#include <rlImGui.h>
#include <tinyfiledialogs.h>
#include <filesystem>
#include <fstream>
#include <array>

#ifdef _WIN32
#include <stdio.h>
#endif

namespace fs = std::filesystem;

static void s_fAppendLogLine(std::vector<std::string>& logs, std::mutex& mtx, std::string_view line)
{
	std::scoped_lock lk(mtx);
	logs.emplace_back(line);
}

static bool s_bfValidateExportFolder(std::string_view out_dir, std::vector<std::string>& logs, std::mutex& mtx)
{
	bool b_Ok = true;
	
	s_fAppendLogLine(logs, mtx, std::string("Validation working directory: ") + fs::current_path().string());
	s_fAppendLogLine(logs, mtx, std::string("Checking export directory: ").append(out_dir));
	
	auto require = [&](const fs::path& p)
	{
		bool b_Exists = fs::exists(p);
		s_fAppendLogLine(logs, mtx, std::string("Checking: ") + p.string() + " - " + (b_Exists ? "EXISTS" : "MISSING"));
		if (!b_Exists) b_Ok = false;
	};
	
	bool b_FoundGameExe = false;
	std::error_code ec;
	if (fs::exists(out_dir, ec) && !ec) 
	{
		for (const auto& ENTRY : fs::directory_iterator(out_dir, ec)) 
		{
			if (!ec && ENTRY.is_regular_file() && ENTRY.path().extension() == ".exe") 
			{
				b_FoundGameExe = true;
				s_fAppendLogLine(logs, mtx, std::string("Found game executable: ") + ENTRY.path().filename().string());
				break;
			}
		}
	}
	if (!b_FoundGameExe) 
	{
		s_fAppendLogLine(logs, mtx, "Missing: Game executable (.exe file)");
		b_Ok = false;
	}
	
	require(fs::path(out_dir) / "GameLogic.dll");
	require(fs::path(out_dir) / "libraylib.dll");
	
	fs::path assets_path = fs::path(out_dir) / "Assets";
	if (fs::exists(assets_path)) 
	{
		s_fAppendLogLine(logs, mtx, "Found Assets folder in export");
	}
	else 
	{
		s_fAppendLogLine(logs, mtx, "No Assets folder found - this is OK if game has no assets");
	}
	
	return b_Ok;
}

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
        ImGui::Text("%s.exe", editor->m_ExportState.m_GameName.c_str());
        ImGui::PopStyleColor();

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

    if (editor->m_ExportState.m_WindowWidth < 320) editor->m_ExportState.m_WindowWidth = 320;
    if (editor->m_ExportState.m_WindowHeight < 240) editor->m_ExportState.m_WindowHeight = 240;
    if (editor->m_ExportState.m_WindowWidth > 7680) editor->m_ExportState.m_WindowWidth = 7680;
    if (editor->m_ExportState.m_WindowHeight > 4320) editor->m_ExportState.m_WindowHeight = 4320;
    if (editor->m_ExportState.m_TargetFPS < 0) editor->m_ExportState.m_TargetFPS = 0;
    if (editor->m_ExportState.m_TargetFPS > 1000) editor->m_ExportState.m_TargetFPS = 1000;

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

        ImGui::PushItemWidth(300.0f);
        ImGui::InputText("##export_path", current_path.data(), current_path.capacity() + 1, ImGuiInputTextFlags_ReadOnly);
        ImGui::PopItemWidth();

        ImGui::SameLine();
        if (ImGui::Button("Browse", ImVec2(80.0f, 0)))
        {
            const char* selected_path = tinyfd_saveFileDialog("Select Export Folder", fs::current_path().string().c_str(), 0, nullptr, nullptr);
            fs::path parent_path = selected_path ? fs::path(selected_path).parent_path() : fs::current_path();
            editor->m_ExportState.m_ExportPath = parent_path.string();
        }

        ImGui::EndTable();
    }

    ImGui::Spacing();
    
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::GetStyle().Colors[ImGuiCol_FrameBgHovered]);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 4.0f);
    if (ImGui::BeginChild("export_info_box", ImVec2(0, 50), false))
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
        
        if (ImGui::Button(ICON_FA_PLAY " Start Export", ImVec2(button_width, 40.0f)))
        {
            editor->m_ExportState.m_bIsExporting = true;
            editor->m_ExportState.m_bCancelExport = false;
            editor->m_ExportState.m_bExportSuccess = false;
            editor->m_ExportState.m_ExportLogs.clear();

            editor->m_ExportState.m_ExportThread = std::thread([editor]() 
            {
                try 
                {
                    fs::create_directories(editor->m_ExportState.m_ExportPath);
                    
                    s_fAppendLogLine(editor->m_ExportState.m_ExportLogs, editor->m_ExportState.m_ExportLogMutex, "Starting export process...");
                
                    fs::path current_path = fs::current_path();
                
                    bool b_IsDistribution = fs::exists(current_path / "game.exe") && !fs::exists(current_path / "Game" / "game.cpp");
                
                    if (b_IsDistribution) 
                    {
                        s_fAppendLogLine(editor->m_ExportState.m_ExportLogs, editor->m_ExportState.m_ExportLogMutex, "Distribution environment detected - using direct file copy...");
                    
                        fs::path game_exe = current_path / "game.exe";
                        fs::path game_logic_dll = current_path / "GameLogic.dll";
                        fs::path raylib_dll = current_path / "libraylib.dll";
                    
                        if (!fs::exists(game_exe)) 
                        {
                            s_fAppendLogLine(editor->m_ExportState.m_ExportLogs, editor->m_ExportState.m_ExportLogMutex, "ERROR: game.exe not found in distribution!");
                            editor->m_ExportState.m_bExportSuccess = false;
                            editor->m_ExportState.m_bIsExporting = false;
                            return;
                        }
                    
                        if (!fs::exists(game_logic_dll)) 
                        {
                            s_fAppendLogLine(editor->m_ExportState.m_ExportLogs, editor->m_ExportState.m_ExportLogMutex, "ERROR: GameLogic.dll not found in distribution!");
                            editor->m_ExportState.m_bExportSuccess = false;
                            editor->m_ExportState.m_bIsExporting = false;
                            return;
                        }
                    
                        if (!fs::exists(raylib_dll)) 
                        {
                            s_fAppendLogLine(editor->m_ExportState.m_ExportLogs, editor->m_ExportState.m_ExportLogMutex, "ERROR: libraylib.dll not found in distribution!");
                            editor->m_ExportState.m_bExportSuccess = false;
                            editor->m_ExportState.m_bIsExporting = false;
                            return;
                        }
                    
                        fs::path export_dir = current_path / editor->m_ExportState.m_ExportPath;
                        fs::create_directories(export_dir);
                        
                        std::string game_exe_name = editor->m_ExportState.m_GameName + ".exe";
                        s_fAppendLogLine(editor->m_ExportState.m_ExportLogs, editor->m_ExportState.m_ExportLogMutex, "Creating game executable: " + game_exe_name);
                        fs::copy_file(game_exe, export_dir / game_exe_name, fs::copy_options::overwrite_existing);
                        
                        s_fAppendLogLine(editor->m_ExportState.m_ExportLogs, editor->m_ExportState.m_ExportLogMutex, "Creating game configuration...");
                        
                        fs::path config_path = export_dir / "config.ini";
                        std::ofstream config_file(config_path.string());
                        if (config_file.is_open())
                        {
                            GameConfig::GetInstance().ApplyExportSettings
                            (
                                editor->m_ExportState.m_WindowWidth,
                                editor->m_ExportState.m_WindowHeight,
                                editor->m_ExportState.m_bFullscreen,
                                editor->m_ExportState.m_bResizable,
                                editor->m_ExportState.m_bVSync,
                                editor->m_ExportState.m_TargetFPS
                            );

                            config_file << GameConfig::GetInstance().GenerateConfigString();
                            config_file.close();
                        }
                        
                        s_fAppendLogLine(editor->m_ExportState.m_ExportLogs, editor->m_ExportState.m_ExportLogMutex, "Copying GameLogic.dll...");
                        fs::copy_file(game_logic_dll, export_dir / "GameLogic.dll", fs::copy_options::overwrite_existing);
                        
                        s_fAppendLogLine(editor->m_ExportState.m_ExportLogs, editor->m_ExportState.m_ExportLogMutex, "Copying libraylib.dll...");
                        fs::copy_file(raylib_dll, export_dir / "libraylib.dll", fs::copy_options::overwrite_existing);
                        
                        fs::path assets_dir = current_path / "Assets";
                        if (fs::exists(assets_dir)) 
                        {
                            s_fAppendLogLine(editor->m_ExportState.m_ExportLogs, editor->m_ExportState.m_ExportLogMutex, "Copying game assets...");
                            
                            fs::path export_assets_dir = export_dir / "Assets";
                            fs::create_directories(export_assets_dir);
                            
                            for (const auto& ENTRY : fs::directory_iterator(assets_dir))
                            {
                                if (ENTRY.is_directory())
                                {
                                    fs::path dest = export_assets_dir / ENTRY.path().filename();
                                    fs::copy(ENTRY.path(), dest, fs::copy_options::recursive | fs::copy_options::overwrite_existing);
                                    
                                    s_fAppendLogLine(editor->m_ExportState.m_ExportLogs, editor->m_ExportState.m_ExportLogMutex, "Copied asset folder: " + ENTRY.path().filename().string());
                                }
                                else if (ENTRY.is_regular_file())
                                {
                                    fs::path dest = export_assets_dir / ENTRY.path().filename();
                                    fs::copy_file(ENTRY.path(), dest, fs::copy_options::overwrite_existing);
                                    
                                    s_fAppendLogLine(editor->m_ExportState.m_ExportLogs, editor->m_ExportState.m_ExportLogMutex, "Copied asset file: " + ENTRY.path().filename().string());
                                }
                            }
                        }
                        else 
                        {
                            s_fAppendLogLine(editor->m_ExportState.m_ExportLogs, editor->m_ExportState.m_ExportLogMutex, "No Assets folder found - skipping asset copy");
                        }
                        
                        s_fAppendLogLine(editor->m_ExportState.m_ExportLogs, editor->m_ExportState.m_ExportLogMutex, "Export completed successfully!");
                        editor->m_ExportState.m_bExportSuccess = true;
                        
                        editor->m_ExportState.m_bIsExporting = false;
                        return;
                    }
                
                    s_fAppendLogLine(editor->m_ExportState.m_ExportLogs, editor->m_ExportState.m_ExportLogMutex, "Source environment detected - checking for running processes...");
                
                    std::stringstream check_cmd;
                    check_cmd << "powershell -Command \"Get-Process -Name 'main' -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Path\"";
                
#ifdef _WIN32
                    FILE* check_pipe = _popen(check_cmd.str().c_str(), "r");
                    if (check_pipe) 
                    {
                        std::array<char, 1024> buffer{};
                        bool b_FoundRunningProcess = false;
                        while (fgets(buffer.data(), sizeof(buffer), check_pipe) != nullptr) 
                        {
                            if(std::string_view{ buffer.data() }.contains("main.exe"))
                            {
                                b_FoundRunningProcess = true;
                                s_fAppendLogLine(editor->m_ExportState.m_ExportLogs, editor->m_ExportState.m_ExportLogMutex, "WARNING: main.exe is currently running. Export may fail.");
                                s_fAppendLogLine(editor->m_ExportState.m_ExportLogs, editor->m_ExportState.m_ExportLogMutex, "Please close the game editor before exporting for best results.");
                                break;
                            }
                        }

                        _pclose(check_pipe);
                        if (!b_FoundRunningProcess) 
                        {
                            s_fAppendLogLine(editor->m_ExportState.m_ExportLogs, editor->m_ExportState.m_ExportLogMutex, "No conflicting processes found. Proceeding with build...");
                        }
                    }
#endif
                
                    s_fAppendLogLine(editor->m_ExportState.m_ExportLogs, editor->m_ExportState.m_ExportLogMutex, "Building game runtime from source...");

                    s_fAppendLogLine(editor->m_ExportState.m_ExportLogs, editor->m_ExportState.m_ExportLogMutex, std::string("Process completed. Validating export folder: ") + editor->m_ExportState.m_ExportPath);
                
                    bool b_Ok = s_bfValidateExportFolder(editor->m_ExportState.m_ExportPath, editor->m_ExportState.m_ExportLogs, editor->m_ExportState.m_ExportLogMutex);
                    editor->m_ExportState.m_bExportSuccess = b_Ok;
                
                    if (!b_Ok) 
                    {
                        s_fAppendLogLine(editor->m_ExportState.m_ExportLogs, editor->m_ExportState.m_ExportLogMutex, "Export validation failed - check export folder contents");
                    }
                
                    editor->m_ExportState.m_bIsExporting = false;
                }
                catch (const std::exception& e)
                {
                    s_fAppendLogLine(editor->m_ExportState.m_ExportLogs, editor->m_ExportState.m_ExportLogMutex, std::string("CRITICAL ERROR: ") + e.what());
                    editor->m_ExportState.m_bExportSuccess = false;
                    editor->m_ExportState.m_bIsExporting = false;
                }
            });
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
        
        float cancel_width = 100.0f;
        ImGui::SetCursorPosX((window_width - cancel_width) * 0.5f);
        
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.3f, 0.3f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.4f, 0.4f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.6f, 0.2f, 0.2f, 1.0f));
        
        if (ImGui::Button("Cancel", ImVec2(cancel_width, 30.0f)))
        {
            editor->m_ExportState.m_bCancelExport = true; 
        }
        ImGui::PopStyleColor(3);
    }

    ImGui::Spacing();

    if (editor->m_ExportState.m_bExportSuccess)
    {
        float window_width = ImGui::GetContentRegionAvail().x;
        ImGui::SetCursorPosX((window_width - ImGui::CalcTextSize("Export Complete!").x) * 0.5f);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 0.8f, 0.2f, 1.0f));
        ImGui::Text("Export Complete!");
        ImGui::PopStyleColor();
    }
    else if (!editor->m_ExportState.m_bIsExporting && !editor->m_ExportState.m_ExportLogs.empty() && !editor->m_ExportState.m_bIsExporting)
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
    
    if (ImGui::BeginChild("export_log", ImVec2(0, 200), true))
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
