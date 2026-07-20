#include "AssetResolver.h"
#include <filesystem>
#include <iostream>

std::string AssetResolver::s_BasePath;

void AssetResolver::SetProjectAssetPath(std::string_view path) {
    s_BasePath = std::string(path);
}

std::string AssetResolver::Resolve(std::string_view relativePath) {
    if (s_BasePath.empty())
        return std::string(relativePath);

    std::filesystem::path base_abs = std::filesystem::absolute(s_BasePath).lexically_normal();
    std::filesystem::path resolved = std::filesystem::absolute(std::filesystem::path(s_BasePath) / relativePath).lexically_normal();
    std::string base_str = base_abs.string();
    std::string resolved_str = resolved.string();

    // Root-jail check: resolved path must be within asset root
    auto rel = resolved.lexically_relative(base_abs);
    if (rel.string().find("..") == 0)
        return std::string(relativePath);

    return resolved_str;
}

std::string AssetResolver::GetProjectAssetPath() {
    return s_BasePath;
}
