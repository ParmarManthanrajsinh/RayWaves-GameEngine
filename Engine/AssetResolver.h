#pragma once
#include <string>
#include <string_view>

class AssetResolver {
public:
    static void SetProjectAssetPath(std::string_view path);
    static std::string Resolve(std::string_view relativePath);
    static std::string GetProjectAssetPath();

private:
    static std::string s_BasePath;
    // Normalized absolute base, computed once in SetProjectAssetPath so
    // Resolve() skips redundant absolute()/lexically_normal() work per call.
    static std::string s_BaseAbs;
};
