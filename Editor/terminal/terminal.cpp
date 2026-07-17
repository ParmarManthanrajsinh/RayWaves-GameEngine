#include <algorithm>
#include <iostream>
#include <utility>
#include <vector>
#include <array>
#include <mutex>
#include <thread>
#include <memory>
#include <chrono>
#include <ctime>
#include <imgui_internal.h>
#define CRTDBG_MAP_ALLOC
#include <cstdlib>
#include <crtdbg.h>

#if defined(_DEBUG) && defined(_MSC_VER) && !defined(__clang__)
#define DEBUG_NEW new(_NORMAL_BLOCK, __FILE__, __LINE__)
#define new DEBUG_NEW
#endif

#include "terminal.h"
#include "../../Engine/Profiler.h"
#include "../GameEditorTheme.h"
#include "../../Engine/ProjectManager.h"

namespace term 
{

    // Output Streams - unique_ptr for automatic cleanup
    static std::unique_ptr<LogStreamBuf> s_cout_buf;
    static std::unique_ptr<LogStreamBuf> s_cerr_buf;

    // Thread-safe callback instance
    std::mutex Terminal::s_callback_mutex;
    Terminal* Terminal::s_callback_instance = nullptr;

    Terminal::Terminal() 
    {
        // Register this instance for Raylib callbacks
        std::scoped_lock lock(s_callback_mutex);
        s_callback_instance = this;
    }

    Terminal::~Terminal() 
    {
        m_ThreadCancelFlag->store(true);
        Shutdown();
    }
    
    void Terminal::Shutdown()
    {
        // Signal shutdown to all threads
        m_is_shutting_down = true;
        
        // Unregister from callbacks
        {
            std::scoped_lock lock(s_callback_mutex);
            if (s_callback_instance == this) 
            {
                s_callback_instance = nullptr;
            }
        }
        
        // Restore streams before deleting buffers
        if (m_old_cout != nullptr) 
        {
            std::cout.rdbuf(m_old_cout);
            m_old_cout = nullptr;
        }
        if (m_old_cerr != nullptr) 
        {
            std::cerr.rdbuf(m_old_cerr);
            m_old_cerr = nullptr;
        }
        
        // unique_ptr automatically deletes - just reset
        s_cout_buf.reset();
        s_cerr_buf.reset();
        
        // Note: Detached threads check m_is_shutting_down before accessing Terminal
    }

    void Terminal::InitCapture() 
    {
        // 1. Raylib Capture
        SetTraceLogCallback(RaylibLogCallback);

        // 2. Std Output Capture - reset old buffers first to prevent leak
        s_cout_buf.reset();
        s_cerr_buf.reset();
        
        m_old_cout = std::cout.rdbuf();
        s_cout_buf = std::make_unique<LogStreamBuf>(this, Severity::Debug);
        std::cout.rdbuf(s_cout_buf.get());

        m_old_cerr = std::cerr.rdbuf();
        s_cerr_buf = std::make_unique<LogStreamBuf>(this, Severity::Error);
        std::cerr.rdbuf(s_cerr_buf.get());
    }

    void Terminal::RaylibLogCallback(int logLevel, const char* text, va_list args) 
    {
        // Thread-safe access to terminal instance
        std::scoped_lock lock(s_callback_mutex);
        if (s_callback_instance == nullptr) return;

        // Thread-safe buffer (on stack)
        char buffer[1024];
        vsnprintf(buffer, sizeof(buffer), text, args);

        Severity s = Severity::Debug;
        switch (logLevel) 
        {
            case LOG_WARNING: s = Severity::Warn; break;
            case LOG_ERROR: s = Severity::Error; break;
            case LOG_FATAL: s = Severity::Error; break;
            default: break;
        }
        
        s_callback_instance->add_text(buffer, s);
    }

    bool Terminal::is_valid_severity(int severity_value)
    {
        return severity_value >= static_cast<int>(Severity::Debug) && 
               severity_value <= static_cast<int>(Severity::Error);
    }

    ImVec4 Terminal::get_severity_color(Severity severity, const Theme& theme)
    {
        switch (severity) 
        {
            case Severity::Debug: return ImGui::GetStyle().Colors[ImGuiCol_Text];
            case Severity::Warn:  return theme.log_warn;
            case Severity::Error: return theme.log_error;
            default:              return ImGui::GetStyle().Colors[ImGuiCol_Text];
        }
    }

