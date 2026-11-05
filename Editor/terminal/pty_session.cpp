#include "pty_session.h"

#include <algorithm>
#include <array>
#include <mutex>
#include <thread>
#include <vector>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#ifndef PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE
#define PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE 0x00020016
#endif

namespace
{
using PFN_CreatePseudoConsole = HRESULT(WINAPI*)(COORD, HANDLE, HANDLE, DWORD, HANDLE*);
using PFN_ResizePseudoConsole = HRESULT(WINAPI*)(HANDLE, COORD);
using PFN_ClosePseudoConsole = void(WINAPI*)(HANDLE);

struct ConPtyApi
{
    PFN_CreatePseudoConsole CreatePseudoConsole = nullptr;
    PFN_ResizePseudoConsole ResizePseudoConsole = nullptr;
    PFN_ClosePseudoConsole ClosePseudoConsole = nullptr;
    bool b_Available = false;
};

ConPtyApi& s_ConPtyApi()
{
    static ConPtyApi api = []
    {
        ConPtyApi loaded;
        HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
        if (kernel32 == nullptr)
        {
            return loaded;
        }

        loaded.CreatePseudoConsole =
            reinterpret_cast<PFN_CreatePseudoConsole>(GetProcAddress(kernel32, "CreatePseudoConsole"));
        loaded.ResizePseudoConsole =
            reinterpret_cast<PFN_ResizePseudoConsole>(GetProcAddress(kernel32, "ResizePseudoConsole"));
        loaded.ClosePseudoConsole =
            reinterpret_cast<PFN_ClosePseudoConsole>(GetProcAddress(kernel32, "ClosePseudoConsole"));
        loaded.b_Available = loaded.CreatePseudoConsole != nullptr && loaded.ResizePseudoConsole != nullptr &&
            loaded.ClosePseudoConsole != nullptr;
        return loaded;
    }();
    return api;
}

std::string s_FormatLastError(unsigned long code)
{
    char buffer[512] = {};
    const unsigned long length = FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr,
        code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buffer, static_cast<DWORD>(sizeof(buffer)), nullptr);
    if (length == 0)
    {
        return "Windows error " + std::to_string(code);
    }
    size_t size = length;
    while (size > 0 && (buffer[size - 1] == '\r' || buffer[size - 1] == '\n'))
    {
        --size;
    }
    return std::string(buffer, size);
}

std::vector<wchar_t> s_BuildEnvironment()
{
    std::vector<wchar_t> result;
    LPWCH environment = GetEnvironmentStringsW();
    if (environment != nullptr)
    {
        for (const wchar_t* entry = environment; *entry != L'\0'; entry += wcslen(entry) + 1)
        {
            const std::wstring variable(entry);
            if (variable.rfind(L"TERM=", 0) == 0 || variable.rfind(L"COLORTERM=", 0) == 0 ||
                variable.rfind(L"TERM_PROGRAM=", 0) == 0 || variable.rfind(L"TERM_PROGRAM_VERSION=", 0) == 0)
            {
                continue;
            }
            result.insert(result.end(), entry, entry + wcslen(entry) + 1);
        }
        FreeEnvironmentStringsW(environment);
    }

    const wchar_t* overrides[] = {
        L"TERM=xterm-256color",
        L"COLORTERM=truecolor",
        L"TERM_PROGRAM=RayWaves",
        L"TERM_PROGRAM_VERSION=1.0",
    };
    for (const wchar_t* entry : overrides)
    {
        result.insert(result.end(), entry, entry + wcslen(entry) + 1);
    }

    result.push_back(L'\0');
    return result;
}

void s_CloseHandle(HANDLE& handle)
{
    if (handle != nullptr && handle != INVALID_HANDLE_VALUE)
    {
        CloseHandle(handle);
    }
    handle = nullptr;
}
} // namespace

struct term::t_PtySession::Impl
{
    HANDLE hPseudoConsole = nullptr;
    HANDLE hInputWrite = nullptr;
    HANDLE hOutputRead = nullptr;
    HANDLE hProcess = nullptr;

    std::thread reader;
    std::mutex write_mutex;
    std::atomic<bool> b_Stopping{ false };
    std::atomic<bool> b_Running{ false };
    std::atomic<int> exit_code{ 0 };

    std::function<void(std::string_view)> on_output;
    std::string last_error;

