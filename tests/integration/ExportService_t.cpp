#include "Engine/PlatformInfo.h"
#include "Engine/PlatformNames.h"
#include "Engine/ProjectManager.h"
#include "ExportService.h"
#include "doctest/doctest.h"
#include "support/TempDir.h"
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

TEST_CASE("ExportService: ClampSettings keeps sane ranges")
{
    FExportSettings tiny;
    tiny.m_WindowWidth = 10;
    tiny.m_WindowHeight = 10;
    tiny.m_TargetFPS = -5;
    ExportService::ClampSettings(tiny);
    CHECK(tiny.m_WindowWidth == 320);
    CHECK(tiny.m_WindowHeight == 240);
    CHECK(tiny.m_TargetFPS == 0);

    FExportSettings huge;
    huge.m_WindowWidth = 99999;
    huge.m_WindowHeight = 99999;
    huge.m_TargetFPS = 99999;
    ExportService::ClampSettings(huge);
    CHECK(huge.m_WindowWidth == 7680);
    CHECK(huge.m_WindowHeight == 4320);
    CHECK(huge.m_TargetFPS == 1000);

    FExportSettings sane;
    ExportService::ClampSettings(sane);
    CHECK(sane.m_WindowWidth == 1280);
    CHECK(sane.m_TargetFPS == 60);
}

TEST_CASE("ExportService: SanitizeGameName rejects traversal and reserved names")
{
    CHECK(ExportService::SanitizeGameName("MyGame") == "MyGame");
    CHECK(ExportService::SanitizeGameName("../evil") == "_evil");
    CHECK(ExportService::SanitizeGameName("bad:name") == "bad_name");
    CHECK(ExportService::SanitizeGameName("  spaced  ") == "spaced");
    CHECK(ExportService::SanitizeGameName("CON") == "_CON");
    CHECK(ExportService::SanitizeGameName("....") == "Game");
    CHECK(ExportService::SanitizeGameName("") == "Game");
}

TEST_CASE("ExportService: ResolveExportDir anchors relatives at project root")
{
    const fs::path project_root = fs::temp_directory_path() / "raywaves_export_project";
    CHECK(ExportService::ResolveExportDir("export", project_root.string()) == (project_root / "export").string());

    const fs::path absolute_output = fs::temp_directory_path() / "raywaves_export_out" / "Game";
    CHECK(ExportService::ResolveExportDir(absolute_output.string(), project_root.string()) == absolute_output.string());
}

TEST_CASE("ExportService: b_WriteGameConfig round-trips all keys")
{
    TempDir temp_dir("raywaves_export_cfg");
    fs::path tmp = temp_dir.Path();
    fs::path cfg = tmp / "config.ini";

    FExportSettings settings;
    settings.m_GameName = "TestGame";
    settings.m_WindowWidth = 1920;
    settings.m_WindowHeight = 1080;
    settings.m_bFullscreen = true;
    settings.m_bResizable = false;
    settings.m_bVSync = false;
    settings.m_TargetFPS = 144;

    REQUIRE(ExportService::b_WriteGameConfig(cfg.string(), settings, 800, 600, 60));

    std::string content;
    {
        std::ifstream ifs(cfg);
        content.assign(std::istreambuf_iterator<char>(ifs), {});
    }
    CHECK(content.find("width=1920") != std::string::npos);
    CHECK(content.find("height=1080") != std::string::npos);
    CHECK(content.find("b_Fullscreen=true") != std::string::npos);
    CHECK(content.find("b_Resizable=false") != std::string::npos);
    CHECK(content.find("b_Vsync=false") != std::string::npos);
    CHECK(content.find("target_fps=144") != std::string::npos);
    CHECK(content.find("scene_width=800") != std::string::npos);
    CHECK(content.find("title=TestGame") != std::string::npos);

    CHECK_FALSE(ExportService::b_WriteGameConfig((tmp / "nope" / "config.ini").string(), settings, 0, 0, 0));
}

TEST_CASE("ExportService: b_ValidateExportFolder detects missing and complete exports")
{
    std::vector<std::string> logs;
    auto sink = [&](std::string_view line) { logs.emplace_back(line); };

    TempDir temp_dir("raywaves_export_validate");
    fs::path tmp = temp_dir.Path();

    // Empty folder: no exe, no DLLs
    logs.clear();
    CHECK_FALSE(ExportService::b_ValidateExportFolder(tmp.string(), sink));
    bool b_HasExeError = false;
    for (const auto& line : logs)
    {
        if (line.find("Game executable") != std::string::npos)
            b_HasExeError = true;
    }
    CHECK(b_HasExeError);

    // Complete folder passes
    const fs::path game_exe = tmp / rw::platform::ExecutableName("MyGame");
    {
        std::ofstream(game_exe) << "x";
    }
    if (!rw::platform::b_IsWindows)
    {
        fs::permissions(game_exe, fs::perms::owner_exec, fs::perm_options::add);
    }
    {
        std::ofstream(tmp / rw::platform::c_GameLogicModuleName) << "x";
    }
    {
        std::ofstream(tmp / rw::platform::c_RaylibRuntimeName) << "x";
    }
    logs.clear();
    CHECK(ExportService::b_ValidateExportFolder(tmp.string(), sink));

    // Missing the runtime library fails again
    fs::remove(tmp / rw::platform::c_RaylibRuntimeName);
    CHECK_FALSE(ExportService::b_ValidateExportFolder(tmp.string(), sink));
}

TEST_CASE("ExportService: end-to-end export produces a runnable folder")
{
    const fs::path repo = PlatformInfo::GetExecutablePath().parent_path().parent_path().parent_path();
    const fs::path template_dir = repo / "templates" / "Simple2D";
    REQUIRE(fs::exists(template_dir / "project.raywaves"));

    TempDir temp_dir("raywaves_export_e2e");
    const fs::path project = temp_dir.Path() / "ExportGame";
    fs::copy(template_dir, project, fs::copy_options::recursive);

    REQUIRE(ProjectManager::b_OpenProject(project.string(), false));

    FExportSettings settings;
    settings.m_ExportPath = (temp_dir.Path() / "out").string();
    settings.m_GameName = "ExportSmoke";

    std::vector<std::string> logs;
    auto sink = [&](std::string_view line) { logs.emplace_back(line); };
    const FExportResult result = ExportService::RunExport(settings, nullptr, sink);
    ProjectManager::CloseProject();

    if (!result.m_bSuccess)
    {
        for (const std::string& line : logs)
        {
            MESSAGE(line);
        }
    }
    REQUIRE(result.m_bSuccess);

    const fs::path out = temp_dir.Path() / "out";
    CHECK(fs::exists(out / (std::string("ExportSmoke") + rw::platform::c_ExecutableSuffix)));
    CHECK(fs::exists(out / rw::platform::c_GameLogicModuleName));
    CHECK(fs::exists(out / "config.ini"));
    CHECK(fs::exists(out / "Assets"));

    bool b_has_raylib = false;
    for (const auto& entry : fs::directory_iterator(out))
    {
        if (entry.path().filename().string().rfind("libraylib.", 0) == 0)
        {
            b_has_raylib = true;
            break;
        }
    }
    CHECK(b_has_raylib);
}
