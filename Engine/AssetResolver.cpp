#include "AssetResolver.h"
#include <filesystem>
#include <iostream>

std::string AssetResolver::s_BasePath = "";

void AssetResolver::SetProjectAssetPath(std::string_view path) {
    s_BasePath = std::string(path);
}

std::string AssetResolver::Resolve(std::string_view relativePath) {
    std::filesystem::path rel(relativePath);
    
    // If the path is already absolute, just normalize it
    if (rel.is_absolute()) return rel.lexically_normal().string();
    
    // If no project asset path is set, return the relative path as-is (CWD fallback)
    if (s_BasePath.empty()) return std::string(relativePath);
    
    // Normalize path to use proper directory separators
    return (std::filesystem::path(s_BasePath) / rel).lexically_normal().string();
}

std::string AssetResolver::GetProjectAssetPath() {
    return s_BasePath;
}
