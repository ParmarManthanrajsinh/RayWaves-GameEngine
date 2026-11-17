#include "Engine/AssetResolver.h"
#include "Engine/GameMap.h"
#include "Engine/GameState.h"
#include "doctest/doctest.h"

class TestGameMap : public GameMap
{
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

TEST_CASE("GameMap: standalone mode defaults off and round-trips")
{
    TestGameMap map;
    CHECK_FALSE(map.b_IsStandalone());

    map.SetStandaloneMode(true);
    CHECK(map.b_IsStandalone());

    map.SetStandaloneMode(false);
    CHECK_FALSE(map.b_IsStandalone());
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

TEST_CASE("GameMap: editor entity accepts a 3D-only position")
{
    TestGameMap map;
    Vector3 position{ 1.0f, 2.0f, 3.0f };
    Vector3 size{ 0.8f, 1.8f, 0.8f };

    t_EditorEntity entity;
    entity.m_Name = "Player3D";
    entity.m_Position3D = &position;
    entity.m_Size3D = &size;

    t_EditorEntity* stored = map.RegisterEditorEntity(entity);
    REQUIRE(stored != nullptr);
    CHECK(stored->m_Position == nullptr);
    CHECK(stored->m_Position3D == &position);
    CHECK(stored->m_Size3D == &size);
}

TEST_CASE("GameMap: AddEntity defaults and options")
{
    TestGameMap map;
    Vector2 position{ 5.0f, 6.0f };
    Vector2 size{ 20.0f, 30.0f };

    t_EditorEntity* entity = map.AddEntity("Player", &position, &size, { .Origin = { -10.0f, -15.0f } });
    REQUIRE(entity != nullptr);
    CHECK(entity->m_Id != 0);
    CHECK(entity->m_Name == "Player");
    CHECK(entity->m_Type == "Player");
    CHECK(entity->m_Position == &position);
    CHECK(entity->m_Size == &size);
    CHECK(entity->m_Origin.x == doctest::Approx(-10.0f));
    CHECK(entity->m_Rotation == nullptr);
    CHECK(entity->m_Visible == nullptr);

    float rotation = 0.0f;
    bool visible = true;
    t_EditorEntity* typed = map.AddEntity("Slime", &position, nullptr,
        { .Type = "Enemy", .Rotation = &rotation, .Visible = &visible, .Id = 42 });
    REQUIRE(typed != nullptr);
    CHECK(typed->m_Type == "Enemy");
    CHECK(typed->m_Rotation == &rotation);
    CHECK(typed->m_Visible == &visible);
    CHECK(typed->m_Id == 42);

    CHECK(map.AddEntity("Broken", static_cast<Vector2*>(nullptr)) == nullptr);
}

TEST_CASE("GameMap: AddEntity 3D overload")
{
    TestGameMap map;
    Vector3 position{ 1.0f, 2.0f, 3.0f };
    Vector3 size{ 0.5f, 1.0f, 0.5f };

    t_EditorEntity* entity =
        map.AddEntity("Camera", &position, &size, { .Origin3D = { 0.0f, 1.0f, 0.0f }, .Type = "Camera" });
    REQUIRE(entity != nullptr);
    CHECK(entity->m_Position == nullptr);
    CHECK(entity->m_Position3D == &position);
    CHECK(entity->m_Size3D == &size);
    CHECK(entity->m_Type == "Camera");
    CHECK(entity->m_Origin3D.y == doctest::Approx(1.0f));
}

TEST_CASE("GameMap: editor 3D camera overrides the render camera")
{
    TestGameMap map;
    CHECK(!map.b_HasEditorViewCamera3D());
    CHECK(map.GetRenderCamera3D().fovy == doctest::Approx(60.0f));

    const Camera3D editor_camera{ { 1.0f, 2.0f, 3.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, 45.0f,
        CAMERA_PERSPECTIVE };
    map.SetEditorViewCamera3D(editor_camera);

    CHECK(map.b_HasEditorViewCamera3D());
    const Camera3D rendered = map.GetRenderCamera3D();
    CHECK(rendered.position.x == doctest::Approx(1.0f));
    CHECK(rendered.position.y == doctest::Approx(2.0f));
    CHECK(rendered.position.z == doctest::Approx(3.0f));
    CHECK(rendered.fovy == doctest::Approx(45.0f));

    map.ClearEditorViewCamera3D();
    CHECK(!map.b_HasEditorViewCamera3D());
    CHECK(map.GetRenderCamera3D().fovy == doctest::Approx(60.0f));
}

TEST_CASE("GameMap: editor entity registration")
{
    TestGameMap map;
    Vector2 position{ 10.0f, 20.0f };
    Vector2 size{ 32.0f, 48.0f };

    t_EditorEntity* entity = map.RegisterEditorEntity("Player", &position, &size);
    REQUIRE(entity != nullptr);
    CHECK(entity->m_Id != 0);
    CHECK(entity->m_Name == "Player");
    CHECK(entity->m_Type == "Player");
    CHECK(entity->m_Position == &position);
    CHECK(entity->m_Size == &size);

    const std::span<const t_EditorEntity> entities = map.GetEditorEntities();
    REQUIRE(entities.size() == 1);
    CHECK(entities[0].m_Id == entity->m_Id);

    entities[0].m_Position->x = 99.0f;
    CHECK(position.x == doctest::Approx(99.0f));

    CHECK(map.UnregisterEditorEntity(entity->m_Id));
    CHECK(map.GetEditorEntities().empty());
    CHECK(!map.UnregisterEditorEntity(entity->m_Id));
}

TEST_CASE("GameMap: editor entity rejects null position")
{
    TestGameMap map;
    CHECK(map.RegisterEditorEntity("Broken", nullptr) == nullptr);
    CHECK(map.GetEditorEntities().empty());
}

TEST_CASE("GameMap: editor entity ids are unique and reset on clear")
{
    TestGameMap map;
    Vector2 first{ 0.0f, 0.0f };
    Vector2 second{ 0.0f, 0.0f };

    t_EditorEntity* a = map.RegisterEditorEntity("A", &first);
    REQUIRE(a != nullptr);
    const uint64_t a_id = a->m_Id;

    t_EditorEntity* b = map.RegisterEditorEntity("B", &second);
    REQUIRE(b != nullptr);
    const uint64_t b_id = b->m_Id;
    CHECK(a_id != b_id);

    const uint64_t duplicate_id = a_id;
    t_EditorEntity custom;
    custom.m_Id = duplicate_id;
    custom.m_Name = "C";
    custom.m_Position = &first;
    t_EditorEntity* c = map.RegisterEditorEntity(custom);
    REQUIRE(c != nullptr);
    CHECK(c->m_Id != duplicate_id);

    map.ClearEditorEntities();
    CHECK(map.GetEditorEntities().empty());
    t_EditorEntity* reset = map.RegisterEditorEntity("Reset", &first);
    REQUIRE(reset != nullptr);
    CHECK(reset->m_Id == 1);
}

TEST_CASE("GameMap: explicit editor entity id advances the allocator")
{
    TestGameMap map;
    Vector2 position{};
    Vector2 first{};
    Vector2 second{};

    t_EditorEntity custom;
    custom.m_Id = 500;
    custom.m_Position = &position;

    t_EditorEntity* explicit_entity = map.RegisterEditorEntity(custom);
    REQUIRE(explicit_entity != nullptr);
    CHECK(explicit_entity->m_Id == 500);
    CHECK(explicit_entity->m_Name == "Entity 500");
    CHECK(explicit_entity->m_Type == "Entity 500");

    t_EditorEntity* next = map.RegisterEditorEntity("Next", &first);
    REQUIRE(next != nullptr);
    CHECK(next->m_Id == 501);

    t_EditorEntity zero_id;
    zero_id.m_Id = 0;
    zero_id.m_Name = "Zero";
    zero_id.m_Position = &second;
    t_EditorEntity* auto_id = map.RegisterEditorEntity(zero_id);
    REQUIRE(auto_id != nullptr);
    CHECK(auto_id->m_Id == 502);
}

TEST_CASE("GameMap: unregister keeps remaining entities stable")
{
    TestGameMap map;
    Vector2 first{};
    Vector2 middle{};
    Vector2 last{};

    t_EditorEntity* a = map.RegisterEditorEntity("A", &first);
    t_EditorEntity* b = map.RegisterEditorEntity("B", &middle);
    t_EditorEntity* c = map.RegisterEditorEntity("C", &last);
    REQUIRE(a != nullptr);
    REQUIRE(b != nullptr);
    REQUIRE(c != nullptr);

    const uint64_t a_id = a->m_Id;
    const uint64_t c_id = c->m_Id;
    CHECK(map.UnregisterEditorEntity(b->m_Id));
    CHECK(!map.UnregisterEditorEntity(0));

    const std::span<const t_EditorEntity> entities = map.GetEditorEntities();
    REQUIRE(entities.size() == 2);
    CHECK(entities[0].m_Id == a_id);
    CHECK(entities[0].m_Name == "A");
    CHECK(entities[1].m_Id == c_id);
    CHECK(entities[1].m_Name == "C");
}

TEST_CASE("GameMap: mutable editor entity span edits live data")
{
    TestGameMap map;
    Vector2 position{ 1.0f, 2.0f };
    REQUIRE(map.RegisterEditorEntity("Sprite", &position) != nullptr);

    std::span<t_EditorEntity> entities = map.GetEditorEntities();
    REQUIRE(entities.size() == 1);
    entities[0].m_Name = "Renamed";
    entities[0].m_Position->y = 12.0f;

    CHECK(map.GetEditorEntities()[0].m_Name == "Renamed");
    CHECK(position.y == doctest::Approx(12.0f));
}

TEST_CASE("GameMap: editor entity keeps optional transform fields")
{
    TestGameMap map;
    Vector2 position{};
    Vector2 size{};
    float rotation = 45.0f;
    bool visible = false;

    t_EditorEntity entity;
    entity.m_Name = "Sprite";
    entity.m_Position = &position;
    entity.m_Size = &size;
    entity.m_Origin = { 4.0f, 8.0f };
    entity.m_Rotation = &rotation;
    entity.m_Visible = &visible;

    t_EditorEntity* registered = map.RegisterEditorEntity(entity);
    REQUIRE(registered != nullptr);
    CHECK(registered->m_Type == "Sprite");
    CHECK(registered->m_Size == &size);
    CHECK(registered->m_Origin.x == doctest::Approx(4.0f));
    CHECK(registered->m_Origin.y == doctest::Approx(8.0f));
    CHECK(registered->m_Rotation == &rotation);
    CHECK(registered->m_Visible == &visible);
}
