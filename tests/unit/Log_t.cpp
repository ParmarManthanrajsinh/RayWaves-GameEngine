#include "Engine/Log.h"
#include "doctest/doctest.h"
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace
{
struct t_RecordedMessage
{
    Log::ELevel m_Level;
    std::string m_Category;
    std::string m_Message;
};

std::vector<t_RecordedMessage> s_Messages;

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
void s_CaptureSink(Log::ELevel level, std::string_view category, std::string_view message)
{
    s_Messages.push_back(t_RecordedMessage{ level, std::string(category), std::string(message) });
}

void s_ResetLog()
{
    s_Messages.clear();
    Log::SetSink({});
    Log::SetLevel(Log::ELevel::Info);
}

// Restores the engine-wide log state even when a check in the test fails.
struct FLogRestore
{
    FLogRestore() { s_ResetLog(); }
    ~FLogRestore() { s_ResetLog(); }
};
} // namespace

TEST_CASE("Log: sink receives level, category and message")
{
    FLogRestore restore;
    Log::SetLevel(Log::ELevel::Debug);
    Log::SetSink(s_CaptureSink);

    Log::Debug("TestCat", "debug message");
    Log::Info("TestCat", "info message");
    Log::Warn("TestCat", "warn message");
    Log::Error("TestCat", "error message");

    REQUIRE(s_Messages.size() == 4);
    CHECK(s_Messages[0].m_Level == Log::ELevel::Debug);
    CHECK(s_Messages[0].m_Category == "TestCat");
    CHECK(s_Messages[0].m_Message == "debug message");
    CHECK(s_Messages[1].m_Level == Log::ELevel::Info);
    CHECK(s_Messages[1].m_Message == "info message");
    CHECK(s_Messages[2].m_Level == Log::ELevel::Warn);
    CHECK(s_Messages[2].m_Message == "warn message");
    CHECK(s_Messages[3].m_Level == Log::ELevel::Error);
    CHECK(s_Messages[3].m_Message == "error message");
    CHECK(s_Messages[3].m_Category == "TestCat");
}

TEST_CASE("Log: minimum level filters lower severities")
{
    FLogRestore restore;
    Log::SetLevel(Log::ELevel::Warn);
    Log::SetSink(s_CaptureSink);

    Log::Debug("TestCat", "debug message");
    Log::Info("TestCat", "info message");
    Log::Warn("TestCat", "warn message");
    Log::Error("TestCat", "error message");

    REQUIRE(s_Messages.size() == 2);
    CHECK(s_Messages[0].m_Level == Log::ELevel::Warn);
    CHECK(s_Messages[1].m_Level == Log::ELevel::Error);
}

TEST_CASE("Log: debug chatter stays out at the default level")
{
    FLogRestore restore;
    Log::SetSink(s_CaptureSink);

    Log::Debug("TestCat", "debug message");
    Log::Info("TestCat", "info message");

    REQUIRE(s_Messages.size() == 1);
    CHECK(s_Messages[0].m_Level == Log::ELevel::Info);
}

TEST_CASE("Log: empty sink restores the default sink")
{
    FLogRestore restore;
    Log::SetSink(s_CaptureSink);

    Log::Info("TestCat", "captured message");
    REQUIRE(s_Messages.size() == 1);

    // Redirect stderr rather than only checking that control returns: the
    // default sink's line format is part of the contract.
    std::ostringstream default_output;
    std::streambuf* previous = std::cerr.rdbuf(default_output.rdbuf());
    Log::SetSink({});
    Log::Error("TestCat", "default sink message");
    std::cerr.rdbuf(previous);

    CHECK(s_Messages.size() == 1);
    CHECK(default_output.str() == "[TestCat] default sink message\n");
}
