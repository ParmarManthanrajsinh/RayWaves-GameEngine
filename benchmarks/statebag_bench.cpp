#include "support/bench.hpp"

#include "GameState.h"

namespace
{
    constexpr int k_Keys = 100;
    constexpr int k_OpsPerCall = 200;
}

BENCHMARK_CASE("StateBag GetFloat + GetInt", k_OpsPerCall)
{
    static StateBag bag;
    static bool b_Initialized = false;
    if (!b_Initialized)
    {
        for (int i = 0; i < k_Keys; ++i)
        {
            const std::string key = "key_" + std::to_string(i);
            bag.SetFloat(key, static_cast<float>(i));
            bag.SetInt(key, i);
        }
        b_Initialized = true;
    }

    volatile float sum = 0.0f;
    for (int i = 0; i < k_Keys; ++i)
    {
        const std::string key = "key_" + std::to_string(i);
        sum += bag.GetFloat(key);
        sum += static_cast<float>(bag.GetInt(key));
    }
    (void)sum;
}

BENCHMARK_CASE("StateBag GetVector2", 1000)
{
    static StateBag bag;
    static bool b_Initialized = false;
    if (!b_Initialized)
    {
        bag.SetVector2("player_pos", {100.0f, 200.0f});
        b_Initialized = true;
    }

    Vector2 value{};
    for (int i = 0; i < 1000; ++i)
    {
        value = bag.GetVector2("player_pos");
    }
    volatile float sink = value.x + value.y;
    (void)sink;
}
