#include "ProjectManager.h"
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <iostream>
#include <print>
#include <windows.h>
#include <shlobj.h>

namespace fs = std::filesystem;

static fs::path GetEngineRootDirectory()
{
    char exe_path[MAX_PATH];
    GetModuleFileNameA(NULL, exe_path, MAX_PATH);
    fs::path base_dir = fs::path(exe_path).parent_path();

    if (fs::exists(base_dir / "Core") && fs::exists(base_dir / "Templates"))
    {
        return base_dir;
    }

    fs::path current = base_dir;
    while (current.has_parent_path() && current != current.parent_path())
    {
        if (fs::exists(current / "Distribution" / "Templates"))
        {
            return current;
        }
        current = current.parent_path();
    }

    return base_dir;
}

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

        // Ensure project local cache directory exists
        fs::path raywaves_dir = fs::path(folder_path) / ".raywaves";
        fs::create_directories(raywaves_dir / "shadows");
        fs::create_directories(raywaves_dir / "build");

        GenerateCMakeLists();

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

    // Resolve template path using robust root discovery
    fs::path root_dir = GetEngineRootDirectory();
    fs::path template_dir = root_dir / "Templates" / template_name;
    
    if (!fs::exists(template_dir))
    {
        template_dir = root_dir / "Distribution" / "Templates" / template_name;
    }

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
            if (!value.empty() && t_Project::b_IsProjectFolder(value))
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
    fs::path root_dir = GetEngineRootDirectory();
    fs::path template_dir = root_dir / "Templates";
    
    if (!fs::exists(template_dir))
    {
        template_dir = root_dir / "Distribution" / "Templates";
    }

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

void ProjectManager::GenerateCMakeLists()
{
    if (!b_HasOpenProject()) return;

    fs::path raywaves_dir = fs::path(s_Current.m_RootPath) / ".raywaves";
    fs::path cmake_path = raywaves_dir / "CMakeLists.txt";

    fs::path engine_dir = GetEngineRootDirectory();
    std::string engine_dir_str = engine_dir.string();
    std::replace(engine_dir_str.begin(), engine_dir_str.end(), '\\', '/');

    std::ofstream file(cmake_path);
    if (!file.is_open()) return;

    fs::path tools_dir = engine_dir / "Core" / "Tools";
    if (!fs::exists(tools_dir / "zig-cc.bat"))
    {
        tools_dir = engine_dir / "Tools";
    }

    std::string tools_dir_str = tools_dir.string();
    std::replace(tools_dir_str.begin(), tools_dir_str.end(), '\\', '/');

    file << "cmake_minimum_required(VERSION 3.10)\n";
    file << "set(CMAKE_C_COMPILER \"" << tools_dir_str << "/zig-cc.bat\")\n";
    file << "set(CMAKE_CXX_COMPILER \"" << tools_dir_str << "/zig-cxx.bat\")\n";
    file << "project(" << s_Current.m_Name << ")\n\n";

    file << "set(CMAKE_CXX_STANDARD 23)\n";
    file << "set(CMAKE_CXX_STANDARD_REQUIRED ON)\n";
    file << "set(CMAKE_CXX_EXTENSIONS OFF)\n\n";
    
    file << "add_compile_options(-msse4.2)\n\n";
    
    file << "set(ENGINE_DIR \"" << engine_dir_str << "\")\n";
    file << "set(PROJECT_SRC_DIR \"${CMAKE_SOURCE_DIR}/../GameLogic\")\n\n";
    
    file << "add_library(GameLogic SHARED)\n";
    file << "set_target_properties(GameLogic PROPERTIES PREFIX \"\")\n\n";
    
    file << "file(GLOB_RECURSE SRC_FILES \"${PROJECT_SRC_DIR}/*.cpp\")\n";
    file << "file(GLOB_RECURSE ENGINE_SRC \"${ENGINE_DIR}/Core/Engine/*.cpp\")\n";
    file << "target_sources(GameLogic PRIVATE ${SRC_FILES} ${ENGINE_SRC})\n\n";
    
    file << "target_include_directories(GameLogic PRIVATE\n";
    file << "    \"${ENGINE_DIR}/Core/Engine\"\n";
    file << "    \"${ENGINE_DIR}/Core/raylib/include\"\n";
    file << ")\n\n";
    
    file << "target_link_directories(GameLogic PRIVATE \"${ENGINE_DIR}/Core/raylib/lib\")\n";
    file << "target_link_libraries(GameLogic PRIVATE raylib dwmapi)\n\n";
    
    file << "set_target_properties(GameLogic PROPERTIES RUNTIME_OUTPUT_DIRECTORY \"${CMAKE_SOURCE_DIR}/..\")\n";
    file << "set_target_properties(GameLogic PROPERTIES LIBRARY_OUTPUT_DIRECTORY \"${CMAKE_SOURCE_DIR}/..\")\n";
}
