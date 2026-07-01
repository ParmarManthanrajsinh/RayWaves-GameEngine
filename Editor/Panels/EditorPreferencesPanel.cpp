#include "EditorPreferencesPanel.h"
#include "../GameEditor.h"
#include "../EditorPreferences.h"
#include "../GameEditorLayout.h"
#include "../GameEditorTheme.h"
#include <filesystem>

void EditorPreferencesPanel::Draw(GameEditor* editor)
{
    if (!editor->m_bShowEditorPreferences) return;

    if (ImGui::Begin(ICON_FA_SLIDERS " Editor Preferences", &editor->m_bShowEditorPreferences))
    {
        auto& prefs = EditorPreferences::GetInstance().GetPreferences();
        bool bNeedsRebake = false;
        bool bSavePrefs = false;

        // GUI Scale
        if (!m_bIsDraggingScale) m_DraggingGuiScale = prefs.GuiScale;

        ImGui::Text("GUI Scale");
        if (ImGui::SliderFloat("##GuiScale", &m_DraggingGuiScale, 0.75f, 2.0f, "%.2f"))
        {
            m_bIsDraggingScale = true;
            ImGui::GetIO().FontGlobalScale = m_DraggingGuiScale / prefs.GuiScale;
        }

        if (ImGui::IsItemDeactivatedAfterEdit())
        {
            m_bIsDraggingScale = false;
            ImGui::GetIO().FontGlobalScale = 1.0f; // Reset trick
            prefs.GuiScale = m_DraggingGuiScale;
            bNeedsRebake = true;
            bSavePrefs = true;
        }

        // Theme Name
        ImGui::Text("Theme");
        const auto& presets = GetThemePresets();
        if (ImGui::BeginCombo("##ThemeCombo", prefs.ThemeName.c_str()))
        {
            for (const auto& preset : presets)
            {
                bool is_selected = (prefs.ThemeName == preset.Name);
                if (ImGui::Selectable(preset.Name.c_str(), is_selected))
                {
                    prefs.ThemeName = preset.Name;
                    bNeedsRebake = true;
                    bSavePrefs = true;
                }
                if (is_selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        // Font Family
        ImGui::Text("Font Family");
        const char* fonts[] = { "Roboto", "Consolas" };
        if (ImGui::BeginCombo("##FontCombo", prefs.FontFamily.c_str()))
        {
            for (int i = 0; i < 2; i++)
            {
                bool is_selected = (prefs.FontFamily == fonts[i]);
                if (ImGui::Selectable(fonts[i], is_selected))
                {
                    prefs.FontFamily = fonts[i];
                    bNeedsRebake = true;
                    bSavePrefs = true;
                }
                if (is_selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::Button("Reset Layout to Default"))
        {
            std::string layout_path = EditorPreferences::GetInstance().GetConfigPath();
            std::filesystem::path dir = std::filesystem::path(layout_path).parent_path();
            std::filesystem::path file = dir / "editor_layout.ini";
            if (std::filesystem::exists(file))
            {
                std::filesystem::remove(file);
            }
            editor->m_bNeedsLayoutReset = true;
        }

        if (bNeedsRebake)
        {
            editor->m_bNeedsThemeRebake = true;
        }

        if (bSavePrefs)
        {
            EditorPreferences::GetInstance().m_bSaveToFile();
        }
    }
    ImGui::End();
}
