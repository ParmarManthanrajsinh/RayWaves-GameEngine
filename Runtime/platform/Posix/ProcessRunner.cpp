#include "ProcessRunner.h"
#include "PlatformInfo.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <poll.h>
#include <string>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <utility>
#include <vector>

namespace ProcessRunner
{
namespace
{
constexpr int k_MaxChunksPerDrain = 64;
constexpr int k_MaxFinalDrains = 1024;
constexpr int k_PollIntervalMs = 10;

void CloseFd(int& fd)
{
    if (fd >= 0)
    {
        ::close(fd);
        fd = -1;
    }
}

bool SetNonBlocking(int fd)
{
    const int flags = fcntl(fd, F_GETFL, 0);
    return flags >= 0 && fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
}

// Turns pipe bytes into complete lines. Output is read until the pipe reports
// no data, so text written just before the child exits is still delivered.
class PipeReader
{
public:
    PipeReader(int pipe, std::function<void(std::string_view, bool)> on_output)
        : m_Pipe(pipe), m_OnOutput(std::move(on_output))
    {
    }

    bool Drain()
    {
        for (int chunk = 0; chunk < k_MaxChunksPerDrain; ++chunk)
        {
            const ssize_t bytes_read = ::read(m_Pipe, m_Buffer.data(), m_Buffer.size());
            if (bytes_read > 0)
            {
                m_Pending.append(m_Buffer.data(), static_cast<size_t>(bytes_read));
                EmitCompleteLines();
                continue;
            }
            if (bytes_read == 0)
            {
                return false; // EOF
            }
            if (errno == EINTR)
            {
                continue;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                return true;
            }
            return false;
        }
        return true;
    }

    void Flush()
    {
        if (!m_Pending.empty() && m_OnOutput)
        {
            m_OnOutput(m_Pending, false);
        }
        m_Pending.clear();
    }

private:
    void EmitCompleteLines()
    {
        size_t start = 0;
        size_t newline = m_Pending.find('\n');
        while (newline != std::string::npos)
        {
            std::string_view line(m_Pending.data() + start, newline - start);
            if (!line.empty() && line.back() == '\r')
            {
                line.remove_suffix(1);
            }
            if (m_OnOutput)
            {
                m_OnOutput(line, false);
            }
            start = newline + 1;
            newline = m_Pending.find('\n', start);
        }
        m_Pending.erase(0, start);
    }

