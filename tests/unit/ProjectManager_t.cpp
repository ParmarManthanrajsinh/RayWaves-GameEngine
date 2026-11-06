#include "doctest/doctest.h"
#include "Engine/ProjectManager.h"
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

        ~RecentPathOverride()
        {
            ProjectManager::SetRecentPathForTesting(m_Previous);
        }

        RecentPathOverride(const RecentPathOverride&) = delete;
        RecentPathOverride& operator=(const RecentPathOverride&) = delete;

    private:
        std::string m_Previous;
    };
}

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
