#include "doctest/doctest.h"
#include "../Engine/GameState.h"
#include "../Engine/GameMap.h"
#include "../Engine/MapManager.h"
#include <memory>

namespace
{
    // Simulates a game map that participates in hot-reload state preservation:
    // fills the StateBag on save, reads it back on load, exactly like the
    // editor's reload flow (SaveState -> destroy -> new instance -> LoadState).
    class ReloadTestMap : public GameMap
    {
    public:
        ReloadTestMap() : GameMap("ReloadTestMap") {}

        void SaveState(StateBag& out) const override
        {
            out.SetFloat("player_x", 120.5f);
            out.SetFloat("player_y", -33.25f);
            out.SetInt("score", 4200);
            out.SetBool("paused", true);
            out.SetString("level_name", "DemoLevel");
            out.SetVector2("velocity", {-4.5f, 9.75f});
        }

        void LoadState(const StateBag& in) override
        {
            m_PlayerX = in.GetFloat("player_x");
            m_PlayerY = in.GetFloat("player_y");
            m_Score = in.GetInt("score");
            m_bPaused = in.GetBool("paused");
            m_LevelName = in.GetString("level_name");
            m_Velocity = in.GetVector2("velocity");
        }

        float m_PlayerX = 0.0f;
        float m_PlayerY = 0.0f;
        int m_Score = 0;
        bool m_bPaused = false;
        std::string m_LevelName;
        Vector2 m_Velocity{};
    };
}

TEST_CASE("Reload round-trip: state survives instance swap")
{
    // 1. Old instance saves its state (like GameEditor before unload)
    StateBag reload_state;
    {
        ReloadTestMap old_map;
        old_map.SaveState(reload_state);
    }

    // 2. New instance is constructed fresh (like CreateGameMap after reload)
    ReloadTestMap new_map;
    CHECK(new_map.m_Score == 0);
    CHECK(new_map.m_bPaused == false);

    // 3. New instance loads the saved state
    new_map.LoadState(reload_state);

    CHECK(new_map.m_PlayerX == doctest::Approx(120.5f));
    CHECK(new_map.m_PlayerY == doctest::Approx(-33.25f));
    CHECK(new_map.m_Score == 4200);
    CHECK(new_map.m_bPaused == true);
    CHECK(new_map.m_LevelName == "DemoLevel");
    CHECK(new_map.m_Velocity.x == doctest::Approx(-4.5f));
    CHECK(new_map.m_Velocity.y == doctest::Approx(9.75f));
}

TEST_CASE("Reload round-trip: defaults apply for keys the new version dropped")
{
    // SaveState from an "old version" that wrote a key the new version
    // never reads: harmless. Conversely, keys missing from the bag fall
    // back to defaults without throwing.
    StateBag reload_state;
    reload_state.SetInt("legacy_key", 7);

    ReloadTestMap new_map;
    new_map.LoadState(reload_state);
    CHECK(new_map.m_Score == 0);
    CHECK(new_map.m_LevelName.empty());
}

TEST_CASE("Reload round-trip: MapManager SaveState/LoadState across swap")
{
    StateBag reload_state;

    {
        MapManager old_manager;
        ReloadTestMap* map = new ReloadTestMap();
        map->SaveState(reload_state);
        old_manager.RegisterMap<ReloadTestMap>("ReloadTestMap");
        old_manager.b_GotoMap("ReloadTestMap");
    }

    // Simulated reload: brand-new MapManager gets the same StateBag
    MapManager new_manager;
    new_manager.RegisterMap<ReloadTestMap>("ReloadTestMap");
    new_manager.b_GotoMap("ReloadTestMap");
    new_manager.LoadState(reload_state);

    CHECK(new_manager.b_IsMapManager() == true);
    CHECK(new_manager.b_IsCurrentMap("ReloadTestMap"));
}

TEST_CASE("Reload round-trip: MapManager identifies itself via b_IsMapManager")
{
    MapManager manager;
    ReloadTestMap plain_map;

    CHECK(manager.b_IsMapManager() == true);
    CHECK(plain_map.b_IsMapManager() == false);

    // Polymorphic query through the base pointer (how the editor sees it)
    GameMap* as_base = &manager;
    CHECK(as_base->b_IsMapManager() == true);
    as_base = &plain_map;
    CHECK(as_base->b_IsMapManager() == false);
}
