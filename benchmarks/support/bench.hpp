#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace bench
{
    struct Case
    {
        std::string name;
        std::function<void()> body;
        int64_t ops_per_call = 1;
    };

    struct Result
    {
        std::string name;
        double ns_per_op = 0.0;
        double ops_per_sec = 0.0;
        double total_ms = 0.0;
        int64_t ops_per_call = 1;
    };

    struct Options
    {
        std::string filter;
        std::string json_path;
        std::string baseline_path;
        int iterations = 2000;
        int warmup = 200;
    };

    std::vector<Case>& Registry();

    struct Registrar
    {
        Registrar(std::string name, std::function<void()> body, int64_t ops_per_call);
    };

    Result RunCase(const Case& test_case, int warmup, int iterations);
    std::vector<Result> RunAll(const Options& options);
    bool WriteJson(const std::vector<Result>& results, const std::string& path);
    bool CheckBaseline(const std::vector<Result>& results, const std::string& path,
                       double max_regression_percent);
}

#define BENCHMARK_CAT_IMPL(a, b) a##b
#define BENCHMARK_CAT(a, b) BENCHMARK_CAT_IMPL(a, b)

// Each body should perform `ops_per_call` operations so the harness can report
// per-operation cost without measuring the loop/function-call overhead.
#define BENCHMARK_CASE(name, ops_per_call)                              \
    static void BENCHMARK_CAT(bench_body_, __LINE__)();                 \
    static ::bench::Registrar BENCHMARK_CAT(bench_reg_, __LINE__)(      \
        name, &BENCHMARK_CAT(bench_body_, __LINE__), ops_per_call);     \
    static void BENCHMARK_CAT(bench_body_, __LINE__)()
