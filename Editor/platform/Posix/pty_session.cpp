#include "pty_session.h"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <mutex>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <termios.h>
#include <thread>
#include <unistd.h>
#include <vector>

namespace
{
void CloseFd(int& fd)
{
    if (fd >= 0)
    {
        ::close(fd);
        fd = -1;
    }
}

std::string ResolveExecutable(const std::string& name)
{
    if (name.empty() || name.find('/') != std::string::npos)
    {
        return name;
    }

    const char* path_env = std::getenv("PATH");
    if (path_env == nullptr)
    {
        return name;
    }

    const std::string paths(path_env);
    size_t start = 0;
    while (start <= paths.size())
    {
        const size_t end = paths.find(':', start);
        const std::string directory = paths.substr(start, (end == std::string::npos ? paths.size() : end) - start);
        const std::string candidate = (directory.empty() ? "." : directory) + "/" + name;
        if (::access(candidate.c_str(), X_OK) == 0)
        {
            return candidate;
        }
        if (end == std::string::npos)
        {
            break;
        }
        start = end + 1;
    }
    return name;
}

std::vector<std::string> SplitArguments(const std::string& text)
{
    std::vector<std::string> result;
    std::string current;
    bool b_in_single = false;
    bool b_in_double = false;
    for (const char character : text)
    {
        if (character == '\'' && !b_in_double)
        {
            b_in_single = !b_in_single;
            continue;
        }
        if (character == '"' && !b_in_single)
        {
            b_in_double = !b_in_double;
            continue;
        }
        if ((character == ' ' || character == '\t') && !b_in_single && !b_in_double)
        {
            if (!current.empty())
            {
                result.push_back(current);
                current.clear();
            }
            continue;
        }
        current += character;
    }
    if (!current.empty())
    {
        result.push_back(current);
    }
    return result;
}
} // namespace

struct term::t_PtySession::Impl
{
    int master_fd = -1;
    pid_t pid = -1;
    std::atomic<bool> running{ false };
    std::atomic<bool> stopping{ false };
    std::atomic<int> exit_code{ 0 };
    std::string last_error;
    std::thread reader;
    std::mutex write_mutex;
};

term::t_PtySession::t_PtySession() = default;

term::t_PtySession::~t_PtySession()
{
    Stop();
    delete m_Impl;
    m_Impl = nullptr;
}

bool term::t_PtySession::b_IsSupported()
{
    const int fd = posix_openpt(O_RDWR | O_NOCTTY | O_CLOEXEC);
    if (fd < 0)
    {
        return false;
    }
    ::close(fd);
    return true;
}

