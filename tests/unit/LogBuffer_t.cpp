#include "LogBuffer.h"
#include "doctest/doctest.h"

#include <string>
#include <thread>
#include <vector>

TEST_CASE("LogBuffer: Add preserves order and copies fields")
{
    LogBuffer buffer(8);
    buffer.Add(Log::ELevel::Info, "Alpha", "first");
    buffer.Add(Log::ELevel::Warn, "Beta", "second");

    REQUIRE(buffer.Count() == 2);
    const std::vector<FLogEntry> entries = buffer.Snapshot();
    REQUIRE(entries.size() == 2);

    CHECK(entries[0].m_Level == Log::ELevel::Info);
    CHECK(entries[0].m_Category == "Alpha");
    CHECK(entries[0].m_Message == "first");

    CHECK(entries[1].m_Level == Log::ELevel::Warn);
    CHECK(entries[1].m_Category == "Beta");
    CHECK(entries[1].m_Message == "second");
}

TEST_CASE("LogBuffer: Snapshot is a copy, not a live view")
{
    LogBuffer buffer(8);
    buffer.Add(Log::ELevel::Info, "Copy", "before");

    std::vector<FLogEntry> snapshot = buffer.Snapshot();
    buffer.Add(Log::ELevel::Error, "Copy", "after");

    REQUIRE(snapshot.size() == 1);
    CHECK(snapshot[0].m_Message == "before");
    CHECK(buffer.Count() == 2);
}

TEST_CASE("LogBuffer: capacity evicts the oldest entries")
{
    LogBuffer buffer(3);
    for (int index = 0; index < 5; ++index)
    {
        buffer.Add(Log::ELevel::Debug, "Capacity", std::to_string(index));
    }

    CHECK(buffer.Count() == 3);
    const std::vector<FLogEntry> entries = buffer.Snapshot();
    REQUIRE(entries.size() == 3);
    CHECK(entries[0].m_Message == "2");
    CHECK(entries[1].m_Message == "3");
    CHECK(entries[2].m_Message == "4");
}

TEST_CASE("LogBuffer: Clear empties the buffer")
{
    LogBuffer buffer(4);
    buffer.Add(Log::ELevel::Info, "Clear", "one");
    buffer.Add(Log::ELevel::Info, "Clear", "two");

    buffer.Clear();

    CHECK(buffer.Count() == 0);
    CHECK(buffer.Snapshot().empty());

    buffer.Add(Log::ELevel::Info, "Clear", "after");
    CHECK(buffer.Count() == 1);
}

TEST_CASE("LogBuffer: concurrent writers are serialized")
{
    LogBuffer buffer(4096);
    constexpr int c_ENTRIES_PER_THREAD = 500;

    auto writer = [&buffer](int thread_index)
    {
        for (int index = 0; index < c_ENTRIES_PER_THREAD; ++index)
        {
            buffer.Add(Log::ELevel::Info, "Thread", std::to_string(thread_index) + ":" + std::to_string(index));
        }
    };

    std::thread first(writer, 0);
    std::thread second(writer, 1);
    first.join();
    second.join();

    CHECK(buffer.Count() == 2 * c_ENTRIES_PER_THREAD);
    const std::vector<FLogEntry> entries = buffer.Snapshot();
    CHECK(entries.size() == buffer.Count());
}
