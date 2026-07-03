#include "doctest/doctest.h"
#include "../Engine/Project.h"
#include <fstream>
#include <filesystem>

TEST_CASE("t_Project: default values")
{
    t_Project p;
    CHECK(p.m_Name.empty());
    CHECK(p.m_Version.empty());
    CHECK(p.m_SourceDir == "GameLogic");
    CHECK(p.m_AssetDir == "Assets");
    CHECK(p.m_EntryDll == "GameLogic.dll");
    CHECK(p.m_SceneWidth == 1280);
    CHECK(p.m_SceneHeight == 720);
    CHECK(p.m_TargetFPS == 60);
}

TEST_CASE("t_Project: save and load roundtrip")
{
    auto tmp_dir = std::filesystem::temp_directory_path() / "raywaves_test_project";
    std::filesystem::create_directories(tmp_dir);
    std::string manifest = (tmp_dir / "project.raywaves").string();

    t_Project p;
    p.m_RootPath = tmp_dir.string();
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
    CHECK(q.m_EntryDll == "Game.dll");
    CHECK(q.m_SceneWidth == 640);
    CHECK(q.m_SceneHeight == 480);
    CHECK(q.m_TargetFPS == 144);
    CHECK(q.m_CameraX == doctest::Approx(100.0f));
    CHECK(q.m_CameraY == doctest::Approx(200.0f));
    CHECK(q.m_LastMapId == "level_1");

    std::filesystem::remove_all(tmp_dir);
}

TEST_CASE("t_Project: missing [editor] section uses defaults")
{
    auto tmp_dir = std::filesystem::temp_directory_path() / "raywaves_test_no_editor";
    std::filesystem::create_directories(tmp_dir);
    std::string manifest = (tmp_dir / "project.raywaves").string();

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

    std::filesystem::remove_all(tmp_dir);
}

TEST_CASE("t_Project: missing file returns false")
{
    t_Project p;
    CHECK_FALSE(p.m_bLoadFromFile("nonexistent_manifest.raywaves"));
}

TEST_CASE("t_Project: b_IsProjectFolder")
{
    auto tmp_dir = std::filesystem::temp_directory_path() / "raywaves_test_isfolder";
    std::filesystem::create_directories(tmp_dir);
    CHECK_FALSE(t_Project::b_IsProjectFolder(tmp_dir.string()));

    std::string manifest = (tmp_dir / "project.raywaves").string();
    {
        std::ofstream f(manifest);
        f << "[project]\nname=Test\n";
    }
    CHECK(t_Project::b_IsProjectFolder(tmp_dir.string()));

    std::filesystem::remove_all(tmp_dir);
}
