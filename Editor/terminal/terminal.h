#pragma once

#include "pty_session.h"
#include "types.h"

#include <atomic>
#include <cstdarg>
#include <memory>
#include <mutex>
#include <streambuf>
#include <string>
#include <string_view>
#include <vector>

namespace term
{

class Terminal;
class t_TerminalView;
class t_VTermScreen;

// Line-buffered std::cout/std::cerr sink. Partial writes are held until a
// newline so token-by-token std::cerr output does not become one log line
// per insertion.
class LogStreamBuf : public std::streambuf
{
public:
    LogStreamBuf(Terminal* terminal, Severity severity);
    ~LogStreamBuf() override;

    void Flush();

protected:
    int_type overflow(int_type ch) override;
    std::streamsize xsputn(const char* text, std::streamsize length) override;
    int sync() override;

private:
    void Emit(char character);

    Terminal* m_Terminal = nullptr;
    Severity m_Severity = Severity::Debug;
    std::string m_Line;
    std::mutex m_Mutex;
};

class Terminal
{
public:
    Terminal();
    ~Terminal();

    Terminal(const Terminal&) = delete;
    Terminal& operator=(const Terminal&) = delete;
    Terminal(Terminal&&) = delete;
    Terminal& operator=(Terminal&&) = delete;

    void show(std::string_view window_title = "Terminal", bool* p_open = nullptr);

    void add_text(std::string_view text, Severity severity = Severity::Debug);
    void clear();

    void InitCapture();
    void Shutdown();

    void SetWorkingDirectory(std::string_view path);
    void SelectConsole();

    bool b_IsKeyboardCaptured() const;
    Theme& theme() { return m_Theme; }

private:
    struct t_Tab;

    void EnsureInitialized();
    void NewShellTab(const t_ShellSpec& shell);
    void RenderConsoleTab();
    void RenderShellTab(t_Tab& tab);
    void ApplyThemeToScreen(t_VTermScreen& screen);
    void CopyConsoleToClipboard();
    void ShutdownTabs();

    std::unique_ptr<t_VTermScreen> m_ConsoleScreen;
    std::unique_ptr<t_TerminalView> m_ConsoleView;
    std::vector<std::unique_ptr<t_Tab>> m_Tabs;

    std::vector<t_ShellSpec> m_Shells;
    std::wstring m_WorkingDirectory;

    Theme m_Theme;
    int m_ActiveTab = 0;
    int m_SelectTab = -1;
    int m_NextShellIndex = 1;
    int m_ScrollbackLimit = 5000;

    bool m_bVisible = true;
    bool m_bInitialized = false;
    bool m_bFirstShow = true;
    std::atomic<bool> m_bShowTimestamps{ true };
    ImU32 m_LastDefaultFg = 0;
    ImU32 m_LastDefaultBg = 0;
    std::atomic<bool> m_bShuttingDown{ false };

    std::unique_ptr<LogStreamBuf> m_CoutCapture;
    std::unique_ptr<LogStreamBuf> m_CerrCapture;
    std::streambuf* m_OldCout = nullptr;
    std::streambuf* m_OldCerr = nullptr;

    static std::mutex s_CallbackMutex;
    static Terminal* s_CallbackInstance;
    static void s_RaylibLogCallback(int logLevel, const char* text, va_list args);
};

} // namespace term
