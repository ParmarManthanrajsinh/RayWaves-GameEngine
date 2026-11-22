#include "LogBuffer.h"

LogBuffer::LogBuffer(size_t capacity) : m_Capacity(capacity)
{
}

void LogBuffer::Add(Log::ELevel level, std::string_view category, std::string_view message)
{
    // Build the entry outside the lock; only the append touches shared state.
    FLogEntry entry;
    entry.m_Level = level;
    entry.m_Category.assign(category);
    entry.m_Message.assign(message);
    entry.m_Time = std::chrono::system_clock::now();

    const std::scoped_lock lock(m_Mutex);
    m_Entries.push_back(std::move(entry));
    while (m_Entries.size() > m_Capacity)
    {
        m_Entries.pop_front();
    }
}

void LogBuffer::Clear()
{
    const std::scoped_lock lock(m_Mutex);
    m_Entries.clear();
}

size_t LogBuffer::Count() const
{
    const std::scoped_lock lock(m_Mutex);
    return m_Entries.size();
}

std::vector<FLogEntry> LogBuffer::Snapshot() const
{
    const std::scoped_lock lock(m_Mutex);
    return { m_Entries.begin(), m_Entries.end() };
}
