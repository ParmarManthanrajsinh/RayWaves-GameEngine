#pragma once

// Path, suffix, and host-identity seam. Every call site that wants a module
// name or the user's config directory goes through here.

#include <cstdlib>
#include <filesystem>
#include <string>
#include <system_error>

namespace platform
{
// GameLogic.so — no import-library twin, no prefix on this target (CMake
// sets PREFIX "").
inline std::string SharedLibrarySuffix()
{
    return ".so";
}

// Linux executables carry no extension.
inline std::string ExecutableSuffix()
{
    return "";
}

// Absolute path of the running executable, via the /proc symlink.
inline std::filesystem::path HostExecutablePath()
{
    std::error_code ec;
    auto path = std::filesystem::read_symlink("/proc/self/exe", ec);
    if (!ec)
    {
        return path;
    }
    return {};
}

// Single source for editor preferences, recent.ini, and anything else that
// used to read %APPDATA%. $XDG_CONFIG_HOME, else ~/.config.
inline std::filesystem::path UserConfigDir()
{
    const char *xdg = std::getenv("XDG_CONFIG_HOME");
    if (xdg != nullptr && *xdg != '\0')
    {
        return std::filesystem::path(xdg);
    }
    const char *home = std::getenv("HOME");
    if (home != nullptr && *home != '\0')
    {
        return std::filesystem::path(home) / ".config";
    }
    return {};
}

} // namespace platform
