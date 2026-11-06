#include "AssetResolver.h"
#include <filesystem>
#include <iostream>

std::string AssetResolver::s_BasePath;
std::string AssetResolver::s_BaseAbs;
std::unordered_map<std::string, std::string, AssetResolver::StringHash, std::equal_to<>> AssetResolver::s_Cache;

void AssetResolver::SetProjectAssetPath(std::string_view path)
{
    s_BasePath = std::string(path);
    s_BaseAbs = s_BasePath.empty() ? std::string() : std::filesystem::absolute(s_BasePath).lexically_normal().string();
    s_Cache.clear();
}

std::string AssetResolver::Resolve(std::string_view relativePath)
{
    if (s_BaseAbs.empty())
        return std::string(relativePath);
    if (relativePath.empty())
        return s_BaseAbs;

    if (auto it = s_Cache.find(relativePath); it != s_Cache.end())
        return it->second;

    std::filesystem::path resolved = (std::filesystem::path(s_BaseAbs) / relativePath).lexically_normal();

    // Root-jail check: resolved path must stay inside the asset root.
    // Check components, not a string prefix, so names like "..foo" are valid.
    const auto rel = resolved.lexically_relative(s_BaseAbs);
    bool b_Escapes = rel.has_root_path();
    for (const auto& part : rel)
    {
        if (part == "..")
        {
            b_Escapes = true;
            break;
        }
    }

    if (b_Escapes)
        return std::string(relativePath);

    std::string resolved_str = resolved.string();
    s_Cache.emplace(std::string(relativePath), resolved_str);
    return resolved_str;
}

std::string AssetResolver::GetProjectAssetPath()
{
    return s_BasePath;
}
