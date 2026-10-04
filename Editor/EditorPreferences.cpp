#include "EditorPreferences.h"
#include "../Engine/Platform/PlatformPaths.h"
#include <fstream>
#include <iostream>
#include <filesystem>
EditorPreferences& EditorPreferences::GetInstance()
{
    static EditorPreferences s_Instance;
    return s_Instance;
}

std::string EditorPreferences::GetConfigPath() 
{
    std::filesystem::path config_dir = platform::UserConfigDir();
    if (!config_dir.empty())
    {
        std::filesystem::path dir = config_dir / "RayWaves";
        return (dir / "editor_preferences.ini").string();
    }
    return "editor_preferences.ini"; // Fallback when neither XDG_CONFIG_HOME nor HOME is set
}

bool EditorPreferences::m_bLoadFromFile()
{
    std::string config_path = GetConfigPath();
    std::ifstream file(config_path);
    if (!file.is_open())
    {
        return false;
    }

    std::string line;
    while (std::getline(file, line))
    {
        if (line.empty() || line[0] == '#' || line[0] == ';')
        {
            continue;
        }

        auto equal_pos = line.find('=');
        if (equal_pos == std::string::npos)
        {
            continue;
        }

        std::string key(line.begin(), line.begin() + equal_pos);
        std::string value(line.begin() + equal_pos + 1, line.end());

        // Trim whitespace
        key.erase(0, key.find_first_not_of(" \t"));
        key.erase(key.find_last_not_of(" \t") + 1);
        value.erase(0, value.find_first_not_of(" \t"));
        value.erase(value.find_last_not_of(" \t") + 1);

        if (key == "GuiScale")
        {
            try { m_Preferences.gui_scale = std::stof(value); } catch (...) {}
        }
        else if (key == "ThemeName")
        {
            m_Preferences.theme_name = value;
        }
        else if (key == "FontFamily")
        {
            m_Preferences.font_family = value;
        }
    }

    file.close();
    return true;
}

bool EditorPreferences::m_bSaveToFile() const
{
    std::string config_path = GetConfigPath();
    
    // Ensure directory exists
    std::filesystem::path dir = std::filesystem::path(config_path).parent_path();
    if (!dir.empty() && !std::filesystem::exists(dir))
    {
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
    }

    std::ofstream file(config_path);
    if (!file.is_open())
    {
        std::cerr << "Failed to create editor preferences file: " << config_path << "\n";
        return false;
    }

    file << "# Editor Preferences\n";
    file << "GuiScale=" << m_Preferences.gui_scale << "\n";
    file << "ThemeName=" << m_Preferences.theme_name << "\n";
    file << "FontFamily=" << m_Preferences.font_family << "\n";

    file.close();
    return true;
}
