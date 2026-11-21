#include "Engine/AssetResolver.h"
#include "Engine/Camera2DMath.h"
#include "Engine/GameEngine.h"
#include "Engine/GameMap.h"
#include "Engine/Scene.h"
#include "doctest/doctest.h"
#include "support/TempDir.h"
#include <filesystem>
#include <fstream>

namespace
{
class SceneTestMap : public GameMap
{
public:
    SceneTestMap() : GameMap("SceneKey") {}
    void Initialize() override {}
    void Update(float /*delta_time*/) override {}
    void Draw() override {}
    void OnSceneChanged() override { ++m_ScenePushes; }

    int m_ScenePushes = 0;
};

class SceneMapA : public GameMap
{
public:
    SceneMapA() : GameMap("MapA") {}
    void Initialize() override {}
    void Update(float /*delta_time*/) override {}
    void Draw() override {}
};

class SceneMapB : public GameMap
{
public:
    SceneMapB() : GameMap("MapB") {}
    void Initialize() override {}
    void Update(float /*delta_time*/) override {}
    void Draw() override {}
};

class NamedSceneMap : public GameMap
{
public:
    using GameMap::GameMap;
    void Initialize() override {}
    void Update(float /*delta_time*/) override {}
    void Draw() override {}
};

class EditorRegistryEngineMap : public GameMap
{
public:
    EditorRegistryEngineMap() : GameMap("RegistryKey") {}
    void Initialize() override { RegisterEditorEntity("Player", &m_Position); }
    void Update(float /*delta_time*/) override {}
    void Draw() override {}

    Vector2 m_Position{ 5.0f, 6.0f };
};
} // namespace

TEST_CASE("Scene: add, find, remove")
{
    Scene scene;
    CHECK(scene.GetObjects().empty());

    t_SceneObject& first = scene.AddObject("Player");
    CHECK(first.m_Id == 1);
    CHECK(first.m_Name == "Player");
    CHECK(scene.b_IsDirty());

    t_SceneObject& second = scene.AddObject("");
    CHECK(second.m_Id == 2);
    CHECK(second.m_Name == "Object");

    CHECK(scene.FindObject(2) == &second);
    CHECK(scene.FindObject(99) == nullptr);

    CHECK(scene.b_RemoveObject(1));
    CHECK(scene.FindObject(1) == nullptr);
    CHECK(!scene.b_RemoveObject(1));
}

TEST_CASE("Scene: duplicate keeps values but not GPU handles")
{
    Scene scene;
    t_SceneObject& source = scene.AddObject("Crate");
    source.m_TexturePath = "crate.png";
    source.m_Position = { 10.0f, 20.0f };
    source.m_Layer = 4;

    t_SceneObject* copy = scene.DuplicateObject(source.m_Id);
    REQUIRE(copy != nullptr);
    CHECK(copy->m_Id != source.m_Id);
    CHECK(copy->m_Name == "Crate");
    CHECK(copy->m_TexturePath == "crate.png");
    CHECK(copy->m_Texture.id == 0);
    CHECK(copy->m_Position.x == doctest::Approx(10.0f));
    CHECK(copy->m_Position.y == doctest::Approx(20.0f));
    CHECK(copy->m_Layer == 4);
}

