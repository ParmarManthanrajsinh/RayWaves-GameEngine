#include "EditorPreferences.h"
#include "Log.h"
#include "PlatformInfo.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace
{
std::string s_NormalizeBuildConfig(const std::string& value)
{
    if (value == "Debug" || value == "Release" || value == "RelWithDebInfo")
    {
        return value;
    }
    return "Release";
}
} // namespace

EditorPreferences& EditorPreferences::GetInstance()
{
    static EditorPreferences s_Instance;
    return s_Instance;
}

std::string EditorPreferences::GetConfigPath()
{
    const std::filesystem::path dir = PlatformInfo::GetAppConfigDirectory() / "RayWaves";
    return (dir / "editor_preferences.ini").string();
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
            try
            {
                m_Preferences.GuiScale = std::stof(value);
            }
            catch (...)
            {
            }
        }
        else if (key == "ThemeName")
        {
            m_Preferences.ThemeName = value;
        }
        else if (key == "FontFamily")
        {
            m_Preferences.FontFamily = value;
        }
        else if (key == "AutoSaveEnabled")
        {
            if (value == "0")
            {
                m_Preferences.AutoSaveEnabled = false;
            }
            else if (value == "1")
            {
                m_Preferences.AutoSaveEnabled = true;
            }
        }
        else if (key == "AutoSaveIntervalSeconds")
        {
            try
            {
                m_Preferences.AutoSaveIntervalSeconds = std::clamp(std::stoi(value), 10, 3600);
            }
            catch (...)
            {
            }
        }
        else if (key == "BuildConfig")
        {
            m_Preferences.BuildConfig = s_NormalizeBuildConfig(value);
        }
        else if (key == "SceneGridEnabled")
        {
            if (value == "0")
            {
                m_Preferences.SceneGridEnabled = false;
            }
            else if (value == "1")
            {
                m_Preferences.SceneGridEnabled = true;
            }
        }
        else if (key == "SceneGridSize")
        {
            try
            {
                m_Preferences.SceneGridSize = std::clamp(std::stof(value), 1.0f, 256.0f);
            }
            catch (...)
            {
            }
        }
        else if (key == "SceneSnapEnabled")
        {
            if (value == "0")
            {
                m_Preferences.SceneSnapEnabled = false;
            }
            else if (value == "1")
            {
                m_Preferences.SceneSnapEnabled = true;
            }
        }
        else if (key == "SceneSnapStep")
        {
            try
            {
                m_Preferences.SceneSnapStep = std::clamp(std::stof(value), 1.0f, 256.0f);
            }
            catch (...)
            {
            }
        }
        else if (key == "SceneAngleSnapDegrees")
        {
            try
            {
                m_Preferences.SceneAngleSnapDegrees = std::clamp(std::stof(value), 1.0f, 90.0f);
            }
            catch (...)
            {
            }
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

    std::ostringstream contents;
    contents << "# Editor Preferences\n";
    contents << "GuiScale=" << m_Preferences.GuiScale << "\n";
    contents << "ThemeName=" << m_Preferences.ThemeName << "\n";
    contents << "FontFamily=" << m_Preferences.FontFamily << "\n";
    contents << "AutoSaveEnabled=" << (m_Preferences.AutoSaveEnabled ? 1 : 0) << "\n";
    contents << "AutoSaveIntervalSeconds=" << std::clamp(m_Preferences.AutoSaveIntervalSeconds, 10, 3600) << "\n";
    contents << "BuildConfig=" << s_NormalizeBuildConfig(m_Preferences.BuildConfig) << "\n";
    contents << "SceneGridEnabled=" << (m_Preferences.SceneGridEnabled ? 1 : 0) << "\n";
    contents << "SceneGridSize=" << std::clamp(m_Preferences.SceneGridSize, 1.0f, 256.0f) << "\n";
    contents << "SceneSnapEnabled=" << (m_Preferences.SceneSnapEnabled ? 1 : 0) << "\n";
    contents << "SceneSnapStep=" << std::clamp(m_Preferences.SceneSnapStep, 1.0f, 256.0f) << "\n";
    contents << "SceneAngleSnapDegrees=" << std::clamp(m_Preferences.SceneAngleSnapDegrees, 1.0f, 90.0f) << "\n";

    // Replace rather than truncate so a crash mid-write cannot leave a
    // half-written preferences file behind.
    std::error_code exists_error;
    if (std::filesystem::exists(config_path, exists_error))
    {
        std::string backup_error;
        if (!PlatformInfo::b_BackupFile(config_path, backup_error))
        {
            Log::Error("EditorPreferences", "Failed to back up preferences: " + backup_error);
            return false;
        }
    }

    std::string error;
    if (!PlatformInfo::b_WriteFileAtomic(config_path, contents.str(), error))
    {
        Log::Error("EditorPreferences", "Failed to save editor preferences: " + error);
        return false;
    }

    return true;
}
