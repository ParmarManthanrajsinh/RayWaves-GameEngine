#include "doctest/doctest.h"
#include "../Engine/MapManager.h"

class MockGameMap : public GameMap {
public:
    using GameMap::GameMap;

    void Initialize() override { m_Initialized = true; }
    void Update(float /*delta_time*/) override {}
    void Draw() override {}
    void SaveState(StateBag& out) const override {}
    void LoadState(const StateBag& in) override {}

    bool m_Initialized = false;
};

TEST_CASE("MapManager: register and navigate maps")
{
    MapManager mgr;
    mgr.Initialize();

    mgr.RegisterMap<MockGameMap>("menu");
    mgr.RegisterMap<MockGameMap>("level1");

    CHECK(mgr.b_IsMapRegistered("menu"));
    CHECK(mgr.b_IsMapRegistered("level1"));
    CHECK_FALSE(mgr.b_IsMapRegistered("nonexistent"));

    CHECK(mgr.b_GotoMap("menu", false));
    CHECK(mgr.b_IsCurrentMap("menu"));

    CHECK(mgr.b_GotoMap("level1", false));
    CHECK(mgr.b_IsCurrentMap("level1"));
}

TEST_CASE("MapManager: get available maps")
{
    MapManager mgr;
    mgr.RegisterMap<MockGameMap>("a");
    mgr.RegisterMap<MockGameMap>("b");

    auto maps = mgr.GetAvailableMaps();
    CHECK(maps.size() == 2);
    CHECK((maps[0] == "a" || maps[0] == "b"));
}

TEST_CASE("MapManager: set initial map")
{
    MapManager mgr;
    mgr.RegisterMap<MockGameMap>("start");
    mgr.SetInitialMap("start");
    mgr.Initialize();
    CHECK(mgr.b_IsCurrentMap("start"));
}

TEST_CASE("MapManager: unload current map clears pointer")
{
    MapManager mgr;
    mgr.RegisterMap<MockGameMap>("test");
    mgr.Initialize();
    mgr.b_GotoMap("test", false);
    mgr.UnloadCurrentMap();
    CHECK_FALSE(mgr.b_IsCurrentMap("test"));
}
