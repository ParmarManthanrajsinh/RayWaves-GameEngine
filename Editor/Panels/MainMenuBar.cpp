#include "MainMenuBar.h"
#include "../GameEditor.h"
#include <imgui.h>
#include <rlImGui.h>

void MainMenuBar::Draw(GameEditor* editor)
{
    if (ImGui::BeginMainMenuBar())
    {
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
