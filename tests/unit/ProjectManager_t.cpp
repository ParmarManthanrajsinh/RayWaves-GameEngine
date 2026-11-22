#include "Engine/ProjectManager.h"
#include "doctest/doctest.h"
#include "support/TempDir.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

namespace
{
// Points recent.ini at a temp file for the test's lifetime, so the suite
// never edits the real %APPDATA%\RayWaves\recent.ini.
class RecentPathOverride
{
public:
    explicit RecentPathOverride(const std::filesystem::path& path)
        : m_Previous(ProjectManager::SetRecentPathForTesting(path.string()))
    {
    }

    ~RecentPathOverride() { ProjectManager::SetRecentPathForTesting(m_Previous); }

    RecentPathOverride(const RecentPathOverride&) = delete;
    RecentPathOverride& operator=(const RecentPathOverride&) = delete;

private:
    std::string m_Previous;
};

// Creates a manifest plus the source/asset folders validation requires.
void CreateProjectFixture(const std::filesystem::path& root, std::string_view name = "FixtureProject",
    std::string_view source_dir = "GameLogic", std::string_view asset_dir = "Assets")
{
    std::filesystem::create_directories(root / std::string(source_dir));
    std::filesystem::create_directories(root / std::string(asset_dir));

    std::ofstream manifest(root / "project.raywaves");
    manifest << "[project]\n";
    manifest << "name=" << name << "\n";
    manifest << "sourceDir=" << source_dir << "\n";
    manifest << "assetDir=" << asset_dir << "\n";
}
} // namespace

TEST_CASE("ProjectManager: SanitizeCMakeProjectName")
{
    CHECK(ProjectManager::SanitizeCMakeProjectName("Hello World!") == "Hello_World_");
    CHECK(ProjectManager::SanitizeCMakeProjectName("My-Game_V2") == "My_Game_V2");
    CHECK(ProjectManager::SanitizeCMakeProjectName("") == "RayWavesProject");
}

TEST_CASE("ProjectManager: b_HasOpenProject initially false")
{
    CHECK_FALSE(ProjectManager::b_HasOpenProject());
}

TEST_CASE("ProjectManager: AddRecent/RemoveRecent round-trip")
{
    TempDir dir;
    RecentPathOverride redirect(dir.Path() / "recent.ini");

    // Regression: the recent-projects delete button did nothing because
    // GetRecent() dropped entries that no longer exist on disk, so
    // RemoveRecent() could never match them. This path does not exist.
    const std::string fake_path = std::filesystem::path("C:/RayWaves_test_fake/proj").lexically_normal().string();
    ProjectManager::AddRecent(fake_path);

    auto recent = ProjectManager::GetRecent();
    CHECK(std::find(recent.begin(), recent.end(), fake_path) != recent.end());

    ProjectManager::RemoveRecent(fake_path);

    recent = ProjectManager::GetRecent();
    CHECK(std::find(recent.begin(), recent.end(), fake_path) == recent.end());
}

TEST_CASE("ProjectManager: b_CreateProject copies templates")
{
    TempDir dir;

    auto available = ProjectManager::GetAvailableTemplates();
    CHECK(std::find(available.begin(), available.end(), "Empty2D") != available.end());
    CHECK(std::find(available.begin(), available.end(), "Simple2D") != available.end());
    CHECK(std::find(available.begin(), available.end(), "Platformer2D") != available.end());
    CHECK(std::find(available.begin(), available.end(), "ThirdPerson3D") != available.end());
    CHECK(std::find(available.begin(), available.end(), "SlimeQuest") != available.end());
    CHECK(std::find(available.begin(), available.end(), "Empty") == available.end());
    CHECK(std::find(available.begin(), available.end(), "FirstPerson3D") == available.end());

    // Sorted so the New Project wizard has a stable default order.
    CHECK(std::is_sorted(available.begin(), available.end()));

    for (const char* name : { "Empty2D", "Simple2D", "ThirdPerson3D" })
    {
        std::filesystem::path target = dir.Path() / name;
        REQUIRE(ProjectManager::b_CreateProject(target.string(), name));

        CHECK(std::filesystem::exists(target / "project.raywaves"));
        CHECK(std::filesystem::is_directory(target / "GameLogic"));
        CHECK(std::filesystem::is_directory(target / "Assets"));
        CHECK(std::filesystem::exists(target / "AGENTS.md"));
        CHECK(std::filesystem::exists(target / ".agents" / "skills" / "raywaves-game" / "SKILL.md"));
        CHECK(std::filesystem::exists(target / ".agents" / "skills" / "raywaves-game" / "reference.md"));
        CHECK(std::filesystem::exists(target / ".raywaves" / "tooling.ini"));
        CHECK(std::filesystem::exists(target / ".raywaves" / "bin" / "rwcli.cmd"));
        CHECK(std::filesystem::exists(target / ".clangd"));
        CHECK(std::filesystem::exists(target / ".vscode" / "c_cpp_properties.json"));

        t_Project project;
        REQUIRE(project.m_bLoadFromFile((target / "project.raywaves").string()));
        CHECK(project.m_Name == name);
    }

    CHECK(std::filesystem::exists(dir.Path() / "Empty2D" / "GameLogic" / "Game.cpp"));
    CHECK(std::filesystem::exists(dir.Path() / "Simple2D" / "GameLogic" / "Game.cpp"));
    CHECK(std::filesystem::exists(dir.Path() / "ThirdPerson3D" / "GameLogic" / "RootManager.cpp"));
}

