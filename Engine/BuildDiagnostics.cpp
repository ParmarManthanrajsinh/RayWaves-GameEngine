#include "BuildDiagnostics.h"
#include <charconv>

namespace BuildDiagnostics
{
namespace
{
int ParseInt(std::string_view text)
{
    int value = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    (void)end;
    return (error == std::errc{}) ? value : 0;
}

void ParseLocation(std::string_view prefix, t_Diagnostic& out)
{
    const size_t last_paren = prefix.find_last_of(')');
    if (last_paren != std::string_view::npos && last_paren > 0)
    {
        const size_t open_paren = prefix.find_last_of('(', last_paren);
        if (open_paren != std::string_view::npos)
        {
            out.m_File = std::string(prefix.substr(0, open_paren));
            std::string_view numbers = prefix.substr(open_paren + 1, last_paren - open_paren - 1);
            const size_t comma = numbers.find(',');
            if (comma == std::string_view::npos)
            {
                out.m_Line = ParseInt(numbers);
            }
            else
            {
                out.m_Line = ParseInt(numbers.substr(0, comma));
                out.m_Column = ParseInt(numbers.substr(comma + 1));
            }
            return;
        }
    }

    // Drop the ':' that usually separates location from severity.
    while (!prefix.empty() && (prefix.back() == ':' || prefix.back() == ' '))
    {
        prefix.remove_suffix(1);
    }

    const size_t last_colon = prefix.find_last_of(':');
    if (last_colon == std::string_view::npos)
    {
        return;
    }

    const std::string_view tail = prefix.substr(last_colon + 1);
    const int tail_number = ParseInt(tail);
    if (tail_number <= 0)
    {
        // 'C:\path' style drive colon without a line number.
        return;
    }

    const size_t previous_colon = prefix.find_last_of(':', (last_colon > 0) ? last_colon - 1 : 0);
    if (previous_colon != std::string_view::npos)
    {
        const std::string_view before = prefix.substr(previous_colon + 1, last_colon - previous_colon - 1);
        const int before_number = ParseInt(before);
        if (before_number > 0)
        {
            out.m_File = std::string(prefix.substr(0, previous_colon));
            out.m_Line = before_number;
            out.m_Column = tail_number;
            return;
        }
    }

    out.m_File = std::string(prefix.substr(0, last_colon));
    out.m_Line = tail_number;
}
} // namespace

bool b_ParseLine(std::string_view line, t_Diagnostic& out)
{
    out = {};

    size_t error_position = line.find("error:");
    if (error_position == std::string_view::npos)
    {
        error_position = line.find("error C");
    }

    size_t warning_position = line.find("warning:");
    if (warning_position == std::string_view::npos)
    {
        warning_position = line.find("warning C");
    }

    if (error_position == std::string_view::npos && warning_position == std::string_view::npos)
    {
        return false;
    }

    out.m_Severity = (error_position != std::string_view::npos) ? ESeverity::Error : ESeverity::Warning;
    out.m_Raw = std::string(line);
    out.m_Message = std::string(line);

    const size_t keyword_position = (error_position != std::string_view::npos) ? error_position : warning_position;
    ParseLocation(line.substr(0, keyword_position), out);
    return true;
}

const char* ToString(ESeverity severity)
{
    switch (severity)
    {
        case ESeverity::Error:
            return "error";
        case ESeverity::Warning:
            return "warning";
        case ESeverity::Info:
        default:
            return "info";
    }
}
} // namespace BuildDiagnostics
