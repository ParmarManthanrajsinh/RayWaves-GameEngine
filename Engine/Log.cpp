#include "Log.h"
#include <iostream>
#include <mutex>
#include <utility>

namespace
{
std::mutex s_SinkMutex;
Log::FSink s_Sink;
Log::ELevel s_MinimumLevel = Log::ELevel::Info;

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
void s_DefaultSink(Log::ELevel, std::string_view category, std::string_view message)
{
    std::cerr << '[' << category << "] " << message << '\n';
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
void s_Emit(Log::ELevel level, std::string_view category, std::string_view message)
{
    // Hold the lock across the sink call so a line stays whole and SetSink
    // cannot replace the sink while another thread is writing through it.
    const std::lock_guard<std::mutex> lock(s_SinkMutex);
    if (static_cast<int>(level) < static_cast<int>(s_MinimumLevel))
    {
        return;
    }
    if (s_Sink)
    {
        s_Sink(level, category, message);
        return;
    }
    s_DefaultSink(level, category, message);
}
} // namespace

void Log::SetSink(FSink sink)
{
    const std::lock_guard<std::mutex> lock(s_SinkMutex);
    s_Sink = std::move(sink);
}

void Log::SetLevel(ELevel minimum)
{
    const std::lock_guard<std::mutex> lock(s_SinkMutex);
    s_MinimumLevel = minimum;
}

void Log::Debug(std::string_view category, std::string_view message)
{
    s_Emit(ELevel::Debug, category, message);
}

void Log::Info(std::string_view category, std::string_view message)
{
    s_Emit(ELevel::Info, category, message);
}

void Log::Warn(std::string_view category, std::string_view message)
{
    s_Emit(ELevel::Warn, category, message);
}

void Log::Error(std::string_view category, std::string_view message)
{
    s_Emit(ELevel::Error, category, message);
}
