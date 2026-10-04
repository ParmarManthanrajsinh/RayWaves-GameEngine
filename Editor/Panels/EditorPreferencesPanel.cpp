#include "EditorPreferencesPanel.h"
#include "../GameEditor.h"
#include "../EditorPreferences.h"
#include "../GameEditorTheme.h"
#include <filesystem>

void EditorPreferencesPanel::Draw(GameEditor* editor)
{
    if (!editor->m_bShowEditorPreferences) return;

    if (ImGui::Begin(ICON_FA_SLIDERS " Editor Preferences", &editor->m_bShowEditorPreferences))
    {
        auto& prefs = EditorPreferences::GetInstance().GetPreferences();
        bool b_NeedsRebake = false;
        bool b_SavePrefs = false;

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
            b_NeedsRebake = true;
            b_SavePrefs = true;
        }

        // Theme Name
        ImGui::Text("Theme");
        const auto& PRESETS = GetThemePresets();
        if (ImGui::BeginCombo("##ThemeCombo", prefs.ThemeName.c_str()))
        {
            for (const auto& preset : PRESETS)
            {
                bool b_IsSelected = (prefs.ThemeName == preset.Name);
                if (ImGui::Selectable(preset.Name.c_str(), b_IsSelected))
                {
                    prefs.ThemeName = preset.Name;
                    b_NeedsRebake = true;
                    b_SavePrefs = true;
                }
                if (b_IsSelected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        // Font Family
        ImGui::Text("Font Family");
        const char* FONTS[] = { "Roboto", "Consolas" };
        if (ImGui::BeginCombo("##FontCombo", prefs.FontFamily.c_str()))
        {
            for (auto &font : FONTS)
            {
                bool b_IsSelected = (prefs.FontFamily == font);
                if (ImGui::Selectable(font, b_IsSelected))
                {
                    prefs.FontFamily = font;
                    b_NeedsRebake = true;
                    b_SavePrefs = true;
                }
                if (b_IsSelected) ImGui::SetItemDefaultFocus();
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

        if (b_NeedsRebake)
        {
            editor->m_bNeedsThemeRebake = true;
        }

        if (b_SavePrefs)
        {
            EditorPreferences::GetInstance().m_bSaveToFile();
        }
    }
    ImGui::End();
}
