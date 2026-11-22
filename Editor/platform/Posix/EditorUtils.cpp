#include "EditorUtils.h"
#include "ProcessRunner.h"

#include <cerrno>
#include <cstring>
#include <iostream>
#include <spawn.h>
#include <string>
#include <sys/wait.h>
#include <system_error>
#include <thread>
#include <unistd.h>
#include <vector>

extern char** environ;

namespace
{
// VS Code first; VSCodium ships the same CLI under "codium" for users who
// replaced Microsoft's build.
constexpr const char* k_EditorCommands[] = { "code", "codium" };

bool SpawnDetached(const std::filesystem::path& target)
{
    const std::string program = "xdg-open";
    std::string argument = target.string();
    char* argv[] = { const_cast<char*>(program.c_str()), argument.data(), nullptr };

    pid_t pid = 0;
    const int spawn_result = posix_spawnp(&pid, program.c_str(), nullptr, nullptr, argv, environ);
    if (spawn_result != 0)
    {
        std::cerr << "Failed to launch xdg-open: " << std::strerror(spawn_result) << '\n';
        return false;
    }

    // xdg-open may hand off to a file manager and linger. Reap it on a helper
    // thread so the editor UI thread never blocks.
    std::thread(
        [pid]()
        {
            int status = 0;
            while (waitpid(pid, &status, 0) < 0 && errno == EINTR)
            {
            }
        })
        .detach();
    return true;
}

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
    std::error_code error;
    if (!std::filesystem::exists(path, error))
    {
        std::cerr << "Failed to open path: directory does not exist. Path: " << path.string() << '\n';
        return false;
    }
    return SpawnDetached(std::filesystem::absolute(path, error));
}

bool OpenInDefaultApp(const std::filesystem::path& path)
{
    std::error_code error;
    if (!std::filesystem::exists(path, error))
    {
        return false;
    }
    return SpawnDetached(std::filesystem::absolute(path, error));
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

    const std::filesystem::path abs_path = std::filesystem::absolute(path, error);
    if (error)
    {
        return false;
    }

    if (TryOpenInEditor(abs_path, line))
    {
        return true;
    }

    // No editor CLI on PATH: hand the file to the shell (line is ignored).
    return OpenInDefaultApp(abs_path);
}

bool OpenURL(std::string_view url)
{
    return SpawnDetached(std::filesystem::path(std::string(url)));
}

bool IsShellSafe(std::string_view s)
{
    constexpr std::string_view dangerous = "&|;$\"`'<>%!^()@#\n\r";
    return s.find_first_of(dangerous) == std::string_view::npos;
}
} // namespace EditorUtils
