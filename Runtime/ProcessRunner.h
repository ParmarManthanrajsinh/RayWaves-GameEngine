#pragma once
#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace ProcessRunner
{
// Runs a command through the platform shell asynchronously and streams output.
// The caller owns the returned thread and must join it before destroying
// objects captured by the callbacks. When cancel is set, the whole process
// tree is killed, so a cancelled build does not leave cmake/ninja running.
std::thread RunBuildCommand(std::string_view cmd, const std::function<void(std::string_view, bool)>& on_output,
    const std::function<void(bool)>& on_complete, const std::shared_ptr<std::atomic<bool>>& cancel = nullptr);

// Runs an explicit command line without a shell on Windows. On POSIX the
// string is handed to /bin/sh, so callers must use QuoteArg().
// Empty working_dir keeps the current directory. Same cancellation rules.
std::thread RunCommandLine(std::string_view command_line, std::string_view working_dir,
    const std::function<void(std::string_view, bool)>& on_output, const std::function<void(bool)>& on_complete,
    const std::shared_ptr<std::atomic<bool>>& cancel = nullptr);

// Runs argv[0] with the remaining arguments directly (no shell). Preferred for
// cmake, clangd, and other tool invocations.
std::thread RunCommand(std::vector<std::string> argv, std::string_view working_dir,
    const std::function<void(std::string_view, bool)>& on_output, const std::function<void(bool)>& on_complete,
    const std::shared_ptr<std::atomic<bool>>& cancel = nullptr);

// Blocking variant for worker threads that already run off the UI thread.
// Returns true only when the command exits with code 0 and was not cancelled.
bool RunAndWait(std::string_view cmd, const std::function<void(std::string_view, bool)>& on_output = nullptr,
    const std::shared_ptr<std::atomic<bool>>& cancel = nullptr);

bool RunAndWaitCommandLine(std::string_view command_line, std::string_view working_dir,
    const std::function<void(std::string_view, bool)>& on_output = nullptr,
    const std::shared_ptr<std::atomic<bool>>& cancel = nullptr, int* out_exit_code = nullptr);

bool RunAndWaitCommand(std::vector<std::string> argv, std::string_view working_dir,
    const std::function<void(std::string_view, bool)>& on_output = nullptr,
    const std::shared_ptr<std::atomic<bool>>& cancel = nullptr, int* out_exit_code = nullptr);

// Quotes one argument for the platform command line. Safe for paths
// containing spaces.
std::string QuoteArg(std::string_view arg);
} // namespace ProcessRunner
