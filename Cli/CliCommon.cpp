#include "CliCommon.h"
#include "Json.h"
#include "PlatformInfo.h"
#include "Scene.h"
#include <algorithm>
#include <iostream>

namespace fs = std::filesystem;

bool b_ResolveProject(const std::string& raw_path, FProjectContext& out, std::string& out_error)
{
    out = {};
    out_error.clear();

    if (raw_path.empty())
    {
        out_error = "Missing project path";
        return false;
    }

    std::error_code error;
    fs::path path = fs::absolute(fs::path(raw_path), error);
    if (error)
    {
        out_error = "Invalid path: " + raw_path;
        return false;
    }

    if (fs::is_regular_file(path, error))
    {
        path = path.parent_path();
    }

    if (!fs::is_directory(path, error))
    {
        out_error = "Not a directory: " + path.string();
        return false;
    }

    const fs::path manifest = path / "project.raywaves";
    if (!fs::exists(manifest, error))
    {
        out_error = "No project.raywaves in " + path.string();
        return false;
    }

    path = fs::weakly_canonical(path, error);
    out.m_Dir = path;
    if (!out.m_Project.m_bLoadFromFile((path / "project.raywaves").string()))
    {
        out_error = "Could not parse project.raywaves";
        return false;
    }

    return true;
}

void PrintError(bool b_json, std::string_view code, std::string_view message)
{
    if (b_json)
    {
        std::cout << ErrorJson(code, message) << "\n";
    }
    else
    {
        std::cerr << "error [" << code << "]: " << message << "\n";
    }
}

std::string ErrorJson(std::string_view code, std::string_view message)
{
    return "{\"ok\":false,\"error\":{\"code\":" + CliJson::Quoted(code) + ",\"message\":" + CliJson::Quoted(message) +
        "}}";
}

fs::path GetSelfPath()
{
    return PlatformInfo::GetExecutablePath();
}

std::vector<fs::path> ListSceneFiles(const fs::path& asset_root)
{
    std::vector<fs::path> scenes;
    std::error_code error;
    if (!fs::is_directory(asset_root, error))
    {
        return scenes;
    }

    for (const fs::directory_entry& entry : fs::directory_iterator(asset_root, error))
    {
        if (entry.is_regular_file(error) && entry.path().extension() == ".scene")
        {
            scenes.push_back(entry.path());
        }
    }

    std::ranges::sort(scenes);
    return scenes;
}

bool b_ResolveSceneKey(
    const FProjectContext& project, const std::string& map_option, std::string& out_key, std::string& out_error)
{
    out_key.clear();
    out_error.clear();

    if (!map_option.empty())
    {
        out_key = Scene::ToFileKey(map_option);
        return true;
    }

    const std::vector<fs::path> scenes = ListSceneFiles(project.m_Project.m_AssetPath);
    if (scenes.empty())
    {
        out_error = "No scene files in " + project.m_Project.m_AssetPath + ". Pass --map <MapId> to create one.";
        return false;
    }

    if (scenes.size() > 1)
    {
        std::string options;
        for (const fs::path& scene : scenes)
        {
            if (!options.empty())
            {
                options += ", ";
            }
            options += scene.stem().string();
        }
        out_error = "Multiple scenes found (" + options + "). Pass --map <MapId>.";
        return false;
    }

    out_key = scenes.front().stem().string();
    return true;
}

std::string JoinTail(const std::vector<std::string>& lines, size_t max_lines)
{
    const size_t start = (lines.size() > max_lines) ? lines.size() - max_lines : 0;
    std::string joined;
    for (size_t index = start; index < lines.size(); ++index)
    {
        if (!joined.empty())
        {
            joined += "\n";
        }
        joined += lines[index];
    }
    return joined;
}
