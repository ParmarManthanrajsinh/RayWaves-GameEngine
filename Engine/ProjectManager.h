#pragma once
#include "Project.h"
#include <string>
#include <filesystem>
#include <string_view>
#include <vector>
#include <mutex>

class ProjectManager 
{
public:
    static std::filesystem::path GetEngineRootDirectory();
    static std::filesystem::path GetToolsDirectory();

    static bool b_OpenProject(std::string_view folder_path);
    static void CloseProject();
    static bool b_CreateProject(std::string_view target_folder, std::string_view template_name = "Empty");
    static bool b_SaveCurrentProject();
    static bool GenerateCMakeLists();

    static std::string SanitizeCMakeProjectName(std::string_view name);

    // NOTE: GetCurrent() hands out a live reference that escapes the
    // internal lock. Use only on the UI thread. Prefer GetCurrentCopy()
    // on worker threads (e.g. export) so background work never races
    // with UI-thread edits to the project.
    static t_Project& GetCurrent();
    static t_Project GetCurrentCopy();
    static bool b_HasOpenProject();

    static void AddRecent(std::string_view path);
    static void RemoveRecent(std::string_view path);
    static std::vector<std::string> GetRecent();

    // Test seam: redirects recent.ini away from %APPDATA%. Returns the
    // previous path so the caller can restore it.
    static std::string SetRecentPathForTesting(std::string_view path);

    static std::vector<std::string> GetAvailableTemplates();

private:
    static t_Project s_Current;
    static bool s_bOpen;
    static std::string s_RecentPath;

    static void InitializeRecentPath();

    static std::recursive_mutex s_Mutex;
};
