#include "AssetResolver.h"

std::string AssetResolver::s_BasePath = "";

void AssetResolver::SetProjectAssetPath(std::string_view path) {
    s_BasePath = std::string(path);
}

std::string AssetResolver::Resolve(std::string_view relativePath) {
    std::filesystem::path base(s_BasePath);
    std::filesystem::path rel(relativePath);
    
    // Normalize path to use proper directory separators
    return (base / rel).lexically_normal().string();
}

std::string AssetResolver::GetProjectAssetPath() {
    return s_BasePath;
}
