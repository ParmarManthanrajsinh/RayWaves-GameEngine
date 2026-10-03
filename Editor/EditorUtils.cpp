#include "EditorUtils.h"
#include <cstdlib>
#include <iostream>
#include <unistd.h>
#include <string>

namespace EditorUtils
{
    // Launch an app/URL handler without going through a shell, so no quoting
    // rules apply. posix_spawn + _exit keeps SIGCHLD out of our process group
    // handling.
    static bool SpawnDetached(char* const argv[])
    {
        pid_t pid = fork();
        if (pid < 0)
        {
            return false;
        }
        if (pid == 0)
        {
            // Child: own session so closing it does not kill the editor.
            setsid();
            execvp(argv[0], argv);
            _exit(127);
        }
        return true;
    }

    bool OpenInExplorer(const std::filesystem::path& path)
    {
        if (!std::filesystem::exists(path))
        {
            std::cerr << "Failed to open path: directory does not exist. Path: "
                      << path.string() << '\n';
            return false;
        }

        std::filesystem::path abs_path = std::filesystem::absolute(path);
        abs_path.make_preferred();
        std::string target = abs_path.string();
        char* argv[] = {const_cast<char*>("xdg-open"),
                        const_cast<char*>(target.c_str()), nullptr};
        if (SpawnDetached(argv))
        {
            return true;
        }

        std::cerr << "Failed to open path: xdg-open could not be spawned. Path: "
                  << target << '\n';
        return false;
    }

    bool OpenURL(std::string_view url)
    {
        std::string target(url);
        char* argv[] = {const_cast<char*>("xdg-open"),
                        const_cast<char*>(target.c_str()), nullptr};
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
        constexpr std::string_view dangerous =
            "&|;$\"`'<>%!^()@#\\\n\r";
        return s.find_first_of(dangerous) == std::string_view::npos;
    }
}
