#include <algorithm>
#include <iostream>
#include "ProjectManager.h"
#include "Platform/PlatformPaths.h"
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

fs::path ProjectManager::GetEngineRootDirectory()
{
    fs::path base_dir = platform::HostExecutablePath().parent_path();
    if (base_dir.empty())
    {
        base_dir = fs::current_path();
    }

    // Source layout: repo root holds Distribution/Templates.
    fs::path current = base_dir;
    while (current.has_parent_path() && current != current.parent_path())
    {
        if (fs::exists(current / "Distribution" / "Templates"))
        {
            return current;
        }
        current = current.parent_path();
    }

    // Distribution layout: Core/ + Templates/ sit beside the runtime.
    if (fs::exists(base_dir / "Core") && fs::exists(base_dir / "Templates"))
    {
        return base_dir;
    }

    return base_dir;
}

std::string ProjectManager::SanitizeCMakeProjectName(std::string_view name)
{
    std::string sanitized;
    for (char c : name)
    {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')
        {
            sanitized += c;
        }
        else
        {
            sanitized += '_';
        }
    }

    if (!sanitized.empty() && sanitized[0] >= '0' && sanitized[0] <= '9')
    {
        sanitized = "_" + sanitized;
    }

    if (sanitized.empty())
    {
        sanitized = "RayWavesProject";
    }

    return sanitized;
}

t_Project ProjectManager::s_Current;
bool ProjectManager::s_bOpen = false;
std::string ProjectManager::s_RecentPath;
std::recursive_mutex ProjectManager::s_Mutex;

void ProjectManager::InitializeRecentPath()
{
    if (!s_RecentPath.empty()) return;

    fs::path config_dir = platform::UserConfigDir();
    if (!config_dir.empty())
    {
        fs::path dir = config_dir / "RayWaves";
        if (!fs::exists(dir)) fs::create_directories(dir);
        s_RecentPath = (dir / "recent.ini").string();
    }
}

bool ProjectManager::b_OpenProject(std::string_view folder_path)
{
    std::scoped_lock lock(s_Mutex);
    std::string manifest_path = (fs::path(folder_path) / "project.raywaves").string();
    if (!fs::exists(manifest_path))
    {
        std::cerr << "Project manifest not found at: " << manifest_path << '\n';
        return false;
    }

    t_Project new_project;
    if (new_project.m_bLoadFromFile(manifest_path))
    {
        s_Current = new_project;
        s_bOpen = true;
        std::cout << "Loaded project: " << s_Current.m_Name << '\n';
        AddRecent(folder_path);

        // Ensure project local cache directory exists
        fs::path raywaves_dir = fs::path(folder_path) / ".raywaves";
        fs::create_directories(raywaves_dir / "shadows");
        fs::create_directories(raywaves_dir / "build");

        return GenerateCMakeLists();
    }
    return false;
}

void ProjectManager::CloseProject()
{
    std::scoped_lock lock(s_Mutex);
    s_bOpen = false;
    s_Current = t_Project{};
}

bool ProjectManager::b_CreateProject(std::string_view target_folder, std::string_view template_name)
{
    fs::path target_path = target_folder;
    if (fs::exists(target_path))
    {
        std::cerr << "Target folder already exists: " << target_folder << '\n';
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
        std::cerr << "Template not found: " << template_dir.string() << '\n';
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
        std::cerr << "Failed to create project from template: " << e.what() << '\n';
        return false;
    }
}

bool ProjectManager::b_SaveCurrentProject()
{
    std::scoped_lock lock(s_Mutex);
    if (!s_bOpen) return false;
    return s_Current.m_bSaveToFile();
}

t_Project& ProjectManager::GetCurrent()
{
    std::scoped_lock lock(s_Mutex);
    return s_Current;
}

bool ProjectManager::b_HasOpenProject()
{
    std::scoped_lock lock(s_Mutex);
    return s_bOpen;
}

