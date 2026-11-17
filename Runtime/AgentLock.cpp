#include "AgentLock.h"
#include <cstdlib>
#include <fstream>
#include <iterator>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace AgentLock
{
namespace
{
std::string Trim(std::string_view value)
{
    const size_t start = value.find_first_not_of(" \t\r\n");
    if (start == std::string_view::npos)
    {
        return {};
    }
    const size_t end = value.find_last_not_of(" \t\r\n");
    return std::string(value.substr(start, (end - start) + 1));
}

std::string ReadValue(std::string_view text, std::string_view key)
{
    const std::string prefix = std::string(key) + "=";
    size_t position = 0;
    while (position < text.size())
    {
        const size_t end = text.find('\n', position);
        const std::string_view line =
            text.substr(position, (end == std::string_view::npos ? text.size() : end) - position);
        if (line.starts_with(prefix))
        {
            return Trim(line.substr(prefix.size()));
        }
        if (end == std::string_view::npos)
        {
            break;
        }
        position = end + 1;
    }
    return {};
}
} // namespace

std::filesystem::path GetLockPath(const std::filesystem::path& project_root)
{
    return project_root / ".raywaves" / "agent.lock";
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

bool b_Query(const std::filesystem::path& project_root, FInfo& out_info)
{
    out_info = {};
    const std::filesystem::path lock_path = GetLockPath(project_root);

    std::error_code error;
    if (!std::filesystem::exists(lock_path, error))
    {
        return false;
    }

    std::ifstream file(lock_path);
    if (!file.is_open())
    {
        return false;
    }

    const std::string text((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();
    const std::string pid_text = ReadValue(text, "pid");
    out_info.m_Pid = static_cast<int>(std::strtol(pid_text.c_str(), nullptr, 10));
    out_info.m_Command = ReadValue(text, "command");
    out_info.m_StartedAt = ReadValue(text, "started");

    if (out_info.m_Pid == 0 || !b_IsProcessAlive(out_info.m_Pid))
    {
        // Owner died: reclaim silently instead of blocking the next agent run.
        std::filesystem::remove(lock_path, error);
        out_info = {};
        return false;
    }

    out_info.m_bHeld = true;
    return true;
}

bool b_Acquire(const std::filesystem::path& project_root, std::string_view command, std::string& out_error)
{
    out_error.clear();

    FInfo existing;
    if (b_Query(project_root, existing))
    {
        out_error = "Project is locked by pid " + std::to_string(existing.m_Pid);
        if (!existing.m_Command.empty())
        {
            out_error += " (" + existing.m_Command + ")";
        }
        return false;
    }

    const std::filesystem::path lock_path = GetLockPath(project_root);
    std::error_code error;
    std::filesystem::create_directories(lock_path.parent_path(), error);

    std::ofstream file(lock_path, std::ios::trunc);
    if (!file.is_open())
    {
        out_error = "Could not create " + lock_path.string();
        return false;
    }

    file << "pid=" << GetCurrentProcessId() << "\n";
    file << "command=" << command << "\n";
    file << "started=" << static_cast<unsigned long long>(GetTickCount64()) << "\n";
    file.close();

    return file.good();
}

void Release(const std::filesystem::path& project_root)
{
    std::error_code error;
    std::filesystem::remove(GetLockPath(project_root), error);
}

bool b_RemoveStale(const std::filesystem::path& project_root, std::string& out_error)
{
    out_error.clear();
    FInfo info;
    if (b_Query(project_root, info))
    {
        out_error = "Lock is held by a live process (pid " + std::to_string(info.m_Pid) + ")";
        return false;
    }

    std::error_code error;
    std::filesystem::remove(GetLockPath(project_root), error);
    return true;
}
} // namespace AgentLock
