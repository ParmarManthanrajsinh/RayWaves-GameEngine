#pragma once
#include "Project.h"
#include <string>
#include <string_view>
#include <vector>

class ProjectManager 
{
public:
    static bool b_OpenProject(std::string_view folder_path);
    static void CloseProject();
    static bool b_CreateProject(std::string_view target_folder, std::string_view template_name = "Empty");
    static bool b_SaveCurrentProject();
    static bool GenerateCMakeLists();

    static std::string SanitizeCMakeProjectName(std::string_view name);

    static const t_Project& GetCurrent();
    static bool b_HasOpenProject();

    static void AddRecent(std::string_view path);
    static void RemoveRecent(std::string_view path);
    static std::vector<std::string> GetRecent();

    static std::vector<std::string> GetAvailableTemplates();

private:
    static t_Project s_Current;
    static bool s_bOpen;
    static std::string s_RecentPath;

    static void InitializeRecentPath();
};
