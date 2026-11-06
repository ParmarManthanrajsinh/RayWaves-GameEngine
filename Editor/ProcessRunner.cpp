#include "ProcessRunner.h"
#include <algorithm>
#include <array>
#include <memory>
#include <string>
#include <thread>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace ProcessRunner
{
namespace
{
constexpr int k_MaxChunksPerDrain = 64;
constexpr int k_MaxFinalDrains = 1024;
constexpr DWORD k_PollIntervalMs = 10;
constexpr DWORD k_ExitWaitMs = 5000;

struct Handle
{
    HANDLE h{ nullptr };

    Handle() = default;
    Handle(HANDLE h_) : h(h_) {}

    ~Handle() { Close(); }

    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;

    Handle(Handle&& other) noexcept : h(other.h) { other.h = nullptr; }

    Handle& operator=(Handle&& other) noexcept
    {
        if (this != std::addressof(other))
        {
            Close();
            h = other.h;
            other.h = nullptr;
        }
        return *this;
    }

    void Close()
    {
        if (h != nullptr)
        {
            CloseHandle(h);
            h = nullptr;
        }
    }

    operator HANDLE() const { return h; }
    HANDLE* operator&() { return &h; }
};

// cmd.exe spawns cmake, ninja, and the compilers. The job lets us kill
// that whole tree at once, and KILL_ON_JOB_CLOSE reaps anything left
// behind when the run ends.
struct Job
{
    Handle h;

    Job() : h(CreateJobObjectA(nullptr, nullptr))
    {
        if (h.h == nullptr)
            return;

        JOBOBJECT_EXTENDED_LIMIT_INFORMATION info{};
        info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        SetInformationJobObject(h, JobObjectExtendedLimitInformation, &info, sizeof(info));
    }
};

enum class EDrain
{
    Idle,
    Busy,
    Closed
};

// Turns pipe bytes into complete lines. Output is read until the pipe
// reports no data, so text written just before the child exits is
// still delivered.
class PipeReader
{
public:
    PipeReader(HANDLE pipe, std::function<void(std::string_view, bool)> on_output)
        : m_Pipe(pipe), m_OnOutput(std::move(on_output))
    {
    }

    EDrain Drain()
    {
        for (int chunk = 0; chunk < k_MaxChunksPerDrain; ++chunk)
        {
            DWORD avail = 0;
            if (!PeekNamedPipe(m_Pipe, nullptr, 0, nullptr, &avail, nullptr))
            {
                return EDrain::Closed;
            }
            if (avail == 0)
            {
                return EDrain::Idle;
            }

            DWORD bytes_read = 0;
            const DWORD want = std::min<DWORD>(avail, static_cast<DWORD>(m_Buffer.size()));
            if (!ReadFile(m_Pipe, m_Buffer.data(), want, &bytes_read, nullptr) || bytes_read == 0)
            {
                return EDrain::Closed;
            }

            m_Pending.append(m_Buffer.data(), bytes_read);
            EmitCompleteLines();
        }
        return EDrain::Busy;
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

    HANDLE m_Pipe;
    std::function<void(std::string_view, bool)> m_OnOutput;
    std::array<char, 4096> m_Buffer{};
    std::string m_Pending;
};
} // namespace

std::thread RunBuildCommand(std::string_view cmd, const std::function<void(std::string_view, bool)>& on_output,
    const std::function<void(bool)>& on_complete, const std::shared_ptr<std::atomic<bool>>& cancel)
{
    return std::thread(
        [cmd_str = std::string(cmd), on_output, on_complete, cancel]()
        {
            auto report_failure = [&](std::string_view message)
            {
                if (on_output)
                {
                    on_output(message, true);
                }
                if (on_complete)
                {
                    on_complete(false);
                }
            };

            SECURITY_ATTRIBUTES sa{};
            sa.nLength = sizeof(SECURITY_ATTRIBUTES);
            sa.bInheritHandle = TRUE;

            Handle hRead;
            Handle hWrite;

            if (!CreatePipe(&hRead, &hWrite, &sa, 0))
            {
                report_failure("Failed to create pipe.");
                return;
            }

            // Only the child's write end should be inherited. Otherwise the
            // child also holds the read end open.
            SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);

            STARTUPINFOA si{};
            si.cb = sizeof(STARTUPINFOA);
            si.hStdError = hWrite;
            si.hStdOutput = hWrite;
            si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
            si.wShowWindow = SW_HIDE;

            Job job;
            PROCESS_INFORMATION pi{};
            std::string cmd_mutable = "cmd.exe /C ";
            cmd_mutable.append(cmd_str);

            // Created suspended so the job assignment happens before cmd.exe
            // can spawn children that would escape the job.
            if (!CreateProcessA(nullptr, cmd_mutable.data(), nullptr, nullptr, TRUE,
                    CREATE_NO_WINDOW | CREATE_SUSPENDED, nullptr, nullptr, &si, &pi))
            {
                report_failure("Failed to create process.");
                return;
            }

            Handle hProcess{ pi.hProcess };
            Handle hThread{ pi.hThread };

            // If assignment fails, cancel falls back to killing cmd.exe only.
            const bool b_InJob = job.h.h != nullptr && AssignProcessToJobObject(job.h, hProcess) != FALSE;
            ResumeThread(hThread);

            hWrite = {}; // close our write end so EOF can arrive

            PipeReader reader(hRead, on_output);
            bool b_PipeOpen = true;
            bool b_Cancelled = false;

            while (true)
            {
                if (cancel && cancel->load())
                {
                    b_Cancelled = true;
                    if (b_InJob)
                    {
                        TerminateJobObject(job.h, 1);
                    }
                    else
                    {
                        TerminateProcess(hProcess, 1);
                    }
                    break;
                }

                if (b_PipeOpen && reader.Drain() == EDrain::Closed)
                {
                    b_PipeOpen = false;
                }

                if (WaitForSingleObject(hProcess, 0) == WAIT_OBJECT_0)
                {
                    break;
                }

                Sleep(k_PollIntervalMs);
            }

            // The child has exited. Drain whatever it wrote last; that is where
            // compiler errors usually are.
            if (b_PipeOpen)
            {
                for (int i = 0; i < k_MaxFinalDrains && reader.Drain() == EDrain::Busy; ++i)
                {
                }
            }
            reader.Flush();

            WaitForSingleObject(hProcess, k_ExitWaitMs);

            DWORD exit_code = STILL_ACTIVE;
            GetExitCodeProcess(hProcess, &exit_code);

            if (on_complete)
            {
                on_complete(!b_Cancelled && exit_code == 0);
            }
        });
}

bool RunAndWait(std::string_view cmd, const std::function<void(std::string_view, bool)>& on_output,
    const std::shared_ptr<std::atomic<bool>>& cancel)
{
    bool b_Success = false;
    std::thread worker = RunBuildCommand(cmd, on_output, [&b_Success](bool success) { b_Success = success; }, cancel);
    worker.join();
    return b_Success;
}
} // namespace ProcessRunner
