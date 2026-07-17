#include "doctest/doctest.h"
#include "../Engine/GameMap.h"
#include "../Engine/GameState.h"
#include "../Engine/AssetResolver.h"

class TestGameMap : public GameMap {
public:
    using GameMap::GameMap;

    void Initialize() override {}
    void Update(float /*delta_time*/) override {}
    void Draw() override {}
    void SaveState(StateBag& out) const override { out = m_SavedState; }
    void LoadState(const StateBag& in) override { m_SavedState = in; }

    StateBag m_SavedState;
};

TEST_CASE("GameMap: SetProjectAssetPath")
{
    TestGameMap map;
    map.SetProjectAssetPath("C:/Test/Assets");
    CHECK(AssetResolver::GetProjectAssetPath() == "C:/Test/Assets");
}

TEST_CASE("GameMap: SetSceneBounds")
{
    TestGameMap map;
    map.SetSceneBounds(1920.0f, 1080.0f);
    auto bounds = map.GetSceneBounds();
    CHECK(bounds.x == 1920.0f);
    CHECK(bounds.y == 1080.0f);
}

TEST_CASE("GameMap: SetTargetFPS")
{
    TestGameMap map;
    map.SetTargetFPS(30);
    CHECK(map.GetTargetFPS() == 30);
    map.SetTargetFPS(60);
    CHECK(map.GetTargetFPS() == 60);
}

TEST_CASE("GameMap: GetMapName")
{
    TestGameMap map("test_map");
    CHECK(map.GetMapName() == "test_map");
}

TEST_CASE("GameMap: SaveState/LoadState roundtrip")
{
    TestGameMap map;
    StateBag bag;
    bag.SetInt("health", 100);
    map.LoadState(bag);

    StateBag saved;
    map.SaveState(saved);
    CHECK(saved.GetInt("health") == 100);
}
