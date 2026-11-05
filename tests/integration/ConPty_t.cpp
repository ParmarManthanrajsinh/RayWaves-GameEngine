#include "doctest/doctest.h"

#include "terminal/pty_session.h"
#include "terminal/shell_detect.h"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>

TEST_CASE("ConPTY: starts a shell and streams output")
{
    if (!term::t_PtySession::b_IsSupported())
    {
        MESSAGE("ConPTY is not available on this system; skipping.");
        return;
    }

    const term::t_ShellSpec shell = term::DefaultShell();
    REQUIRE_FALSE(shell.executable.empty());

    std::mutex output_mutex;
    std::string output;
    std::atomic<bool> found{ false };

    term::t_PtySession session;
    const bool b_started = session.b_Start(shell, std::filesystem::current_path().wstring(), 80, 24,
        [&](std::string_view bytes)
        {
            std::scoped_lock lock(output_mutex);
            output.append(bytes);
            if (output.find("RAYWAVES_CONPTY_OK") != std::string::npos)
            {
                found.store(true);
            }
        });

    REQUIRE_MESSAGE(b_started, session.LastError());

    std::this_thread::sleep_for(std::chrono::milliseconds(250));
    session.Write("echo RAYWAVES_CONPTY_OK\r");

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (!found.load() && std::chrono::steady_clock::now() < deadline)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }

    session.Stop();

    std::string captured;
    {
        std::scoped_lock lock(output_mutex);
        captured = output;
    }

    CHECK_MESSAGE(found.load(), "ConPTY output was: ", captured, "\nLast error: ", session.LastError());
}
