#pragma once
#include <cmath>
#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

// Minimal JSON writing helpers. Parsing is not needed: rwcli only reads
// project/scene files whose formats already exist.
namespace CliJson
{
inline std::string Escape(std::string_view text)
{
    std::string out;
    out.reserve(text.size() + 8);
    for (const char character : text)
    {
        switch (character)
        {
            case '"':
                out += "\\\"";
                break;
            case '\\':
                out += "\\\\";
                break;
            case '\n':
                out += "\\n";
                break;
            case '\r':
                out += "\\r";
                break;
            case '\t':
                out += "\\t";
                break;
            default:
                if (static_cast<unsigned char>(character) < 0x20)
                {
                    char buffer[8]{};
                    std::snprintf(buffer, sizeof(buffer), "\\u%04x", static_cast<unsigned char>(character));
                    out += buffer;
                }
                else
                {
                    out += character;
                }
                break;
        }
    }
    return out;
}

inline std::string Quoted(std::string_view text)
{
    return "\"" + Escape(text) + "\"";
}

inline std::string Number(double value)
{
    if (!std::isfinite(value))
    {
        return "null";
    }
    char buffer[64]{};
    std::snprintf(buffer, sizeof(buffer), "%.6g", value);
    return buffer;
}

inline std::string Number(float value)
{
    return Number(static_cast<double>(value));
}

inline std::string Number(int value)
{
    return std::to_string(value);
}

inline std::string Bool(bool value)
{
    return value ? "true" : "false";
}

inline std::string Key(std::string_view key)
{
    return Quoted(key) + ":";
}

inline std::string Array(const std::vector<std::string>& items)
{
    std::string out = "[";
    for (size_t index = 0; index < items.size(); ++index)
    {
        if (index > 0)
        {
            out += ",";
        }
        out += items[index];
    }
    out += "]";
    return out;
}
} // namespace CliJson
