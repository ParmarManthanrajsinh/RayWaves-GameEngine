#include "Engine/GameState.h"
#include "Engine/rw.h"
#include "doctest/doctest.h"
#include <span>
#include <string>
#include <utility>

namespace
{
class TestLevel : public rw::Level
{
public:
    TestLevel() : rw::Level("TestLevel") {}

    int m_StartCount = 0;

protected:
    void OnStart() override
    {
        ++m_StartCount;
        Spawn("Player", { 100.0f, 200.0f }, { 40.0f, 60.0f }, rw::Origin::Center);
        Spawn("Crate", { 10.0f, 20.0f });
    }

    void OnSave(StateBag& out) const override { out.SetInt("test_start_count", m_StartCount); }
    void OnLoad(const StateBag& in) override { m_StartCount = in.GetInt("test_start_count", m_StartCount); }
};
} // namespace

TEST_CASE("rw::Level: Spawn registers entities with the editor")
{
    TestLevel level;
    level.Initialize();

    const auto& entities = level.Entities();
    REQUIRE(entities.size() == 2);

    const rw::Entity& player = *entities[0];
    CHECK(player.name == "Player");
    CHECK(player.id != 0);
    CHECK(player.position.x == doctest::Approx(100.0f));
    CHECK(player.origin.x == doctest::Approx(-20.0f));
    CHECK(player.origin.y == doctest::Approx(-30.0f));

    const std::span<const t_EditorEntity> registered = level.GetEditorEntities();
    REQUIRE(registered.size() == 2);
    CHECK(registered[0].m_Name == "Player");
    CHECK(registered[0].m_Position == &player.position);
    CHECK(registered[0].m_Size == &player.size);
    CHECK(registered[0].m_Visible == &player.visible);
    CHECK(registered[0].m_Rotation == &player.rotation);
}

TEST_CASE("rw::Level: Inspector size edits reach the entity")
{
    TestLevel level;
    level.Initialize();

    std::span<t_EditorEntity> registered = level.GetEditorEntities();
    REQUIRE(!registered.empty());
    registered[0].m_Size->x = 128.0f;

    CHECK(level.Entities()[0]->size.x == doctest::Approx(128.0f));
}

TEST_CASE("rw::Level: Initialize rebuilds the level from scratch")
{
    TestLevel level;
    level.Initialize();
    level.Initialize();

    CHECK(level.m_StartCount == 2);
    CHECK(level.Entities().size() == 2);
    CHECK(level.GetEditorEntities().size() == 2);
}

TEST_CASE("rw::Level: Destroy unregisters and removes one entity")
{
    TestLevel level;
    level.Initialize();

    rw::Entity* first = level.Entities()[0].get();
    level.Destroy(first);

    CHECK(level.Entities().size() == 1);
    CHECK(level.GetEditorEntities().size() == 1);
    CHECK(level.Entities()[0]->name == "Crate");

    level.Destroy(nullptr);
    CHECK(level.Entities().size() == 1);
}

TEST_CASE("rw::Level: ClearEntities empties entities and registry")
{
    TestLevel level;
    level.Initialize();

    level.ClearEntities();

    CHECK(level.Entities().empty());
    CHECK(level.GetEditorEntities().empty());
}

TEST_CASE("rw::Level: Spawn can set a type separate from the name")
{
    TestLevel level;
    level.Initialize();

    rw::Entity* entity = level.Spawn("Slime 2", { 0.0f, 0.0f }, { 10.0f, 10.0f }, rw::Origin::TopLeft, "Enemy");

    REQUIRE(entity != nullptr);
    CHECK(entity->name == "Slime 2");
    CHECK(entity->type == "Enemy");
    CHECK(level.GetEditorEntities().back().m_Type == "Enemy");
}

