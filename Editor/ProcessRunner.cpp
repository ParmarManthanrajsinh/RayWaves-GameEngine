#include "ProcessRunner.h"
#include <string>
#include <thread>
#include <vector>
#include <array>
#include <functional>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace ProcessRunner
{
    namespace
    {
        struct Handle
        {
            HANDLE h{ nullptr };
            ~Handle() { if (h) CloseHandle(h); }
            operator HANDLE() const { return h; }
            HANDLE* operator&() { return &h; }
        };
    }

    void RunBuildCommand
    (
        const std::string& cmd,
        std::function<void(const std::string&, bool)> on_output,
        std::function<void(bool)> on_complete
    )
    {
        std::thread([cmd, on_output, on_complete]()
        {
            SECURITY_ATTRIBUTES sa{};
            sa.nLength = sizeof(SECURITY_ATTRIBUTES);
            sa.bInheritHandle = TRUE;

            Handle hRead;
            Handle hWrite;

            if (!CreatePipe(&hRead, &hWrite, &sa, 0))
            {
                if (on_output)
                {
                    on_output("Failed to create pipe.", true);
                }
                if (on_complete)
                {
                    on_complete(false);
                }
                return;
            }

            STARTUPINFOA si{};
            si.cb = sizeof(STARTUPINFOA);
            si.hStdError = hWrite;
            si.hStdOutput = hWrite;
            si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
            si.wShowWindow = SW_HIDE;

            PROCESS_INFORMATION pi{};
            std::string cmd_mutable = "cmd.exe /C " + cmd;

            if
            (
                !CreateProcessA
                (
                    nullptr,
                    cmd_mutable.data(),
                    nullptr,
                    nullptr,
                    TRUE,
                    CREATE_NO_WINDOW,
                    nullptr,
                    nullptr,
                    &si,
                    &pi
                )
            )
            {
                if (on_output)
                {
                    on_output("Failed to create process.", true);
                }
                if (on_complete)
                {
                    on_complete(false);
                }
                return;
            }

            Handle hProcess{ pi.hProcess };
            Handle hThread{ pi.hThread };

            hWrite = {}; // close write end

            DWORD bytes_read = 0;
            std::array<char, 128> buffer{};
            std::string current_line;

            while
            (
                ReadFile
                (
                    hRead,
                    buffer.data(),
                    static_cast<DWORD>(buffer.size()),
                    &bytes_read,
                    nullptr
                ) && bytes_read > 0
            )
            {
                current_line.append(buffer.data(), bytes_read);

                size_t pos;
                while ((pos = current_line.find('\n')) != std::string::npos)
                {
                    std::string line = current_line.substr(0, pos);
                    if (!line.empty() && line.back() == '\r')
                    {
                        line.pop_back();
                    }
                    if (on_output)
                    {
                        on_output(line, false);
                    }
                    current_line.erase(0, pos + 1);
                }
            }

            if (!current_line.empty())
            {
                if (on_output)
                {
                    on_output(current_line, false);
                }
            }

            WaitForSingleObject(hProcess, INFINITE);

            DWORD exit_code = 0;
            GetExitCodeProcess(hProcess, &exit_code);

            if (on_complete)
            {
                on_complete(exit_code == 0);
            }

        }).detach();
    }
}