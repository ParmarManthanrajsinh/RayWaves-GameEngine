#include "MapSelectionPanel.h"
#include "../GameEditor.h"
#include "../../Engine/MapManager.h"
#include <imgui.h>
#include <rlImGui.h>
#include <vector>
#include <string>
#include "../../Engine/Profiler.h"

void MapSelectionPanel::Draw(GameEditor* editor)
{
	SCOPED_TIMER("panel_map_selection");
	if (!editor->GetMapManager()) 
	{
		return;
	}
	ImGui::Begin(ICON_FA_MAP " Map Selection", nullptr, ImGuiWindowFlags_NoCollapse);
    
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 4.0f);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::GetStyle().Colors[ImGuiCol_FrameBg]);
    ImGui::BeginChild("map_status_box", ImVec2(0, 80), false, ImGuiWindowFlags_NoScrollbar);
    ImGui::SetCursorPos(ImVec2(10, 10));
    ImGui::TextDisabled("Map Status");
    
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImGui::GetWindowDrawList()->AddCircleFilled(ImVec2(p.x + ImGui::GetContentRegionAvail().x - 10, p.y - 10), 4.0f, IM_COL32(214, 48, 49, 255));
    
    ImGui::SetCursorPos(ImVec2(10, 40));
    ImGui::Text("Current Map:");
    {
        std::string_view curId = editor->GetMapManager()->GetCurrentMapId();
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(curId.data(), curId.data() + curId.size()).x - 5);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.839f, 0.188f, 0.192f, 1.0f));
        ImGui::Text("%.*s", (int)curId.size(), curId.data());
        ImGui::PopStyleColor();
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();

	ImGui::Spacing();
    ImGui::Spacing();

	const auto& available_maps = editor->GetMapManager()->GetAvailableMaps();

	if (available_maps.empty())
	{
        ImGui::SeparatorText("Warning");
		ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "No maps registered in MapManager");
		ImGui::TextDisabled("Register maps using RegisterMap<YourMap>(\"MAP_ID\")");
	}
	else
	{
        ImGui::SetCursorPosX(10);
        ImGui::TextDisabled(ICON_FA_LAYER_GROUP " Available Maps");
		ImGui::Spacing();

		static int s_SelectedIndex = 0;
		std::string curr_map_id(editor->GetMapManager()->GetCurrentMapId());

		for (int i = 0; i < available_maps.size(); i++)
		{
			if (available_maps[i] == curr_map_id)
			{
				s_SelectedIndex = i;
				break;
			}
		}

        if (ImGui::BeginTable("##map_selection_table", 2, ImGuiTableFlags_None))
        {
            ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 80.0f);
            ImGui::TableSetupColumn("Widget", ImGuiTableColumnFlags_WidthStretch);

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::AlignTextToFramePadding();
            ImGui::Text("Select Map:");

            ImGui::TableSetColumnIndex(1);
            ImGui::SetNextItemWidth(-1);
            
            if (ImGui::BeginCombo("##Select Map", curr_map_id.empty() ? "No map loaded" : curr_map_id.c_str()))
            {
                for (int i = 0; i < available_maps.size(); ++i)
                {
                    bool b_IsSelected = (s_SelectedIndex == i);
                    bool b_IsCurrent = (available_maps[i] == curr_map_id);

                    if (b_IsCurrent)
                    {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.83f, 0.18f, 0.18f, 1.0f));
                    }
                    else if (i == 0)
                    {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
                    }

                    if (ImGui::Selectable(available_maps[i].c_str(), b_IsSelected))
                    {
                        s_SelectedIndex = i;
                        editor->m_SelectedMapId = available_maps[i];

                        if (editor->m_SelectedMapId != curr_map_id)
                        {
                            editor->GetMapManager()->b_GotoMap(editor->m_SelectedMapId);
                        }
                    }

                    if (b_IsSelected) ImGui::SetItemDefaultFocus();

                    if (b_IsCurrent || i == 0) ImGui::PopStyleColor();
                }
                ImGui::EndCombo();
            }
            ImGui::EndTable();
        }

		ImGui::Spacing();
        ImGui::Spacing();

        ImGui::SetCursorPosX(10);
		ImGui::TextDisabled(ICON_FA_BOLT " Quick Access");
		ImGui::Spacing();

        ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.05f, 0.5f));

		for (int i = 0; i < available_maps.size(); ++i)
		{
			const auto& MAP_ID = available_maps[i];
			bool b_IsCurrent = (MAP_ID == curr_map_id);
            bool b_IsMain = (i == 0);

			if (b_IsCurrent)
			{
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.839f, 0.188f, 0.192f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.939f, 0.288f, 0.292f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.739f, 0.088f, 0.092f, 1.0f));
			}
			else if (b_IsMain)
			{
				ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_Button]);
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyle().Colors[ImGuiCol_ButtonHovered]);
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::GetStyle().Colors[ImGuiCol_ButtonActive]);
			}

			std::string button_label = MAP_ID;
			if (b_IsMain) button_label += " (Main)";

            ImVec2 start_pos = ImGui::GetCursorScreenPos();
            
			if (ImGui::Button(button_label.c_str(), ImVec2(-1, 36.0f)))
			{
				if (MAP_ID != curr_map_id)
				{
					editor->GetMapManager()->b_GotoMap(MAP_ID);
				}
			}
            
            ImGui::GetWindowDrawList()->AddText(ImVec2(start_pos.x + ImGui::GetContentRegionAvail().x - 20, start_pos.y + 10), ImGui::GetColorU32(ImGuiCol_Text), ICON_FA_ARROW_RIGHT_TO_BRACKET);

			if (b_IsCurrent || b_IsMain)
			{
				ImGui::PopStyleColor(3);
			}
            
            ImGui::Spacing();
            ImGui::Spacing();
		}
        
        ImGui::PopStyleVar();
	}

	ImGui::End();
}