TEST_CASE("rw::Level: camera is exposed to the editor")
{
    TestLevel level;
    level.Initialize();

    Camera2D* camera = level.GetMutableViewCamera();
    REQUIRE(camera != nullptr);
    CHECK(camera == &level.Cam().Get());

    level.Cam().SetZoom(2.0f);
    CHECK(level.GetViewCamera().zoom == doctest::Approx(2.0f));
}

TEST_CASE("rw::Level: hot reload restores entity transforms")
{
    TestLevel level;
    level.Initialize();

    StateBag saved;
    level.SaveState(saved);
    CHECK(saved.GetInt("rw.entity.count", 0) == 2);

    level.Entities()[0]->position = { 500.0f, 600.0f };
    level.Entities()[0]->rotation = 45.0f;
    level.LoadState(saved);

    CHECK(level.Entities()[0]->position.x == doctest::Approx(100.0f));
    CHECK(level.Entities()[0]->position.y == doctest::Approx(200.0f));
    CHECK(level.Entities()[0]->rotation == doctest::Approx(0.0f));
}

TEST_CASE("rw::Camera: bounds clamp the target")
{
    rw::Camera camera;
    camera.SetViewport(800.0f, 600.0f);
    camera.SetZoom(1.0f);
    camera.SetBounds(0.0f, 1600.0f, 0.0f, 1200.0f);

    camera.Snap({ 5000.0f, 5000.0f });
    CHECK(camera.GetTarget().x == doctest::Approx(1200.0f));
    CHECK(camera.GetTarget().y == doctest::Approx(900.0f));

    camera.Snap({ -5000.0f, -5000.0f });
    CHECK(camera.GetTarget().x == doctest::Approx(400.0f));
    CHECK(camera.GetTarget().y == doctest::Approx(300.0f));
}

TEST_CASE("rw::Camera: bounds smaller than the view center the target")
{
    rw::Camera camera;
    camera.SetViewport(800.0f, 600.0f);
    camera.SetZoom(1.0f);
    camera.SetBounds(0.0f, 400.0f, 0.0f, 300.0f);

    camera.Snap({ 1234.0f, 5678.0f });
    CHECK(camera.GetTarget().x == doctest::Approx(200.0f));
    CHECK(camera.GetTarget().y == doctest::Approx(150.0f));
}

TEST_CASE("rw::Camera: follow eases toward the target")
{
    rw::Camera camera;
    camera.SetViewport(800.0f, 600.0f);
    camera.SetZoom(1.0f);
    camera.Snap({ 400.0f, 300.0f });

    camera.Follow({ 800.0f, 300.0f }, 0.1f, 8.0f);
    CHECK(camera.GetTarget().x == doctest::Approx(720.0f));
    CHECK(camera.GetTarget().y == doctest::Approx(300.0f));

    camera.Follow({ 800.0f, 300.0f }, 1.0f, 8.0f);
    CHECK(camera.GetTarget().x == doctest::Approx(800.0f));
}

TEST_CASE("rw::Camera: zoom respects the minimum")
{
    rw::Camera camera;
    camera.SetMinZoom(2.0f);
    camera.SetZoom(0.5f);
    CHECK(camera.GetZoom() == doctest::Approx(2.0f));

    camera.SetZoom(3.0f);
    CHECK(camera.GetZoom() == doctest::Approx(3.0f));
}

TEST_CASE("rw::Texture: empty texture is inert")
{
    rw::Texture texture;
    CHECK(!texture.IsValid());

    texture.Unload();
    CHECK(!texture.IsValid());

    rw::Texture moved = std::move(texture);
    CHECK(!moved.IsValid());
}

TEST_CASE("rw::Game: levels register and the initial level starts")
{
    rw::Game game;
    game.AddLevel<TestLevel>("test");
    game.Start("test");
    game.Initialize();

    REQUIRE(game.GetCurrentMap() != nullptr);
    CHECK(game.GetCurrentMapId() == "test");
    CHECK(game.GetCurrentMap()->GetEditorEntities().size() == 2);
    CHECK(game.GetCurrentMap()->GetMutableViewCamera() != nullptr);
}
