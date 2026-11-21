#include "EditorUtils.h"

#include <cerrno>
#include <cstring>
#include <iostream>
#include <spawn.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

extern char** environ;

namespace
{
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
