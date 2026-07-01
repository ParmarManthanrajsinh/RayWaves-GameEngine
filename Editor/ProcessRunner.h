#pragma once
#include <string>
#include <functional>

namespace ProcessRunner 
{
    // Runs a command asynchronously and streams output to callback
    // Returns void immediately (detached thread)
    void RunBuildCommand
    (
        std::string_view cmd,
        std::function<void(std::string_view, bool)> on_output, 
        std::function<void(bool)> on_complete
    );
}