TEST_CASE("Scene: save and load roundtrip")
{
    TempDir temp_dir("raywaves_scene");
    const std::string path = (temp_dir.Path() / "Level.scene").string();

    Scene scene;
    t_SceneObject& object = scene.AddObject("Player");
    object.m_Type = "spawn";
    object.m_TexturePath = "player.png";
    object.m_Position = { 100.5f, 220.25f };
    object.m_Rotation = 45.0f;
    object.m_Scale = { 2.0f, 3.0f };
    object.m_Source = { 1.0f, 2.0f, 32.0f, 48.0f };
    object.m_Tint = Color{ 10, 20, 30, 40 };
    object.m_Layer = 3;
    object.m_bVisible = false;

    CHECK(scene.b_SaveToFile(path));
    CHECK(!scene.b_IsDirty());

    Scene loaded;
    CHECK(loaded.b_LoadFromFile(path));
    REQUIRE(loaded.GetObjects().size() == 1);

    const t_SceneObject& copy = loaded.GetObjects().front();
    CHECK(copy.m_Id == 1);
    CHECK(copy.m_Name == "Player");
    CHECK(copy.m_Type == "spawn");
    CHECK(copy.m_TexturePath == "player.png");
    CHECK(copy.m_Position.x == doctest::Approx(100.5f));
    CHECK(copy.m_Position.y == doctest::Approx(220.25f));
    CHECK(copy.m_Rotation == doctest::Approx(45.0f));
    CHECK(copy.m_Scale.x == doctest::Approx(2.0f));
    CHECK(copy.m_Scale.y == doctest::Approx(3.0f));
    CHECK(copy.m_Source.width == doctest::Approx(32.0f));
    CHECK(copy.m_Source.height == doctest::Approx(48.0f));
    CHECK(copy.m_Tint.r == 10);
    CHECK(copy.m_Tint.g == 20);
    CHECK(copy.m_Tint.b == 30);
    CHECK(copy.m_Tint.a == 40);
    CHECK(copy.m_Layer == 3);
    CHECK(!copy.m_bVisible);
}

TEST_CASE("Scene: malformed values keep defaults")
{
    TempDir temp_dir("raywaves_scene_bad");
    const std::filesystem::path path = temp_dir.Path() / "Bad.scene";

    {
        std::ofstream file(path);
        file << "[scene]\n";
        file << "[object]\n";
        file << "name=Broken\n";
        file << "rotation=not_a_number\n";
        file << "layer=7\n";
    }

    Scene scene;
    CHECK(scene.b_LoadFromFile(path.string()));
    REQUIRE(scene.GetObjects().size() == 1);
    CHECK(scene.GetObjects().front().m_Rotation == doctest::Approx(0.0f));
    CHECK(scene.GetObjects().front().m_Layer == 7);
}

TEST_CASE("Scene: next id continues after loaded ids")
{
    TempDir temp_dir("raywaves_scene_ids");
    const std::filesystem::path path = temp_dir.Path() / "Ids.scene";

    {
        std::ofstream file(path);
        file << "[object]\n";
        file << "id=42\n";
        file << "name=Existing\n";
    }

    Scene scene;
    CHECK(scene.b_LoadFromFile(path.string()));
    t_SceneObject& next = scene.AddObject("Next");
    CHECK(next.m_Id == 43);
}

TEST_CASE("Scene: missing file is not an error")
{
    Scene scene;
    CHECK(!scene.b_LoadFromFile("does_not_exist.scene"));
    CHECK(scene.GetObjects().empty());
}

TEST_CASE("Scene: file key is path-safe")
{
    CHECK(Scene::ToFileKey("Platformer2D") == "Platformer2D");
    CHECK(Scene::ToFileKey("My Level") == "My_Level");
    CHECK(Scene::ToFileKey("../evil") == "___evil");
    CHECK(Scene::ToFileKey("a/b\\c") == "a_b_c");
}

TEST_CASE("Scene: clear resets ids and dirty state")
{
    Scene scene;
    scene.AddObject("First");
    CHECK(scene.b_IsDirty());

    scene.Clear();

    CHECK(scene.GetObjects().empty());
    CHECK_FALSE(scene.b_IsDirty());
    t_SceneObject& next = scene.AddObject("Second");
    CHECK(next.m_Id == 1);
}

TEST_CASE("Scene: duplicate and remove only dirty on success")
{
    Scene scene;
    t_SceneObject& source = scene.AddObject("Source");
    const uint32_t source_id = source.m_Id;
    scene.ClearDirty();

    CHECK(scene.DuplicateObject(9999) == nullptr);
    CHECK_FALSE(scene.b_IsDirty());
    CHECK_FALSE(scene.b_RemoveObject(9999));
    CHECK_FALSE(scene.b_IsDirty());

    t_SceneObject* copy = scene.DuplicateObject(source_id);
    REQUIRE(copy != nullptr);
    CHECK(scene.b_IsDirty());

    scene.ClearDirty();
    CHECK(scene.b_RemoveObject(copy->m_Id));
    CHECK(scene.b_IsDirty());
}

