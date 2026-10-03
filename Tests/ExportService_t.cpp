#include "ExportService.h"
#include "doctest/doctest.h"
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

TEST_CASE("ExportService: ClampSettings keeps sane ranges")
{
    FExportSettings tiny;
    tiny.m_WindowWidth  = 10;
    tiny.m_WindowHeight = 10;
    tiny.m_TargetFPS    = -5;
    ExportService::ClampSettings(tiny);
    CHECK(tiny.m_WindowWidth == 320);
    CHECK(tiny.m_WindowHeight == 240);
    CHECK(tiny.m_TargetFPS == 0);

    FExportSettings huge;
    huge.m_WindowWidth  = 99999;
    huge.m_WindowHeight = 99999;
    huge.m_TargetFPS    = 99999;
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
    CHECK(ExportService::ResolveExportDir("export", "/home/dev/Games/MyGame") ==
          (fs::path("/home/dev/Games/MyGame") / "export").string());
    CHECK(ExportService::ResolveExportDir("/var/out/Game", "/home/dev/Games/MyGame") ==
          "/var/out/Game");
    // Trailing separators must not survive: empty filename() breaks tar.
    CHECK(ExportService::ResolveExportDir("export/", "/home/dev/Games/MyGame") ==
          (fs::path("/home/dev/Games/MyGame") / "export").string());
    CHECK(ExportService::ResolveExportDir("/var/out/Game/", "/home/dev") ==
          "/var/out/Game");
}

TEST_CASE("ExportService: b_WriteGameConfig round-trips all keys")
{
    fs::path tmp = fs::temp_directory_path() / "raywaves_export_cfg";
    fs::create_directories(tmp);
    fs::path cfg = tmp / "config.ini";

    FExportSettings settings;
    settings.m_GameName     = "TestGame";
    settings.m_WindowWidth  = 1920;
    settings.m_WindowHeight = 1080;
    settings.m_bFullscreen  = true;
    settings.m_bResizable   = false;
    settings.m_bVSync       = false;
    settings.m_TargetFPS    = 144;

    REQUIRE(
        ExportService::b_WriteGameConfig(cfg.string(), settings, 800, 600, 60));

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

    CHECK_FALSE(ExportService::b_WriteGameConfig(
        (tmp / "nope" / "config.ini").string(), settings, 0, 0, 0));

    fs::remove_all(tmp);
}

TEST_CASE("ExportService: b_ValidateExportFolder detects missing and complete "
          "exports")
{
    std::vector<std::string> logs;
    auto sink = [&](std::string_view line) { logs.emplace_back(line); };

    fs::path tmp = fs::temp_directory_path() / "raywaves_export_validate";
    fs::create_directories(tmp);

    // Empty folder: no ELF executable, no shared libraries
    logs.clear();
    CHECK_FALSE(ExportService::b_ValidateExportFolder(tmp.string(), sink));
    bool b_HasExeError = false;
    for (const auto &line : logs)
    {
        if (line.find("Game executable") != std::string::npos)
            b_HasExeError = true;
    }
    CHECK(b_HasExeError);

    // Complete folder passes: ELF magic + exec bit, both shared libraries.
    auto write_elf = [](const fs::path& p)
    {
        std::ofstream f(p, std::ios::binary);
        f << "\x7f"
          << "ELF"
          << "\x02\x01\x01" << std::string(9, '\0');
        f.close();
        fs::permissions(p, fs::perms::owner_exec | fs::perms::owner_read |
                               fs::perms::owner_write);
    };
    write_elf(tmp / "MyGame");
    {
        std::ofstream(tmp / "GameLogic.so") << "x";
    }
    {
        std::ofstream(tmp / "libraylib.so") << "x";
    }
    logs.clear();
    CHECK(ExportService::b_ValidateExportFolder(tmp.string(), sink));

    // A plain non-ELF file must not count as the game executable
    fs::remove(tmp / "MyGame");
    {
        std::ofstream(tmp / "MyGame") << "not an elf";
    }
    CHECK_FALSE(ExportService::b_ValidateExportFolder(tmp.string(), sink));

    // Missing one shared library fails again
    fs::remove(tmp / "MyGame");
    write_elf(tmp / "MyGame");
    fs::remove(tmp / "libraylib.so");
    CHECK_FALSE(ExportService::b_ValidateExportFolder(tmp.string(), sink));

    fs::remove_all(tmp);
}