    void Terminal::add_text(std::string_view text, Severity severity) 
    {
        if (is_shutting_down()) return;

        auto now = std::chrono::system_clock::now();
        std::time_t now_c = std::chrono::system_clock::to_time_t(now);
        std::tm now_tm;
#ifdef _WIN32
        localtime_s(&now_tm, &now_c);
#else
        localtime_r(&now_c, &now_tm);
#endif
        char time_buf[16];
        std::strftime(time_buf, sizeof(time_buf), "%H:%M:%S", &now_tm);

        Message msg(text, severity, time_buf);
        add_message(msg);
    }

    void Terminal::add_message(const Message& msg) 
    {
        if (is_shutting_down()) return;
        
        std::scoped_lock lock(m_mutex);
        
        // Limit log size
        if (m_messages.size() >= m_max_log_size) 
        {
            m_messages.pop_front();
        }
        m_messages.push_back(msg);
        
        if (m_auto_scroll) m_scroll_to_bottom = true;
    }

    void Terminal::clear() 
    {
        std::scoped_lock lock(m_mutex);
        m_messages.clear();
    }

    void Terminal::show(std::string_view window_title, bool* p_open) 
    {
        SCOPED_TIMER("panel_terminal");
        ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_FirstUseEver);

        ImGui::PushStyleVar
        (
            ImGuiStyleVar_WindowPadding, 
            ImVec2
            (
                m_theme.window_padding, 
                m_theme.window_padding
            )
        );
        ImGui::PushStyleVar
        (
            ImGuiStyleVar_ItemSpacing, 
            ImVec2
            (
                m_theme.item_spacing,
                m_theme.item_spacing
            )
        );

        // Use a generic window so it can be docked
        ImGuiStyle& style = ImGui::GetStyle();
        ImGuiDir old_menu_pos = style.WindowMenuButtonPosition;
        style.WindowMenuButtonPosition = ImGuiDir_None;

        bool b_begin = ImGui::Begin(window_title.data(), p_open);

        style.WindowMenuButtonPosition = old_menu_pos;

        if (!b_begin)
        {
            ImGui::PopStyleVar(2);
            ImGui::End();
            return;
        }

        ImGui::SetWindowFontScale(m_theme.font_scale);

        float footer_height = ImGui::GetFrameHeight() + (m_theme.item_spacing * 2.0f);
        float settings_height = ImGui::GetFrameHeight() + (m_theme.item_spacing * 2.0f);
        ImVec2 avail = ImGui::GetContentRegionAvail();
        
        render_settings_bar(ImVec2(avail.x, settings_height));

        float log_height = avail.y - footer_height - settings_height;
        log_height = std::max<float>(log_height, 50);

        render_log_window(ImVec2(avail.x, log_height));
        render_input_bar(ImVec2(avail.x, footer_height));