TEST_CASE("Scene: empty paths are rejected")
{
    Scene scene;
    CHECK_FALSE(scene.b_SaveToFile(""));
    CHECK_FALSE(scene.b_LoadFromFile(""));
}

TEST_CASE("Scene: load replaces the existing object list")
{
    TempDir temp_dir("raywaves_scene_replace");
    const std::filesystem::path path = temp_dir.Path() / "Replace.scene";
    {
        std::ofstream file(path);
        file << "[object]\n";
        file << "id=7\n";
        file << "name=Replaced\n";
    }

    Scene scene;
    scene.AddObject("Old");
    REQUIRE(scene.b_LoadFromFile(path.string()));

    REQUIRE(scene.GetObjects().size() == 1);
    CHECK(scene.GetObjects().front().m_Id == 7);
    CHECK(scene.GetObjects().front().m_Name == "Replaced");
    CHECK_FALSE(scene.b_IsDirty());
}

TEST_CASE("Scene: multiple objects keep order and ids")
{
    TempDir temp_dir("raywaves_scene_multi");
    const std::filesystem::path path = temp_dir.Path() / "Multi.scene";

    Scene scene;
    t_SceneObject& back = scene.AddObject("Back");
    back.m_Layer = -2;
    t_SceneObject& front = scene.AddObject("Front");
    front.m_Layer = 5;

    REQUIRE(scene.b_SaveToFile(path.string()));

    Scene loaded;
    REQUIRE(loaded.b_LoadFromFile(path.string()));
    REQUIRE(loaded.GetObjects().size() == 2);
    CHECK(loaded.GetObjects()[0].m_Name == "Back");
    CHECK(loaded.GetObjects()[0].m_Id == 1);
    CHECK(loaded.GetObjects()[0].m_Layer == -2);
    CHECK(loaded.GetObjects()[1].m_Name == "Front");
    CHECK(loaded.GetObjects()[1].m_Id == 2);
    CHECK(loaded.GetObjects()[1].m_Layer == 5);

    t_SceneObject& next = loaded.AddObject("Next");
    CHECK(next.m_Id == 3);
}

TEST_CASE("Scene: objects without ids get sequential ids")
{
    TempDir temp_dir("raywaves_scene_no_ids");
    const std::filesystem::path path = temp_dir.Path() / "NoIds.scene";
    {
        std::ofstream file(path);
        file << "[object]\n";
        file << "name=One\n";
        file << "[object]\n";
        file << "name=Two\n";
    }

    Scene scene;
    REQUIRE(scene.b_LoadFromFile(path.string()));
    REQUIRE(scene.GetObjects().size() == 2);
    CHECK(scene.GetObjects()[0].m_Id == 1);
    CHECK(scene.GetObjects()[1].m_Id == 2);

    t_SceneObject& next = scene.AddObject("Three");
    CHECK(next.m_Id == 3);
}

TEST_CASE("Scene: tint clamps and malformed visibility keeps default")
{
    TempDir temp_dir("raywaves_scene_values");
    const std::filesystem::path path = temp_dir.Path() / "Values.scene";
    {
        std::ofstream file(path);
        file << "[object]\n";
        file << "name=Clamped\n";
        file << "tint=300,-5,128,999\n";
        file << "visible=0\n";
        file << "[object]\n";
        file << "name=DefaultVisible\n";
        file << "visible=not_a_number\n";
    }

    Scene scene;
    REQUIRE(scene.b_LoadFromFile(path.string()));
    REQUIRE(scene.GetObjects().size() == 2);

    const t_SceneObject& clamped = scene.GetObjects()[0];
    CHECK(clamped.m_Tint.r == 255);
    CHECK(clamped.m_Tint.g == 0);
    CHECK(clamped.m_Tint.b == 128);
    CHECK(clamped.m_Tint.a == 255);
    CHECK_FALSE(clamped.m_bVisible);

    CHECK(scene.GetObjects()[1].m_bVisible);
}

TEST_CASE("Scene: empty file key stays empty")
{
    CHECK(Scene::ToFileKey("") == "");
}

