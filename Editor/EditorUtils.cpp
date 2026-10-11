#include "EditorUtils.h"
#include "../Engine/Platform/PlatformPaths.h"
#include "../Engine/ProjectManager.h"
#include <cstdlib> // IWYU pragma: keep
#include <filesystem>
#include <iostream>
#include <spawn.h>
#include <string>
#include <system_error>

extern char **environ;

namespace EditorUtils
{
    // Launch an app/URL handler without going through a shell, so no quoting
    // rules apply. posix_spawn + POSIX_SPAWN_SETSID runs the child in its own
    // session (closing it does not kill the editor) and keeps SIGCHLD out of
    // our process group handling — no fork() copy of this process.
    static bool SpawnDetached(char *const argv[])
    {
        posix_spawnattr_t attr{};

        if (posix_spawnattr_init(&attr) != 0)
        {
            return false;
        }

        if (posix_spawnattr_setflags(&attr, POSIX_SPAWN_SETSID) != 0)
        {
            posix_spawnattr_destroy(&attr);
            return false;
        }

        pid_t pid{};
        const int RC = posix_spawnp(&pid, argv[0], nullptr, &attr, argv, environ);
        posix_spawnattr_destroy(&attr);
        return RC == 0;
    }

    bool OpenInExplorer(const std::filesystem::path &path)
    {
        std::error_code ec;

        if (!std::filesystem::exists(path, ec))
        {
            std::cerr << "Failed to open path: directory does not exist. Path: "
                      << path << ec.message() << '\n';
            return false;
        }

        const std::filesystem::path abs_path = std::filesystem::absolute(path);
        std::string target                   = abs_path.string();
        char *argv[] = {const_cast<char *>("xdg-open"),
                        const_cast<char *>(target.c_str()), nullptr};
        if (SpawnDetached(argv))
        {
            return true;
        }

        std::cerr
            << "Failed to open path: xdg-open could not be spawned. Path: "
            << target << '\n';
        return false;
    }

    bool OpenURL(std::string_view url)
    {
        std::string target(url);
        char *argv[] = 
        {
            const_cast<char *>("xdg-open"),
            const_cast<char *>(target.c_str()), nullptr
        };
        return SpawnDetached(argv);
    }

    bool IsShellSafe(std::string_view s)
    {
        if (s.empty())
        {
            return false;
        }
        // Backslash is a live escape character under sh, so it joins the
        // deny-list. Callers gate every project path that reaches `sh -c`
        // with this.
        constexpr std::string_view c_DANGEROUS = "&|;$\"`'<>%!^()@#\\\n\r";
        return s.find_first_of(c_DANGEROUS) == std::string_view::npos;
    }

    void EnsureValidCwd()
    {
        namespace fs = std::filesystem;
        std::error_code ec;

        // This is actually for varify that actual error is correct or not ...
        // Even if someone initialized wrong error code in ec, then you will not
        // get error due to current_path(ec) varification
        static_cast<void>(fs::current_path(ec));

        if (!ec)
        {
            return;
        }
        // CWD inode is gone (dist folder deleted or moved while running).
        // Try the directory holding the executable, then "/".
        const fs::path EXE = platform::HostExecutablePath();
        if (!EXE.empty())
        {
            std::error_code ec2;
            fs::current_path(EXE.parent_path(), ec2);
            if (!ec2)
            {
                return;
            }
        }
        fs::current_path("/", ec);
    }

    std::string GameConfigPath()
    {
        const std::filesystem::path DIR = platform::UserConfigDir();
        if (DIR.empty())
        {
            return "config.ini";
        }
        return (DIR / "RayWaves" / "config.ini").string();
    }

    std::string DefaultDialogDir()
    {
        namespace fs = std::filesystem;
        if (ProjectManager::b_HasOpenProject())
        {
            return ProjectManager::GetCurrent().m_RootPath;
        }
        std::error_code ec;
        fs::path cwd = fs::current_path(ec);
        if (!ec)
        {
            return cwd.string();
        }
        return "/";
    }
} // namespace EditorUtils
