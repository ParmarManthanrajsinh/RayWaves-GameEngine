#pragma once
#include <filesystem>
#include <string>
#include <string_view>

// Host services kept raylib-free. Including <windows.h> next to raylib
// collides on Rectangle/CloseWindow/ShowCursor/LoadImage, so the Win32 branch
// is isolated inside PlatformInfo.cpp.
namespace PlatformInfo
{
std::filesystem::path GetExecutablePath();
int GetCurrentProcessId();
std::filesystem::path FindExecutable(std::string_view name);

// Base directory for per-user RayWaves state (APPDATA on Windows,
// XDG_CONFIG_HOME/~/.config on Linux). Callers append "RayWaves".
std::filesystem::path GetAppConfigDirectory();

bool b_IsProcessAlive(int pid);

// Monotonic milliseconds for lock/accounting fields.
long long GetMonotonicMilliseconds();

// Atomically create a new file with the given contents. Fails when the file
// already exists. Used by the cross-process agent lock.
bool b_CreateFileExclusive(const std::filesystem::path& path, std::string_view contents, std::string& out_error);

// Crash-safe replacement: writes a sibling temp file, flushes it, then swaps it
// over the target so readers never see a partial file. The parent directory
// must already exist; on any failure the original target is left untouched.
bool b_WriteFileAtomic(const std::filesystem::path& path, std::string_view contents, std::string& out_error);

// Copies an existing file to path + ".bak", overwriting a previous backup.
// A missing source is an error so callers cannot mistake a no-op for a backup.
bool b_BackupFile(const std::filesystem::path& path, std::string& out_error);

bool b_SetEnvironmentVariable(std::string_view name, std::string_view value);
} // namespace PlatformInfo