TEST_CASE("ProjectManager: b_CreateProject creates asset and source folders")
{
    TempDir dir;
    const std::filesystem::path target = dir.Path() / "EmptyProject";

    REQUIRE(ProjectManager::b_CreateProject(target.string(), "Simple2D"));

    CHECK(std::filesystem::exists(target / "project.raywaves"));
    CHECK(std::filesystem::is_directory(target / "Assets"));
    CHECK(std::filesystem::is_directory(target / "GameLogic"));
}

TEST_CASE("ProjectManager: generated CMake honors sourceDir and defaults to Release")
{
    TempDir dir;
    RecentPathOverride recent_override(dir.Path() / "recent.ini");

    const std::filesystem::path project_root = dir.Path() / "CustomSourceProject";
    std::filesystem::create_directories(project_root / "Source");
    std::filesystem::create_directories(project_root / "Assets");
    {
        std::ofstream manifest(project_root / "project.raywaves");
        manifest << "[project]\n";
        manifest << "name=CustomSourceProject\n";
        manifest << "assetDir=Assets\n";
        manifest << "sourceDir=Source\n";
    }

    REQUIRE(ProjectManager::b_OpenProject(project_root.string()));

    std::ifstream generated(project_root / ".raywaves" / "CMakeLists.txt");
    REQUIRE(generated.is_open());
    const std::string content((std::istreambuf_iterator<char>(generated)), std::istreambuf_iterator<char>());

    // The recipe must follow the manifest, not hardcode GameLogic.
    CHECK(content.find("${CMAKE_CURRENT_LIST_DIR}/../Source") != std::string::npos);
    // Ninja is single-config, so Release must be the template default.
    CHECK(content.find("set(CMAKE_BUILD_TYPE Release CACHE STRING") != std::string::npos);

    ProjectManager::CloseProject();
}

TEST_CASE("ProjectManager: opening a project creates missing contract folders")
{
    TempDir dir;
    RecentPathOverride recent_override(dir.Path() / "recent.ini");

    const std::filesystem::path project_root = dir.Path() / "LegacyProject";
    std::filesystem::create_directories(project_root);
    {
        std::ofstream manifest(project_root / "project.raywaves");
        manifest << "[project]\n";
        manifest << "name=LegacyProject\n";
        manifest << "assetDir=Assets\n";
        manifest << "sourceDir=GameLogic\n";
    }

    REQUIRE(ProjectManager::b_OpenProject(project_root.string()));
    CHECK(ProjectManager::b_HasOpenProject());
    CHECK(std::filesystem::is_directory(project_root / "Assets"));
    CHECK(std::filesystem::is_directory(project_root / "GameLogic"));
    CHECK(std::filesystem::exists(project_root / ".raywaves"));

    ProjectManager::CloseProject();
}

TEST_CASE("ProjectManager: b_ValidateProject resolves a project folder")
{
    TempDir dir;
    RecentPathOverride recent_override(dir.Path() / "recent.ini");

    const std::filesystem::path project_root = dir.Path() / "ValidProject";
    CreateProjectFixture(project_root, "ValidProject");

    t_Project project;
    std::string error;
    REQUIRE(ProjectManager::b_ValidateProject(project_root.string(), project, error));
    CHECK(error.empty());
    CHECK(project.m_Name == "ValidProject");
    CHECK(std::filesystem::equivalent(project.m_RootPath, project_root));
    CHECK(std::filesystem::equivalent(project.m_SourcePath, project_root / "GameLogic"));
    CHECK(std::filesystem::equivalent(project.m_AssetPath, project_root / "Assets"));

    // Validation is read-only: it must not open the project or touch recents.
    CHECK_FALSE(ProjectManager::b_HasOpenProject());
    CHECK(ProjectManager::GetRecent().empty());
    CHECK_FALSE(std::filesystem::exists(project_root / ".raywaves"));
}

