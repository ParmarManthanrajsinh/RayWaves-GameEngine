#include "PlatformInfo.h"
#include <array>
#include <string>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace PlatformInfo
{
std::filesystem::path GetExecutablePath()
{
    std::array<char, MAX_PATH> path{};
    GetModuleFileNameA(nullptr, path.data(), static_cast<DWORD>(path.size()));
    return { path.data() };
}

int GetCurrentProcessId()
{
    return static_cast<int>(::GetCurrentProcessId());
}

std::filesystem::path FindExecutable(std::string_view name)
{
    std::array<char, MAX_PATH> buffer{};
    const DWORD length = SearchPathA(
        nullptr, std::string(name).c_str(), nullptr, static_cast<DWORD>(buffer.size()), buffer.data(), nullptr);
    if (length == 0 || length >= buffer.size())
    {
        return {};
    }
    return { buffer.data() };
}
} // namespace PlatformInfo
