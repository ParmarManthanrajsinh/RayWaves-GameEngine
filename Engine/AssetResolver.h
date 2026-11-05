#pragma once
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>

class AssetResolver {
public:
    static void SetProjectAssetPath(std::string_view path);
    static std::string Resolve(std::string_view relativePath);
    static std::string GetProjectAssetPath();

private:
    struct StringHash {
        using is_transparent = void;
        size_t operator()(std::string_view value) const { return std::hash<std::string_view>{}(value); }
        size_t operator()(const std::string& value) const { return std::hash<std::string>{}(value); }
    };

    static std::string s_BasePath;
    // Normalized absolute base, computed once in SetProjectAssetPath so
    // Resolve() skips redundant absolute()/lexically_normal() work per call.
    static std::string s_BaseAbs;
    static std::unordered_map<std::string, std::string, StringHash, std::equal_to<>> s_Cache;
};
