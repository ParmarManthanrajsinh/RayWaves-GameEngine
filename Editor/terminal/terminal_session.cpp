#include "terminal_session.h"

#include <mutex>
#include <utility>

namespace term
{
t_TerminalSession::t_TerminalSession(int rows, int cols, int scrollback_limit) : m_Screen(rows, cols, scrollback_limit)
{
}

t_TerminalSession::~t_TerminalSession()
{
    Stop();
}

bool t_TerminalSession::b_Start(const t_ShellSpec& shell, const std::wstring& working_directory)
{
    Stop();
    m_bExitReported = false;
    m_bExited.store(false);
    m_ExitCode.store(0);
    m_StartError.clear();

    m_Screen.SetOutputCallback([this](std::string_view bytes) { m_Pty.Write(bytes); });

    m_bStarted = m_Pty.b_Start(shell, working_directory, m_Screen.Cols(), m_Screen.Rows(),
        [this](std::string_view bytes) { m_Screen.Write(bytes); });

    if (!m_bStarted)
    {
        m_StartError = m_Pty.LastError();
        const std::string message =
            "\r\n\x1b[31m[Failed to start " + shell.display_name + ": " + m_StartError + "]\x1b[0m\r\n";
        m_Screen.Write(message);
        m_bExited.store(true);
        m_bExitReported = true;
    }

    return m_bStarted;
}

void t_TerminalSession::Stop()
{
    m_Pty.Stop();
    m_bStarted = false;
}

void t_TerminalSession::Pump()
{
    if (!m_bStarted || m_bExitReported)
    {
        return;
    }

    const auto now = std::chrono::steady_clock::now();
    if (now - m_LastTitlePoll > std::chrono::milliseconds(250))
    {
        m_LastTitlePoll = now;
        std::scoped_lock lock(m_Screen.Mutex());
        m_Title = m_Screen.Title();
    }

    if (!m_Pty.b_IsRunning())
    {
        m_ExitCode.store(m_Pty.ExitCode());
        const std::string message =
            "\r\n\x1b[90m[Process exited with code " + std::to_string(m_ExitCode.load()) + "]\x1b[0m\r\n";
        m_Screen.Write(message);
        m_bExited.store(true);
        m_bExitReported = true;
    }
}

void t_TerminalSession::Resize(int rows, int cols)
{
    m_Screen.Resize(rows, cols);
    m_Pty.Resize(cols, rows);
}

void t_TerminalSession::WriteInput(std::string_view bytes)
{
    m_Pty.Write(bytes);
}

void t_TerminalSession::SendPaste(std::string_view text)
{
    m_Screen.SendPaste(text);
}

bool t_TerminalSession::b_IsRunning() const
{
    return m_bStarted && m_Pty.b_IsRunning();
}

int t_TerminalSession::ExitCode() const
{
    return m_ExitCode.load();
}

std::string t_TerminalSession::Title() const
{
    if (!m_Title.empty())
    {
        return m_Title;
    }
    return {};
}

std::string t_TerminalSession::LastError() const
{
    return m_StartError.empty() ? m_Pty.LastError() : m_StartError;
}

} // namespace term
