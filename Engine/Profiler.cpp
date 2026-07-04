#include "Profiler.h"

#ifndef RAYWAVES_PROFILER_DISABLED

#include <algorithm>
#include <fstream>
#include <iostream>
#include <set>

Profiler& Profiler::Get()
{
    static Profiler instance;
    return instance;
}

void Profiler::Record(const char* name, uint64_t us)
{
    auto& map = m_Frames[m_CurrentFrame].m_NameToUs;
    auto [it, inserted] = map.try_emplace(name, 0);
    it->second += us;
}

void Profiler::NextFrame()
{
    m_CurrentFrame = (m_CurrentFrame + 1) % k_FrameCount;
    m_Frames[m_CurrentFrame].m_NameToUs.clear();
    if (m_FramesRecorded < k_FrameCount) ++m_FramesRecorded;
}

std::vector<ProfilerSnapshot> Profiler::GetAverages() const
{
    size_t count = std::min(m_FramesRecorded, k_FrameCount);
    size_t oldest = (m_FramesRecorded < k_FrameCount) ? 0 : (m_CurrentFrame + 1) % k_FrameCount;

    std::set<std::string> all_names;
    for (size_t j = 0; j < count; ++j)
    {
        size_t idx = (oldest + j) % k_FrameCount;
        for (const auto& pair : m_Frames[idx].m_NameToUs)
        {
            all_names.insert(pair.first);
        }
    }

    std::vector<ProfilerSnapshot> result;
    for (const auto& name : all_names)
    {
        double total = 0.0;
        double max_val = 0.0;
        double last = 0.0;
        for (size_t j = 0; j < count; ++j)
        {
            size_t idx = (oldest + j) % k_FrameCount;
            auto it = m_Frames[idx].m_NameToUs.find(name);
            if (it != m_Frames[idx].m_NameToUs.end())
            {
                double ms = it->second / 1000.0;
                total += ms;
                max_val = std::max(max_val, ms);
                last = ms;
            }
        }
        double avg = (count > 0) ? total / count : 0.0;
        result.push_back({name, avg, max_val, last});
    }
    return result;
}

bool Profiler::SaveToFile(const std::string& path) const
{
    std::ofstream file(path);
    if (!file.is_open()) return false;

    size_t count = std::min(m_FramesRecorded, k_FrameCount);
    size_t oldest = (m_FramesRecorded < k_FrameCount) ? 0 : (m_CurrentFrame + 1) % k_FrameCount;

    std::set<std::string> all_names;
    for (size_t j = 0; j < count; ++j)
    {
        size_t idx = (oldest + j) % k_FrameCount;
        for (const auto& pair : m_Frames[idx].m_NameToUs)
        {
            all_names.insert(pair.first);
        }
    }

    file << "frame";
    for (const auto& name : all_names) file << "," << name << "_us";
    file << "\n";

    for (size_t j = 0; j < count; ++j)
    {
        size_t idx = (oldest + j) % k_FrameCount;
        file << j;
        for (const auto& name : all_names)
        {
            auto it = m_Frames[idx].m_NameToUs.find(name);
            file << "," << (it != m_Frames[idx].m_NameToUs.end() ? it->second : 0);
        }
        file << "\n";
    }

    return true;
}

#endif
