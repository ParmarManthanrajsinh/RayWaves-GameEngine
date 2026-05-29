#include "MessageLogPanel.h"
#include "../GameEditor.h"
#include <imgui.h>
#include <rlImGui.h>
#include <vector>
#include <mutex>

void MessageLogPanel::Draw(GameEditor* editor)
{
    // Draw Notifications
	if (editor->BuildStatus != EBuildStatus::None)
	{
        if (editor->NotificationTimer > 0.0f || editor->BuildStatus == EBuildStatus::Compiling || editor->BuildStatus == EBuildStatus::Failed)
        {
            if (editor->BuildStatus == EBuildStatus::Success)
            {
                editor->NotificationTimer -= GetFrameTime();
            }
                
            ImGuiIO& io = ImGui::GetIO();
            ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 20.0f, io.DisplaySize.y - 20.0f), ImGuiCond_Always, ImVec2(1.0f, 1.0f));
            ImGui::SetNextWindowBgAlpha(0.85f);
            
            ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove;
            
            if (ImGui::Begin("BuildNotification", nullptr, flags))
            {
                if (editor->BuildStatus == EBuildStatus::Compiling)
                {
                    ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), ICON_FA_HAMMER " Compiling...");
                }
                else if (editor->BuildStatus == EBuildStatus::Success)
                {
                    ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), ICON_FA_CHECK " Compile Successful!");
                }
                else if (editor->BuildStatus == EBuildStatus::Failed)
                {
                    ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), ICON_FA_XMARK " Compile Failed!");
                    ImGui::Separator();
                    if (ImGui::Button("View Message Log"))
                    {
                        editor->bShowMessageLog = true;
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Dismiss"))
                    {
                        editor->BuildStatus = EBuildStatus::None;
                    }
                }
            }
            ImGui::End();
        }
	}

    // Draw Message Log
    if (!editor->bShowMessageLog) 
	{
		return;
	}

    ImGui::SetNextWindowSize(ImVec2(800, 300), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Message Log", &editor->bShowMessageLog))
    {
        float content_width = ImGui::GetContentRegionAvail().x;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + content_width - 60.0f);
        if (ImGui::Button("Clear", ImVec2(60, 0)))
        {
            std::lock_guard<std::mutex> lock(editor->BuildMessagesMutex);
            editor->BuildMessages.clear();
        }
        ImGui::Separator();

        std::vector<FBuildMessage> local_messages;
        {
            std::lock_guard<std::mutex> lock(editor->BuildMessagesMutex);
            local_messages = editor->BuildMessages;
        }

        if (ImGui::BeginTable("MessageTable", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY))
        {
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 80.0f);
            ImGui::TableSetupColumn("Location", ImGuiTableColumnFlags_WidthFixed, 150.0f);
            ImGui::TableSetupColumn("Description", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableHeadersRow();

            for (const auto& msg : local_messages)
            {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                
                ImGui::Dummy(ImVec2(0, 2));
                if (msg.Severity == FBuildMessage::ESeverity::Error)
				{
					ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), ICON_FA_XMARK " Error");
				}
                else
				{
					ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.2f, 1.0f), ICON_FA_TRIANGLE_EXCLAMATION " Warn");
				}
                
                ImGui::TableNextColumn();
                ImGui::Dummy(ImVec2(0, 2));
                if (!msg.File.empty())
                {
                    std::string filename = msg.File;
                    size_t slash = filename.find_last_of("/\\");
                    if (slash != std::string::npos)
					{
						filename = filename.substr(slash + 1);
					}

                    ImGui::Text("%s:%d", filename.c_str(), msg.Line);
                    if (ImGui::IsItemHovered()) 
					{
						ImGui::SetTooltip("%s:%d", msg.File.c_str(), msg.Line);
					}
                }
                else
                {
                    ImGui::Text("-");
                }

                ImGui::TableNextColumn();
                ImGui::Dummy(ImVec2(0, 2));
                ImGui::TextWrapped("%s", msg.Text.c_str());
                ImGui::Dummy(ImVec2(0, 2));
            }
            ImGui::EndTable();
        }
    }
    ImGui::End();
}
