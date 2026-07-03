#include "doctest/doctest.h"
#include "../Engine/Profiler.h"
#include "../Engine/GameState.h"
#include "../Engine/AssetResolver.h"
#include <chrono>
#include <iostream>

static constexpr int k_Iterations = 100000;

TEST_CASE("PERF: StateBag get/set with string keys")
{
    StateBag bag;
    for (int i = 0; i < 100; ++i)
    {
        std::string key = "key_" + std::to_string(i);
        bag.SetFloat(key, float(i));
        bag.SetInt(key, i);
        bag.SetBool(key, true);
        bag.SetString(key, key);
    }

    auto start = std::chrono::steady_clock::now();
    volatile float sum = 0;
    for (int iter = 0; iter < k_Iterations; ++iter)
    {
        for (int i = 0; i < 100; ++i)
        {
            std::string key = "key_" + std::to_string(i);
            sum += bag.GetFloat(key);
            sum += float(bag.GetInt(key));
        }
    }
    auto end = std::chrono::steady_clock::now();
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    std::cout << "[PERF] StateBag lookup " << k_Iterations * 100 << " times: " << us << " us" << std::endl;
    (void)sum;
}

TEST_CASE("PERF: Profiler::Record single name")
{
    Profiler::Get();
    auto start = std::chrono::steady_clock::now();
    for (int iter = 0; iter < k_Iterations; ++iter)
    {
        Profiler::Get().Record("test_timer", 42);
    }
    auto end = std::chrono::steady_clock::now();
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    std::cout << "[PERF] Profiler::Record " << k_Iterations << " times (single name): " << us << " us" << std::endl;
}

TEST_CASE("PERF: Profiler::Record 100 names ring")
{
    for (int iter = 0; iter < 100; ++iter)
        Profiler::Get().Record(("timer_" + std::to_string(iter)).c_str(), iter);

    auto start = std::chrono::steady_clock::now();
    for (int iter = 0; iter < k_Iterations; ++iter)
    {
        Profiler::Get().Record(("timer_" + std::to_string(iter % 100)).c_str(), 42);
    }
    auto end = std::chrono::steady_clock::now();
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    std::cout << "[PERF] Profiler::Record " << k_Iterations << " times (100 names ring): " << us << " us" << std::endl;
}

TEST_CASE("PERF: AssetResolver::Resolve")
{
    AssetResolver::SetProjectAssetPath("C:/MyGame/Assets");
    auto start = std::chrono::steady_clock::now();
    for (int iter = 0; iter < k_Iterations; ++iter)
    {
        volatile auto r = AssetResolver::Resolve("textures/player.png");
        (void)r;
    }
    auto end = std::chrono::steady_clock::now();
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    std::cout << "[PERF] AssetResolver::Resolve " << k_Iterations << " times: " << us << " us" << std::endl;
}

TEST_CASE("PERF: StateBag GetVector2 (two hash lookups)")
{
    StateBag bag;
    bag.SetVector2("player_pos", {100.0f, 200.0f});

    auto start = std::chrono::steady_clock::now();
    for (int iter = 0; iter < k_Iterations; ++iter)
    {
        volatile auto v = bag.GetVector2("player_pos");
        (void)v;
    }
    auto end = std::chrono::steady_clock::now();
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    std::cout << "[PERF] StateBag GetVector2 " << k_Iterations << " times: " << us << " us" << std::endl;
}
