#include "AgentLock.h"
#include "PlatformInfo.h"
#include "doctest/doctest.h"
#include "support/TempDir.h"
#include <fstream>

namespace fs = std::filesystem;

TEST_CASE("agent lock acquires and releases")
{
    TempDir temp("raywaves_lock");
    std::string error;

    REQUIRE(AgentLock::b_Acquire(temp.Path(), "build", error));
    CHECK(error.empty());

    AgentLock::FInfo info;
    CHECK(AgentLock::b_Query(temp.Path(), info));
    CHECK(info.m_bHeld);
    CHECK(info.m_Pid == PlatformInfo::GetCurrentProcessId());

    CHECK_FALSE(AgentLock::b_Acquire(temp.Path(), "build", error));
    CHECK(error.find("locked") != std::string::npos);

    AgentLock::Release(temp.Path());
    CHECK_FALSE(AgentLock::b_Query(temp.Path(), info));
}

TEST_CASE("stale agent lock is reclaimed")
{
    TempDir temp("raywaves_lock_stale");
    const fs::path lock_path = AgentLock::GetLockPath(temp.Path());
    fs::create_directories(lock_path.parent_path());

    {
        std::ofstream file(lock_path);
        file << "pid=999999\ncommand=stale\nstarted=0\n";
    }

    AgentLock::FInfo info;
    CHECK_FALSE(AgentLock::b_Query(temp.Path(), info));
    CHECK_FALSE(fs::exists(lock_path));

    std::string error;
    REQUIRE(AgentLock::b_Acquire(temp.Path(), "build", error));
    AgentLock::Release(temp.Path());
}
