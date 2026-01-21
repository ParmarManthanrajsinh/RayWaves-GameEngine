#include "ProcessRunner.h"
#include <string>
#include <thread>
#include <vector>
#include <array>
#include <functional>
#include <memory>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace ProcessRunner
{
    namespace
    {
        struct Handle
        {
            HANDLE h{ nullptr };

            Handle() = default;
            Handle(HANDLE h_) : h(h_) {}

            ~Handle() { Close(); }

            Handle(const Handle&) = delete;
            Handle& operator=(const Handle&) = delete;

            Handle(Handle&& other) noexcept : h(other.h)
            {
                other.h = nullptr;
            }

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
                if (h)
                {
                    CloseHandle(h);
                    h = nullptr;
                }
            }

            operator HANDLE() const { return h; }
            HANDLE* operator&() { return &h; }
        };
    }

    void RunBuildCommand
    (
        const std::string_view cmd,
        std::function<void(const std::string_view, bool)> on_output,
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
            std::string cmd_mutable = "cmd.exe /C ";
            cmd_mutable.append(cmd);

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

                std::string_view view = current_line;

                while (view.contains('\n'))
                {
                    const auto pos = view.find('\n');

                    std::string_view line = view.substr(0, pos);

                    if (!line.empty() && line.back() == '\r')
                    {
                        line.remove_suffix(1);
                    }

                    if (on_output)
                    {
                        on_output(line, false);
                    }

                    view.remove_prefix(pos + 1);
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