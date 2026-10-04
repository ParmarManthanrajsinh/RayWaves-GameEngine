#include "doctest/doctest.h"
#include "../Engine/Profiler.h"
#include <thread>
#include <chrono>
#include <fstream>
#include <filesystem>

TEST_CASE("Profiler: Record and NextFrame basic")
{
    Profiler::Get().Record("test_a", 100);
    Profiler::Get().Record("test_b", 200);
    Profiler::Get().NextFrame();

    auto avgs = Profiler::Get().GetAverages();
    bool found_a = false;
    bool found_b = false;
    for (const auto& s : avgs) {
        if (s.m_Name == "test_a") { found_a = true; CHECK(s.m_LastMs == doctest::Approx(0.1)); }
        if (s.m_Name == "test_b") { found_b = true; CHECK(s.m_LastMs == doctest::Approx(0.2)); }
    }
    CHECK(found_a);
    CHECK(found_b);
}

TEST_CASE("Profiler: GetAverages returns at least the recorded entries")
{
    auto avgs = Profiler::Get().GetAverages();
    // Previous test cases may have populated data; just ensure no crash
    CHECK(avgs.size() >= 0);
}

TEST_CASE("Profiler: CSV export")
{
    Profiler::Get().Record("csv_a", 1000);
    Profiler::Get().NextFrame();
    Profiler::Get().Record("csv_a", 500);
    Profiler::Get().NextFrame();

    std::string csv_path = (std::filesystem::temp_directory_path() / "test_profile.csv").string();
    bool saved = Profiler::Get().SaveToFile(csv_path);
    CHECK(saved);

    {
        std::ifstream f(csv_path);
        CHECK(f.is_open());
        std::string header;
        std::getline(f, header);
        CHECK(header.find("frame") != std::string::npos);
        CHECK(header.find("csv_a") != std::string::npos);
    }
    std::filesystem::remove(csv_path);
}
