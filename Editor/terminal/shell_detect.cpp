#include "shell_detect.h"

#include <algorithm>
#include <utility>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace term
{
namespace
{
std::wstring s_ResolveExecutable(const std::wstring& name)
{
#if defined(_WIN32)
    wchar_t buffer[MAX_PATH] = {};
    const DWORD length = SearchPathW(nullptr, name.c_str(), nullptr, MAX_PATH, buffer, nullptr);
    if (length > 0 && length < MAX_PATH)
    {
        return std::wstring(buffer, length);
    }
#endif
    return name;
}

bool s_DefaultExists(const std::wstring& name)
{
#if defined(_WIN32)
    wchar_t buffer[MAX_PATH] = {};
    const DWORD length = SearchPathW(nullptr, name.c_str(), nullptr, MAX_PATH, buffer, nullptr);
    return length > 0 && length < MAX_PATH;
#else
    (void)name;
    return false;
#endif
}

t_ShellSpec s_MakeShell(std::wstring executable, std::wstring arguments, std::string display_name)
{
    t_ShellSpec shell;
    shell.executable = std::move(executable);
    shell.arguments = std::move(arguments);
    shell.display_name = std::move(display_name);
    return shell;
}
} // namespace

std::vector<t_ShellSpec> DetectShells(const FShellExists& exists)
{
    const FShellExists& check = exists ? exists : FShellExists(s_DefaultExists);
    std::vector<t_ShellSpec> shells;

    if (check(L"pwsh.exe"))
    {
        shells.push_back(s_MakeShell(s_ResolveExecutable(L"pwsh.exe"), L"-NoLogo", "PowerShell 7"));
    }
    if (check(L"powershell.exe"))
    {
        shells.push_back(s_MakeShell(s_ResolveExecutable(L"powershell.exe"), L"-NoLogo", "Windows PowerShell"));
    }
    if (check(L"cmd.exe"))
    {
        shells.push_back(s_MakeShell(s_ResolveExecutable(L"cmd.exe"), L"", "Command Prompt"));
    }

    return shells;
}

t_ShellSpec DefaultShell(const FShellExists& exists)
{
    std::vector<t_ShellSpec> shells = DetectShells(exists);
    if (!shells.empty())
    {
        return shells.front();
    }

    return s_MakeShell(L"cmd.exe", L"", "Command Prompt");
}

} // namespace term
