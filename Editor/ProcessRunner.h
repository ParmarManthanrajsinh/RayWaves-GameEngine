#pragma once
#include <atomic>
#include <memory>
#include <string>
#include <string_view>
#include <functional>
#include <thread>

namespace ProcessRunner 
{
    // Runs a command asynchronously and streams output to callback.
    // The caller owns the returned thread and must join it before
    // destroying objects captured by the callbacks.
    // When cancel is set, the whole process tree (cmd.exe and everything it
    // spawned) is killed through a Job Object, so a cancelled build does not
    // leave cmake/ninja running and holding build files.
    std::thread RunBuildCommand
    (
        std::string_view cmd,
        const std::function<void(std::string_view, bool)>& on_output, 
        const std::function<void(bool)>& on_complete,
        const std::shared_ptr<std::atomic<bool>>& cancel = nullptr
    );

    // Blocking variant for worker threads that already run off the UI thread.
    // Returns true only when the command exits with code 0 and was not cancelled.
    bool RunAndWait
    (
        std::string_view cmd,
        const std::function<void(std::string_view, bool)>& on_output = nullptr,
        const std::shared_ptr<std::atomic<bool>>& cancel = nullptr
    );
}
