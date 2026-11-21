#include "AssetResolver.h"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace
{
std::string NormalizeRelative(std::string_view relativePath)
{
    std::string normalized(relativePath);
    std::replace(normalized.begin(), normalized.end(), '\\', '/');
    return normalized;
}

char LowerAscii(char character)
{
    return static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
}

bool EqualsIgnoreCase(std::string_view left, std::string_view right)
{
    return left.size() == right.size() &&
        std::equal(
            left.begin(), left.end(), right.begin(), [](char a, char b) { return LowerAscii(a) == LowerAscii(b); });
}

// Resolve one path component against a directory. Returns an empty path when
// there is no match, and reports ambiguity instead of guessing.
std::filesystem::path FindComponentIgnoreCase(
    const std::filesystem::path& directory, const std::string& component, bool& out_ambiguous)
{
    out_ambiguous = false;
    std::error_code error;
    if (!std::filesystem::is_directory(directory, error))
    {
        return {};
    }

    std::vector<std::filesystem::path> matches;
    for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(directory, error))
    {
        if (error)
        {
            break;
        }
        if (EqualsIgnoreCase(entry.path().filename().string(), component))
        {
            matches.push_back(entry.path());
        }
    }

    if (matches.empty())
    {
        return {};
    }
    if (matches.size() > 1)
    {
        out_ambiguous = true;
        std::cerr << "[AssetResolver] ambiguous case-insensitive match for '" << component << "':";
        for (const std::filesystem::path& match : matches)
        {
            std::cerr << ' ' << match.string();
        }
        std::cerr << '\n';
        return {};
    }
    return matches.front();
}
} // namespace

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

    const std::string normalized = NormalizeRelative(relativePath);
    if (auto it = s_Cache.find(normalized); it != s_Cache.end())
        return it->second;

    std::filesystem::path resolved = (std::filesystem::path(s_BaseAbs) / normalized).lexically_normal();

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

    std::error_code error;
    if (std::filesystem::exists(resolved, error))
    {
        const std::string result = resolved.string();
        s_Cache.emplace(normalized, result);
        return result;
    }

    // Windows paths are already case-insensitive; on POSIX this is a fallback
    // for projects authored on a case-insensitive filesystem.
    std::filesystem::path current = s_BaseAbs;
    for (const std::filesystem::path& part : std::filesystem::path(normalized))
    {
        if (part == "." || part.empty())
        {
            continue;
        }

        const std::filesystem::path candidate = current / part;
        if (std::filesystem::exists(candidate, error))
        {
            current = candidate;
            continue;
        }

        bool b_ambiguous = false;
        const std::filesystem::path match = FindComponentIgnoreCase(current, part.string(), b_ambiguous);
        if (b_ambiguous)
        {
            return std::string(relativePath);
        }
        if (match.empty())
        {
            // Keep the absolute candidate for callers that create new files
            // (for example scene saves). Reads simply fail later in raylib.
            return resolved.string();
        }
        current = match;
    }

    if (!std::filesystem::exists(current, error))
    {
        return resolved.string();
    }

    const std::string result = current.lexically_normal().string();
    s_Cache.emplace(normalized, result);
    return result;
}

std::string AssetResolver::GetProjectAssetPath()
{
    return s_BasePath;
}
