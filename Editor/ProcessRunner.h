#pragma once
#include <string>
#include <functional>
#include <thread>

namespace ProcessRunner 
{
    // Runs a command asynchronously and streams output to callback.
    // Returns the worker thread; the caller must join it (GameEditor stores
    // it and joins in its destructor). Never detached: a detached thread
    // racing editor destruction is a use-after-free.
    [[nodiscard]] std::thread RunBuildCommand
    (
        std::string_view cmd,
        const std::function<void(std::string_view, bool)>& on_output, 
        const std::function<void(bool)>& on_complete
    );
}