bool term::t_PtySession::b_Start(const t_ShellSpec& shell, const std::string& working_directory, int cols, int rows,
    std::function<void(std::string_view)> on_output)
{
    Stop();

    if (m_Impl == nullptr)
    {
        m_Impl = new Impl();
    }

    if (shell.executable.empty())
    {
        m_Impl->last_error = "No shell executable configured.";
        return false;
    }

    cols = std::max(cols, 2);
    rows = std::max(rows, 2);

    int master_fd = posix_openpt(O_RDWR | O_NOCTTY | O_CLOEXEC);
    if (master_fd < 0)
    {
        m_Impl->last_error = std::string("posix_openpt failed: ") + std::strerror(errno);
        return false;
    }
    if (grantpt(master_fd) != 0 || unlockpt(master_fd) != 0)
    {
        m_Impl->last_error = std::string("grantpt/unlockpt failed: ") + std::strerror(errno);
        CloseFd(master_fd);
        return false;
    }

    char slave_name[256] = {};
    if (ptsname_r(master_fd, slave_name, sizeof(slave_name)) != 0)
    {
        m_Impl->last_error = std::string("ptsname_r failed: ") + std::strerror(errno);
        CloseFd(master_fd);
        return false;
    }

    winsize size{};
    size.ws_col = static_cast<unsigned short>(cols);
    size.ws_row = static_cast<unsigned short>(rows);
    ioctl(master_fd, TIOCSWINSZ, &size);

    // Prepare the child command before fork; only async-signal-safe calls are
    // made between fork and exec.
    std::vector<std::string> arguments;
    if (!shell.arguments.empty())
    {
        arguments = SplitArguments(shell.arguments);
    }

    // Resolve PATH before fork; execvp() is not async-signal-safe.
    const std::string executable = ResolveExecutable(shell.executable);

    std::vector<char*> argv;
    argv.push_back(const_cast<char*>(executable.c_str()));
    for (std::string& argument : arguments)
    {
        argv.push_back(argument.data());
    }
    argv.push_back(nullptr);

    const std::string slave_path(slave_name);
    const std::string cwd = working_directory;

    const pid_t pid = ::fork();
    if (pid < 0)
    {
        m_Impl->last_error = std::string("fork failed: ") + std::strerror(errno);
        CloseFd(master_fd);
        return false;
    }

    if (pid == 0)
    {
        ::setsid();

        const int slave_fd = ::open(slave_path.c_str(), O_RDWR);
        if (slave_fd < 0)
        {
            _exit(126);
        }
        ioctl(slave_fd, TIOCSCTTY, 0);

        ::dup2(slave_fd, STDIN_FILENO);
        ::dup2(slave_fd, STDOUT_FILENO);
        ::dup2(slave_fd, STDERR_FILENO);
        if (slave_fd > STDERR_FILENO)
        {
            ::close(slave_fd);
        }
        ::close(master_fd);

        if (!cwd.empty() && ::chdir(cwd.c_str()) != 0)
        {
            _exit(126);
        }

        ::execv(executable.c_str(), argv.data());
        _exit(127);
    }

    m_Impl->master_fd = master_fd;
    m_Impl->pid = pid;
    m_Impl->exit_code.store(0);
    m_Impl->stopping.store(false);
    m_Impl->running.store(true);
    m_Impl->last_error.clear();

    m_Impl->reader = std::thread(
        [impl = m_Impl, on_output = std::move(on_output)]()
        {
            std::vector<char> buffer(4096);
            while (!impl->stopping.load())
            {
                pollfd fd{ impl->master_fd, POLLIN | POLLHUP, 0 };
                const int ready = ::poll(&fd, 1, 100);
                if (ready < 0)
                {
                    if (errno == EINTR)
                    {
                        continue;
                    }
                    break;
                }
                if (ready == 0)
                {
                    continue;
                }

                const ssize_t bytes = ::read(impl->master_fd, buffer.data(), buffer.size());
                if (bytes > 0)
                {
                    if (on_output)
                    {
                        on_output(std::string_view(buffer.data(), static_cast<size_t>(bytes)));
                    }
                    continue;
                }
                if (bytes == 0)
                {
                    break;
                }
                if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)
                {
                    continue;
                }
                break;
            }

            int status = 0;
            while (::waitpid(impl->pid, &status, 0) < 0 && errno == EINTR)
            {
            }
            if (WIFEXITED(status))
            {
                impl->exit_code.store(WEXITSTATUS(status));
            }
            else if (WIFSIGNALED(status))
            {
                impl->exit_code.store(128 + WTERMSIG(status));
            }
            impl->running.store(false);
        });

    return true;
}

void term::t_PtySession::Write(std::string_view bytes)
{
    if (m_Impl == nullptr || m_Impl->master_fd < 0)
    {
        return;
    }

    std::scoped_lock lock(m_Impl->write_mutex);
    size_t offset = 0;
    while (offset < bytes.size())
    {
        const ssize_t written = ::write(m_Impl->master_fd, bytes.data() + offset, bytes.size() - offset);
        if (written < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            break;
        }
        offset += static_cast<size_t>(written);
    }
}

void term::t_PtySession::Resize(int cols, int rows)
{
    if (m_Impl == nullptr || m_Impl->master_fd < 0)
    {
        return;
    }

    winsize size{};
    size.ws_col = static_cast<unsigned short>(std::max(cols, 2));
    size.ws_row = static_cast<unsigned short>(std::max(rows, 2));
    ioctl(m_Impl->master_fd, TIOCSWINSZ, &size);
}

void term::t_PtySession::Stop()
{
    if (m_Impl == nullptr)
    {
        return;
    }

    m_Impl->stopping.store(true);
    if (m_Impl->pid > 0)
    {
        // Ask the shell to exit, give it a short grace period to flush, then
        // force it. Closing a tab should not hang on a shell stuck in a
        // prompt hook or a foreground child.
        ::kill(-m_Impl->pid, SIGHUP);
        ::kill(m_Impl->pid, SIGHUP);
        for (int attempt = 0; attempt < 15 && m_Impl->running.load(); ++attempt)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        if (m_Impl->running.load())
        {
            ::kill(-m_Impl->pid, SIGKILL);
            ::kill(m_Impl->pid, SIGKILL);
        }
    }

    // The reader polls with a short timeout, so it notices `stopping` without
    // needing the master fd closed from another thread.
    if (m_Impl->reader.joinable())
    {
        m_Impl->reader.join();
    }
    CloseFd(m_Impl->master_fd);

    if (m_Impl->pid > 0)
    {
        int status = 0;
        while (::waitpid(m_Impl->pid, &status, WNOHANG) < 0 && errno == EINTR)
        {
        }
        m_Impl->pid = -1;
    }
    m_Impl->running.store(false);
}

bool term::t_PtySession::b_IsRunning() const
{
    return m_Impl != nullptr && m_Impl->running.load();
}

int term::t_PtySession::ExitCode() const
{
    return m_Impl != nullptr ? m_Impl->exit_code.load() : 0;
}

std::string term::t_PtySession::LastError() const
{
    return m_Impl != nullptr ? m_Impl->last_error : std::string();
}
