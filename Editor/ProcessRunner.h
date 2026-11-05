#pragma once
#include <string>
#include <functional>
#include <thread>

namespace ProcessRunner 
{
    // Runs a command asynchronously and streams output to callback.
    // The caller owns the returned thread and must join it before
    // destroying objects captured by the callbacks.
    std::thread RunBuildCommand
    (
        std::string_view cmd,
        const std::function<void(std::string_view, bool)>& on_output, 
        const std::function<void(bool)>& on_complete
    );
}
