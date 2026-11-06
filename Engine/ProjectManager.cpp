#include <algorithm>
#include <iostream>
#include "ProjectManager.h"
#include <filesystem>
#include <fstream>
#include <iterator>
#include <windows.h>
#include <shlobj.h>

namespace fs = std::filesystem;

fs::path ProjectManager::GetEngineRootDirectory()
{
    char exe_path[MAX_PATH];
    GetModuleFileNameA(nullptr, exe_path, MAX_PATH);
    fs::path base_dir = fs::path(exe_path).parent_path();

    if (fs::exists(base_dir / "Tools" / "setup_zig.ps1"))
    {
        return base_dir;
    }

    fs::path current = base_dir;
    while (current.has_parent_path() && current != current.parent_path())
    {
        if (fs::exists(current / "Tools" / "setup_zig.ps1"))
        {
            return current;
        }
        current = current.parent_path();
    }

    // Fallback: check for dist layout (Core/ + Templates/)
    if (fs::exists(base_dir / "Core") && fs::exists(base_dir / "Templates"))
    {
        return base_dir;
    }

    current = base_dir;
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

fs::path ProjectManager::GetToolsDirectory()
{
    fs::path engine_root = GetEngineRootDirectory();
    fs::path tools_dir = engine_root / "Core" / "Tools";
    if (!fs::exists(tools_dir / "zig-cc.bat"))
    {
        tools_dir = engine_root / "Tools";
    }
    return tools_dir;
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
    
    char path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(nullptr, CSIDL_APPDATA, nullptr, 0, path)))
    {
        fs::path dir = fs::path(path) / "RayWaves";
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

t_Project ProjectManager::GetCurrentCopy()
{
    std::scoped_lock lock(s_Mutex);
    return s_Current;
}

bool ProjectManager::b_HasOpenProject()
{
    std::scoped_lock lock(s_Mutex);
    return s_bOpen;
}

std::string ProjectManager::SetRecentPathForTesting(std::string_view path)
{
    std::scoped_lock lock(s_Mutex);
    std::string previous = s_RecentPath;
    s_RecentPath = std::string(path);
    return previous;
}

void ProjectManager::AddRecent(std::string_view path)
{
    InitializeRecentPath();
    std::vector<std::string> recent = GetRecent();

    // Normalize all paths so comparison works even for legacy entries
    // stored without normalization.
    for (auto& entry : recent)
    {
        entry = fs::path(entry).lexically_normal().string();
    }

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

    // Normalize all paths so comparison works even for legacy entries
    // stored without normalization.
    for (auto& entry : recent)
    {
        entry = fs::path(entry).lexically_normal().string();
    }

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
            // Keep missing entries: UI shows them greyed as "(missing)" and
            // lets the user remove them from the list.
            if (!value.empty())
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

namespace
{
    std::string ReplaceToken(std::string text, std::string_view token, std::string_view value)
    {
        size_t position = 0;
        while ((position = text.find(token, position)) != std::string::npos)
        {
            text.replace(position, token.size(), value);
            position += value.size();
        }
        return text;
    }

    std::string ToForwardSlashes(fs::path path)
    {
        std::string result = path.string();
        std::ranges::replace(result, '\\', '/');
        return result;
    }

    // The same template ships in dev builds (build/<preset>/packaging) and in
    // distributions (Core/). Keeping one file avoids the two project build
    // recipes drifting apart again.
    fs::path FindProjectTemplate(const fs::path& exe_dir, const fs::path& engine_root)
    {
        const fs::path candidates[] = {
            exe_dir / "packaging" / "Project.CMakeLists.in",
            exe_dir / "Core" / "Project.CMakeLists.in",
            engine_root / "packaging" / "Project.CMakeLists.in",
            engine_root / "Core" / "Project.CMakeLists.in",
        };

        for (const auto& candidate : candidates)
        {
            if (fs::exists(candidate)) return candidate;
        }
        return {};
    }
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

    // Resolve raylib path relative to exe (staged by main build or dist layout)
    char exe_path[MAX_PATH];
    GetModuleFileNameA(nullptr, exe_path, MAX_PATH);
    fs::path exe_dir = fs::path(exe_path).parent_path();
    fs::path raylib_dir = exe_dir / "raylib";
    if (fs::exists(exe_dir / "Core" / "raylib" / "include" / "raylib.h"))
    {
        raylib_dir = exe_dir / "Core" / "raylib";
    }

    // Older build trees predate raylib staging and leave raylib/lib empty.
    // Fall back to the FetchContent build tree so project builds keep working
    // until the engine is rebuilt.
    auto has_raylib_import = [](const fs::path& dir)
    {
        return fs::exists(dir / "lib" / "libraylib.a")
            || fs::exists(dir / "lib" / "libraylib.dll.a")
            || fs::exists(dir / "lib" / "raylib.lib")
            || fs::exists(dir / "libraylib.dll.a")
            || fs::exists(dir / "libraylib.a")
            || fs::exists(dir / "raylib.lib");
    };
    if (!has_raylib_import(raylib_dir))
    {
        fs::path fetched = exe_dir / "_deps" / "raylib-build" / "raylib";
        if (has_raylib_import(fetched)) raylib_dir = fetched;
    }

    fs::path template_path = FindProjectTemplate(exe_dir, engine_root);
    if (template_path.empty())
    {
        std::cerr << "Project CMake template not found. Expected packaging/Project.CMakeLists.in "
                     "next to the editor or in Core/.\n";
        return false;
    }

    std::ifstream template_file(template_path);
    if (!template_file.is_open()) return false;

    std::string content((std::istreambuf_iterator<char>(template_file)),
                        std::istreambuf_iterator<char>());

    content = ReplaceToken(content, "@PROJECT_NAME@", SanitizeCMakeProjectName(s_Current.m_Name));
    content = ReplaceToken(content, "@TOOLS_DIR@", ToForwardSlashes(GetToolsDirectory()));
    content = ReplaceToken(content, "@ENGINE_ROOT@", ToForwardSlashes(engine_root));
    content = ReplaceToken(content, "@ENGINE_DIR@", ToForwardSlashes(engine_dir));
    content = ReplaceToken(content, "@RAYLIB_DIR@", ToForwardSlashes(raylib_dir));

    std::ofstream file(cmake_path);
    if (!file.is_open()) return false;
    file << content;
    return file.good();
}
