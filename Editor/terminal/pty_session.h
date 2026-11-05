#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>

namespace term
{

struct t_ShellSpec
{
    std::wstring executable;
    std::wstring arguments;
    std::string display_name;
};

// Windows ConPTY-backed shell process. Output is delivered from a reader
// thread; input and resize must be called from the UI thread.
class t_PtySession
{
public:
    t_PtySession();
    ~t_PtySession();

    t_PtySession(const t_PtySession&) = delete;
    t_PtySession& operator=(const t_PtySession&) = delete;

    static bool b_IsSupported();

    bool b_Start(const t_ShellSpec& shell, const std::wstring& working_directory, int cols, int rows,
        std::function<void(std::string_view)> on_output);

    void Write(std::string_view bytes);
    void Resize(int cols, int rows);
    void Stop();

    bool b_IsRunning() const;
    int ExitCode() const;
    std::string LastError() const;

private:
    struct Impl;
    Impl* m_Impl = nullptr;
};

} // namespace term
