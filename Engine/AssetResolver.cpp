#include "AssetResolver.h"
#include <filesystem>
#include <iostream>

std::string AssetResolver::s_BasePath = "";

void AssetResolver::SetProjectAssetPath(std::string_view path) {
    s_BasePath = std::string(path);
}

std::string AssetResolver::Resolve(std::string_view relativePath) {
    // Quick absolute check without constructing fs::path
    if (!relativePath.empty() && (relativePath[0] == '/' || relativePath[0] == '\\'
        || (relativePath.size() > 1 && relativePath[1] == ':')))
    {
        return std::filesystem::path(relativePath).lexically_normal().string();
    }

    if (s_BasePath.empty())
    {
        return std::string(relativePath);
    }

    return (std::filesystem::path(s_BasePath) / relativePath).lexically_normal().string();
}

std::string AssetResolver::GetProjectAssetPath() {
    return s_BasePath;
}
