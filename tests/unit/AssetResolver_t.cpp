#include "Engine/AssetResolver.h"
#include "doctest/doctest.h"
#include "support/TempDir.h"
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

namespace
{
void Touch(const fs::path& path)
{
    fs::create_directories(path.parent_path());
    std::ofstream(path) << "x";
}
} // namespace

TEST_CASE("AssetResolver: default project path")
{
    AssetResolver::SetProjectAssetPath("");
    CHECK(AssetResolver::GetProjectAssetPath() == "");
}

TEST_CASE("AssetResolver: resolves an existing relative path")
{
    TempDir temp("raywaves_assets");
    const fs::path asset = temp.Path() / "Textures" / "player.png";
    Touch(asset);
    AssetResolver::SetProjectAssetPath(temp.Path().string());

    CHECK(AssetResolver::GetProjectAssetPath() == temp.Path().string());
    const std::string resolved = AssetResolver::Resolve("Textures/player.png");
    CHECK(fs::path(resolved) == fs::path(asset).lexically_normal());
}

TEST_CASE("AssetResolver: empty relative path returns the asset root")
{
    TempDir temp("raywaves_assets_root");
    AssetResolver::SetProjectAssetPath(temp.Path().string());
    CHECK(AssetResolver::Resolve("") == temp.Path().string());
}

TEST_CASE("AssetResolver: path jail bypass check")
{
    TempDir temp("raywaves_assets_jail");
    fs::create_directories(temp.Path() / "Assets");
    AssetResolver::SetProjectAssetPath((temp.Path() / "Assets").string());

    CHECK(AssetResolver::Resolve("../Assets2/hacked.png") == "../Assets2/hacked.png");
}

TEST_CASE("AssetResolver: case-insensitive fallback resolves Windows-authored paths")
{
    TempDir temp("raywaves_assets_case");
    const fs::path asset = temp.Path() / "Textures" / "Player.PNG";
    Touch(asset);
    AssetResolver::SetProjectAssetPath(temp.Path().string());

    const std::string resolved = AssetResolver::Resolve("textures/player.png");
    CHECK(fs::path(resolved) == fs::path(asset).lexically_normal());
}

TEST_CASE("AssetResolver: backslash input is normalized")
{
    TempDir temp("raywaves_assets_slashes");
    const fs::path asset = temp.Path() / "Textures" / "Player.PNG";
    Touch(asset);
    AssetResolver::SetProjectAssetPath(temp.Path().string());

    const std::string resolved = AssetResolver::Resolve("textures\\player.png");
    CHECK(fs::path(resolved) == fs::path(asset).lexically_normal());
}

TEST_CASE("AssetResolver: ambiguous case matches fail")
{
    TempDir temp("raywaves_assets_ambiguous");
    Touch(temp.Path() / "Foo.png");
    Touch(temp.Path() / "foo.png");
    AssetResolver::SetProjectAssetPath(temp.Path().string());

    CHECK(AssetResolver::Resolve("FOO.png") == "FOO.png");
}

TEST_CASE("AssetResolver: missing file keeps an absolute write path")
{
    TempDir temp("raywaves_assets_missing");
    AssetResolver::SetProjectAssetPath(temp.Path().string());
    const std::string resolved = AssetResolver::Resolve("missing/player.png");
    CHECK(fs::path(resolved).is_absolute());
    CHECK(resolved.ends_with("missing/player.png"));
}
