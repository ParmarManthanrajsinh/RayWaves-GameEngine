#include "EditorPreferencesPanel.h"
#include "../EditorPreferences.h"
#include "../GameEditor.h"
#include "../GameEditorLayout.h"
#include "../GameEditorTheme.h"
#include <algorithm>
#include <filesystem>

void EditorPreferencesPanel::Draw(GameEditor* editor)
{
    if (!editor->m_bShowEditorPreferences)
        return;

    if (ImGui::Begin(ICON_FA_SLIDERS " Editor Preferences", &editor->m_bShowEditorPreferences))
    {
        auto& prefs = EditorPreferences::GetInstance().GetPreferences();
        bool bNeedsRebake = false;
        bool bSavePrefs = false;

        // GUI Scale
        if (!m_bIsDraggingScale)
            m_DraggingGuiScale = prefs.GuiScale;

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
                if (is_selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        // Font Family
        ImGui::Text("Font Family");
        const char* fonts[] = { "Roboto", "Consolas" };
        if (ImGui::BeginCombo("##FontCombo", prefs.FontFamily.c_str()))
        {
            for (auto& font : fonts)
            {
                bool is_selected = (prefs.FontFamily == font);
                if (ImGui::Selectable(font, is_selected))
                {
                    prefs.FontFamily = font;
                    bNeedsRebake = true;
                    bSavePrefs = true;
                }
                if (is_selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        ImGui::Separator();
        ImGui::Spacing();

        ImGui::SeparatorText("Grid & Snapping");

        if (ImGui::Checkbox("Show grid  (G)", &prefs.SceneGridEnabled))
        {
            bSavePrefs = true;
        }

        ImGui::SetNextItemWidth(-1.0f);
        ImGui::SliderFloat("##SceneGridSize", &prefs.SceneGridSize, 1.0f, 256.0f, "Grid size: %.0f px");
        if (ImGui::IsItemDeactivatedAfterEdit())
        {
            bSavePrefs = true;
        }
        prefs.SceneGridSize = std::clamp(prefs.SceneGridSize, 1.0f, 256.0f);

        if (ImGui::Checkbox("Snap with Ctrl  (Shift+G)", &prefs.SceneSnapEnabled))
        {
            bSavePrefs = true;
        }

        ImGui::SetNextItemWidth(-1.0f);
        ImGui::SliderFloat("##SceneSnapStep", &prefs.SceneSnapStep, 1.0f, 256.0f, "Snap step: %.0f");
        if (ImGui::IsItemDeactivatedAfterEdit())
        {
            bSavePrefs = true;
        }
        prefs.SceneSnapStep = std::clamp(prefs.SceneSnapStep, 1.0f, 256.0f);

        ImGui::SetNextItemWidth(-1.0f);
        ImGui::SliderFloat("##SceneAngleSnap", &prefs.SceneAngleSnapDegrees, 1.0f, 90.0f, "Angle snap: %.0f deg");
        if (ImGui::IsItemDeactivatedAfterEdit())
        {
            bSavePrefs = true;
        }
        prefs.SceneAngleSnapDegrees = std::clamp(prefs.SceneAngleSnapDegrees, 1.0f, 90.0f);

        ImGui::Separator();
        ImGui::Spacing();

        ImGui::SeparatorText("Build");

        ImGui::Text("Build Configuration");
        const char* build_configs[] = { "Debug", "Release", "RelWithDebInfo" };
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::BeginCombo("##BuildConfig", prefs.BuildConfig.c_str()))
        {
            for (const char* config : build_configs)
            {
                const bool b_selected = (prefs.BuildConfig == config);
                if (ImGui::Selectable(config, b_selected))
                {
                    prefs.BuildConfig = config;
                    bSavePrefs = true;
                }
                if (b_selected)
                {
                    ImGui::SetItemDefaultFocus();
                }
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
