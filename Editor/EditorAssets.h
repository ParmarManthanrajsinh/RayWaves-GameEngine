#pragma once
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace EditorAssets
{
bool b_IsImage(std::string_view path);
bool b_IsAudio(std::string_view path);
bool b_IsCode(std::string_view path);
bool b_IsScene(std::string_view path);

std::vector<std::filesystem::path> ListFiles(const std::filesystem::path& directory, bool recursive);

// Relative path inside the project's Assets folder, or empty when the file
// lives outside it. Scene code relies on relative paths so projects stay
// portable.
std::string ToRelativeAssetPath(const std::filesystem::path& absolute_path);

// Copies an external file into Assets (flat), or returns its existing relative
// path when it is already inside. Name collisions get a numeric suffix instead
// of overwriting user data.
bool b_ImportFile(const std::filesystem::path& source, std::string& out_relative_path);

// Renames an asset inside the project's Assets folder. The new name must be a
// bare filename in the same directory: no separators, no "..", no characters
// Windows rejects. The extension may change; callers own reference updates.
// Returns the absolute path of the renamed file, or false with an error.
bool b_RenameAsset(const std::filesystem::path& asset, std::string_view new_filename,
    std::filesystem::path& out_new_path, std::string& out_error);
} // namespace EditorAssets
