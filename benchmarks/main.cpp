#include "support/bench.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

namespace bench
{
    std::vector<Case>& Registry()
    {
        static std::vector<Case> cases;
        return cases;
    }

    Registrar::Registrar(std::string name, std::function<void()> body, int64_t ops_per_call)
    {
        Registry().push_back({std::move(name), std::move(body), ops_per_call});
    }

    Result RunCase(const Case& test_case, int warmup, int iterations)
    {
        for (int i = 0; i < warmup; ++i)
        {
            test_case.body();
        }

        auto start = std::chrono::steady_clock::now();
        for (int i = 0; i < iterations; ++i)
        {
            test_case.body();
        }
        auto end = std::chrono::steady_clock::now();

        const double elapsed_ns =
            static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());
        const double total_ops = static_cast<double>(iterations) * static_cast<double>(test_case.ops_per_call);
        const double elapsed_seconds = elapsed_ns / 1'000'000'000.0;

        Result result;
        result.name = test_case.name;
        result.ns_per_op = total_ops > 0.0 ? elapsed_ns / total_ops : 0.0;
        result.ops_per_sec = elapsed_seconds > 0.0 ? total_ops / elapsed_seconds : 0.0;
        result.total_ms = elapsed_ns / 1'000'000.0;
        result.ops_per_call = test_case.ops_per_call;
        return result;
    }

    std::vector<Result> RunAll(const Options& options)
    {
        std::vector<Result> results;
        for (const auto& test_case : Registry())
        {
            if (!options.filter.empty()
                && test_case.name.find(options.filter) == std::string::npos)
            {
                continue;
            }

            results.push_back(RunCase(test_case, options.warmup, options.iterations));
            const Result& result = results.back();
            std::printf("%-42s %12.2f ns/op  %14.0f ops/s  %8.2f ms\n",
                        result.name.c_str(), result.ns_per_op, result.ops_per_sec,
                        result.total_ms);
        }
        return results;
    }

    bool WriteJson(const std::vector<Result>& results, const std::string& path)
    {
        const std::filesystem::path output_path(path);
        if (output_path.has_parent_path())
        {
            std::filesystem::create_directories(output_path.parent_path());
        }

        std::ofstream out(path);
        if (!out.is_open())
        {
            return false;
        }

        out << "{\n  \"results\": {\n";
        for (size_t i = 0; i < results.size(); ++i)
        {
            const Result& result = results[i];
            out << "    \"" << result.name << "\": {"
                << "\"ns_per_op\": " << result.ns_per_op << ", "
                << "\"ops_per_sec\": " << result.ops_per_sec
                << "}";
            if (i + 1 < results.size())
            {
                out << ",";
            }
            out << "\n";
        }
        out << "  }\n}\n";
        return true;
    }

    namespace
    {
        // Deliberately tiny parser for the flat JSON shape WriteJson emits.
        // Avoids a JSON dependency for a developer-only regression check.
        bool ReadBaselineValue(const std::string& content, const std::string& name, double& value)
        {
            const std::string name_key = "\"" + name + "\"";
            size_t name_pos = content.find(name_key);
            if (name_pos == std::string::npos)
            {
                return false;
            }

            size_t value_pos = content.find("\"ns_per_op\"", name_pos);
            if (value_pos == std::string::npos)
            {
                return false;
            }

            value_pos = content.find(':', value_pos);
            if (value_pos == std::string::npos)
            {
                return false;
            }

            std::istringstream stream(content.substr(value_pos + 1));
            stream >> value;
            return stream.good();
        }
    }

    bool CheckBaseline(const std::vector<Result>& results, const std::string& path,
                       double max_regression_percent)
    {
        std::ifstream in(path);
        if (!in.is_open())
        {
            std::cerr << "Cannot open baseline: " << path << '\n';
            return false;
        }

        std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        bool ok = true;
        for (const auto& result : results)
        {
            double baseline = 0.0;
            if (!ReadBaselineValue(content, result.name, baseline))
            {
                std::cerr << "No baseline entry for: " << result.name << '\n';
                ok = false;
                continue;
            }

            const double delta_percent = baseline > 0.0
                ? ((result.ns_per_op - baseline) / baseline) * 100.0
                : 0.0;
            if (delta_percent > max_regression_percent)
            {
                std::cerr << "REGRESSION " << result.name << ": "
                          << delta_percent << "% slower than baseline\n";
                ok = false;
            }
        }
        return ok;
    }
}

namespace
{
    void PrintUsage()
    {
        std::cout
            << "Usage: raywaves_benchmarks [options]\n"
            << "  --filter <text>        Run only cases whose name contains text\n"
            << "  --iterations <n>       Timed iterations per case (default 2000)\n"
            << "  --warmup <n>           Warm-up iterations per case (default 200)\n"
            << "  --json <path>          Write results as JSON\n"
            << "  --check <path>         Compare against a JSON baseline\n"
            << "  --max-regression <pct> Allowed slowdown for --check (default 10)\n";
    }
}

int main(int argc, char** argv)
{
    bench::Options options;
    double max_regression = 10.0;

    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];
        auto next = [&](const char* what) -> std::string
        {
            if (i + 1 >= argc)
            {
                std::cerr << "Missing value for " << what << '\n';
                std::exit(2);
            }
            return argv[++i];
        };

        if (arg == "--filter")
        {
            options.filter = next("--filter");
        }
        else if (arg == "--iterations")
        {
            options.iterations = std::max(1, std::atoi(next("--iterations").c_str()));
        }
        else if (arg == "--warmup")
        {
            options.warmup = std::max(0, std::atoi(next("--warmup").c_str()));
        }
        else if (arg == "--json")
        {
            options.json_path = next("--json");
        }
        else if (arg == "--check")
        {
            options.baseline_path = next("--check");
        }
        else if (arg == "--max-regression")
        {
            max_regression = std::atof(next("--max-regression").c_str());
        }
        else if (arg == "--help" || arg == "-h")
        {
            PrintUsage();
            return 0;
        }
        else
        {
            std::cerr << "Unknown option: " << arg << '\n';
            PrintUsage();
            return 2;
        }
    }

    const auto results = bench::RunAll(options);
    if (results.empty())
    {
        std::cerr << "No benchmark cases matched.\n";
        return 1;
    }

    if (!options.json_path.empty() && !bench::WriteJson(results, options.json_path))
    {
        std::cerr << "Failed to write " << options.json_path << '\n';
        return 1;
    }

    if (!options.baseline_path.empty()
        && !bench::CheckBaseline(results, options.baseline_path, max_regression))
    {
        return 1;
    }

    return 0;
}
