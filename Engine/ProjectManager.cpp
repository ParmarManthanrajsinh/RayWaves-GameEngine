#include "ProjectManager.h"
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <iostream>
#include <print>
#include <windows.h>
#include <shlobj.h>

namespace fs = std::filesystem;

t_Project ProjectManager::s_Current;
bool ProjectManager::s_bOpen = false;
std::string ProjectManager::s_RecentPath = "";

void ProjectManager::InitializeRecentPath()
{
    if (!s_RecentPath.empty()) return;
    
    char path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, path)))
    {
        fs::path dir = fs::path(path) / "RayWaves";
        if (!fs::exists(dir)) fs::create_directories(dir);
        s_RecentPath = (dir / "recent.ini").string();
    }
}

bool ProjectManager::b_OpenProject(std::string_view folder_path)
{
    std::string manifest_path = (fs::path(folder_path) / "project.raywaves").string();
    if (!fs::exists(manifest_path))
    {
        std::println(std::cerr, "Project manifest not found at: {}", manifest_path);
        return false;
    }

    t_Project new_project;
    if (new_project.m_bLoadFromFile(manifest_path))
    {
        s_Current = new_project;
        s_bOpen = true;
        AddRecent(folder_path);
        return true;
    }
    return false;
}

void ProjectManager::CloseProject()
{
    s_bOpen = false;
    s_Current = t_Project{};
}

bool ProjectManager::b_CreateProject(std::string_view target_folder, std::string_view template_name)
{
    fs::path target_path = target_folder;
    if (fs::exists(target_path))
    {
        std::println(std::cerr, "Target folder already exists: {}", target_folder);
        return false;
    }

    // Resolve template path relative to the executable
    char exe_path[MAX_PATH];
    GetModuleFileNameA(NULL, exe_path, MAX_PATH);
    fs::path template_dir = fs::path(exe_path).parent_path() / "Templates" / template_name;

    if (!fs::exists(template_dir))
    {
        std::println(std::cerr, "Template not found: {}", template_dir.string());
        return false;
    }

    try
    {
        fs::copy(template_dir, target_path, fs::copy_options::recursive);
        
        // Rewrite the manifest name
        std::string manifest_path = (target_path / "project.raywaves").string();
        t_Project proj;
        if (proj.m_bLoadFromFile(manifest_path))
        {
            proj.m_Name = target_path.filename().string();
            proj.m_bSaveToFile();
        }
        return true;
    }
    catch (const std::exception& e)
    {
        std::println(std::cerr, "Failed to create project from template: {}", e.what());
        return false;
    }
}

bool ProjectManager::b_SaveCurrentProject()
{
    if (!s_bOpen) return false;
    return s_Current.m_bSaveToFile();
}

const t_Project& ProjectManager::GetCurrent()
{
    return s_Current;
}

bool ProjectManager::b_HasOpenProject()
{
    return s_bOpen;
}

void ProjectManager::AddRecent(std::string_view path)
{
    InitializeRecentPath();
    std::vector<std::string> recent = GetRecent();
    
    // Normalize path
    std::string norm_path = fs::path(path).lexically_normal().string();

    // Remove if already exists
    recent.erase(std::remove(recent.begin(), recent.end(), norm_path), recent.end());
    
    // Insert at front
    recent.insert(recent.begin(), norm_path);
    
    // Keep only top 10
    if (recent.size() > 10) recent.resize(10);
    
    // Save
    std::ofstream file(s_RecentPath);
    if (file.is_open())
    {
        for (size_t i = 0; i < recent.size(); ++i)
        {
            file << "path" << i << "=" << recent[i] << "\n";
        }
    }
}

std::vector<std::string> ProjectManager::GetRecent()
{
    InitializeRecentPath();
    std::vector<std::string> recent;
    
    std::ifstream file(s_RecentPath);
    if (!file.is_open()) return recent;
    
    std::string line;
    while (std::getline(file, line))
    {
        auto equal_pos = line.find('=');
        if (equal_pos != std::string::npos)
        {
            std::string value(line.begin() + equal_pos + 1, line.end());
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);
            if (!value.empty() && fs::exists(value))
            {
                recent.push_back(value);
            }
        }
    }
    return recent;
}

std::vector<std::string> ProjectManager::GetAvailableTemplates()
{
    std::vector<std::string> templates;
    char exe_path[MAX_PATH];
    GetModuleFileNameA(NULL, exe_path, MAX_PATH);
    fs::path template_dir = fs::path(exe_path).parent_path() / "Templates";

    if (fs::exists(template_dir) && fs::is_directory(template_dir))
    {
        for (const auto& entry : fs::directory_iterator(template_dir))
        {
            if (entry.is_directory())
            {
                templates.push_back(entry.path().filename().string());
            }
        }
    }
    return templates;
}