    void ReadLoop()
    {
        std::array<char, 8192> buffer{};

        while (!b_Stopping.load())
        {
            DWORD bytes_read = 0;
            if (!ReadFile(hOutputRead, buffer.data(), static_cast<DWORD>(buffer.size()), &bytes_read, nullptr) ||
                bytes_read == 0)
            {
                break;
            }

            if (!b_Stopping.load() && on_output)
            {
                on_output(std::string_view(buffer.data(), bytes_read));
            }
        }

        if (hProcess != nullptr)
        {
            WaitForSingleObject(hProcess, 2000);
            DWORD code = 0;
            if (GetExitCodeProcess(hProcess, &code))
            {
                exit_code.store(static_cast<int>(code));
            }
        }
        b_Running.store(false);
    }
};

namespace term
{

t_PtySession::t_PtySession() : m_Impl(new Impl())
{
}

t_PtySession::~t_PtySession()
{
    Stop();
    delete m_Impl;
    m_Impl = nullptr;
}

bool t_PtySession::b_IsSupported()
{
    return s_ConPtyApi().b_Available;
}

bool t_PtySession::b_Start(const t_ShellSpec& shell, const std::wstring& working_directory, int cols, int rows,
    std::function<void(std::string_view)> on_output)
{
    Stop();

    if (m_Impl == nullptr)
    {
        m_Impl = new Impl();
    }

    ConPtyApi& api = s_ConPtyApi();
    if (!api.b_Available)
    {
        m_Impl->last_error = "ConPTY requires Windows 10 version 1809 or newer.";
        return false;
    }

    cols = std::max(cols, 2);
    rows = std::max(rows, 2);

    SECURITY_ATTRIBUTES attributes{};
    attributes.nLength = sizeof(SECURITY_ATTRIBUTES);
    attributes.bInheritHandle = TRUE;
    attributes.lpSecurityDescriptor = nullptr;

    HANDLE input_read = nullptr;
    HANDLE input_write = nullptr;
    HANDLE output_read = nullptr;
    HANDLE output_write = nullptr;

    if (!CreatePipe(&input_read, &input_write, &attributes, 0) ||
        !CreatePipe(&output_read, &output_write, &attributes, 0))
    {
        m_Impl->last_error = "Failed to create ConPTY pipes: " + s_FormatLastError(GetLastError());
        s_CloseHandle(input_read);
        s_CloseHandle(input_write);
        s_CloseHandle(output_read);
        s_CloseHandle(output_write);
        return false;
    }

    HANDLE pseudo_console = nullptr;
    const COORD size{ static_cast<SHORT>(cols), static_cast<SHORT>(rows) };
    const HRESULT create_result = api.CreatePseudoConsole(size, input_read, output_write, 0, &pseudo_console);
    if (FAILED(create_result))
    {
        m_Impl->last_error =
            "CreatePseudoConsole failed: " + s_FormatLastError(static_cast<unsigned long>(create_result));
        s_CloseHandle(input_read);
        s_CloseHandle(input_write);
        s_CloseHandle(output_read);
        s_CloseHandle(output_write);
        return false;
    }

    s_CloseHandle(input_read);
    s_CloseHandle(output_write);

    STARTUPINFOEXW startup{};
    startup.StartupInfo.cb = sizeof(STARTUPINFOEXW);
    // Prevent the child from inheriting the host process std handles. When
    // the host has redirected stdio (CI, IDE capture), inheritance sends
    // shell output to the host console instead of the ConPTY.
    startup.StartupInfo.dwFlags |= STARTF_USESTDHANDLES;
    startup.StartupInfo.hStdInput = nullptr;
    startup.StartupInfo.hStdOutput = nullptr;
    startup.StartupInfo.hStdError = nullptr;

    SIZE_T attribute_size = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &attribute_size);
    std::vector<unsigned char> attribute_buffer(attribute_size);
    startup.lpAttributeList = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attribute_buffer.data());

    bool b_attributes_ready =
        InitializeProcThreadAttributeList(startup.lpAttributeList, 1, 0, &attribute_size) != FALSE;

    if (b_attributes_ready)
    {
        b_attributes_ready = UpdateProcThreadAttribute(startup.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE,
                                 pseudo_console, sizeof(pseudo_console), nullptr, nullptr) != FALSE;
    }

    if (!b_attributes_ready)
    {
        m_Impl->last_error = "Failed to set ConPTY process attribute: " + s_FormatLastError(GetLastError());
        api.ClosePseudoConsole(pseudo_console);
        s_CloseHandle(input_write);
        s_CloseHandle(output_read);
        return false;
    }

    std::wstring command_line = L"\"" + shell.executable + L"\"";
    if (!shell.arguments.empty())
    {
        command_line += L" " + shell.arguments;
    }

    std::vector<wchar_t> environment = s_BuildEnvironment();

    PROCESS_INFORMATION process{};
    const BOOL b_created = CreateProcessW(nullptr, command_line.data(), nullptr, nullptr, FALSE,
        EXTENDED_STARTUPINFO_PRESENT | CREATE_UNICODE_ENVIRONMENT, environment.data(),
        working_directory.empty() ? nullptr : working_directory.c_str(), &startup.StartupInfo, &process);

    DeleteProcThreadAttributeList(startup.lpAttributeList);

    if (!b_created)
    {
        m_Impl->last_error = "CreateProcess failed: " + s_FormatLastError(GetLastError());
        api.ClosePseudoConsole(pseudo_console);
        s_CloseHandle(input_write);
        s_CloseHandle(output_read);
        return false;
    }

    CloseHandle(process.hThread);
    process.hThread = nullptr;

    m_Impl->hPseudoConsole = pseudo_console;
    m_Impl->hInputWrite = input_write;
    m_Impl->hOutputRead = output_read;
    m_Impl->hProcess = process.hProcess;
    m_Impl->on_output = std::move(on_output);
    m_Impl->b_Stopping.store(false);
    m_Impl->b_Running.store(true);
    m_Impl->exit_code.store(0);
    m_Impl->last_error.clear();

    m_Impl->reader = std::thread([this]() { m_Impl->ReadLoop(); });
    return true;
}

