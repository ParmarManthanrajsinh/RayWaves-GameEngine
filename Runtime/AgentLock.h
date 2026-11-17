#pragma once
#include <filesystem>
#include <string>
#include <string_view>

// Cross-process marker used to stop the editor and rwcli from linking the
// project GameLogic.dll at the same time. The lock file lives in
// <project>/.raywaves/agent.lock and is removed by the owner.
namespace AgentLock
{
struct FInfo
{
    int m_Pid = 0;
    std::string m_Command;
    std::string m_StartedAt;
    bool m_bHeld = false;
};

std::filesystem::path GetLockPath(const std::filesystem::path& project_root);
bool b_IsProcessAlive(int pid);

// True when another live process holds the lock. A lock owned by a dead
// process is removed and reported as not held.
bool b_Query(const std::filesystem::path& project_root, FInfo& out_info);

// Creates the lock for the current process. Fails when a live owner exists.
bool b_Acquire(const std::filesystem::path& project_root, std::string_view command, std::string& out_error);

void Release(const std::filesystem::path& project_root);

bool b_RemoveStale(const std::filesystem::path& project_root, std::string& out_error);
} // namespace AgentLock
