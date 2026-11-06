#include "Engine/MapManager.h"
#include "doctest/doctest.h"

class MockGameMap : public GameMap
{
public:
    using GameMap::GameMap;

    void Initialize() override { m_Initialized = true; }
    void Update(float /*delta_time*/) override {}
    void Draw() override {}
    void SaveState(StateBag& out) const override {}
    void LoadState(const StateBag& in) override {}

    bool m_Initialized = false;
};

// Requests a transition from inside Initialize(). If the swap were applied
// synchronously, this map would be destroyed while Initialize() is still on
// the stack. The counters detect that without touching freed memory.
class ChainedInitMap : public GameMap
{
public:
    using GameMap::GameMap;

    static inline int s_InitDepth = 0;
    static inline int s_DestroyedDuringInit = 0;

    ~ChainedInitMap() override
    {
        if (s_InitDepth > 0)
            ++s_DestroyedDuringInit;
    }

    void Initialize() override
    {
        ++s_InitDepth;
        RequestGotoMap("b", false);
        --s_InitDepth;
    }
};

// Requests a transition from inside Update().
class ChainedUpdateMap : public GameMap
{
public:
    using GameMap::GameMap;

    void Update(float /*delta_time*/) override { RequestGotoMap("b", false); }
};

struct PingPongTally
{
    static inline int s_Count = 0;
};

// Each map asks for the other from Initialize(). Applied synchronously this
// recurses without bound; with the per-frame cap it stops after a few swaps.
class PingMap : public GameMap
{
public:
    using GameMap::GameMap;

    void Initialize() override
    {
        ++PingPongTally::s_Count;
        RequestGotoMap("pong", false);
    }
};

class PongMap : public GameMap
{
public:
    using GameMap::GameMap;

    void Initialize() override
    {
        ++PingPongTally::s_Count;
        RequestGotoMap("ping", false);
    }
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

TEST_CASE("MapManager: transition requested in Initialize is deferred until the frame")
{
    ChainedInitMap::s_DestroyedDuringInit = 0;

    MapManager mgr;
    mgr.RegisterMap<ChainedInitMap>("a");
    mgr.RegisterMap<MockGameMap>("b");

    CHECK(mgr.b_GotoMap("a", false));
    CHECK(mgr.b_IsCurrentMap("a"));
    CHECK(ChainedInitMap::s_DestroyedDuringInit == 0);

    mgr.Update(0.016f);
    CHECK(mgr.b_IsCurrentMap("b"));
    CHECK(ChainedInitMap::s_DestroyedDuringInit == 0);
}

TEST_CASE("MapManager: transition requested in Update applies after the frame")
{
    MapManager mgr;
    mgr.RegisterMap<ChainedUpdateMap>("a");
    mgr.RegisterMap<MockGameMap>("b");

    CHECK(mgr.b_GotoMap("a", false));
    CHECK(mgr.b_IsCurrentMap("a"));

    mgr.Update(0.016f);
    CHECK(mgr.b_IsCurrentMap("b"));
}

TEST_CASE("MapManager: mutual transitions stop at the per-frame limit")
{
    PingPongTally::s_Count = 0;

    MapManager mgr;
    mgr.RegisterMap<PingMap>("ping");
    mgr.RegisterMap<PongMap>("pong");

    CHECK(mgr.b_GotoMap("ping", false));
    mgr.Update(0.016f);

    CHECK(PingPongTally::s_Count > 1);
    CHECK(PingPongTally::s_Count <= 1 + MapManager::k_MaxTransitionsPerDrain);
}

TEST_CASE("MapManager: unloading drops a queued transition")
{
    MapManager mgr;
    mgr.RegisterMap<ChainedInitMap>("a");
    mgr.RegisterMap<MockGameMap>("b");

    CHECK(mgr.b_GotoMap("a", false));
    mgr.UnloadCurrentMap();
    mgr.Update(0.016f);

    CHECK_FALSE(mgr.b_IsCurrentMap("b"));
}
