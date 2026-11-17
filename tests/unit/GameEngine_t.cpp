#include "Engine/AssetResolver.h"
#include "Engine/GameEngine.h"
#include "Engine/GameMap.h"
#include "support/TempDir.h"
#include "doctest/doctest.h"
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace
{
class TestMap : public GameMap
{
public:
    TestMap() : GameMap("GuardTest") {}
};
} // namespace

TEST_CASE("scene save guard can block a dirty scene write")
{
    TempDir temp("raywaves_scene_guard");
    const fs::path assets = temp.Path() / "Assets";
    fs::create_directories(assets);

    GameEngine engine;
    TestMap map;
    AssetResolver::SetProjectAssetPath(assets.string());
    engine.SetMap(&map);

    engine.GetScene().AddObject("Crate");
    engine.SetSceneSaveGuard([]() { return false; });
    CHECK_FALSE(engine.b_SaveActiveScene());
    CHECK_FALSE(fs::exists(assets / "GuardTest.scene"));

    engine.SetSceneSaveGuard([]() { return true; });
    CHECK(engine.b_SaveActiveScene());
}
