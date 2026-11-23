#include "ExportCommand.h"
#include "ExportService.h"
#include "Json.h"
#include "ProjectManager.h"
#include <iostream>
#include <string>
#include <vector>

namespace
{
// JSON mode must put exactly one object on stdout, so export logs are
// collected here and only used to describe the failure.
std::string LastErrorMessage(const std::vector<std::string>& logs)
{
    for (auto it = logs.rbegin(); it != logs.rend(); ++it)
    {
        if (it->find("ERROR") != std::string::npos)
        {
            return *it;
        }
    }
    if (!logs.empty())
    {
        return logs.back();
    }
    return "Export failed";
}
} // namespace

namespace Cli
{
int ExportCommand(const FProjectContext& project, const FArgs& args, bool b_json)
{
    const t_Project& data = project.m_Project;

    // Persisted export settings are the CLI defaults; --out and --clean are
    // explicit per-run overrides.
    FExportSettings settings;
    settings.m_GameName = data.m_ExportName.empty() ? data.m_Name : data.m_ExportName;
    settings.m_ExportPath = data.m_ExportPath.empty() ? std::string("export") : data.m_ExportPath;
    settings.m_WindowWidth = data.m_ExportWidth;
    settings.m_WindowHeight = data.m_ExportHeight;
    settings.m_TargetFPS = data.m_ExportFPS;
    settings.m_bFullscreen = data.m_bExportFullscreen;
    settings.m_bResizable = data.m_bExportResizable;
    settings.m_bVSync = data.m_bExportVSync;
    settings.m_bCleanOutput = args.b_Has("clean");

    const std::string out_option = args.Get("out");
    if (!out_option.empty())
    {
        settings.m_ExportPath = ExportService::ResolveExportDir(out_option, data.m_RootPath);
    }
    ExportService::ClampSettings(settings);

    // Export configures and builds .raywaves/CMakeLists.txt, which only
    // exists once the project has been opened. Same preparation as `build`.
    if (!ProjectManager::b_OpenProject(project.m_Dir.string(), false))
    {
        PrintError(b_json, "invalid_project", "ProjectManager could not open the project");
        return 3;
    }

    std::vector<std::string> logs;
    ExportService::LogSink sink;
    if (b_json)
    {
        sink = [&logs](std::string_view line) { logs.emplace_back(line); };
    }
    else
    {
        sink = [](std::string_view line) { std::cerr << line << "\n"; };
    }

    const FExportResult result = ExportService::RunExport(data, settings, nullptr, sink);
    const std::string output = result.m_OutputPath.empty()
        ? ExportService::ResolveExportDir(settings.m_ExportPath, data.m_RootPath)
        : result.m_OutputPath;

    if (b_json)
    {
        if (result.m_bSuccess)
        {
            std::cout << "{" << CliJson::Key("ok") << "true," << CliJson::Key("output") << CliJson::Quoted(output)
                      << "}\n";
        }
        else
        {
            std::cout << "{" << CliJson::Key("ok") << "false," << CliJson::Key("output") << CliJson::Quoted(output)
                      << "," << CliJson::Key("error") << "{" << CliJson::Key("code") << CliJson::Quoted("export_failed")
                      << "," << CliJson::Key("message") << CliJson::Quoted(LastErrorMessage(logs)) << "}}\n";
        }
    }
    else
    {
        std::cout << (result.m_bSuccess ? "export ok: " : "export failed: ") << output << "\n";
    }

    return result.m_bSuccess ? 0 : 4;
}
} // namespace Cli
