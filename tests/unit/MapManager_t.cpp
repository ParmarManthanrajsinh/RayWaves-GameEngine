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

class Mock3DMap : public GameMap
{
public:
    using GameMap::GameMap;

    void Initialize() override {}
    void Update(float /*delta_time*/) override {}
    void Draw() override {}
    bool b_Uses3DView() const override { return true; }
    Camera3D GetViewCamera3D() const override
    {
        return Camera3D{ { 1.0f, 2.0f, 3.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, 60.0f, CAMERA_PERSPECTIVE };
    }
};

// Registers one editor entity every Initialize(). The manager must clear the
// registry first so repeated Initialize() does not accumulate duplicates.
class EditorRegistryMap : public GameMap
{
public:
    using GameMap::GameMap;

    void Initialize() override { RegisterEditorEntity("Player", &m_Position); }

    Vector2 m_Position{ 10.0f, 20.0f };
};

// Second registry map used to prove a transition replaces the old registry
// instead of carrying stale entities across maps.
class EditorRegistryOtherMap : public GameMap
{
public:
    using GameMap::GameMap;

    void Initialize() override { RegisterEditorEntity("Enemy", &m_Position); }

    Vector2 m_Position{ 30.0f, 40.0f };
};

// Records the runtime mode visible to the map when it is initialized, so the
// test proves MapManager forwards standalone mode to newly created maps.
class StandaloneProbeMap : public GameMap
{
public:
    using GameMap::GameMap;

    static inline bool s_StandaloneAtInit = false;

    void Initialize() override { s_StandaloneAtInit = b_IsStandalone(); }
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

TEST_CASE("MapManager: standalone mode reaches new maps")
{
    StandaloneProbeMap::s_StandaloneAtInit = false;

    MapManager mgr;
    mgr.SetStandaloneMode(true);
    CHECK(mgr.b_IsStandalone());

    mgr.RegisterMap<StandaloneProbeMap>("probe");
    mgr.SetInitialMap("probe");
    mgr.Initialize();
    CHECK(StandaloneProbeMap::s_StandaloneAtInit);

    // Future transitions must inherit the same mode.
    mgr.SetStandaloneMode(false);
    mgr.RegisterMap<StandaloneProbeMap>("other");
    CHECK(mgr.b_GotoMap("other", false));
    CHECK_FALSE(StandaloneProbeMap::s_StandaloneAtInit);
}

TEST_CASE("MapManager: 3D camera forwards to the current map")
{
    MapManager mgr;
    mgr.RegisterMap<Mock3DMap>("3d");
    CHECK(mgr.b_GotoMap("3d", false));

    CHECK(mgr.b_Uses3DView());
    const Camera3D camera = mgr.GetViewCamera3D();
    CHECK(camera.position.x == doctest::Approx(1.0f));
    CHECK(camera.position.y == doctest::Approx(2.0f));
    CHECK(camera.position.z == doctest::Approx(3.0f));
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

TEST_CASE("MapManager: GetCurrentMap and editor registry lifetime")
{
    MapManager manager;
    CHECK(manager.GetCurrentMap() == nullptr);

    manager.RegisterMap<EditorRegistryMap>("level");
    REQUIRE(manager.b_GotoMap("level", false));
    REQUIRE(manager.GetCurrentMap() != nullptr);
    CHECK(manager.GetCurrentMap()->GetEditorEntities().size() == 1);

    manager.Initialize();
    CHECK(manager.GetCurrentMap()->GetEditorEntities().size() == 1);
}

TEST_CASE("MapManager: unloading clears the current map pointer and id")
{
    MapManager manager;
    manager.RegisterMap<EditorRegistryMap>("level");
    REQUIRE(manager.b_GotoMap("level", false));
    REQUIRE(manager.GetCurrentMap() != nullptr);
    CHECK(manager.GetCurrentMapId() == "level");

    manager.UnloadCurrentMap();

    CHECK(manager.GetCurrentMap() == nullptr);
    CHECK(manager.GetCurrentMapId().empty());
    CHECK_FALSE(manager.b_IsCurrentMap("level"));
}

TEST_CASE("MapManager: failed transition keeps the current map alive")
{
    MapManager manager;
    manager.RegisterMap<EditorRegistryMap>("level");
    REQUIRE(manager.b_GotoMap("level", false));
    GameMap* current = manager.GetCurrentMap();
    REQUIRE(current != nullptr);

    CHECK_FALSE(manager.b_GotoMap("missing", false));

    CHECK(manager.GetCurrentMap() == current);
    CHECK(manager.GetCurrentMapId() == "level");
    CHECK(manager.GetCurrentMap()->GetEditorEntities().size() == 1);
}

TEST_CASE("MapManager: switching maps replaces the editor registry")
{
    MapManager manager;
    manager.RegisterMap<EditorRegistryMap>("first");
    manager.RegisterMap<EditorRegistryOtherMap>("second");

    REQUIRE(manager.b_GotoMap("first", false));
    REQUIRE(manager.GetCurrentMap() != nullptr);
    REQUIRE(manager.GetCurrentMap()->GetEditorEntities().size() == 1);
    CHECK(manager.GetCurrentMap()->GetEditorEntities().front().m_Name == "Player");

    REQUIRE(manager.b_GotoMap("second", false));
    REQUIRE(manager.GetCurrentMap() != nullptr);
    REQUIRE(manager.GetCurrentMap()->GetEditorEntities().size() == 1);
    CHECK(manager.GetCurrentMap()->GetEditorEntities().front().m_Name == "Enemy");
    CHECK(manager.GetCurrentMap()->GetEditorEntities().front().m_Position->x == doctest::Approx(30.0f));
}
