#pragma once
#include <string>
#include <functional>

namespace ProcessRunner 
{
    // Runs a command asynchronously and streams output to callback
    // Returns void immediately (detached thread)
    void RunBuildCommand(const std::string& cmd, std::function<void(const std::string&, bool)> onOutput, std::function<void(bool)> onComplete);
}