TEST_CASE("ProjectManager: b_ValidateProject accepts manifest and inner paths")
{
    TempDir dir;

    const std::filesystem::path project_root = dir.Path() / "PathForms";
    CreateProjectFixture(project_root, "PathForms");
    std::filesystem::create_directories(project_root / "Assets" / "Maps");
    {
        std::ofstream map_file(project_root / "Assets" / "Maps" / "level.map");
        map_file << "map\n";
    }

    const std::vector<std::filesystem::path> inputs = {
        project_root / "project.raywaves",
        project_root / "Assets" / "Maps" / "level.map",
        project_root / "GameLogic",
    };

    for (const std::filesystem::path& input : inputs)
    {
        INFO(input.string());
        t_Project project;
        std::string error;
        REQUIRE(ProjectManager::b_ValidateProject(input.string(), project, error));
        CHECK(error.empty());
        CHECK(project.m_Name == "PathForms");
        CHECK(std::filesystem::equivalent(project.m_RootPath, project_root));
    }
}

TEST_CASE("ProjectManager: b_ValidateProject reports missing pieces")
{
    TempDir dir;

    SUBCASE("missing manifest")
    {
        const std::filesystem::path project_root = dir.Path() / "NoManifest";
        std::filesystem::create_directories(project_root / "Assets");

        t_Project project;
        std::string error;
        CHECK_FALSE(ProjectManager::b_ValidateProject(project_root.string(), project, error));
        CHECK(error.find("manifest") != std::string::npos);
    }

    SUBCASE("missing contract folders are repairable")
    {
        const std::filesystem::path project_root = dir.Path() / "NoFolders";
        std::filesystem::create_directories(project_root);
        {
            std::ofstream manifest(project_root / "project.raywaves");
            manifest << "[project]\n";
            manifest << "name=NoFolders\n";
            manifest << "sourceDir=GameLogic\n";
            manifest << "assetDir=Assets\n";
        }

        t_Project project;
        std::string error;
        CHECK(ProjectManager::b_ValidateProject(project_root.string(), project, error));
        CHECK(error.empty());
        CHECK_FALSE(project.m_bIsValid());
    }
}

TEST_CASE("ProjectManager: failed validation leaves the open project untouched")
{
    TempDir dir;
    RecentPathOverride recent_override(dir.Path() / "recent.ini");

    const std::filesystem::path open_root = dir.Path() / "OpenProject";
    CreateProjectFixture(open_root, "OpenProject");
    REQUIRE(ProjectManager::b_OpenProject(open_root.string(), false));

    const t_Project before = ProjectManager::GetCurrentCopy();
    REQUIRE(ProjectManager::b_HasOpenProject());

    const std::filesystem::path broken_root = dir.Path() / "BrokenProject";
    std::filesystem::create_directories(broken_root);

    t_Project validated;
    std::string error;
    CHECK_FALSE(ProjectManager::b_ValidateProject(broken_root.string(), validated, error));
    CHECK_FALSE(error.empty());

    // Structured open must fail the same way and leave the current project,
    // open flag, and recent list alone.
    error.clear();
    CHECK_FALSE(ProjectManager::b_OpenProject(broken_root.string(), true, error));
    CHECK_FALSE(error.empty());

    const t_Project after = ProjectManager::GetCurrentCopy();
    CHECK(ProjectManager::b_HasOpenProject());
    CHECK(after.m_Name == before.m_Name);
    CHECK(after.m_RootPath == before.m_RootPath);
    CHECK(ProjectManager::GetRecent().empty());

    ProjectManager::CloseProject();
}

TEST_CASE("ProjectManager: structured open records the resolved project root")
{
    TempDir dir;
    RecentPathOverride recent_override(dir.Path() / "recent.ini");

    const std::filesystem::path project_root = dir.Path() / "RecentRoot";
    CreateProjectFixture(project_root, "RecentRoot");

    std::string error;
    REQUIRE(ProjectManager::b_OpenProject((project_root / "project.raywaves").string(), true, error));
    CHECK(error.empty());
    CHECK(ProjectManager::b_HasOpenProject());
    CHECK(ProjectManager::GetCurrentCopy().m_Name == "RecentRoot");

    const std::vector<std::string> recent = ProjectManager::GetRecent();
    REQUIRE(recent.size() == 1);
    CHECK(std::filesystem::equivalent(recent.front(), project_root));

    ProjectManager::CloseProject();
}