TEST_CASE("Camera2D: world and screen roundtrip")
{
    Camera2D camera{ { 640.0f, 360.0f }, { 100.0f, 300.0f }, 0.0f, 2.5f };
    const Vector2 world{ 100.0f, 300.0f };

    const Vector2 screen = GetWorldToScreen2D(world, camera);
    const Vector2 roundtrip = GetScreenToWorld2D(screen, camera);

    CHECK(roundtrip.x == doctest::Approx(world.x));
    CHECK(roundtrip.y == doctest::Approx(world.y));
}

TEST_CASE("Camera2DMath: pan moves target opposite the world drag")
{
    Camera2D camera{ { 640.0f, 360.0f }, { 100.0f, 300.0f }, 0.0f, 2.0f };
    Camera2DMath::Pan(camera, { 100.0f, 300.0f }, { 120.0f, 280.0f });
    CHECK(camera.target.x == doctest::Approx(80.0f));
    CHECK(camera.target.y == doctest::Approx(320.0f));
}

TEST_CASE("Camera2DMath: zoom keeps the world point under the cursor")
{
    Camera2D camera{ { 640.0f, 360.0f }, { 100.0f, 300.0f }, 0.0f, 1.5f };
    const Vector2 cursor{ 700.0f, 420.0f };
    const Vector2 before = GetScreenToWorld2D(cursor, camera);

    CHECK(Camera2DMath::b_ZoomAt(camera, cursor, 2.0f));
    CHECK(camera.zoom > 1.5f);

    const Vector2 after = GetScreenToWorld2D(cursor, camera);
    CHECK(after.x == doctest::Approx(before.x));
    CHECK(after.y == doctest::Approx(before.y));
}

TEST_CASE("GameEngine: loads and saves the active map scene")
{
    TempDir temp_dir("raywaves_engine_scene");
    const std::filesystem::path asset_root = temp_dir.Path() / "Assets";
    std::filesystem::create_directories(asset_root);
    AssetResolver::SetProjectAssetPath(asset_root.string());

    const std::filesystem::path scene_path = asset_root / "SceneKey.scene";
    {
        Scene scene;
        t_SceneObject& object = scene.AddObject("Loaded");
        object.m_Position = { 11.0f, 22.0f };
        REQUIRE(scene.b_SaveToFile(scene_path.string()));
    }

    GameEngine engine;
    SceneTestMap map;
    engine.SetMap(&map);

    REQUIRE(engine.GetScene().GetObjects().size() == 1);
    CHECK(engine.GetScene().GetObjects().front().m_Name == "Loaded");
    CHECK(engine.GetScene().GetObjects().front().m_Position.x == doctest::Approx(11.0f));
    CHECK(engine.GetScene().GetObjects().front().m_Position.y == doctest::Approx(22.0f));

    CHECK(map.m_ScenePushes == 1);
    REQUIRE(map.GetSceneObjects().size() == 1);
    CHECK(map.GetSceneObjects().front().m_Name == "Loaded");

    engine.GetScene().AddObject("Marker");
    engine.RefreshSceneObjects();
    CHECK(map.GetSceneObjects().size() == 2);

    engine.GetScene().GetObjects().front().m_Position = { 33.0f, 44.0f };
    engine.GetScene().MarkDirty();
    CHECK(engine.b_SaveActiveScene());

    Scene saved;
    REQUIRE(saved.b_LoadFromFile(scene_path.string()));
    CHECK(saved.GetObjects().front().m_Position.x == doctest::Approx(33.0f));
    CHECK(saved.GetObjects().front().m_Position.y == doctest::Approx(44.0f));

    engine.SetMap(nullptr);
    AssetResolver::SetProjectAssetPath("");
}

TEST_CASE("GameEngine: active scene key follows the attached map")
{
    GameEngine engine;
    SceneMapA map;
    engine.SetMap(&map);
    CHECK(engine.GetActiveSceneKey() == "MapA");

    engine.SetMap(nullptr);
    CHECK(engine.GetActiveSceneKey().empty());
}

