#include "PlatformInfo.h"

#if defined(_WIN32)

#include <array>
#include <cstdlib>
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

std::filesystem::path GetAppConfigDirectory()
{
    const char* appdata = std::getenv("APPDATA");
    if (appdata != nullptr && appdata[0] != '\0')
    {
        return std::filesystem::path(appdata);
    }
    const char* local_appdata = std::getenv("LOCALAPPDATA");
    if (local_appdata != nullptr && local_appdata[0] != '\0')
    {
        return std::filesystem::path(local_appdata);
    }
    return std::filesystem::current_path();
}

bool b_IsProcessAlive(int pid)
{
    if (pid <= 0)
    {
        return false;
    }

    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, static_cast<DWORD>(pid));
    if (process == nullptr)
    {
        return false;
    }

    DWORD exit_code = STILL_ACTIVE;
    const bool b_alive = GetExitCodeProcess(process, &exit_code) != FALSE && exit_code == STILL_ACTIVE;
    CloseHandle(process);
    return b_alive;
}

long long GetMonotonicMilliseconds()
{
    return static_cast<long long>(GetTickCount64());
}

bool b_SetEnvironmentVariable(std::string_view name, std::string_view value)
{
    return _putenv_s(std::string(name).c_str(), std::string(value).c_str()) == 0;
}

bool b_CreateFileExclusive(const std::filesystem::path& path, std::string_view contents, std::string& out_error)
{
    out_error.clear();
    HANDLE file =
        CreateFileA(path.string().c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
    {
        out_error = "Could not create " + path.string() + " (error " + std::to_string(GetLastError()) + ")";
        return false;
    }

    DWORD written = 0;
    const bool b_ok =
        WriteFile(file, contents.data(), static_cast<DWORD>(contents.size()), &written, nullptr) != FALSE &&
        written == contents.size();
    CloseHandle(file);
    if (!b_ok)
    {
        std::error_code ignored;
        std::filesystem::remove(path, ignored);
        out_error = "Could not write " + path.string();
        return false;
    }
    return true;
}
} // namespace PlatformInfo

#else

#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <string>

#include <fcntl.h>
#include <unistd.h>

namespace PlatformInfo
{
std::filesystem::path GetExecutablePath()
{
    std::error_code error;
    const std::filesystem::path proc_path = std::filesystem::read_symlink("/proc/self/exe", error);
    if (!error && !proc_path.empty())
    {
        return proc_path;
    }
    return {};
}

int GetCurrentProcessId()
{
    return static_cast<int>(::getpid());
}

std::filesystem::path FindExecutable(std::string_view name)
{
    const std::string executable(name);
    if (executable.empty())
    {
        return {};
    }

    // An explicit path is used as-is.
    if (executable.find('/') != std::string::npos)
    {
        std::error_code error;
        if (::access(executable.c_str(), X_OK) == 0 && std::filesystem::is_regular_file(executable, error))
        {
            return executable;
        }
        return {};
    }

    const char* path_env = std::getenv("PATH");
    if (path_env == nullptr)
    {
        return {};
    }

    std::string paths(path_env);
    size_t start = 0;
    while (start <= paths.size())
    {
        const size_t end = paths.find(':', start);
        const std::string directory = paths.substr(start, (end == std::string::npos ? paths.size() : end) - start);
        const std::filesystem::path candidate =
            (directory.empty() ? std::filesystem::path(".") : std::filesystem::path(directory)) / executable;
        std::error_code error;
        if (::access(candidate.c_str(), X_OK) == 0 && std::filesystem::is_regular_file(candidate, error))
        {
            return candidate;
        }
        if (end == std::string::npos)
        {
            break;
        }
        start = end + 1;
    }
    return {};
}

std::filesystem::path GetAppConfigDirectory()
{
    const char* xdg_config = std::getenv("XDG_CONFIG_HOME");
    if (xdg_config != nullptr && xdg_config[0] != '\0')
    {
        return std::filesystem::path(xdg_config);
    }
    const char* home = std::getenv("HOME");
    if (home != nullptr && home[0] != '\0')
    {
        return std::filesystem::path(home) / ".config";
    }
    return std::filesystem::current_path();
}

bool b_IsProcessAlive(int pid)
{
    if (pid <= 0)
    {
        return false;
    }
    if (::kill(static_cast<pid_t>(pid), 0) == 0)
    {
        return true;
    }
    // EPERM means the process exists but belongs to another user.
    return errno == EPERM;
}

long long GetMonotonicMilliseconds()
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

bool b_SetEnvironmentVariable(std::string_view name, std::string_view value)
{
    return ::setenv(std::string(name).c_str(), std::string(value).c_str(), 1) == 0;
}

bool b_CreateFileExclusive(const std::filesystem::path& path, std::string_view contents, std::string& out_error)
{
    out_error.clear();
    const int fd = ::open(path.c_str(), O_CREAT | O_EXCL | O_WRONLY, 0644);
    if (fd < 0)
    {
        out_error = "Could not create " + path.string() + ": " + std::strerror(errno);
        return false;
    }

    size_t offset = 0;
    while (offset < contents.size())
    {
        const ssize_t written = ::write(fd, contents.data() + offset, contents.size() - offset);
        if (written < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            ::close(fd);
            std::error_code ignored;
            std::filesystem::remove(path, ignored);
            out_error = "Could not write " + path.string() + ": " + std::strerror(errno);
            return false;
        }
        offset += static_cast<size_t>(written);
    }

    if (::close(fd) != 0)
    {
        std::error_code ignored;
        std::filesystem::remove(path, ignored);
        out_error = "Could not close " + path.string() + ": " + std::strerror(errno);
        return false;
    }
    return true;
}
} // namespace PlatformInfo

#endif
