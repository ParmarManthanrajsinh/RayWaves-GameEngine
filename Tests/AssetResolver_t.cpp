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

TEST_CASE("AssetResolver: path jail bypass check")
{
    AssetResolver::SetProjectAssetPath("C:/MyGame/Assets");
    
    // Attempt to access a sibling directory Assets2
    std::string resolved = AssetResolver::Resolve("../Assets2/hacked.png");
    
    // Should reject and return the relative path
    CHECK(resolved == "../Assets2/hacked.png");
}
