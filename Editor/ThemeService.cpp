#include "ThemeService.h"
#include "../Engine/ProjectManager.h"
#include "EditorPreferences.h"
#include "GameEditorTheme.h"
#include <filesystem>

std::string ThemeService::GetEngineContentPath(std::string_view sub_path)
{
    std::filesystem::path root = ProjectManager::GetEngineRootDirectory();
    std::filesystem::path core_path = root / "Core" / "EngineContent" / sub_path;
    if (std::filesystem::exists(core_path))
        return core_path.string();
    return (root / "EngineContent" / sub_path).string();
}

void ThemeService::RebakeNow()
{
    const auto& prefs = EditorPreferences::GetInstance().GetPreferences();
    const FThemePreset* selected_preset = GetThemePresets().data();
    for (const auto& preset : GetThemePresets())
    {
        if (preset.Name == prefs.theme_name)
        {
            selected_preset = &preset;
            break;
        }
    }

    std::string base_font = GetEngineContentPath("Roboto-Regular.ttf");
    std::string mono_font = GetEngineContentPath("Consolas-Regular.ttf");
    std::string icon_font = GetEngineContentPath("fa-solid-900.ttf");
    if (prefs.font_family == "Consolas")
    {
        base_font = mono_font;
    }
    SetEngineTheme(*selected_preset, prefs.gui_scale, base_font, mono_font, icon_font);
}