        ImGui::End();
        ImGui::PopStyleVar(2);
    }

    void Terminal::render_settings_bar(const ImVec2& size) 
    {
        ImGui::BeginGroup();
        
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0)); // transparent button
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, m_theme.button_hover);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, m_theme.button_active);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);

        float clear_btn_width = ImGui::CalcTextSize(ICON_FA_TRASH_CAN " Clear Log").x + (ImGui::GetStyle().FramePadding.x * 2.0f);
        
        // Search/Filter
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - clear_btn_width - (ImGui::GetStyle().ItemSpacing.x * 2.0f));
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImGui::GetStyle().Colors[ImGuiCol_FrameBg]);
        ImGui::InputTextWithHint
        (
            "##Filter", 
            ICON_FA_FILTER " Search logs...", 
            m_filter_buf, 
            sizeof(m_filter_buf)
        );
        ImGui::PopStyleColor(); // InputBg

        ImGui::SameLine();
        
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - clear_btn_width);
        
        if (ImGui::Button(ICON_FA_TRASH_CAN " Clear Log")) clear();

        ImGui::PopStyleVar();
        ImGui::PopStyleColor(3); // Buttons
        ImGui::EndGroup();
    }

    void Terminal::render_log_window(const ImVec2& size) 
    {
        // Background for the terminal screen
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::GetStyle().Colors[ImGuiCol_FrameBg]);
        ImGui::PushStyleColor(ImGuiCol_Border, ImGui::GetStyle().Colors[ImGuiCol_Border]);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 4.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8)); // padding inside child
        
        ImGuiWindowFlags flags = ImGuiWindowFlags_HorizontalScrollbar;
        if (!m_auto_wrap) flags |= ImGuiWindowFlags_AlwaysHorizontalScrollbar;

        ImGui::BeginChild("##LogWindow", ImVec2(size.x, size.y), 1, flags);
        
        // Push Consolas Mono font
        ImGuiIO& io = ImGui::GetIO();
        if (Font_MonoSmall < io.Fonts->Fonts.Size)
        {
            ImGui::PushFont(io.Fonts->Fonts[Font_MonoSmall]);
        }

        std::scoped_lock lock(m_mutex);
        
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 4));

        bool has_filter = (m_filter_buf[0] != '\0');

        if (!has_filter) 
        {
            ImGuiListClipper clipper;
            clipper.Begin(static_cast<int>(m_messages.size()));
            while (clipper.Step()) 
            {
                for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) 
                {
                    const term::Message& msg = m_messages[i];
                    
                    if (!is_valid_severity(static_cast<int>(msg.severity))) continue;
                    
                    if (m_auto_wrap) ImGui::PushTextWrapPos(ImGui::GetContentRegionAvail().x);

                    if (!msg.timestamp.empty())
                    {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]); // Timestamp color
                        ImGui::TextUnformatted(msg.timestamp.c_str());
                        ImGui::PopStyleColor();
                        ImGui::SameLine(0, 10.0f);
                    }

                    ImGui::PushID(i);
                    ImVec4 color = get_severity_color(msg.severity, m_theme);
                    ImGui::PushStyleColor(ImGuiCol_Text, color);
                    
                    // Draw invisible selectable over the line for mouse interaction
                    ImVec2 pos = ImGui::GetCursorScreenPos();
                    ImGui::Selectable("##line", false, ImGuiSelectableFlags_AllowOverlap);
                    ImGui::SameLine();
                    ImGui::SetCursorScreenPos(pos);
                    
                    ImGui::TextUnformatted(msg.text.c_str());
                    ImGui::PopStyleColor();

                    if (ImGui::BeginPopupContextItem("##terminal_context"))
                    {
                        if (ImGui::MenuItem("Copy Message"))
                        {
                            ImGui::SetClipboardText(msg.text.c_str());
                        }
                        if (!msg.timestamp.empty())
                        {
                            if (ImGui::MenuItem("Copy Timestamp + Message"))
                            {
                                std::string full_msg = msg.timestamp + " " + msg.text;
                                ImGui::SetClipboardText(full_msg.c_str());
                            }
                        }
                        ImGui::EndPopup();
                    }
                    ImGui::PopID();
                    
                    if (m_auto_wrap) ImGui::PopTextWrapPos();
                }
            }
        } 
        else 
        {
             for (const auto& msg : m_messages) 
             {
                if (!is_valid_severity(static_cast<int>(msg.severity))) continue;
                if (!pass_filter(msg)) continue;

                if (m_auto_wrap) ImGui::PushTextWrapPos(ImGui::GetContentRegionAvail().x);

                if (!msg.timestamp.empty())
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]); // Timestamp color
                    ImGui::TextUnformatted(msg.timestamp.c_str());
                    ImGui::PopStyleColor();
                    ImGui::SameLine(0, 10.0f);
                }

                ImGui::PushID(&msg);
                ImVec4 color = get_severity_color(msg.severity, m_theme);
                ImGui::PushStyleColor(ImGuiCol_Text, color);
                
                ImVec2 pos = ImGui::GetCursorScreenPos();
                ImGui::Selectable("##line", false, ImGuiSelectableFlags_AllowOverlap);
                ImGui::SameLine();
                ImGui::SetCursorScreenPos(pos);
                
                ImGui::TextUnformatted(msg.text.c_str());
                ImGui::PopStyleColor();

                if (ImGui::BeginPopupContextItem("##terminal_context"))
                {
                    if (ImGui::MenuItem("Copy Message"))
                    {
                        ImGui::SetClipboardText(msg.text.c_str());
                    }
                    if (!msg.timestamp.empty())
                    {
                        if (ImGui::MenuItem("Copy Timestamp + Message"))
                        {
                            std::string full_msg = msg.timestamp + " " + msg.text;
                            ImGui::SetClipboardText(full_msg.c_str());
                        }
                    }
                    ImGui::EndPopup();
                }
                ImGui::PopID();

                if (m_auto_wrap) ImGui::PopTextWrapPos();
             }
        }

        if (m_scroll_to_bottom)
        {
            ImGui::SetScrollHereY(1.0f);
            m_scroll_to_bottom = false;
        }

        if (Font_MonoSmall < io.Fonts->Fonts.Size)
        {
            ImGui::PopFont();
        }

        ImGui::PopStyleVar();
        ImGui::EndChild();
        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor(2);
    }

    bool Terminal::pass_filter(const Message& msg) const 
    {
        // Skip invalid severity messages
        if (!is_valid_severity(static_cast<int>(msg.severity)))
        {
            return false;
        }
        
        // Text filtering only
        if (m_filter_buf[0] == '\0') return true;
        return msg.text.contains(m_filter_buf);
    }

    void Terminal::render_input_bar(const ImVec2& size) 
    {
        ImGui::Spacing();
        
        ImGuiIO& io = ImGui::GetIO();
        bool has_font = (Font_MonoSmall < io.Fonts->Fonts.Size);
        if (has_font) ImGui::PushFont(io.Fonts->Fonts[Font_MonoSmall]);

        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.75f, 0.15f, 0.15f, 1.0f)); // Accent color prompt
        ImGui::AlignTextToFramePadding();
        ImGui::Text("> ");
        ImGui::PopStyleColor();
        
        ImGui::SameLine(0, 0);

        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImGui::GetStyle().Colors[ImGuiCol_FrameBg]);
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_Text]);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);

        ImGuiInputTextFlags flags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackHistory;
        
        auto callback = [](ImGuiInputTextCallbackData* data) -> int 
            {
            auto* term = static_cast<Terminal*>(data->UserData);
            
            if (data->EventFlag == ImGuiInputTextFlags_CallbackHistory) 
            {
                if (data->EventKey == ImGuiKey_UpArrow) 
                {
                    if (!term->m_history.empty()) 
                    {
                        term->m_history_pos++;
                         if (std::cmp_greater_equal(term->m_history_pos ,term->m_history.size())) term->m_history_pos = static_cast<int>(term->m_history.size()) - 1;
                    }
                } else if (data->EventKey == ImGuiKey_DownArrow) 
                {
                     term->m_history_pos--;
                     term->m_history_pos = std::max(term->m_history_pos, -1);
                }
                
                if 
                (
                    term->m_history_pos >= 0 && 
                    term->m_history_pos < term->m_history.size()
                ) 
                {
                    int idx = static_cast<int>(term->m_history.size()) - 1 - term->m_history_pos;
                    data->DeleteChars(0, data->BufTextLen);
                    data->InsertChars(0, term->m_history[idx].c_str());
                }
            }
            return 0;
        };
        
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        
        if 
        (
            ImGui::InputText
            (
                "##Input", 
                m_input_buf, 
                sizeof(m_input_buf), 
                flags, 
                callback, 
                this
            )
        ) 
        {
            std::string cmd = m_input_buf;
            if (!cmd.empty()) 
            {
                execute_command(cmd);
                m_input_buf[0] = '\0';
                m_history_pos = -1;
                ImGui::SetKeyboardFocusHere(-1);
            }
        }
        if (has_font) ImGui::PopFont();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(2);
    }

    void Terminal::execute_command(std::string_view cmd) 
    {
        m_history.emplace_back(cmd);
        add_text(std::string("> ") + std::string(cmd), Severity::Debug);
        
        std::string command_str(cmd);

        if (command_str == "clear") 
        {
            clear();
            return;
        } 
        


        // Async execution for system commands
        // Capture shared cancel flag so thread can check it even after Terminal destruction
        auto cancel = m_ThreadCancelFlag;
        std::thread
        (
            [this, cancel, command_str]() 
            {
                if (cancel->load()) return;
                
                std::string full_cmd = command_str + " 2>&1";
                if (ProjectManager::b_HasOpenProject())
                {
                    std::string proj_dir = ProjectManager::GetCurrent().m_RootPath;
                    full_cmd = "cd /d \"" + proj_dir + "\" && " + command_str + " 2>&1";
                }
                
                FILE* pipe = _popen(full_cmd.c_str(), "r");

                if (!pipe)
                {
                    if (!cancel->load())
                    {
                        char error_msg[256];
                        strerror_s(error_msg, sizeof(error_msg), errno);
                        if (!cancel->load())
                            this->add_text(std::string("Failed to start command: ") + error_msg, Severity::Error);
                    }
                    return;
                }

                char buffer[128];
                while (fgets(buffer, sizeof(buffer), pipe)) 
                {
                    if (cancel->load()) 
                    {
                        _pclose(pipe);
                        return;
                    }
                    
                    std::string res(buffer);
                    while (!res.empty() && (res.back() == '\n' || res.back() == '\r')) 
                    {
                        res.pop_back();
                    }
                    
                    if (!cancel->load())
                    {
                        this->add_text(res, Severity::Debug);
                    }
                }
                
                int return_code = _pclose(pipe);

                if (!cancel->load())
                {
                    if (return_code != 0) 
                    {
                         this->add_text
                         (
                             "Command exited with code " + 
                             std::to_string(return_code), 
                             Severity::Warn
                         );
                    }
                    else 
                    {
                         this->add_text("Command finished.", Severity::Debug);
                    }
                }
            }
        ).detach();
    }

}