void ProjectManager::AddRecent(std::string_view path)
{
    InitializeRecentPath();
    std::vector<std::string> recent = GetRecent();
    
    // Normalize path
    std::string norm_path = fs::path(path).lexically_normal().string();

    // Remove if already exists
    std::erase(recent, norm_path);
    
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

void ProjectManager::RemoveRecent(std::string_view path)
{
    InitializeRecentPath();
    std::vector<std::string> recent = GetRecent();
    
    // Normalize path
    std::string norm_path = fs::path(path).lexically_normal().string();

    // Remove if exists
    std::erase(recent, norm_path);
    
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

bool ProjectManager::GenerateCMakeLists()
{
    if (!b_HasOpenProject()) return false;

    fs::path raywaves_dir = fs::path(s_Current.m_RootPath) / ".raywaves";
    fs::path cmake_path = raywaves_dir / "CMakeLists.txt";

    fs::path engine_root = GetEngineRootDirectory();
    fs::path engine_dir = engine_root;
    if (fs::exists(engine_root / "Core" / "Engine"))
    {
        engine_dir = engine_root / "Core";
    }
    std::string engine_dir_str = engine_dir.string();

    // Resolve raylib path relative to exe (staged by main build or dist layout)
    fs::path exe_dir = platform::HostExecutablePath().parent_path();
    fs::path raylib_dir = exe_dir / "raylib";
    if (fs::exists(exe_dir / "Core" / "raylib" / "include" / "raylib.h"))
    {
        raylib_dir = exe_dir / "Core" / "raylib";
    }
    std::string raylib_dir_str = raylib_dir.string();

    std::ofstream file(cmake_path);
    if (!file.is_open()) return false;

    file << "cmake_minimum_required(VERSION 3.10)\n\n";

    // System compiler, system build tools: nothing to bootstrap.
    file << "project(" << SanitizeCMakeProjectName(s_Current.m_Name) << ")\n\n";

    file << "set(CMAKE_CXX_STANDARD 23)\n";
    file << "set(CMAKE_CXX_STANDARD_REQUIRED ON)\n";
    file << "set(CMAKE_CXX_EXTENSIONS OFF)\n\n";

    file << "set(ENGINE_DIR \"" << engine_dir_str << "\")\n";
    file << "set(RAYLIB_DIR \"" << raylib_dir_str << "\")\n";
    file << "set(PROJECT_SRC_DIR \"${CMAKE_SOURCE_DIR}/../GameLogic\")\n\n";
    
    file << "add_library(GameLogic SHARED)\n";
    file << "set_target_properties(GameLogic PROPERTIES PREFIX \"\")\n\n";
    
    // CONFIGURE_DEPENDS: a newly added .cpp must show up without clearing the cache.
    file << "file(GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS \"${PROJECT_SRC_DIR}/*.cpp\")\n";
    file << "file(GLOB_RECURSE ENGINE_SRC CONFIGURE_DEPENDS \"${ENGINE_DIR}/Engine/*.cpp\")\n";
    file << "target_sources(GameLogic PRIVATE ${SRC_FILES} ${ENGINE_SRC})\n\n";
    
    file << "target_include_directories(GameLogic PRIVATE\n";
    file << "    \"${ENGINE_DIR}\"\n";
    file << "    \"${ENGINE_DIR}/Engine\"\n";
    file << "    \"${RAYLIB_DIR}/include\"\n";
    file << ")\n\n";
    
    file << "target_link_directories(GameLogic PRIVATE \"${RAYLIB_DIR}/lib\")\n";
    file << "target_link_libraries(GameLogic PRIVATE raylib)\n\n";

    // Plugin dlopen'd by the editor must find libraylib.so on its own.
    file << "set_target_properties(GameLogic PROPERTIES\n";
    file << "    BUILD_RPATH \"${RAYLIB_DIR}/bin\"\n";
    file << "    INSTALL_RPATH \"$ORIGIN\"\n";
    file << "    RUNTIME_OUTPUT_DIRECTORY \"${CMAKE_SOURCE_DIR}/..\"\n";
    file << "    LIBRARY_OUTPUT_DIRECTORY \"${CMAKE_SOURCE_DIR}/..\")\n";

    return true;
}
