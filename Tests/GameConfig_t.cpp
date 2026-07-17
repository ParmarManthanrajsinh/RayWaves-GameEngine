#include "doctest/doctest.h"
#include "../Engine/GameConfig.h"
#include <fstream>
#include <filesystem>

TEST_CASE("GameConfig: default values")
{
    auto& config = GameConfig::GetInstance();
    config.GetWindowConfig() = t_WindowConfig{};
    CHECK(config.GetWindowConfig().width == 1280);
    CHECK(config.GetWindowConfig().height == 720);
    CHECK(config.GetWindowConfig().scene_width == 1280);
    CHECK(config.GetWindowConfig().scene_height == 720);
    CHECK(config.GetWindowConfig().scene_fps == 60);
    CHECK(config.GetWindowConfig().b_Fullscreen == false);
    CHECK(config.GetWindowConfig().b_Vsync == true);
}

TEST_CASE("GameConfig: GenerateConfigString roundtrip")
{
    auto& config = GameConfig::GetInstance();
    config.GetWindowConfig() = t_WindowConfig{};
    config.GetWindowConfig().width = 1920;
    config.GetWindowConfig().height = 1080;
    config.GetWindowConfig().scene_width = 640;
    config.GetWindowConfig().scene_height = 480;
    config.GetWindowConfig().scene_fps = 144;
    config.GetWindowConfig().title = "TestGame";

    std::string ini = config.GenerateConfigString();
    CHECK(ini.find("width=1920") != std::string::npos);
    CHECK(ini.find("height=1080") != std::string::npos);
    CHECK(ini.find("scene_width=640") != std::string::npos);
    CHECK(ini.find("scene_height=480") != std::string::npos);
    CHECK(ini.find("scene_fps=144") != std::string::npos);
    CHECK(ini.find("title=TestGame") != std::string::npos);
}

TEST_CASE("GameConfig: save and load roundtrip")
{
    auto& config = GameConfig::GetInstance();
    config.GetWindowConfig() = t_WindowConfig{};
    config.GetWindowConfig().width = 800;
    config.GetWindowConfig().height = 600;
    config.GetWindowConfig().scene_width = 400;
    config.GetWindowConfig().scene_fps = 30;

    std::string test_path = (std::filesystem::temp_directory_path() / "test_config.ini").string();
    CHECK(config.m_bSaveToFile(test_path));

    GameConfig& fresh = GameConfig::GetInstance();
    fresh.GetWindowConfig() = t_WindowConfig{};
    CHECK(fresh.m_bLoadFromFile(test_path));

    CHECK(fresh.GetWindowConfig().width == 800);
    CHECK(fresh.GetWindowConfig().height == 600);
    CHECK(fresh.GetWindowConfig().scene_width == 400);
    CHECK(fresh.GetWindowConfig().scene_fps == 30);

    std::filesystem::remove(test_path);
}

TEST_CASE("GameConfig: missing file returns false, keeps defaults")
{
    auto& config = GameConfig::GetInstance();
    config.GetWindowConfig() = t_WindowConfig{};
    config.GetWindowConfig().width = 999;
    bool loaded = config.m_bLoadFromFile("nonexistent_file_xyz.ini");
    CHECK_FALSE(loaded);
    CHECK(config.GetWindowConfig().width == 999);
}

TEST_CASE("GameConfig: ApplyExportSettings")
{
    auto& config = GameConfig::GetInstance();
    config.GetWindowConfig() = t_WindowConfig{};
    config.ApplyExportSettings(1024, 768, true, false, true, 144);

    CHECK(config.GetWindowConfig().width == 1024);
    CHECK(config.GetWindowConfig().height == 768);
    CHECK(config.GetWindowConfig().b_Fullscreen == true);
    CHECK(config.GetWindowConfig().b_Resizable == false);
    CHECK(config.GetWindowConfig().b_Vsync == true);
    CHECK(config.GetWindowConfig().target_fps == 144);
}
