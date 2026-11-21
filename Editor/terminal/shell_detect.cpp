#include "shell_detect.h"

#include <algorithm>
#include <cstdlib>
#include <utility>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace term
{
namespace
{
std::string s_ResolveExecutable(const std::string& name)
{
#if defined(_WIN32)
    char buffer[MAX_PATH] = {};
    const DWORD length = SearchPathA(nullptr, name.c_str(), nullptr, MAX_PATH, buffer, nullptr);
    if (length > 0 && length < MAX_PATH)
    {
        return std::string(buffer, length);
    }
#endif
    return name;
}

bool s_DefaultExists(const std::string& name)
{
#if defined(_WIN32)
    char buffer[MAX_PATH] = {};
    const DWORD length = SearchPathA(nullptr, name.c_str(), nullptr, MAX_PATH, buffer, nullptr);
    return length > 0 && length < MAX_PATH;
#else
    return ::access(name.c_str(), X_OK) == 0;
#endif
}

t_ShellSpec s_MakeShell(std::string executable, std::string arguments, std::string display_name)
{
    t_ShellSpec shell;
    shell.executable = std::move(executable);
    shell.arguments = std::move(arguments);
    shell.display_name = std::move(display_name);
    return shell;
}

std::string s_BaseName(const std::string& path)
{
    const size_t slash = path.find_last_of("/\\");
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

std::string s_DisplayNameFor(const std::string& path)
{
    const std::string base = s_BaseName(path);
    if (base == "bash")
        return "Bash";
    if (base == "zsh")
        return "Zsh";
    if (base == "fish")
        return "Fish";
    if (base == "sh")
        return "sh";
    return base;
}

void s_AddIfAvailable(const FShellExists& check, std::vector<t_ShellSpec>& shells, const std::string& executable,
    const char* display_name)
{
    if (!check(executable))
    {
        return;
    }
    for (const t_ShellSpec& existing : shells)
    {
        if (existing.executable == executable)
        {
            return;
        }
    }
    shells.push_back(s_MakeShell(s_ResolveExecutable(executable), "", display_name));
}
} // namespace

std::vector<t_ShellSpec> DetectShells(const FShellExists& exists)
{
    const FShellExists& check = exists ? exists : FShellExists(s_DefaultExists);
    std::vector<t_ShellSpec> shells;

#if defined(_WIN32)
    if (check("pwsh.exe"))
    {
        shells.push_back(s_MakeShell(s_ResolveExecutable("pwsh.exe"), "-NoLogo", "PowerShell 7"));
    }
    if (check("powershell.exe"))
    {
        shells.push_back(s_MakeShell(s_ResolveExecutable("powershell.exe"), "-NoLogo", "Windows PowerShell"));
    }
    if (check("cmd.exe"))
    {
        shells.push_back(s_MakeShell(s_ResolveExecutable("cmd.exe"), "", "Command Prompt"));
    }
#else
    const char* env_shell = std::getenv("SHELL");
    if (env_shell != nullptr && env_shell[0] != '\0')
    {
        const std::string shell_path(env_shell);
        if (check(shell_path))
        {
            shells.push_back(s_MakeShell(shell_path, "", s_DisplayNameFor(shell_path)));
        }
    }

    s_AddIfAvailable(check, shells, "/bin/bash", "Bash");
    s_AddIfAvailable(check, shells, "/bin/zsh", "Zsh");
    s_AddIfAvailable(check, shells, "/bin/fish", "Fish");
    s_AddIfAvailable(check, shells, "/bin/sh", "sh");
#endif

    return shells;
}

t_ShellSpec DefaultShell(const FShellExists& exists)
{
    std::vector<t_ShellSpec> shells = DetectShells(exists);
    if (!shells.empty())
    {
        return shells.front();
    }

#if defined(_WIN32)
    return s_MakeShell("cmd.exe", "", "Command Prompt");
#else
    return s_MakeShell("/bin/sh", "", "sh");
#endif
}
} // namespace term
