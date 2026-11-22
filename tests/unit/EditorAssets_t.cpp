#include "Editor/EditorAssets.h"
#include "Engine/ProjectManager.h"
#include "doctest/doctest.h"
#include "support/TempDir.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace
{
// Opens a temp project for the test and always closes it before the next test
// case runs, so global ProjectManager state does not leak between suites.
class ProjectFixture
{
public:
    explicit ProjectFixture(const std::filesystem::path& project_root)
    {
        m_PreviousRecent = ProjectManager::SetRecentPathForTesting((project_root / "recent.ini").string());
        std::filesystem::create_directories(project_root);
        std::ofstream manifest(project_root / "project.raywaves");
        manifest << "[project]\n";
        manifest << "name=AssetsTest\n";
        manifest << "assetDir=Assets\n";
        manifest << "sourceDir=GameLogic\n";

        ProjectManager::b_OpenProject(project_root.string());
    }

    ~ProjectFixture()
    {
        ProjectManager::CloseProject();
        ProjectManager::SetRecentPathForTesting(m_PreviousRecent);
    }

    ProjectFixture(const ProjectFixture&) = delete;
    ProjectFixture& operator=(const ProjectFixture&) = delete;

private:
    std::string m_PreviousRecent;
};

void TouchFile(const std::filesystem::path& path)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path);
    file << "test";
}
} // namespace

TEST_CASE("EditorAssets: extension classification is case-insensitive")
{
    CHECK(EditorAssets::b_IsImage("sprite.PNG"));
    CHECK(EditorAssets::b_IsImage("photo.jpeg"));
    CHECK(EditorAssets::b_IsImage("texture.tga"));
    CHECK_FALSE(EditorAssets::b_IsImage("sprite.txt"));
    CHECK_FALSE(EditorAssets::b_IsImage("no_extension"));

    CHECK(EditorAssets::b_IsAudio("theme.OGG"));
    CHECK(EditorAssets::b_IsAudio("step.wav"));
    CHECK_FALSE(EditorAssets::b_IsAudio("theme.png"));

    CHECK(EditorAssets::b_IsCode("player.CPP"));
    CHECK(EditorAssets::b_IsCode("header.hpp"));
    CHECK_FALSE(EditorAssets::b_IsCode("notes.md"));

    CHECK(EditorAssets::b_IsScene("Level.SCENE"));
    CHECK_FALSE(EditorAssets::b_IsScene("Level.scene.bak"));
}

TEST_CASE("EditorAssets: ListFiles skips hidden files and sorts")
{
    TempDir temp_dir("raywaves_assets_list");
    TouchFile(temp_dir.Path() / "alpha.txt");
    TouchFile(temp_dir.Path() / "Beta.cpp");
    TouchFile(temp_dir.Path() / ".hidden.txt");
    TouchFile(temp_dir.Path() / "sub" / "nested.png");

    const std::vector<std::filesystem::path> flat = EditorAssets::ListFiles(temp_dir.Path(), false);
    REQUIRE(flat.size() == 2);
    CHECK(flat[0].filename().string() == "Beta.cpp");
    CHECK(flat[1].filename().string() == "alpha.txt");

    const std::vector<std::filesystem::path> recursive = EditorAssets::ListFiles(temp_dir.Path(), true);
    REQUIRE(recursive.size() == 3);
    CHECK(std::ranges::any_of(
        recursive, [](const std::filesystem::path& path) { return path.filename().string() == "nested.png"; }));

    CHECK(EditorAssets::ListFiles(temp_dir.Path() / "missing", false).empty());
}

TEST_CASE("EditorAssets: relative paths stay inside the Assets root")
{
    TempDir temp_dir("raywaves_assets_relative");
    ProjectFixture fixture(temp_dir.Path() / "Project");
    const std::filesystem::path asset_root = ProjectManager::GetCurrentCopy().m_AssetPath;

    TouchFile(asset_root / "sub" / "icon.png");
    TouchFile(temp_dir.Path() / "Project" / "outside.png");

    CHECK(EditorAssets::ToRelativeAssetPath(asset_root / "sub" / "icon.png") == "sub/icon.png");
    CHECK(EditorAssets::ToRelativeAssetPath(temp_dir.Path() / "Project" / "outside.png").empty());
}

TEST_CASE("EditorAssets: import copies files and avoids collisions")
{
    TempDir temp_dir("raywaves_assets_import");
    ProjectFixture fixture(temp_dir.Path() / "Project");
    const std::filesystem::path asset_root = ProjectManager::GetCurrentCopy().m_AssetPath;

    const std::filesystem::path external = temp_dir.Path() / "outside.png";
    TouchFile(external);

    std::string relative;
    REQUIRE(EditorAssets::b_ImportFile(external, relative));
    CHECK(relative == "outside.png");
    CHECK(std::filesystem::exists(asset_root / "outside.png"));

    std::string collision;
    REQUIRE(EditorAssets::b_ImportFile(external, collision));
    CHECK(collision == "outside_1.png");
    CHECK(std::filesystem::exists(asset_root / "outside_1.png"));

    TouchFile(asset_root / "sub" / "inside.png");
    std::string existing;
    REQUIRE(EditorAssets::b_ImportFile(asset_root / "sub" / "inside.png", existing));
    CHECK(existing == "sub/inside.png");

    std::string missing;
    CHECK_FALSE(EditorAssets::b_ImportFile(temp_dir.Path() / "missing.png", missing));
}

