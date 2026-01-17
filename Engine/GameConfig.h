#pragma once
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <print>
#include <filesystem>

struct t_WindowConfig 
{
    int width = 1280;
    int height = 720;
    bool b_Fullscreen = false;
    bool b_Resizable = true;
    bool b_Vsync = true;
    int target_fps = 60;
    std::string title = "My Game";

    // Editor Scene Settings
    int scene_width = 1280;
    int scene_height = 720;
    int scene_fps = 60;
};

class GameConfig 
{
public:
    static GameConfig& GetInstance();
    bool m_bLoadFromFile(const std::string& config_path = "config.ini");
    bool m_bSaveToFile
    (
        const std::string& config_path = "config.ini"
    ) const;
    
    t_WindowConfig& GetWindowConfig() { return m_WindowConfig; }
    const t_WindowConfig& GetWindowConfig() const { return m_WindowConfig; }
    std::string GenerateConfigString() const;
    void ApplyExportSettings
    (
        int width, int height,
        bool fullscreen, bool resizable,
        bool vsync, int target_fps
    );

private:
    t_WindowConfig m_WindowConfig;
    GameConfig() = default;
};