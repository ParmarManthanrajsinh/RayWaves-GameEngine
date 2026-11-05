#pragma once

#include "pty_session.h"
#include "vterm_screen.h"

#include <atomic>
#include <chrono>
#include <string>
#include <string_view>

namespace term
{

class t_TerminalSession
{
public:
    t_TerminalSession(int rows, int cols, int scrollback_limit = 5000);
    ~t_TerminalSession();

    t_TerminalSession(const t_TerminalSession&) = delete;
    t_TerminalSession& operator=(const t_TerminalSession&) = delete;

    bool b_Start(const t_ShellSpec& shell, const std::wstring& working_directory);
    void Stop();

    // Called once per frame on the UI thread before rendering.
    void Pump();

    void Resize(int rows, int cols);
    void WriteInput(std::string_view bytes);
    void SendPaste(std::string_view text);

    t_VTermScreen& Screen() { return m_Screen; }
    const t_VTermScreen& Screen() const { return m_Screen; }

    bool b_IsRunning() const;
    bool b_HasExited() const { return m_bExited.load(); }
    int ExitCode() const;
    std::string Title() const;
    std::string LastError() const;

private:
    t_VTermScreen m_Screen;
    t_PtySession m_Pty;

    std::atomic<bool> m_bExited{ false };
    std::atomic<int> m_ExitCode{ 0 };
    bool m_bStarted = false;
    bool m_bExitReported = false;
    std::string m_StartError;
    std::string m_Title;
    std::chrono::steady_clock::time_point m_LastTitlePoll{};
};

} // namespace term
