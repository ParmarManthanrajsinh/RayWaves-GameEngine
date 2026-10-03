#include "ProcessRunner.h"
#include <array>
#include <cstdio>
#include <functional>
#include <string>
#include <string_view>
#include <thread>
#include <sys/wait.h>

namespace ProcessRunner
{
    std::thread RunBuildCommand
    (
        std::string_view cmd,
        const std::function<void(std::string_view, bool)>& on_output,
        const std::function<void(bool)>& on_complete
    )
    {
        return std::thread([cmd_str = std::string(cmd), on_output, on_complete]()
        {
            // popen already runs the string through `sh -c`, so callers pass
            // the pipeline itself with no shell prefix.
            FILE* pipe = popen(cmd_str.c_str(), "r");
            if (pipe == nullptr)
            {
                if (on_output)
                {
                    on_output("Failed to create process.", true);
                }
                if (on_complete)
                {
                    on_complete(false);
                }
                return;
            }

            std::array<char, 128> buffer{};
            std::string current_line;

            while (std::size_t bytes_read =
                       fread(buffer.data(), 1, buffer.size(), pipe))
            {
                current_line.append(buffer.data(), bytes_read);

                std::string_view view = current_line;

                while (view.contains('\n'))
                {
                    const auto pos = view.find('\n');

                    std::string_view line = view.substr(0, pos);

                    if (!line.empty() && line.back() == '\r')
                    {
                        line.remove_suffix(1);
                    }

                    if (on_output)
                    {
                        on_output(line, false);
                    }

                    view.remove_prefix(pos + 1);
                }

                current_line.erase(0, current_line.length() - view.length());
            }

            if (!current_line.empty())
            {
                if (on_output)
                {
                    on_output(current_line, false);
                }
            }

            int status = pclose(pipe);
            bool success =
                status != -1 && WIFEXITED(status) && WEXITSTATUS(status) == 0;

            if (on_complete)
            {
                on_complete(success);
            }

        });
    }
}
