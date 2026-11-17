#pragma once
#include "Project.h"
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

struct FProjectContext
{
    std::filesystem::path m_Dir;
    t_Project m_Project;
};

// Accepts a project folder, a path inside it, or the manifest itself.
bool b_ResolveProject(const std::string& raw_path, FProjectContext& out, std::string& out_error);

void PrintError(bool b_json, std::string_view code, std::string_view message);
std::string ErrorJson(std::string_view code, std::string_view message);

std::filesystem::path GetSelfPath();
std::vector<std::filesystem::path> ListSceneFiles(const std::filesystem::path& asset_root);

// Picks --map when provided, otherwise the only scene file in Assets/.
bool b_ResolveSceneKey(
    const FProjectContext& project, const std::string& map_option, std::string& out_key, std::string& out_error);

std::string JoinTail(const std::vector<std::string>& lines, size_t max_lines);
