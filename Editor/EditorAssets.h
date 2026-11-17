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
} // namespace EditorAssets
