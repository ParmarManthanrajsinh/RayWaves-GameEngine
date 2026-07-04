#pragma once
#include <filesystem>
#include <string_view>

namespace EditorUtils
{
    bool OpenInExplorer(const std::filesystem::path& path);
    bool OpenURL(std::string_view url);
    bool IsShellSafe(std::string_view s);
}
