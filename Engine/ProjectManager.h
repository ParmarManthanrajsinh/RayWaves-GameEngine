#pragma once
#include "Project.h"
#include <filesystem>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

class ProjectManager
{
public:
    static std::filesystem::path GetEngineRootDirectory();
    static std::filesystem::path GetToolsDirectory();

    static bool b_OpenProject(std::string_view folder_path, bool b_add_recent = true);
    // Structured-error variant of b_OpenProject. The two-argument overload
    // above keeps working and delegates here.
    static bool b_OpenProject(std::string_view folder_path, bool b_add_recent, std::string& out_error);

    // Resolve a folder path, a path inside it, or project.raywaves; parse and
    // validate without changing the current project or recent list.
    static bool b_ValidateProject(std::string_view path, t_Project& out_project, std::string& out_error);

    static void CloseProject();
    static bool b_CreateProject(std::string_view target_folder, std::string_view template_name = "Simple2D");
    // Structured-error variant used by rwcli and other non-UI callers.
    static bool b_CreateProject(std::string_view target_folder, std::string_view template_name, std::string& out_error);
    static bool b_SaveCurrentProject();
    static bool GenerateCMakeLists();
    static bool b_WriteIdeConfigs(bool b_force = false);

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
