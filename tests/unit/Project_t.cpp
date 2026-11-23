#include "Engine/PlatformNames.h"
#include "Engine/Project.h"
#include "doctest/doctest.h"
#include "support/TempDir.h"
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

namespace
{
std::string s_ReadAll(const std::filesystem::path& path)
{
    std::ifstream file(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}
} // namespace

TEST_CASE("t_Project: default values")
{
    t_Project p;
    CHECK(p.m_Name.empty());
    CHECK(p.m_Version.empty());
    CHECK(p.m_SourceDir == "GameLogic");
    CHECK(p.m_AssetDir == "Assets");
    CHECK(p.m_EntryDll == "GameLogic");
    CHECK(p.m_SceneWidth == 1280);
    CHECK(p.m_SceneHeight == 720);
    CHECK(p.m_TargetFPS == 60);
    CHECK(p.m_ExportName.empty());
    CHECK(p.m_ExportPath == "export");
    CHECK(p.m_ExportWidth == 1280);
    CHECK(p.m_ExportHeight == 720);
    CHECK(p.m_ExportFPS == 60);
    CHECK_FALSE(p.m_bExportFullscreen);
    CHECK(p.m_bExportResizable);
    CHECK(p.m_bExportVSync);
}

TEST_CASE("t_Project: save and load roundtrip")
{
    TempDir tmp_dir("raywaves_project");
    std::string manifest = (tmp_dir.Path() / "project.raywaves").string();

    t_Project p;
    p.m_RootPath = tmp_dir.String();
    p.m_Name = "TestProject";
    p.m_Version = "2.0.0";
    p.m_SourceDir = "Src";
    p.m_AssetDir = "Art";
    p.m_EntryDll = "Game.dll";
    p.m_SceneWidth = 640;
    p.m_SceneHeight = 480;
    p.m_TargetFPS = 144;
    p.m_CameraX = 100.0f;
    p.m_CameraY = 200.0f;
    p.m_LastMapId = "level_1";

    CHECK(p.m_bSaveToFile());

    t_Project q;
    CHECK(q.m_bLoadFromFile(manifest));
    CHECK(q.m_Name == "TestProject");
    CHECK(q.m_Version == "2.0.0");
    CHECK(q.m_SourceDir == "Src");
    CHECK(q.m_AssetDir == "Art");
    CHECK(q.m_EntryDll == "Game");
    CHECK(q.m_SceneWidth == 640);
    CHECK(q.m_SceneHeight == 480);
    CHECK(q.m_TargetFPS == 144);
    CHECK(q.m_CameraX == doctest::Approx(100.0f));
    CHECK(q.m_CameraY == doctest::Approx(200.0f));
    CHECK(q.m_LastMapId == "level_1");
}

TEST_CASE("t_Project: saving twice keeps the previous manifest as a backup")
{
    TempDir tmp_dir("raywaves_project_backup");
    const std::filesystem::path manifest = tmp_dir.Path() / "project.raywaves";
    std::filesystem::path backup = manifest;
    backup += ".bak";

    t_Project p;
    p.m_RootPath = tmp_dir.String();
    p.m_Name = "FirstProject";
    REQUIRE(p.m_bSaveToFile());
    CHECK_FALSE(std::filesystem::exists(backup));

    const std::string first_contents = s_ReadAll(manifest);
    CHECK(first_contents.find("name=FirstProject") != std::string::npos);

    p.m_Name = "SecondProject";
    REQUIRE(p.m_bSaveToFile());

    REQUIRE(std::filesystem::exists(backup));
    CHECK(s_ReadAll(backup) == first_contents);
    CHECK(first_contents.find("name=SecondProject") == std::string::npos);

    const std::string current_contents = s_ReadAll(manifest);
    CHECK(current_contents.find("name=SecondProject") != std::string::npos);
    CHECK(current_contents != first_contents);
}

TEST_CASE("t_Project: resolves the host module suffix")
{
    TempDir tmp_dir("raywaves_project_module");
    std::string manifest = (tmp_dir.Path() / "project.raywaves").string();
    {
        std::ofstream file(manifest);
        file << "[project]\n"
                "name=SuffixGame\n"
                "entryDll=GameLogic.dll\n";
    }

    t_Project project;
    REQUIRE(project.m_bLoadFromFile(manifest));
    CHECK(project.m_EntryDll == "GameLogic");
    CHECK(project.m_DllPath.ends_with(rw::platform::c_SharedLibrarySuffix));
}

TEST_CASE("t_Project: missing [editor] section uses defaults")
{
    TempDir tmp_dir("raywaves_no_editor");
    std::string manifest = (tmp_dir.Path() / "project.raywaves").string();

    {
        std::ofstream f(manifest);
        f << "[project]\n";
        f << "name=NoEditorProject\n";
    }

    t_Project p;
    CHECK(p.m_bLoadFromFile(manifest));
    CHECK(p.m_Name == "NoEditorProject");
    CHECK(p.m_SceneWidth == 1280);
    CHECK(p.m_SceneHeight == 720);
    CHECK(p.m_TargetFPS == 60);
}

TEST_CASE("t_Project: export settings round-trip through save and load")
{
    TempDir tmp_dir("raywaves_project_export");
    std::string manifest = (tmp_dir.Path() / "project.raywaves").string();

    t_Project p;
    p.m_RootPath = tmp_dir.String();
    p.m_Name = "ExportProject";
    p.m_ExportName = "ShippedGame";
    p.m_ExportPath = "dist/export";
    p.m_ExportWidth = 1920;
    p.m_ExportHeight = 1080;
    p.m_ExportFPS = 144;
    p.m_bExportFullscreen = true;
    p.m_bExportResizable = false;
    p.m_bExportVSync = false;

    REQUIRE(p.m_bSaveToFile());

    const std::string contents = s_ReadAll(tmp_dir.Path() / "project.raywaves");
    CHECK(contents.find("[export]") != std::string::npos);
    CHECK(contents.find("gameName=ShippedGame") != std::string::npos);
    CHECK(contents.find("path=dist/export") != std::string::npos);
    CHECK(contents.find("width=1920") != std::string::npos);
    CHECK(contents.find("height=1080") != std::string::npos);
    CHECK(contents.find("targetFPS=144") != std::string::npos);
    CHECK(contents.find("fullscreen=1") != std::string::npos);
    CHECK(contents.find("resizable=0") != std::string::npos);
    CHECK(contents.find("vsync=0") != std::string::npos);

    t_Project q;
    REQUIRE(q.m_bLoadFromFile(manifest));
    CHECK(q.m_ExportName == "ShippedGame");
    CHECK(q.m_ExportPath == "dist/export");
    CHECK(q.m_ExportWidth == 1920);
    CHECK(q.m_ExportHeight == 1080);
    CHECK(q.m_ExportFPS == 144);
    CHECK(q.m_bExportFullscreen);
    CHECK_FALSE(q.m_bExportResizable);
    CHECK_FALSE(q.m_bExportVSync);
}

TEST_CASE("t_Project: missing [export] section keeps defaults")
{
    TempDir tmp_dir("raywaves_no_export");
    std::string manifest = (tmp_dir.Path() / "project.raywaves").string();
    {
        std::ofstream f(manifest);
        f << "[project]\n"
             "name=NoExportProject\n"
             "\n"
             "[editor]\n"
             "sceneWidth=800\n"
             "sceneHeight=600\n"
             "targetFPS=30\n";
    }

    t_Project p;
    REQUIRE(p.m_bLoadFromFile(manifest));
    CHECK(p.m_Name == "NoExportProject");
    CHECK(p.m_ExportName.empty());
    CHECK(p.m_ExportPath == "export");
    CHECK(p.m_ExportWidth == 1280);
    CHECK(p.m_ExportHeight == 720);
    CHECK(p.m_ExportFPS == 60);
    CHECK_FALSE(p.m_bExportFullscreen);
    CHECK(p.m_bExportResizable);
    CHECK(p.m_bExportVSync);
}

TEST_CASE("t_Project: malformed export values keep defaults")
{
    TempDir tmp_dir("raywaves_bad_export");
    std::string manifest = (tmp_dir.Path() / "project.raywaves").string();
    {
        std::ofstream f(manifest);
        f << "[project]\n"
             "name=BadExportProject\n"
             "\n"
             "[export]\n"
             "gameName=ManifestGame\n"
             "path=custom/out\n"
             "width=wide\n"
             "height=720p\n"
             "targetFPS=fast\n"
             "fullscreen=maybe\n"
             "resizable=sometimes\n"
             "vsync=perhaps\n";
    }

    t_Project p;
    REQUIRE(p.m_bLoadFromFile(manifest));
    // String keys still parse; malformed numbers and booleans keep defaults.
    CHECK(p.m_ExportName == "ManifestGame");
    CHECK(p.m_ExportPath == "custom/out");
    CHECK(p.m_ExportWidth == 1280);
    CHECK(p.m_ExportHeight == 720);
    CHECK(p.m_ExportFPS == 60);
    CHECK_FALSE(p.m_bExportFullscreen);
    CHECK(p.m_bExportResizable);
    CHECK(p.m_bExportVSync);
}

TEST_CASE("t_Project: missing file returns false")
{
    t_Project p;
    CHECK_FALSE(p.m_bLoadFromFile("nonexistent_manifest.raywaves"));
}

TEST_CASE("t_Project: b_IsProjectFolder")
{
    TempDir tmp_dir("raywaves_isfolder");
    CHECK_FALSE(t_Project::b_IsProjectFolder(tmp_dir.String()));

    std::string manifest = (tmp_dir.Path() / "project.raywaves").string();
    {
        std::ofstream f(manifest);
        f << "[project]\nname=Test\n";
    }
    CHECK(t_Project::b_IsProjectFolder(tmp_dir.String()));
}
