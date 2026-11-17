#include "Engine/ProjectManager.h"
#include "doctest/doctest.h"
#include "support/TempDir.h"
#include <algorithm>
#include <filesystem>
#include <fstream>

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
    CHECK(std::find(available.begin(), available.end(), "Simple2D") != available.end());
    CHECK(std::find(available.begin(), available.end(), "Platformer2D") != available.end());
    CHECK(std::find(available.begin(), available.end(), "ThirdPerson3D") != available.end());
    CHECK(std::find(available.begin(), available.end(), "SlimeQuest") != available.end());
    CHECK(std::find(available.begin(), available.end(), "Empty") == available.end());
    CHECK(std::find(available.begin(), available.end(), "FirstPerson3D") == available.end());

    // Sorted so the New Project wizard has a stable default order.
    CHECK(std::is_sorted(available.begin(), available.end()));

    for (const char* name : { "Simple2D", "ThirdPerson3D" })
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

    // GenerateCMakeLists may fail in a bare test tree; opening the project must
    // still repair the folders older projects are missing.
    ProjectManager::b_OpenProject(project_root.string());

    CHECK(ProjectManager::b_HasOpenProject());
    CHECK(std::filesystem::is_directory(project_root / "Assets"));
    CHECK(std::filesystem::is_directory(project_root / "GameLogic"));

    ProjectManager::CloseProject();
}
