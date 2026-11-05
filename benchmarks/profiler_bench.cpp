#include "support/bench.hpp"

#include "Profiler.h"

BENCHMARK_CASE("Profiler::Record single name", 1000)
{
    for (int i = 0; i < 1000; ++i)
    {
        Profiler::Get().Record("test_timer", 42);
    }
}

BENCHMARK_CASE("Profiler::Record 100-name ring", 1000)
{
    static bool b_Seeded = false;
    if (!b_Seeded)
    {
        for (int i = 0; i < 100; ++i)
        {
            Profiler::Get().Record(("timer_" + std::to_string(i)).c_str(), i);
        }
        b_Seeded = true;
    }

    for (int i = 0; i < 1000; ++i)
    {
        Profiler::Get().Record(("timer_" + std::to_string(i % 100)).c_str(), 42);
    }
}
