#include "Editor/Autosave.h"
#include "Engine/Scene.h"
#include "doctest/doctest.h"
#include "support/TempDir.h"
#include <filesystem>
#include <string>

TEST_CASE("Autosave: missing snapshot is not an error")
{
    TempDir dir("raywaves_autosave_missing");
    Autosave::FSnapshot snapshot;
    snapshot.m_MapId = "stale";

    CHECK_FALSE(Autosave::b_Query(dir.Path(), snapshot));
    CHECK(snapshot.m_MapId.empty());
    CHECK(snapshot.m_ProjectRoot.empty());
    CHECK(snapshot.m_UnixSeconds == 0);
}

TEST_CASE("Autosave: save and query round-trip")
{
    TempDir dir("raywaves_autosave_roundtrip");
    const std::filesystem::path project_root = dir.Path();

    Scene scene;
    t_SceneObject& player = scene.AddObject("Player");
    player.m_Position = { 12.5f, 34.0f };
    REQUIRE(scene.b_IsDirty());

    std::string error;
    REQUIRE(Autosave::b_Save(project_root, "level1", scene, error));
    CHECK(error.empty());
    // Snapshotting must not make the author's unsaved work look saved.
    CHECK(scene.b_IsDirty());

    Autosave::FSnapshot snapshot;
    REQUIRE(Autosave::b_Query(project_root, snapshot));
    CHECK(snapshot.m_MapId == "level1");
    CHECK(snapshot.m_ProjectRoot == project_root.string());
    CHECK(snapshot.m_UnixSeconds > 0);

    const std::filesystem::path snapshot_dir = Autosave::GetSnapshotDirectory(project_root);
    CHECK(std::filesystem::is_regular_file(snapshot_dir / "autosave.scene"));
    CHECK(std::filesystem::is_regular_file(snapshot_dir / "autosave.ini"));
}

TEST_CASE("Autosave: load restores the saved scene")
{
    TempDir dir("raywaves_autosave_load");
    const std::filesystem::path project_root = dir.Path();

    Scene scene;
    t_SceneObject& object = scene.AddObject("Crate");
    object.m_Type = "prop";
    object.m_Position = { -4.0f, 8.0f };

    std::string error;
    REQUIRE(Autosave::b_Save(project_root, "level1", scene, error));

    Scene restored;
    Autosave::FSnapshot snapshot;
    REQUIRE(Autosave::b_Load(project_root, restored, snapshot, error));
    CHECK(error.empty());
    CHECK(snapshot.m_MapId == "level1");

    REQUIRE(restored.GetObjects().size() == 1);
    CHECK(restored.GetObjects()[0].m_Name == "Crate");
    CHECK(restored.GetObjects()[0].m_Type == "prop");
    CHECK(restored.GetObjects()[0].m_Position.x == doctest::Approx(-4.0f));
    CHECK(restored.GetObjects()[0].m_Position.y == doctest::Approx(8.0f));
}

TEST_CASE("Autosave: second save overwrites and clear removes the snapshot")
{
    TempDir dir("raywaves_autosave_clear");
    const std::filesystem::path project_root = dir.Path();

    Scene scene;
    scene.AddObject("First");

    std::string error;
    REQUIRE(Autosave::b_Save(project_root, "level1", scene, error));

    scene.AddObject("Second");
    REQUIRE(Autosave::b_Save(project_root, "level2", scene, error));

    Autosave::FSnapshot snapshot;
    REQUIRE(Autosave::b_Query(project_root, snapshot));
    CHECK(snapshot.m_MapId == "level2");

    Scene restored;
    REQUIRE(Autosave::b_Load(project_root, restored, snapshot, error));
    CHECK(restored.GetObjects().size() == 2);
    CHECK(restored.GetObjects()[1].m_Name == "Second");

    CHECK(Autosave::b_Clear(project_root));
    CHECK_FALSE(Autosave::b_Query(project_root, snapshot));
    CHECK(snapshot.m_MapId.empty());
    CHECK_FALSE(std::filesystem::exists(Autosave::GetSnapshotDirectory(project_root)));

    // Clearing twice is a no-op, not a failure.
    CHECK(Autosave::b_Clear(project_root));
}
