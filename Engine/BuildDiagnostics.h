#pragma once
#include <string>
#include <string_view>

// One compiler/build line normalized into a file/line/column shape.
// Compiler output differs between clang, zig, ninja, and MSVC; parsing is
// best-effort and the raw line is always kept.
namespace BuildDiagnostics
{
enum class ESeverity
{
    Info,
    Warning,
    Error
};

struct t_Diagnostic
{
    ESeverity m_Severity = ESeverity::Info;
    std::string m_File;
    int m_Line = 0;
    int m_Column = 0;
    std::string m_Message;
    std::string m_Raw;
};

bool b_ParseLine(std::string_view line, t_Diagnostic& out);
const char* ToString(ESeverity severity);
} // namespace BuildDiagnostics
