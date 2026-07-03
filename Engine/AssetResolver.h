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
};
