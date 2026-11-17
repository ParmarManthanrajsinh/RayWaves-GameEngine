#pragma once
#include <filesystem>
#include <string_view>

// Win32 helpers isolated behind a raylib-free header. Including <windows.h>
// next to raylib collides on Rectangle/CloseWindow/ShowCursor/LoadImage.
namespace PlatformInfo
{
std::filesystem::path GetExecutablePath();
int GetCurrentProcessId();
std::filesystem::path FindExecutable(std::string_view name);
} // namespace PlatformInfo
