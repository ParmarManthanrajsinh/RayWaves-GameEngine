#include "GameConfig.h"
#include "Log.h"
#include "PlatformInfo.h"
#include <filesystem>
#include <fstream>
#include <sstream>

GameConfig& GameConfig::GetInstance()
{
    static GameConfig s_Instance;
    return s_Instance;
}

bool GameConfig::m_bLoadFromFile(std::string_view config_path)
{
    std::string path_str(config_path);
    std::ifstream file(path_str);
    if (!file.is_open())
    {
        Log::Error("GameConfig", "Config file not found: " + std::string(config_path) + ". Using defaults.");
        return false;
    }

    std::string line;
    while (std::getline(file, line))
    {
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#' || line[0] == ';')
        {
            continue;
        }

        // Find the equals sign
        auto equal_pos = line.find('=');
        if (equal_pos == std::string::npos)
        {
            continue;
        }

        std::string key = line.substr(0, equal_pos);
        std::string value = line.substr(equal_pos + 1);

        // Trim whitespace
        key.erase(0, key.find_first_not_of(" \t"));
        key.erase(key.find_last_not_of(" \t") + 1);
        value.erase(0, value.find_first_not_of(" \t"));
        value.erase(value.find_last_not_of(" \t") + 1);

        // Parse configuration values
        auto safe_stoi = [&](const std::string& str_val, int fallback) -> int
        {
            try
            {
                return std::stoi(str_val);
            }
            catch (...)
            {
                return fallback;
            }
        };

        if (key == "width")
        {
            m_WindowConfig.width = safe_stoi(value, m_WindowConfig.width);
        }
        else if (key == "height")
        {
            m_WindowConfig.height = safe_stoi(value, m_WindowConfig.height);
        }
        else if (key == "b_Fullscreen")
        {
            m_WindowConfig.b_Fullscreen = (value == "true" || value == "1");
        }
        else if (key == "b_Resizable")
        {
            m_WindowConfig.b_Resizable = (value == "true" || value == "1");
        }
        else if (key == "b_Vsync")
        {
            m_WindowConfig.b_Vsync = (value == "true" || value == "1");
        }
        else if (key == "target_fps")
        {
            m_WindowConfig.target_fps = safe_stoi(value, m_WindowConfig.target_fps);
        }
        else if (key == "title")
        {
            m_WindowConfig.title = value;
        }
        else if (key == "scene_width")
        {
            m_WindowConfig.scene_width = safe_stoi(value, m_WindowConfig.scene_width);
        }
        else if (key == "scene_height")
        {
            m_WindowConfig.scene_height = safe_stoi(value, m_WindowConfig.scene_height);
        }
        else if (key == "scene_fps")
        {
            m_WindowConfig.scene_fps = safe_stoi(value, m_WindowConfig.scene_fps);
        }
    }

    file.close();
    Log::Info("GameConfig", "Loaded configuration from: " + std::string(config_path));
    return true;
}

bool GameConfig::m_bSaveToFile(std::string_view config_path) const
{
    const std::filesystem::path file_path{ std::string(config_path) };
    const std::string contents = GenerateConfigString();

    // Keep the previous config beside the new one so a crash mid-save cannot
    // lose the user's window settings. Missing target means nothing to copy.
    std::error_code exists_error;
    if (std::filesystem::exists(file_path, exists_error))
    {
        std::string backup_error;
        if (!PlatformInfo::b_BackupFile(file_path, backup_error))
        {
            Log::Error("GameConfig", "Failed to back up configuration " + file_path.string() + ": " + backup_error);
            return false;
        }
    }

    std::string write_error;
    if (!PlatformInfo::b_WriteFileAtomic(file_path, contents, write_error))
    {
        Log::Error("GameConfig", "Failed to save configuration " + file_path.string() + ": " + write_error);
        return false;
    }

    Log::Info("GameConfig", "Saved configuration to: " + file_path.string());
    return true;
}

std::string GameConfig::GenerateConfigString() const
{
    std::ostringstream out_stream;

    out_stream << "# Game Configuration File\n"
               << "# Window Settings\n"
               << "width=" << m_WindowConfig.width << "\n"
               << "height=" << m_WindowConfig.height << "\n"
               << "b_Fullscreen=" << (m_WindowConfig.b_Fullscreen ? "true" : "false") << "\n"
               << "b_Resizable=" << (m_WindowConfig.b_Resizable ? "true" : "false") << "\n"
               << "b_Vsync=" << (m_WindowConfig.b_Vsync ? "true" : "false") << "\n"
               << "target_fps=" << m_WindowConfig.target_fps << "\n"
               << "title=" << m_WindowConfig.title << "\n"
               << "scene_width=" << m_WindowConfig.scene_width << "\n"
               << "scene_height=" << m_WindowConfig.scene_height << "\n"
               << "scene_fps=" << m_WindowConfig.scene_fps << "\n";

    return out_stream.str();
}

void GameConfig::ApplyExportSettings(int width, int height, // NOLINT(bugprone-easily-swappable-parameters)
    bool fullscreen, bool resizable, bool vsync, int target_fps)
{
    m_WindowConfig.width = width;
    m_WindowConfig.height = height;
    m_WindowConfig.b_Fullscreen = fullscreen;
    m_WindowConfig.b_Resizable = resizable;
    m_WindowConfig.b_Vsync = vsync;
    m_WindowConfig.target_fps = target_fps;
}
