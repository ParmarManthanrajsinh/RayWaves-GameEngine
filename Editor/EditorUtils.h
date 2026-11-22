#pragma once
#include <filesystem>
#include <string_view>

namespace EditorUtils
{
bool OpenInExplorer(const std::filesystem::path& path);
bool OpenInDefaultApp(const std::filesystem::path& path);
// Opens a source file in a text editor and jumps to a 1-based line. Line 0
// opens the file without a line target. Falls back to the OS default handler.
bool OpenFileAtLine(const std::filesystem::path& path, int line);
bool OpenURL(std::string_view url);
bool IsShellSafe(std::string_view s);
} // namespace EditorUtils