void t_PtySession::Write(std::string_view bytes)
{
    if (m_Impl == nullptr || m_Impl->hInputWrite == nullptr || bytes.empty())
    {
        return;
    }

    std::scoped_lock lock(m_Impl->write_mutex);
    DWORD written = 0;
    if (!WriteFile(m_Impl->hInputWrite, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr))
    {
        m_Impl->last_error = "WriteFile failed: " + s_FormatLastError(GetLastError());
    }
}

void t_PtySession::Resize(int cols, int rows)
{
    ConPtyApi& api = s_ConPtyApi();
    if (m_Impl == nullptr || m_Impl->hPseudoConsole == nullptr || !api.b_Available)
    {
        return;
    }

    const COORD size{ static_cast<SHORT>(std::max(cols, 2)), static_cast<SHORT>(std::max(rows, 2)) };
    api.ResizePseudoConsole(m_Impl->hPseudoConsole, size);
}

void t_PtySession::Stop()
{
    if (m_Impl == nullptr)
    {
        return;
    }

    m_Impl->b_Stopping.store(true);
    m_Impl->b_Running.store(false);

    if (m_Impl->hProcess != nullptr)
    {
        TerminateProcess(m_Impl->hProcess, 0);
    }

    if (m_Impl->hOutputRead != nullptr)
    {
        CancelIoEx(m_Impl->hOutputRead, nullptr);
    }

    ConPtyApi& api = s_ConPtyApi();
    if (m_Impl->hPseudoConsole != nullptr && api.b_Available)
    {
        api.ClosePseudoConsole(m_Impl->hPseudoConsole);
        m_Impl->hPseudoConsole = nullptr;
    }

    s_CloseHandle(m_Impl->hInputWrite);
    s_CloseHandle(m_Impl->hOutputRead);

    if (m_Impl->reader.joinable())
    {
        if (m_Impl->reader.get_id() == std::this_thread::get_id())
        {
            m_Impl->reader.detach();
        }
        else
        {
            m_Impl->reader.join();
        }
    }

    s_CloseHandle(m_Impl->hProcess);
}

bool t_PtySession::b_IsRunning() const
{
    return m_Impl != nullptr && m_Impl->b_Running.load();
}

int t_PtySession::ExitCode() const
{
    return m_Impl != nullptr ? m_Impl->exit_code.load() : 0;
}

std::string t_PtySession::LastError() const
{
    return m_Impl != nullptr ? m_Impl->last_error : std::string();
}

} // namespace term

#else

struct term::t_PtySession::Impl
{
};

namespace term
{
t_PtySession::t_PtySession() = default;
t_PtySession::~t_PtySession() = default;

bool t_PtySession::b_IsSupported()
{
    return false;
}

bool t_PtySession::b_Start(const t_ShellSpec&, const std::wstring&, int, int, std::function<void(std::string_view)>)
{
    return false;
}

void t_PtySession::Write(std::string_view)
{
}

void t_PtySession::Resize(int, int)
{
}

void t_PtySession::Stop()
{
}

bool t_PtySession::b_IsRunning() const
{
    return false;
}

int t_PtySession::ExitCode() const
{
    return 0;
}

std::string t_PtySession::LastError() const
{
    return "ConPTY is only available on Windows.";
}
} // namespace term

#endif
