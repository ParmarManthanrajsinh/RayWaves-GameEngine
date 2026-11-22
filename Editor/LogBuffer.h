#pragma once

#include "../Engine/Log.h"
#include <chrono>
#include <cstddef>
#include <deque>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

struct FLogEntry
{
    Log::ELevel m_Level = Log::ELevel::Info;
    std::string m_Category;
    std::string m_Message;
    std::chrono::system_clock::time_point m_Time;
};

// Thread-safe ring of recent log lines. Log sinks may run on worker threads,
// while the Log panel reads snapshots on the UI thread, so every access is
// mutex-protected. The oldest entry is dropped once capacity is exceeded.
class LogBuffer
{
public:
    explicit LogBuffer(size_t capacity = 2000);

    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
    void Add(Log::ELevel level, std::string_view category, std::string_view message);
    void Clear();
    size_t Count() const;
    std::vector<FLogEntry> Snapshot() const;

private:
    mutable std::mutex m_Mutex;
    std::deque<FLogEntry> m_Entries;
    size_t m_Capacity;
};
