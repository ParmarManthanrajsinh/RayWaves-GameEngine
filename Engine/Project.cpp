#include "Project.h"
#include <fstream>
#include <filesystem>
#include <iostream>
#include <print>

namespace fs = std::filesystem;

bool t_Project::m_bLoadFromFile(std::string_view manifest_path) 
{
    std::string path_str(manifest_path);
    std::ifstream file(path_str);
    if (!file.is_open()) 
    {
        std::println("Project manifest not found: {}", manifest_path);
        return false;
    }
    
    // Store root path relative to manifest
    m_RootPath = fs::path(manifest_path).parent_path().string();
    
    // Set default values before parsing
    m_Name = "NewProject";
    m_Version = "1.0.0";
    m_EngineVersion = "0.6.0";
    m_SourceDir = "GameLogic";
    m_AssetDir = "Assets";
    m_EntryDll = "GameLogic.dll";
    
    m_CameraX = 0.f;
    m_CameraY = 0.f;
    m_LastMapId = "";
    m_SceneWidth = 1280;
    m_SceneHeight = 720;
    m_TargetFPS = 60;

    std::string line;
    std::string currentSection = "";
    
    while (std::getline(file, line)) 
    {
        // Strip trailing \r for files with Windows CRLF line endings
        if (!line.empty() && line.back() == '\r') line.pop_back();
        
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#' || line[0] == ';') 
        {
            continue;
        }
        
        // Trim leading whitespace
        line.erase(0, line.find_first_not_of(" \t"));
        if (line.empty()) continue;

        // Check for section headers
        if (line[0] == '[' && line.back() == ']')
        {
            currentSection = line.substr(1, line.length() - 2);
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
        
        if (currentSection == "project")
        {
            if (key == "name") m_Name = value;
            else if (key == "version") m_Version = value;
            else if (key == "engineVersion") m_EngineVersion = value;
            else if (key == "sourceDir") m_SourceDir = value;
            else if (key == "assetDir") m_AssetDir = value;
            else if (key == "entryDll") m_EntryDll = value;
        }
        else if (currentSection == "editor")
        {
            if (key == "cameraX") m_CameraX = std::stof(value);
            else if (key == "cameraY") m_CameraY = std::stof(value);
            else if (key == "lastMapId") m_LastMapId = value;
            else if (key == "sceneWidth") m_SceneWidth = std::stoi(value);
            else if (key == "sceneHeight") m_SceneHeight = std::stoi(value);
            else if (key == "targetFPS") m_TargetFPS = std::stoi(value);
        }
    }
    
    file.close();
    
    // Resolve absolute paths
    m_SourcePath = (fs::path(m_RootPath) / m_SourceDir).string();
    m_AssetPath = (fs::path(m_RootPath) / m_AssetDir).string();
    m_DllPath = (fs::path(m_RootPath) / m_EntryDll).string();
    
    std::println("Loaded project: {}", m_Name);
    return true;
}

bool t_Project::m_bSaveToFile() const
{
    std::string manifest_path = (fs::path(m_RootPath) / "project.raywaves").string();
    std::ofstream file(manifest_path);
    if (!file.is_open()) 
    {
        std::println(std::cerr, "Failed to create project manifest: {}", manifest_path);
        return false;
    }
    
    file << "[project]\n";
    file << "name=" << m_Name << "\n";
    file << "version=" << m_Version << "\n";
    file << "engineVersion=" << m_EngineVersion << "\n";
    file << "sourceDir=" << m_SourceDir << "\n";
    file << "assetDir=" << m_AssetDir << "\n";
    file << "entryDll=" << m_EntryDll << "\n\n";
    
    file << "[editor]\n";
    file << "cameraX=" << m_CameraX << "\n";
    file << "cameraY=" << m_CameraY << "\n";
    file << "lastMapId=" << m_LastMapId << "\n";
    file << "sceneWidth=" << m_SceneWidth << "\n";
    file << "sceneHeight=" << m_SceneHeight << "\n";
    file << "targetFPS=" << m_TargetFPS << "\n";
    
    file.close();
    std::println("Saved project manifest to: {}", manifest_path);
    return true;
}

bool t_Project::m_bIsValid() const
{
    return fs::exists(m_SourcePath) && fs::exists(m_AssetPath);
}

bool t_Project::b_IsProjectFolder(std::string_view folder)
{
    return fs::exists(fs::path(folder) / "project.raywaves");
}
