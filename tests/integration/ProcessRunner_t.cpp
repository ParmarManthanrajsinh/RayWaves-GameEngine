#include "ProcessRunner.h"
#include "doctest/doctest.h"
#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <vector>

TEST_CASE("ProcessRunner: RunAndWait streams lines and reports success")
{
    std::vector<std::string> lines;
#if defined(_WIN32)
    // No space before '&': cmd keeps trailing spaces in echo output.
    const char* command = "echo hello&echo world";
#else
    const char* command = "printf 'hello\\nworld\\n'";
#endif
    const bool ok =
        ProcessRunner::RunAndWait(command, [&lines](std::string_view line, bool) { lines.emplace_back(line); });

    CHECK(ok);
    REQUIRE(lines.size() == 2);
    CHECK(lines[0] == "hello");
    CHECK(lines[1] == "world");
}

TEST_CASE("ProcessRunner: RunAndWait reports a non-zero exit as failure")
{
#if defined(_WIN32)
    CHECK_FALSE(ProcessRunner::RunAndWait("exit /b 3"));
#else
    CHECK_FALSE(ProcessRunner::RunAndWait("exit 3"));
#endif
}

TEST_CASE("ProcessRunner: output written just before exit is not truncated")
{
    // Roughly 9 KB across 300 lines. The earlier reader stopped after one
    // 128-byte read once the child exited, which dropped most of this.
    std::vector<std::string> lines;
#if defined(_WIN32)
    const char* command = "for /L %i in (1,1,300) do @echo line_%i_xxxxxxxxxxxxxxxxxxxxxxxxxxxx";
#else
    const char* command = "i=1; while [ $i -le 300 ]; do echo \"line_${i}_xxxxxxxxxxxxxxxxxxxxxxxxxxxx\"; "
                          "i=$((i+1)); done";
#endif
    const bool ok =
        ProcessRunner::RunAndWait(command, [&lines](std::string_view line, bool) { lines.emplace_back(line); });

    CHECK(ok);
    REQUIRE(lines.size() == 300);
    CHECK(lines.front() == "line_1_xxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    CHECK(lines.back() == "line_300_xxxxxxxxxxxxxxxxxxxxxxxxxxxx");
}

TEST_CASE("ProcessRunner: cancel stops a long-running command promptly")
{
    auto cancel = std::make_shared<std::atomic<bool>>(false);
    bool completed = false;
    bool success = true;

#if defined(_WIN32)
    const char* long_command = "ping -n 60 127.0.0.1 > nul";
#else
    const char* long_command = "sleep 60";
#endif
    std::thread worker = ProcessRunner::RunBuildCommand(
        long_command, nullptr,
        [&](bool ok)
        {
            completed = true;
            success = ok;
        },
        cancel);

    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    const auto requested = std::chrono::steady_clock::now();
    cancel->store(true);
    worker.join();
    const auto elapsed = std::chrono::steady_clock::now() - requested;

    CHECK(completed);
    CHECK_FALSE(success);
    CHECK(elapsed < std::chrono::seconds(3));
}