TEST_CASE("GameEngine: scene save and load need a project asset path")
{
    AssetResolver::SetProjectAssetPath("");

    GameEngine engine;
    SceneMapA map;
    engine.SetMap(&map);
    engine.GetScene().AddObject("Unsaved");

    CHECK_FALSE(engine.b_SaveActiveScene());
    CHECK_FALSE(engine.b_LoadActiveScene());
    CHECK(engine.GetScene().GetObjects().empty());
}

TEST_CASE("GameEngine: ResetMap clears editor entities before Initialize")
{
    GameEngine engine;
    EditorRegistryEngineMap map;
    engine.SetMap(&map);
    REQUIRE(engine.GetScene().GetObjects().empty());
    REQUIRE(map.GetEditorEntities().size() == 1);
    CHECK(map.GetEditorEntities().front().m_Name == "Player");

    engine.ResetMap();
    REQUIRE(map.GetEditorEntities().size() == 1);
    CHECK(map.GetEditorEntities().front().m_Name == "Player");
}

TEST_CASE("GameEngine: UnloadScene persists dirty scene data")
{
    TempDir temp_dir("raywaves_engine_unload");
    const std::filesystem::path asset_root = temp_dir.Path() / "Assets";
    std::filesystem::create_directories(asset_root);
    AssetResolver::SetProjectAssetPath(asset_root.string());

    GameEngine engine;
    SceneMapA map;
    engine.SetMap(&map);
    engine.GetScene().AddObject("Persisted");
    engine.GetScene().MarkDirty();

    engine.UnloadScene();

    CHECK(engine.GetScene().GetObjects().empty());
    CHECK(std::filesystem::exists(asset_root / "MapA.scene"));

    GameEngine reopened;
    reopened.SetMap(&map);
    REQUIRE(reopened.GetScene().GetObjects().size() == 1);
    CHECK(reopened.GetScene().GetObjects().front().m_Name == "Persisted");

    reopened.SetMap(nullptr);
    AssetResolver::SetProjectAssetPath("");
}

TEST_CASE("GameEngine: switching maps saves and reloads each scene")
{
    TempDir temp_dir("raywaves_engine_switch");
    const std::filesystem::path asset_root = temp_dir.Path() / "Assets";
    std::filesystem::create_directories(asset_root);
    AssetResolver::SetProjectAssetPath(asset_root.string());

    GameEngine engine;
    SceneMapA map_a;
    SceneMapB map_b;

    engine.SetMap(&map_a);
    engine.GetScene().AddObject("AObject");
    engine.GetScene().MarkDirty();

    engine.SetMap(&map_b);
    CHECK(engine.GetActiveSceneKey() == "MapB");
    CHECK(engine.GetScene().GetObjects().empty());
    engine.GetScene().AddObject("BObject");
    engine.GetScene().MarkDirty();

    engine.SetMap(&map_a);
    CHECK(engine.GetActiveSceneKey() == "MapA");
    REQUIRE(engine.GetScene().GetObjects().size() == 1);
    CHECK(engine.GetScene().GetObjects().front().m_Name == "AObject");

    engine.SetMap(&map_b);
    REQUIRE(engine.GetScene().GetObjects().size() == 1);
    CHECK(engine.GetScene().GetObjects().front().m_Name == "BObject");

    CHECK(std::filesystem::exists(asset_root / "MapA.scene"));
    CHECK(std::filesystem::exists(asset_root / "MapB.scene"));

    engine.SetMap(nullptr);
    AssetResolver::SetProjectAssetPath("");
}

TEST_CASE("GameEngine: scene file key sanitizes map names")
{
    TempDir temp_dir("raywaves_engine_key");
    const std::filesystem::path asset_root = temp_dir.Path() / "Assets";
    std::filesystem::create_directories(asset_root);
    AssetResolver::SetProjectAssetPath(asset_root.string());

    GameEngine engine;
    NamedSceneMap map("My Map/Level");
    engine.SetMap(&map);
    CHECK(engine.GetActiveSceneKey() == "My Map/Level");

    engine.GetScene().AddObject("Sanitized");
    engine.GetScene().MarkDirty();
    CHECK(engine.b_SaveActiveScene());
    CHECK(std::filesystem::exists(asset_root / "My_Map_Level.scene"));

    engine.SetMap(nullptr);
    AssetResolver::SetProjectAssetPath("");
}
