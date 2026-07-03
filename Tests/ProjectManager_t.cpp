#include "doctest/doctest.h"
#include "../Engine/ProjectManager.h"
#include <fstream>
#include <filesystem>

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
