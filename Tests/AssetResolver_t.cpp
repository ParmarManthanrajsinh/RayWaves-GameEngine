#include "doctest/doctest.h"
#include "../Engine/AssetResolver.h"

TEST_CASE("AssetResolver: default project path")
{
    CHECK(AssetResolver::GetProjectAssetPath() == "");
}

TEST_CASE("AssetResolver: SetProjectAssetPath and resolve")
{
    AssetResolver::SetProjectAssetPath("C:/MyGame/Assets");
    CHECK(AssetResolver::GetProjectAssetPath() == "C:/MyGame/Assets");

    std::string resolved = AssetResolver::Resolve("textures/player.png");
    CHECK(resolved.find("Assets") != std::string::npos);
    CHECK(resolved.find("textures") != std::string::npos);
    CHECK(resolved.find("player.png") != std::string::npos);
}

TEST_CASE("AssetResolver: empty relative path")
{
    AssetResolver::SetProjectAssetPath("/base/path");
    std::string resolved = AssetResolver::Resolve("");
    CHECK_FALSE(resolved.empty());
}
