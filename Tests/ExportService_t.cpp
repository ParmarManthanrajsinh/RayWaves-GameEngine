#include "doctest/doctest.h"
#include "ExportService.h"
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

TEST_CASE("ExportService: ResolveExportDir anchors relatives at project root")
{
    CHECK(ExportService::ResolveExportDir("export", "C:/Games/MyGame")
        == (fs::path("C:/Games/MyGame") / "export").string());
    CHECK(ExportService::ResolveExportDir("C:/Out/Game", "C:/Games/MyGame") == "C:/Out/Game");
}

TEST_CASE("ExportService: b_WriteGameConfig round-trips all keys")
{
    fs::path tmp = fs::temp_directory_path() / "raywaves_export_cfg";
    fs::create_directories(tmp);
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
    { std::ifstream ifs(cfg); content.assign(std::istreambuf_iterator<char>(ifs), {}); }
    CHECK(content.find("width=1920") != std::string::npos);
    CHECK(content.find("height=1080") != std::string::npos);
    CHECK(content.find("b_Fullscreen=true") != std::string::npos);
    CHECK(content.find("b_Resizable=false") != std::string::npos);
    CHECK(content.find("b_Vsync=false") != std::string::npos);
    CHECK(content.find("target_fps=144") != std::string::npos);
    CHECK(content.find("scene_width=800") != std::string::npos);
    CHECK(content.find("title=TestGame") != std::string::npos);

    CHECK_FALSE(ExportService::b_WriteGameConfig((tmp / "nope" / "config.ini").string(), settings, 0, 0, 0));

    fs::remove_all(tmp);
}

TEST_CASE("ExportService: b_ValidateExportFolder detects missing and complete exports")
{
    std::vector<std::string> logs;
    auto sink = [&](std::string_view line) { logs.emplace_back(line); };

    fs::path tmp = fs::temp_directory_path() / "raywaves_export_validate";
    fs::create_directories(tmp);

    // Empty folder: no exe, no DLLs
    logs.clear();
    CHECK_FALSE(ExportService::b_ValidateExportFolder(tmp.string(), sink));
    bool b_HasExeError = false;
    for (const auto& line : logs)
    {
        if (line.find("Game executable") != std::string::npos) b_HasExeError = true;
    }
    CHECK(b_HasExeError);

    // Complete folder passes
    { std::ofstream(tmp / "MyGame.exe") << "x"; }
    { std::ofstream(tmp / "GameLogic.dll") << "x"; }
    { std::ofstream(tmp / "libraylib.dll") << "x"; }
    logs.clear();
    CHECK(ExportService::b_ValidateExportFolder(tmp.string(), sink));

    // Missing one DLL fails again
    fs::remove(tmp / "libraylib.dll");
    CHECK_FALSE(ExportService::b_ValidateExportFolder(tmp.string(), sink));

    fs::remove_all(tmp);
}
