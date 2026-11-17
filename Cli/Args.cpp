#include "Args.h"
#include <cstdlib>
#include <string>

namespace
{
const char* const k_BoolFlags[] = { "json", "clean", "force", "dry-run", "strict", "visible", "audio", "all",
    "force-unlock", "help", "version" };

bool b_IsBoolFlag(std::string_view name)
{
    for (const char* flag : k_BoolFlags)
    {
        if (name == flag)
        {
            return true;
        }
    }
    return false;
}
} // namespace

bool FArgs::b_Has(std::string_view name) const
{
    return m_Options.find(std::string(name)) != m_Options.end();
}

std::string FArgs::Get(std::string_view name, std::string_view fallback) const
{
    const auto found = m_Options.find(std::string(name));
    if (found == m_Options.end() || found->second.empty())
    {
        return std::string(fallback);
    }
    return found->second.back();
}

int FArgs::GetInt(std::string_view name, int fallback) const
{
    const std::string value = Get(name);
    if (value.empty())
    {
        return fallback;
    }
    return std::atoi(value.c_str());
}

float FArgs::GetFloat(std::string_view name, float fallback) const
{
    const std::string value = Get(name);
    if (value.empty())
    {
        return fallback;
    }
    return static_cast<float>(std::atof(value.c_str()));
}

bool b_ParseArgs(int argc, char** argv, FArgs& out, std::string& out_error)
{
    out = {};
    out_error.clear();

    for (int index = 1; index < argc; ++index)
    {
        std::string token = argv[index];
        if (token.rfind("--", 0) == 0)
        {
            std::string name = token.substr(2);
            std::string inline_value;
            const size_t equal = name.find('=');
            if (equal != std::string::npos)
            {
                inline_value = name.substr(equal + 1);
                name = name.substr(0, equal);
            }

            if (name.empty())
            {
                out_error = "Empty option name";
                return false;
            }

            if (!inline_value.empty())
            {
                out.m_Options[name].push_back(inline_value);
            }
            else if (b_IsBoolFlag(name))
            {
                out.m_Options[name].push_back("1");
            }
            else if (index + 1 < argc && std::string_view(argv[index + 1]).rfind("--", 0) != 0)
            {
                out.m_Options[name].push_back(argv[index + 1]);
                ++index;
            }
            else
            {
                out.m_Options[name].push_back("1");
            }
        }
        else
        {
            out.m_Positional.push_back(token);
        }
    }

    return true;
}
