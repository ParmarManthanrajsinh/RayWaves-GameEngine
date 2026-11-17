#pragma once
#include <map>
#include <string>
#include <string_view>
#include <vector>

// Tiny argv parser. Positionals keep order (command words, project path);
// options accept --name value, --name=value, or bare --flag.
struct FArgs
{
    std::vector<std::string> m_Positional;
    std::map<std::string, std::vector<std::string>> m_Options;

    bool b_Has(std::string_view name) const;
    std::string Get(std::string_view name, std::string_view fallback = "") const;
    int GetInt(std::string_view name, int fallback) const;
    float GetFloat(std::string_view name, float fallback) const;
};

bool b_ParseArgs(int argc, char** argv, FArgs& out, std::string& out_error);
