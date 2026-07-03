#pragma once
#include <string>
#include <vector>
#include <chrono>
#include <unordered_map>
#include <array>

#ifdef RAYWAVES_PROFILER_DISABLED

#define SCOPED_TIMER(name) ((void)0)

struct ProfilerSnapshot
{
    std::string m_Name;
    double m_AvgMs = 0.0;
    double m_MaxMs = 0.0;
    double m_LastMs = 0.0;
};

class Profiler
{
public:
    static Profiler& Get() { static Profiler p; return p; }
    void Record(const char*, uint64_t) {}
    void NextFrame() {}
    std::vector<ProfilerSnapshot> GetAverages() const { return {}; }
    bool SaveToFile(const std::string&) const { return false; }
private:
    Profiler() = default;
};

class ScopedTimer
{
public:
    ScopedTimer(const char*) {}
};

#else

#define PROFILER_CONCAT_IMPL(a, b) a##b
#define PROFILER_CONCAT(a, b) PROFILER_CONCAT_IMPL(a, b)
#define SCOPED_TIMER(name) ScopedTimer PROFILER_CONCAT(scoped_timer_, __LINE__)(name)

struct ProfilerSnapshot
{
    std::string m_Name;
    double m_AvgMs;
    double m_MaxMs;
    double m_LastMs;
};

class Profiler
{
public:
    static Profiler& Get();

    void Record(const char* name, uint64_t us);
    void NextFrame();

    std::vector<ProfilerSnapshot> GetAverages() const;
    bool SaveToFile(const std::string& path) const;

private:
    static constexpr size_t k_FrameCount = 120;

    struct FrameData
    {
        std::unordered_map<std::string, uint64_t> m_NameToUs;
    };

    std::array<FrameData, k_FrameCount> m_Frames;
    size_t m_CurrentFrame = 0;
    size_t m_FramesRecorded = 0;

    Profiler() = default;
    Profiler(const Profiler&) = delete;
    Profiler& operator=(const Profiler&) = delete;
};

class ScopedTimer
{
    const char* m_Name;
    std::chrono::steady_clock::time_point m_Start;
public:
    ScopedTimer(const char* name)
        : m_Name(name)
        , m_Start(std::chrono::steady_clock::now())
    {}
    ~ScopedTimer()
    {
        auto us = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - m_Start).count();
        Profiler::Get().Record(m_Name, us);
    }
};

#endif
