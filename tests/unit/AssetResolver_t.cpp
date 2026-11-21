#include "Engine/AssetResolver.h"
#include "doctest/doctest.h"
#include "support/TempDir.h"
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

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

    // Windows resolves the differently-cased path through its case-insensitive
    // filesystem, so compare identity instead of requiring on-disk casing.
    const std::string resolved = AssetResolver::Resolve("textures/player.png");
    std::error_code error;
    CHECK(fs::equivalent(fs::path(resolved), asset, error));
    CHECK_FALSE(error);
}

TEST_CASE("AssetResolver: backslash input is normalized")
{
    TempDir temp("raywaves_assets_slashes");
    const fs::path asset = temp.Path() / "Textures" / "Player.PNG";
    Touch(asset);
    AssetResolver::SetProjectAssetPath(temp.Path().string());

    const std::string resolved = AssetResolver::Resolve("textures\\player.png");
    std::error_code error;
    CHECK(fs::equivalent(fs::path(resolved), asset, error));
    CHECK_FALSE(error);
}

// NTFS cannot store two names that differ only by case in the same directory,
// so the ambiguity guard is only observable on a case-sensitive filesystem.
#if !defined(_WIN32)
TEST_CASE("AssetResolver: ambiguous case matches fail")
{
    TempDir temp("raywaves_assets_ambiguous");
    Touch(temp.Path() / "Foo.png");
    Touch(temp.Path() / "foo.png");
    AssetResolver::SetProjectAssetPath(temp.Path().string());

    CHECK(AssetResolver::Resolve("FOO.png") == "FOO.png");
}
#endif

TEST_CASE("AssetResolver: missing file keeps an absolute write path")
{
    TempDir temp("raywaves_assets_missing");
    AssetResolver::SetProjectAssetPath(temp.Path().string());
    const std::string resolved = AssetResolver::Resolve("missing/player.png");
    const fs::path expected = (temp.Path() / "missing" / "player.png").lexically_normal();
    CHECK(fs::path(resolved).is_absolute());
    CHECK(fs::path(resolved).lexically_normal() == expected);
}