TEST_CASE("EditorAssets: rename moves the file and returns its new absolute path")
{
    TempDir temp_dir("raywaves_assets_rename");
    ProjectFixture fixture(temp_dir.Path() / "Project");
    const std::filesystem::path asset_root = ProjectManager::GetCurrentCopy().m_AssetPath;

    TouchFile(asset_root / "sub" / "old_sprite.png");

    std::filesystem::path new_path;
    std::string error;
    REQUIRE(EditorAssets::b_RenameAsset(asset_root / "sub" / "old_sprite.png", "new_sprite.png", new_path, error));
    CHECK(error.empty());
    CHECK(new_path.is_absolute());
    CHECK(new_path.filename().string() == "new_sprite.png");
    CHECK(std::filesystem::equivalent(new_path.parent_path(), asset_root / "sub"));
    CHECK(std::filesystem::exists(new_path));
    CHECK_FALSE(std::filesystem::exists(asset_root / "sub" / "old_sprite.png"));
    CHECK(EditorAssets::ToRelativeAssetPath(new_path) == "sub/new_sprite.png");

    // Extensions may change; callers are responsible for reference updates.
    REQUIRE(EditorAssets::b_RenameAsset(new_path, "renamed.txt", new_path, error));
    CHECK(error.empty());
    CHECK(new_path.extension().string() == ".txt");
    CHECK(std::filesystem::exists(asset_root / "sub" / "renamed.txt"));
    CHECK_FALSE(std::filesystem::exists(asset_root / "sub" / "new_sprite.png"));
}

TEST_CASE("EditorAssets: rename rejects collisions, missing sources, and unsafe names")
{
    TempDir temp_dir("raywaves_assets_rename_invalid");
    ProjectFixture fixture(temp_dir.Path() / "Project");
    const std::filesystem::path asset_root = ProjectManager::GetCurrentCopy().m_AssetPath;

    TouchFile(asset_root / "source.png");
    TouchFile(asset_root / "taken.png");

    std::filesystem::path new_path;
    std::string error;

    CHECK_FALSE(EditorAssets::b_RenameAsset(asset_root / "source.png", "taken.png", new_path, error));
    CHECK_FALSE(error.empty());
    CHECK(new_path.empty());
    CHECK(std::filesystem::exists(asset_root / "source.png"));
    CHECK(std::filesystem::exists(asset_root / "taken.png"));

    error.clear();
    CHECK_FALSE(EditorAssets::b_RenameAsset(asset_root / "missing.png", "renamed.png", new_path, error));
    CHECK_FALSE(error.empty());

    const std::vector<std::string> unsafe_names = { "", "sub/child.png", "sub\\child.png", ".", "..", "a..b.png",
        "bad<name>.png", "bad>name.png", "bad:name.png", "bad\"name.png", "bad|name.png", "bad?name.png",
        "bad*name.png", "trailing.", "trailing " };
    for (const std::string& name : unsafe_names)
    {
        error.clear();
        CHECK_FALSE(EditorAssets::b_RenameAsset(asset_root / "source.png", name, new_path, error));
        CHECK_FALSE(error.empty());
        CHECK(std::filesystem::exists(asset_root / "source.png"));
    }

    TouchFile(temp_dir.Path() / "outside.png");
    error.clear();
    CHECK_FALSE(EditorAssets::b_RenameAsset(temp_dir.Path() / "outside.png", "renamed.png", new_path, error));
    CHECK_FALSE(error.empty());
    CHECK(std::filesystem::exists(temp_dir.Path() / "outside.png"));

    std::filesystem::create_directories(asset_root / "folder");
    error.clear();
    CHECK_FALSE(EditorAssets::b_RenameAsset(asset_root / "folder", "renamed_folder", new_path, error));
    CHECK_FALSE(error.empty());
    CHECK(std::filesystem::exists(asset_root / "folder"));
}

TEST_CASE("EditorAssets: renaming to the current name is a no-op")
{
    TempDir temp_dir("raywaves_assets_rename_same");
    ProjectFixture fixture(temp_dir.Path() / "Project");
    const std::filesystem::path asset_root = ProjectManager::GetCurrentCopy().m_AssetPath;
    const std::filesystem::path file = asset_root / "same.png";
    TouchFile(file);

    std::filesystem::path new_path;
    std::string error;
    REQUIRE(EditorAssets::b_RenameAsset(file, "same.png", new_path, error));
    CHECK(error.empty());
    CHECK(new_path.filename().string() == "same.png");
    CHECK(std::filesystem::exists(file));
}

TEST_CASE("EditorAssets: relative paths and imports require an open project")
{
    ProjectManager::CloseProject();

    CHECK(EditorAssets::ToRelativeAssetPath("C:/some/file.png").empty());

    std::string relative = "unchanged";
    CHECK_FALSE(EditorAssets::b_ImportFile("C:/some/file.png", relative));
    CHECK(relative == "unchanged");

    std::filesystem::path new_path;
    std::string rename_error;
    CHECK_FALSE(EditorAssets::b_RenameAsset("C:/some/file.png", "renamed.png", new_path, rename_error));
    CHECK_FALSE(rename_error.empty());
}
