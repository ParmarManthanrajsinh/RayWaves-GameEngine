#include "EditorUtils.h"
#include "ProcessRunner.h"
#include <iostream>
#include <string>
#include <system_error>
#include <vector>
// clang-format off
#include <windows.h>
#include <shellapi.h>
// clang-format on

namespace
{
// VS Code's CLI ships as code.cmd next to code.exe; the shim resolves the
// install for both user and system installs, so try it before a bare "code".
constexpr const char* k_EditorCommands[] = { "code.cmd", "code" };

bool TryOpenInEditor(const std::filesystem::path& absolute_path, int line)
{
    const std::string target = absolute_path.string();
    const std::string location = target + ":" + std::to_string(line);

    for (const char* command : k_EditorCommands)
    {
        std::vector<std::string> argv;
        if (line > 0)
        {
            argv = { command, "--goto", location };
        }
        else
        {
            argv = { command, target };
        }

        // argv form keeps spaces in the path intact without a shell string.
        if (ProcessRunner::RunAndWaitCommand(argv, {}))
        {
            return true;
        }
    }
    return false;
}
} // namespace

namespace EditorUtils
{
bool OpenInExplorer(const std::filesystem::path& path)
{
    if (std::filesystem::exists(path))
    {
        std::filesystem::path abs_path = std::filesystem::absolute(path);
        abs_path.make_preferred();
        HINSTANCE result =
            ShellExecuteW(nullptr, L"explore", abs_path.wstring().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        if ((intptr_t)result > 32)
            return true;
        std::cerr << "Failed to open path: ShellExecuteW failed with error code: " << (intptr_t)result
                  << " Path: " << abs_path.string() << '\n';
        return false;
    }

    std::cerr << "Failed to open path: directory does not exist. Path: " << path.string() << '\n';
    return false;
}

bool OpenInDefaultApp(const std::filesystem::path& path)
{
    if (!std::filesystem::exists(path))
    {
        return false;
    }

    std::filesystem::path abs_path = std::filesystem::absolute(path);
    abs_path.make_preferred();
    HINSTANCE result = ShellExecuteW(nullptr, L"open", abs_path.wstring().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    return (intptr_t)result > 32;
}

bool OpenFileAtLine(const std::filesystem::path& path, int line)
{
    if (line < 0)
    {
        return false;
    }

    std::error_code error;
    if (!std::filesystem::exists(path, error) || error)
    {
        std::cerr << "Failed to open file: path does not exist. Path: " << path.string() << '\n';
        return false;
    }

    std::filesystem::path abs_path = std::filesystem::absolute(path, error);
    if (error)
    {
        abs_path = path;
        error.clear();
    }
    abs_path.make_preferred();

    if (TryOpenInEditor(abs_path, line))
    {
        return true;
    }

    // No editor CLI on PATH: hand the file to the shell (line is ignored).
    return OpenInDefaultApp(abs_path);
}

bool OpenURL(std::string_view url)
{
    std::wstring wurl(url.begin(), url.end());
    HINSTANCE result = ShellExecuteW(nullptr, L"open", wurl.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    return (intptr_t)result > 32;
}

bool IsShellSafe(std::string_view s)
{
    constexpr std::string_view dangerous = "&|;$\"`'<>%!^()@#\n\r";
    return s.find_first_of(dangerous) == std::string_view::npos;
}
} // namespace EditorUtils