    int m_Pipe;
    std::function<void(std::string_view, bool)> m_OnOutput;
    std::array<char, 4096> m_Buffer{};
    std::string m_Pending;
};

void RunProcessInternal(std::vector<std::string> argv, std::string working_dir,
    const std::function<void(std::string_view, bool)>& on_output, const std::function<void(bool, int)>& on_complete,
    const std::shared_ptr<std::atomic<bool>>& cancel)
{
    auto report_failure = [&](std::string_view message)
    {
        if (on_output)
        {
            on_output(message, true);
        }
        if (on_complete)
        {
            on_complete(false, -1);
        }
    };

    if (argv.empty())
    {
        report_failure("Empty command.");
        return;
    }

    int pipe_fds[2] = { -1, -1 };
#if defined(__linux__)
    const int pipe_result = ::pipe2(pipe_fds, O_CLOEXEC);
#else
    const int pipe_result = ::pipe(pipe_fds);
    if (pipe_result == 0)
    {
        // Keep unrelated children from inheriting the build pipe.
        fcntl(pipe_fds[0], F_SETFD, FD_CLOEXEC);
        fcntl(pipe_fds[1], F_SETFD, FD_CLOEXEC);
    }
#endif
    if (pipe_result != 0)
    {
        report_failure(std::string("Failed to create pipe: ") + std::strerror(errno));
        return;
    }

    // Resolve PATH before fork. execvp() may allocate and is not
    // async-signal-safe; execv() on a pre-resolved path is.
    std::string exec_path = argv[0];
    if (exec_path.find('/') == std::string::npos)
    {
        const std::filesystem::path resolved = PlatformInfo::FindExecutable(exec_path);
        if (!resolved.empty())
        {
            exec_path = resolved.string();
        }
    }

    std::vector<char*> exec_argv;
    exec_argv.reserve(argv.size() + 1);
    for (std::string& arg : argv)
    {
        exec_argv.push_back(arg.data());
    }
    exec_argv.push_back(nullptr);

    const pid_t pid = ::fork();
    if (pid < 0)
    {
        CloseFd(pipe_fds[0]);
        CloseFd(pipe_fds[1]);
        report_failure(std::string("Failed to fork: ") + std::strerror(errno));
        return;
    }

    if (pid == 0)
    {
        // Child: only async-signal-safe calls until execvp.
        ::setpgid(0, 0);
        if (!working_dir.empty())
        {
            if (::chdir(working_dir.c_str()) != 0)
            {
                _exit(126);
            }
        }
        if (::dup2(pipe_fds[1], STDOUT_FILENO) < 0 || ::dup2(pipe_fds[1], STDERR_FILENO) < 0)
        {
            _exit(126);
        }
        ::close(pipe_fds[0]);
        ::close(pipe_fds[1]);
        ::execv(exec_path.c_str(), exec_argv.data());
        _exit(127);
    }

    // Parent. Set the group here too so cancellation cannot race the child's
    // own setpgid() before it runs. EACCES means the child already exec'd.
    ::setpgid(pid, pid);
    CloseFd(pipe_fds[1]);
    int read_fd = pipe_fds[0];
    SetNonBlocking(read_fd);

    PipeReader reader(read_fd, on_output);
    bool b_pipe_open = true;
    bool b_cancelled = false;
    int status = 0;
    bool b_reaped = false;

    while (true)
    {
        if (cancel && cancel->load())
        {
            b_cancelled = true;
            ::kill(-pid, SIGTERM);
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(750);
            while (std::chrono::steady_clock::now() < deadline)
            {
                if (::waitpid(pid, &status, WNOHANG) == pid)
                {
                    b_reaped = true;
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            if (!b_reaped)
            {
                ::kill(-pid, SIGKILL);
            }
            break;
        }

        if (b_pipe_open)
        {
            pollfd fd{ read_fd, POLLIN | POLLHUP, 0 };
            const int ready = ::poll(&fd, 1, k_PollIntervalMs);
            if (ready > 0 && (fd.revents & (POLLIN | POLLHUP)) != 0)
            {
                b_pipe_open = reader.Drain();
            }
        }
        else
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(k_PollIntervalMs));
        }

        const pid_t result = ::waitpid(pid, &status, WNOHANG);
        if (result == pid)
        {
            b_reaped = true;
            break;
        }
    }

    // Drain whatever the child wrote last; compiler errors usually live here.
    if (b_pipe_open)
    {
        for (int i = 0; i < k_MaxFinalDrains && reader.Drain(); ++i)
        {
        }
    }
    reader.Flush();

    if (!b_reaped)
    {
        while (::waitpid(pid, &status, 0) < 0 && errno == EINTR)
        {
        }
    }

    int exit_code = -1;
    if (WIFEXITED(status))
    {
        exit_code = WEXITSTATUS(status);
    }
    else if (WIFSIGNALED(status))
    {
        exit_code = 128 + WTERMSIG(status);
    }

    CloseFd(read_fd);

    if (on_complete)
    {
        on_complete(!b_cancelled && exit_code == 0, exit_code);
    }
}
} // namespace

std::thread RunBuildCommand(std::string_view cmd, const std::function<void(std::string_view, bool)>& on_output,
    const std::function<void(bool)>& on_complete, const std::shared_ptr<std::atomic<bool>>& cancel)
{
    std::vector<std::string> argv = { "/bin/sh", "-c", std::string(cmd) };
    return std::thread(
        [argv = std::move(argv), on_output, on_complete, cancel]()
        {
            RunProcessInternal(
                argv, {}, on_output, [on_complete](bool b_success, int) { on_complete(b_success); }, cancel);
        });
}

std::thread RunCommandLine(std::string_view command_line, std::string_view working_dir,
    const std::function<void(std::string_view, bool)>& on_output, const std::function<void(bool)>& on_complete,
    const std::shared_ptr<std::atomic<bool>>& cancel)
{
    std::vector<std::string> argv = { "/bin/sh", "-c", std::string(command_line) };
    return std::thread(
        [argv = std::move(argv), working_dir = std::string(working_dir), on_output, on_complete, cancel]()
        {
            RunProcessInternal(
                argv, working_dir, on_output, [on_complete](bool b_success, int) { on_complete(b_success); }, cancel);
        });
}

std::thread RunCommand(std::vector<std::string> argv, std::string_view working_dir,
    const std::function<void(std::string_view, bool)>& on_output, const std::function<void(bool)>& on_complete,
    const std::shared_ptr<std::atomic<bool>>& cancel)
{
    return std::thread(
        [argv = std::move(argv), working_dir = std::string(working_dir), on_output, on_complete, cancel]()
        {
            RunProcessInternal(
                argv, working_dir, on_output, [on_complete](bool b_success, int) { on_complete(b_success); }, cancel);
        });
}

bool RunAndWait(std::string_view cmd, const std::function<void(std::string_view, bool)>& on_output,
    const std::shared_ptr<std::atomic<bool>>& cancel)
{
    bool b_success = false;
    std::thread worker = RunBuildCommand(cmd, on_output, [&b_success](bool success) { b_success = success; }, cancel);
    worker.join();
    return b_success;
}

bool RunAndWaitCommandLine(std::string_view command_line, std::string_view working_dir,
    const std::function<void(std::string_view, bool)>& on_output, const std::shared_ptr<std::atomic<bool>>& cancel,
    int* out_exit_code)
{
    std::vector<std::string> argv = { "/bin/sh", "-c", std::string(command_line) };
    bool b_success = false;
    RunProcessInternal(
        argv, std::string(working_dir), on_output,
        [&b_success, out_exit_code](bool b_ok, int exit_code)
        {
            b_success = b_ok;
            if (out_exit_code != nullptr)
            {
                *out_exit_code = exit_code;
            }
        },
        cancel);
    return b_success;
}

bool RunAndWaitCommand(std::vector<std::string> argv, std::string_view working_dir,
    const std::function<void(std::string_view, bool)>& on_output, const std::shared_ptr<std::atomic<bool>>& cancel,
    int* out_exit_code)
{
    bool b_success = false;
    RunProcessInternal(
        std::move(argv), std::string(working_dir), on_output,
        [&b_success, out_exit_code](bool b_ok, int exit_code)
        {
            b_success = b_ok;
            if (out_exit_code != nullptr)
            {
                *out_exit_code = exit_code;
            }
        },
        cancel);
    return b_success;
}

std::string QuoteArg(std::string_view arg)
{
    if (arg.empty())
    {
        return "''";
    }

    const bool b_needs_quotes = arg.find_first_of(" \t\n\v\"'\\$`&|;<>(){}[]*?!#~") != std::string_view::npos;
    if (!b_needs_quotes)
    {
        return std::string(arg);
    }

    std::string quoted = "'";
    for (const char character : arg)
    {
        if (character == '\'')
        {
            quoted += "'\\''";
        }
        else
        {
            quoted += character;
        }
    }
    quoted += '\'';
    return quoted;
}
} // namespace ProcessRunner
