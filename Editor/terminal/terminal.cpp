#include "terminal.h"

#include "shell_detect.h"
#include "terminal_session.h"
#include "terminal_view.h"
#include "vterm_screen.h"

#include "../../Engine/Profiler.h"
#include "../GameEditorTheme.h"

#include <algorithm>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <imgui.h>
#include <raylib.h>

namespace term
{
namespace
{
t_Rgb s_ToRgb(const ImVec4& color)
{
    return t_Rgb{ static_cast<uint8_t>(std::clamp(color.x, 0.0f, 1.0f) * 255.0f),
        static_cast<uint8_t>(std::clamp(color.y, 0.0f, 1.0f) * 255.0f),
        static_cast<uint8_t>(std::clamp(color.z, 0.0f, 1.0f) * 255.0f) };
}

std::string s_Timestamp()
{
    const auto now = std::chrono::system_clock::now();
    const std::time_t now_time = std::chrono::system_clock::to_time_t(now);
    std::tm local{};
#if defined(_WIN32)
    localtime_s(&local, &now_time);
#else
    localtime_r(&now_time, &local);
#endif
    char buffer[16] = {};
    std::strftime(buffer, sizeof(buffer), "%H:%M:%S", &local);
    return buffer;
}

std::string s_NormalizeNewlines(std::string_view text)
{
    std::string result;
    result.reserve(text.size() + 8);
    for (size_t index = 0; index < text.size(); ++index)
    {
        const char character = text[index];
        if (character == '\n' && (index == 0 || text[index - 1] != '\r'))
        {
            result.push_back('\r');
        }
        result.push_back(character);
    }
    return result;
}

ImFont* s_MonoFont()
{
    ImGuiIO& io = ImGui::GetIO();
    if (Font_Mono < io.Fonts->Fonts.Size)
    {
        return io.Fonts->Fonts[Font_Mono];
    }
    return nullptr;
}
} // namespace

struct Terminal::t_Tab
{
    std::string name;
    bool b_Console = false;
    t_ShellSpec shell;
    std::unique_ptr<t_TerminalSession> session;
    t_TerminalView view;
};

std::mutex Terminal::s_CallbackMutex;
Terminal* Terminal::s_CallbackInstance = nullptr;

LogStreamBuf::LogStreamBuf(Terminal* terminal, Severity severity) : m_Terminal(terminal), m_Severity(severity)
{
}

LogStreamBuf::~LogStreamBuf()
{
    Flush();
}

void LogStreamBuf::Flush()
{
    std::scoped_lock lock(m_Mutex);
    if (!m_Line.empty() && m_Terminal != nullptr)
    {
        m_Terminal->add_text(m_Line, m_Severity);
        m_Line.clear();
    }
}

LogStreamBuf::int_type LogStreamBuf::overflow(LogStreamBuf::int_type ch)
{
    if (traits_type::eq_int_type(ch, traits_type::eof()))
    {
        return traits_type::not_eof(ch);
    }

    std::scoped_lock lock(m_Mutex);
    Emit(static_cast<char>(ch));
    return ch;
}

std::streamsize LogStreamBuf::xsputn(const char* text, std::streamsize length)
{
    std::scoped_lock lock(m_Mutex);
    for (std::streamsize index = 0; index < length; ++index)
    {
        Emit(text[index]);
    }
    return length;
}

int LogStreamBuf::sync()
{
    // Do not flush partial lines: std::cerr is unbuffered and would split
    // each insertion into its own console entry.
    return 0;
}

void LogStreamBuf::Emit(char character)
{
    if (character == '\r')
    {
        return;
    }

    if (character == '\n')
    {
        if (!m_Line.empty() && m_Terminal != nullptr)
        {
            m_Terminal->add_text(m_Line, m_Severity);
            m_Line.clear();
        }
        return;
    }

    m_Line.push_back(character);
    if (m_Line.size() >= 64 * 1024)
    {
        if (m_Terminal != nullptr)
        {
            m_Terminal->add_text(m_Line, m_Severity);
        }
        m_Line.clear();
    }
}

Terminal::Terminal()
{
    m_ConsoleScreen = std::make_unique<t_VTermScreen>(24, 80, static_cast<size_t>(m_ScrollbackLimit));
    m_ConsoleView = std::make_unique<t_TerminalView>();

    auto console = std::make_unique<t_Tab>();
    console->name = "Console";
    console->b_Console = true;
    m_Tabs.push_back(std::move(console));

    std::scoped_lock lock(s_CallbackMutex);
    s_CallbackInstance = this;
}

Terminal::~Terminal()
{
    Shutdown();
}

void Terminal::Shutdown()
{
    if (m_bShuttingDown.exchange(true))
    {
        return;
    }

    if (m_CoutCapture)
    {
        m_CoutCapture->Flush();
    }
    if (m_CerrCapture)
    {
        m_CerrCapture->Flush();
    }

    if (m_OldCout != nullptr)
    {
        std::cout.rdbuf(m_OldCout);
        m_OldCout = nullptr;
    }
    if (m_OldCerr != nullptr)
    {
        std::cerr.rdbuf(m_OldCerr);
        m_OldCerr = nullptr;
    }

    m_CoutCapture.reset();
    m_CerrCapture.reset();

    {
        std::scoped_lock lock(s_CallbackMutex);
        if (s_CallbackInstance == this)
        {
            s_CallbackInstance = nullptr;
        }
    }

    ShutdownTabs();
}

void Terminal::InitCapture()
{
    if (m_bShuttingDown.load())
    {
        return;
    }

    SetTraceLogCallback(&Terminal::s_RaylibLogCallback);

    if (m_OldCout != nullptr)
    {
        std::cout.rdbuf(m_OldCout);
    }
    if (m_OldCerr != nullptr)
    {
        std::cerr.rdbuf(m_OldCerr);
    }

    m_CoutCapture.reset();
    m_CerrCapture.reset();

    m_OldCout = std::cout.rdbuf();
    m_CoutCapture = std::make_unique<LogStreamBuf>(this, Severity::Debug);
    std::cout.rdbuf(m_CoutCapture.get());

    m_OldCerr = std::cerr.rdbuf();
    m_CerrCapture = std::make_unique<LogStreamBuf>(this, Severity::Error);
    std::cerr.rdbuf(m_CerrCapture.get());
}

void Terminal::s_RaylibLogCallback(int logLevel, const char* text, va_list args)
{
    std::scoped_lock lock(s_CallbackMutex);
    if (s_CallbackInstance == nullptr)
    {
        return;
    }

    char buffer[2048] = {};
    vsnprintf(buffer, sizeof(buffer), text, args);

    Severity severity = Severity::Debug;
    if (logLevel == LOG_WARNING)
    {
        severity = Severity::Warn;
    }
    else if (logLevel == LOG_ERROR || logLevel == LOG_FATAL)
    {
        severity = Severity::Error;
    }

    s_CallbackInstance->add_text(buffer, severity);
}

void Terminal::add_text(std::string_view text, Severity severity)
{
    if (m_bShuttingDown.load() || m_ConsoleScreen == nullptr)
    {
        return;
    }

    std::string line;
    if (m_bShowTimestamps.load())
    {
        line += "\x1b[90m";
        line += s_Timestamp();
        line += "\x1b[0m ";
    }

    switch (severity)
    {
        case Severity::Warn:
            line += "\x1b[33m";
            break;
        case Severity::Error:
            line += "\x1b[31m";
            break;
        case Severity::Debug:
        default:
            break;
    }

    line += s_NormalizeNewlines(text);
    line += "\x1b[0m\r\n";
    m_ConsoleScreen->Write(line);
}

void Terminal::clear()
{
    if (m_ConsoleScreen == nullptr)
    {
        return;
    }

    m_ConsoleScreen->Reset();
    if (m_ConsoleView != nullptr)
    {
        m_ConsoleView->ScrollToBottom();
    }
}

void Terminal::SetWorkingDirectory(std::string_view path)
{
    if (path.empty())
    {
        return;
    }

    std::error_code error;
    const std::filesystem::path candidate(path);
    if (std::filesystem::is_directory(candidate, error))
    {
        m_WorkingDirectory = candidate.wstring();
    }
}

void Terminal::SelectConsole()
{
    for (size_t index = 0; index < m_Tabs.size(); ++index)
    {
        if (m_Tabs[index]->b_Console)
        {
            m_SelectTab = static_cast<int>(index);
            return;
        }
    }
}

bool Terminal::b_IsKeyboardCaptured() const
{
    if (!m_bVisible)
    {
        return false;
    }
    if (m_ActiveTab < 0 || m_ActiveTab >= static_cast<int>(m_Tabs.size()))
    {
        return false;
    }

    const t_Tab& tab = *m_Tabs[static_cast<size_t>(m_ActiveTab)];
    return !tab.b_Console && tab.view.b_KeyboardCaptured();
}

void Terminal::EnsureInitialized()
{
    if (m_bInitialized)
    {
        return;
    }
    m_bInitialized = true;

    if (!t_PtySession::b_IsSupported())
    {
        add_text("ConPTY is not available. Terminal shells are disabled; Windows 10 1809 or newer is required.",
            Severity::Warn);
        return;
    }

    m_Shells = DetectShells();
    if (m_Shells.empty())
    {
        add_text("No shell was found. Install PowerShell or use Command Prompt.", Severity::Error);
        return;
    }

    const int requested_tab = m_SelectTab;
    NewShellTab(m_Shells.front());
    if (requested_tab >= 0)
    {
        m_SelectTab = requested_tab;
    }
}

void Terminal::NewShellTab(const t_ShellSpec& shell)
{
    if (m_bShuttingDown.load())
    {
        return;
    }

    auto tab = std::make_unique<t_Tab>();
    tab->name = shell.display_name + " " + std::to_string(m_NextShellIndex++);
    tab->shell = shell;
    tab->session = std::make_unique<t_TerminalSession>(24, 80, m_ScrollbackLimit);
    tab->view.SetFont(s_MonoFont());
    ApplyThemeToScreen(tab->session->Screen());

    if (m_WorkingDirectory.empty())
    {
        std::error_code error;
        m_WorkingDirectory = std::filesystem::current_path(error).wstring();
    }

    tab->session->b_Start(shell, m_WorkingDirectory);
    m_Tabs.push_back(std::move(tab));
    m_SelectTab = static_cast<int>(m_Tabs.size()) - 1;
}

void Terminal::ApplyThemeToScreen(t_VTermScreen& screen)
{
    const ImVec4& text = ImGui::GetStyle().Colors[ImGuiCol_Text];
    const ImVec4& background = ImGui::GetStyle().Colors[ImGuiCol_FrameBg];
    screen.SetDefaultColors(s_ToRgb(text), s_ToRgb(background));

    const ImVec4* colors[16] = {
        &m_Theme.ansi_30,
        &m_Theme.ansi_31,
        &m_Theme.ansi_32,
        &m_Theme.ansi_33,
        &m_Theme.ansi_34,
        &m_Theme.ansi_35,
        &m_Theme.ansi_36,
        &m_Theme.ansi_37,
        &m_Theme.ansi_90,
        &m_Theme.ansi_91,
        &m_Theme.ansi_92,
        &m_Theme.ansi_93,
        &m_Theme.ansi_94,
        &m_Theme.ansi_95,
        &m_Theme.ansi_96,
        &m_Theme.ansi_97,
    };

    for (int index = 0; index < 16; ++index)
    {
        screen.SetPaletteColor(index, s_ToRgb(*colors[index]));
    }
}

void Terminal::CopyConsoleToClipboard()
{
    if (m_ConsoleScreen == nullptr)
    {
        return;
    }

    const int total = static_cast<int>(m_ConsoleScreen->TotalLines());
    if (total <= 0)
    {
        return;
    }

    const std::string text = m_ConsoleScreen->GetText(0, 0, total - 1, std::max(m_ConsoleScreen->Cols() - 1, 0));

    if (!text.empty())
    {
        ImGui::SetClipboardText(text.c_str());
    }
}

void Terminal::ShutdownTabs()
{
    for (auto& tab : m_Tabs)
    {
        if (tab && tab->session)
        {
            tab->session->Stop();
        }
    }
    m_Tabs.clear();
}

void Terminal::RenderConsoleTab()
{
    bool b_timestamps = m_bShowTimestamps.load();
    if (ImGui::Checkbox("Timestamps", &b_timestamps))
    {
        m_bShowTimestamps.store(b_timestamps);
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear"))
    {
        clear();
    }
    ImGui::SameLine();
    if (ImGui::Button("Copy All"))
    {
        CopyConsoleToClipboard();
    }
    ImGui::SameLine();
    ImGui::TextDisabled("%zu lines", m_ConsoleScreen->ScrollbackSize() + static_cast<size_t>(m_ConsoleScreen->Rows()));

    ImGui::Separator();
    m_ConsoleView->SetFont(s_MonoFont());
    m_ConsoleView->SetDefaultColors(ImGui::GetColorU32(ImGuiCol_Text), ImGui::GetColorU32(ImGuiCol_FrameBg));
    m_ConsoleView->Draw("##console_grid", *m_ConsoleScreen, false, {});
}

void Terminal::RenderShellTab(t_Tab& tab)
{
    t_TerminalSession& session = *tab.session;
    session.Pump();

    if (ImGui::Button("Restart"))
    {
        session.b_Start(tab.shell, m_WorkingDirectory);
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear"))
    {
        session.Screen().Reset();
        tab.view.ScrollToBottom();
    }
    ImGui::SameLine();
    if (ImGui::Button(session.b_IsRunning() ? "Kill" : "Start"))
    {
        if (session.b_IsRunning())
        {
            session.Stop();
        }
        else
        {
            session.b_Start(tab.shell, m_WorkingDirectory);
        }
    }
    ImGui::SameLine();
    ImGui::TextDisabled("%s", tab.shell.display_name.c_str());

    if (!session.b_IsRunning() && session.b_HasExited())
    {
        ImGui::SameLine();
        ImGui::TextDisabled("(exit %d)", session.ExitCode());
    }

    ImGui::Separator();

    tab.view.SetFont(s_MonoFont());
    tab.view.SetDefaultColors(ImGui::GetColorU32(ImGuiCol_Text), ImGui::GetColorU32(ImGuiCol_FrameBg));
    tab.view.Draw(
        tab.name.c_str(), session.Screen(), true, [&session](int rows, int cols) { session.Resize(rows, cols); });
}

void Terminal::show(std::string_view window_title, bool* p_open)
{
    const bool b_was_visible = m_bVisible;
    m_bVisible = (p_open == nullptr || *p_open);
    if (!m_bVisible)
    {
        return;
    }
    if (!b_was_visible && m_SelectTab < 0 && m_ActiveTab >= 0 && m_ActiveTab < static_cast<int>(m_Tabs.size()))
    {
        m_SelectTab = m_ActiveTab;
    }

    SCOPED_TIMER("panel_terminal");
    EnsureInitialized();

    ImGui::SetNextWindowSize(ImVec2(800, 500), ImGuiCond_FirstUseEver);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(m_Theme.window_padding, m_Theme.window_padding));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(m_Theme.item_spacing, m_Theme.item_spacing));

    const bool b_begin = ImGui::Begin(window_title.data(), p_open);
    if (!b_begin)
    {
        m_bVisible = false;
        ImGui::PopStyleVar(2);
        ImGui::End();
        return;
    }

    const ImU32 current_fg = ImGui::GetColorU32(ImGuiCol_Text);
    const ImU32 current_bg = ImGui::GetColorU32(ImGuiCol_FrameBg);
    if (m_bFirstShow || current_fg != m_LastDefaultFg || current_bg != m_LastDefaultBg)
    {
        m_LastDefaultFg = current_fg;
        m_LastDefaultBg = current_bg;
        ApplyThemeToScreen(*m_ConsoleScreen);
        for (auto& tab : m_Tabs)
        {
            if (tab && tab->session)
            {
                ApplyThemeToScreen(tab->session->Screen());
            }
        }
        m_bFirstShow = false;
    }

    m_ConsoleView->SetFont(s_MonoFont());

    bool b_open_new_tab = false;
    bool b_close_active_tab = false;
    std::vector<size_t> close_tabs;

    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
        !ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId) && ImGui::GetIO().KeyCtrl && ImGui::GetIO().KeyShift)
    {
        if (ImGui::IsKeyPressed(ImGuiKey_T) && !m_Shells.empty())
        {
            NewShellTab(m_Shells.front());
        }
        else if (ImGui::IsKeyPressed(ImGuiKey_W))
        {
            b_close_active_tab = true;
        }
    }

    const int select_tab = m_SelectTab;
    m_SelectTab = -1;

    if (ImGui::BeginTabBar("##terminal_tabs",
            ImGuiTabBarFlags_Reorderable | ImGuiTabBarFlags_AutoSelectNewTabs | ImGuiTabBarFlags_FittingPolicyScroll))
    {
        for (size_t index = 0; index < m_Tabs.size(); ++index)
        {
            t_Tab& tab = *m_Tabs[index];
            bool b_tab_open = true;
            ImGuiTabItemFlags flags = 0;
            if (static_cast<int>(index) == select_tab)
            {
                flags |= ImGuiTabItemFlags_SetSelected;
            }

            const bool b_selected = tab.b_Console ? ImGui::BeginTabItem(tab.name.c_str(), nullptr, flags)
                                                  : ImGui::BeginTabItem(tab.name.c_str(), &b_tab_open, flags);

            if (b_selected)
            {
                m_ActiveTab = static_cast<int>(index);
                if (tab.b_Console)
                {
                    RenderConsoleTab();
                }
                else
                {
                    RenderShellTab(tab);
                }
                ImGui::EndTabItem();
            }

            if (!tab.b_Console && !b_tab_open)
            {
                close_tabs.push_back(index);
            }
        }

        if (ImGui::TabItemButton(ICON_FA_PLUS "##new_terminal", ImGuiTabItemFlags_Trailing))
        {
            b_open_new_tab = true;
        }

        ImGui::EndTabBar();
    }

    if (b_open_new_tab)
    {
        ImGui::OpenPopup("##new_terminal_popup");
    }

    if (ImGui::BeginPopup("##new_terminal_popup"))
    {
        if (m_Shells.empty())
        {
            ImGui::MenuItem("No shell available", nullptr, false, false);
        }
        for (const t_ShellSpec& shell : m_Shells)
        {
            if (ImGui::MenuItem(shell.display_name.c_str()))
            {
                NewShellTab(shell);
            }
        }
        ImGui::EndPopup();
    }

    if (b_close_active_tab && m_ActiveTab >= 0 && m_ActiveTab < static_cast<int>(m_Tabs.size()) &&
        !m_Tabs[static_cast<size_t>(m_ActiveTab)]->b_Console)
    {
        close_tabs.push_back(static_cast<size_t>(m_ActiveTab));
    }

    for (auto iterator = close_tabs.rbegin(); iterator != close_tabs.rend(); ++iterator)
    {
        const size_t index = *iterator;
        if (index >= m_Tabs.size())
        {
            continue;
        }

        if (static_cast<int>(index) == m_ActiveTab)
        {
            m_ActiveTab = index > 0 ? static_cast<int>(index) - 1 : 0;
        }
        else if (static_cast<int>(index) < m_ActiveTab)
        {
            --m_ActiveTab;
        }

        if (m_Tabs[index]->session)
        {
            m_Tabs[index]->session->Stop();
        }
        m_Tabs.erase(m_Tabs.begin() + static_cast<ptrdiff_t>(index));
    }

    if (m_ActiveTab >= static_cast<int>(m_Tabs.size()))
    {
        m_ActiveTab = m_Tabs.empty() ? 0 : static_cast<int>(m_Tabs.size()) - 1;
    }

    ImGui::End();
    ImGui::PopStyleVar(2);
}

} // namespace term
