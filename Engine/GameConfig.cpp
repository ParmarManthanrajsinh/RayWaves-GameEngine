#include "GameConfig.h"


GameConfig& GameConfig::GetInstance() 
{
    static GameConfig s_Instance;
    return s_Instance;
}

bool GameConfig::m_bLoadFromFile(const std::string& config_path) 
{
    std::ifstream file(config_path);
    if (!file.is_open()) 
    {
        std::println("Config file not found: {}. Using defaults. ",config_path);
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

        std::string key(line.begin(), line.begin() + equal_pos);
        std::string value(line.begin() + equal_pos + 1, line.end());
        
        // Trim whitespace
        key.erase(0, key.find_first_not_of(" \t"));
        key.erase(key.find_last_not_of(" \t") + 1);
        value.erase(0, value.find_first_not_of(" \t"));
        value.erase(value.find_last_not_of(" \t") + 1);
        
        // Parse configuration values
        if (key == "width") 
        {
            m_WindowConfig.width = std::stoi(value);
        } 
        else if (key == "height") 
        {
            m_WindowConfig.height = std::stoi(value);
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
            m_WindowConfig.target_fps = std::stoi(value);
        }
        else if (key == "title") 
        {
            m_WindowConfig.title = value;
        }
        else if (key == "scene_width")
        {
            m_WindowConfig.scene_width = std::stoi(value);
        }
        else if (key == "scene_height")
        {
            m_WindowConfig.scene_height = std::stoi(value);
        }
        else if (key == "scene_fps")
        {
            m_WindowConfig.scene_fps = std::stoi(value);
        }
    }
    
    file.close();
    std::println("Loaded configuration from: {}", config_path);
    return true;
}

bool GameConfig::m_bSaveToFile(const std::string& config_path) const 
{
    std::ofstream file(config_path);
    if (!file.is_open()) 
    {
        std::println(std::cerr, "Failed to create config file: {}", config_path);
        return false;
    }
    
    file << "# Game Configuration File" << "\n";
    file << "# Window Settings" << "\n";
    file << "width=" << m_WindowConfig.width << "\n";
    file << "height=" << m_WindowConfig.height << "\n";
    file << "b_Fullscreen=" 
         << (m_WindowConfig.b_Fullscreen ? "true" : "false") << "\n";
    file << "b_Resizable=" 
         << (m_WindowConfig.b_Resizable ? "true" : "false") << "\n";
    file << "b_Vsync=" 
         << (m_WindowConfig.b_Vsync ? "true" : "false") << "\n";
    file << "target_fps=" << m_WindowConfig.target_fps << "\n";
    file << "title=" << m_WindowConfig.title << "\n";
    file << "scene_width=" << m_WindowConfig.scene_width << "\n";
    file << "scene_height=" << m_WindowConfig.scene_height << "\n";
    file << "scene_fps=" << m_WindowConfig.scene_fps << "\n";
    
    file.close();
    std::println("Saved configuration to: {}", config_path);
    return true;
}

std::string GameConfig::GenerateConfigString() const
{
    std::ostringstream ss;

    ss << "# Game Configuration File\n"
       << "# Window Settings\n"
       << "width=" << m_WindowConfig.width << "\n"
       << "height=" << m_WindowConfig.height << "\n"
       << "b_Fullscreen=" 
       << (m_WindowConfig.b_Fullscreen ? "true" : "false") << "\n"
       << "b_Resizable=" 
       << (m_WindowConfig.b_Resizable ? "true" : "false") << "\n"
       << "b_Vsync=" << (m_WindowConfig.b_Vsync ? "true" : "false") << "\n"
       << "target_fps=" << m_WindowConfig.target_fps << "\n"
       << "title=" << m_WindowConfig.title << "\n"
       << "scene_width=" << m_WindowConfig.scene_width << "\n"
       << "scene_height=" << m_WindowConfig.scene_height << "\n"
       << "scene_fps=" << m_WindowConfig.scene_fps << "\n";

    return ss.str();
}

void GameConfig::ApplyExportSettings
(
    int width, int height, 
    bool fullscreen, bool resizable,
    bool vsync, int target_fps
)
{
    m_WindowConfig.width = width;
    m_WindowConfig.height = height;
    m_WindowConfig.b_Fullscreen = fullscreen;
    m_WindowConfig.b_Resizable = resizable;
    m_WindowConfig.b_Vsync = vsync;
    m_WindowConfig.target_fps = target_fps;
}