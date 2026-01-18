#include <algorithm>
#include <iostream>
#include <vector>
#include <array>
#include <mutex>
#include <thread>
#include <memory>
#include <imgui_internal.h>
#include "terminal.h"

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
        std::lock_guard<std::mutex> lock(s_callback_mutex);
        s_callback_instance = this;
    }

    Terminal::~Terminal() 
    {
        // Ensure proper cleanup
        Shutdown();
    }
    
    void Terminal::Shutdown()
    {
        // Signal shutdown to all threads
        m_is_shutting_down = true;
        
        // Unregister from callbacks
        {
            std::lock_guard<std::mutex> lock(s_callback_mutex);
            if (s_callback_instance == this) 
            {
                s_callback_instance = nullptr;
            }
        }
        
        // Restore streams before deleting buffers
        if (m_old_cout) 
        {
            std::cout.rdbuf(m_old_cout);
            m_old_cout = nullptr;
        }
        if (m_old_cerr) 
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
        std::lock_guard<std::mutex> lock(s_callback_mutex);
        if (!s_callback_instance) return;

        // Thread-safe buffer (on stack)
        char buffer[1024];
        vsnprintf(buffer, sizeof(buffer), text, args);

        Severity s = Severity::Debug;
        switch (logLevel) 
        {
            case LOG_TRACE: s = Severity::Debug; break;
            case LOG_DEBUG: s = Severity::Debug; break;
            case LOG_INFO: s = Severity::Debug; break;
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
            case Severity::Debug: return theme.log_debug;
            case Severity::Warn:  return theme.log_warn;
            case Severity::Error: return theme.log_error;
            default:              return theme.text_default;
        }
    }

    void Terminal::add_text(std::string_view text, Severity severity) 
    {
        if (is_shutting_down()) return;
        Message msg(text, severity);
        add_message(msg);
    }

    void Terminal::add_message(const Message& msg) 
    {
        if (is_shutting_down()) return;
        
        std::lock_guard<std::mutex> lock(m_mutex);
        
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
        std::lock_guard<std::mutex> lock(m_mutex);
        m_messages.clear();
    }

    void Terminal::show(std::string_view window_title, bool* p_open) 
    {
        ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_FirstUseEver);

        ImGui::PushStyleColor(ImGuiCol_WindowBg, m_theme.window_bg);
        ImGui::PushStyleColor(ImGuiCol_Border, m_theme.border_color);
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
        if (!ImGui::Begin(window_title.data(), p_open))
        {
            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor(2);
            ImGui::End();
            return;
        }

        ImGui::SetWindowFontScale(m_theme.font_scale);

        float footer_height = ImGui::GetFrameHeight() + m_theme.item_spacing * 2.0f;
        float settings_height = ImGui::GetFrameHeight() + m_theme.item_spacing * 2.0f;
        ImVec2 avail = ImGui::GetContentRegionAvail();
        
        render_settings_bar(ImVec2(avail.x, settings_height));

        float log_height = avail.y - footer_height - settings_height;
        if (log_height < 50) log_height = 50;

        render_log_window(ImVec2(avail.x, log_height));
        render_input_bar(ImVec2(avail.x, footer_height));

        ImGui::End();
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(2);
    }

    void Terminal::render_settings_bar(const ImVec2& size) 
    {
        ImGui::BeginGroup();
        
        ImGui::PushStyleColor(ImGuiCol_Button, m_theme.button_bg);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, m_theme.button_hover);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, m_theme.button_active);
        ImGui::PushStyleColor(ImGuiCol_Text, m_theme.text_default);

        if (ImGui::Button("Clear")) clear();
        
        ImGui::SameLine();
        
        // Options Menu
        if (ImGui::BeginCombo("##Options", "Options", ImGuiComboFlags_NoPreview))
        {
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2));

            ImGui::Checkbox("Auto-scroll", &m_auto_scroll);
            ImGui::Checkbox("Wrap", &m_auto_wrap);
            
            ImGui::PopStyleVar();
            ImGui::EndCombo();
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Terminal Options");

        ImGui::SameLine();

        ImGui::SetNextItemWidth(200.0f);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, m_theme.input_bg);
        ImGui::InputTextWithHint
        (
            "##Filter", 
            "Filter...", 
            m_filter_buf, 
            sizeof(m_filter_buf)
        );
        ImGui::PopStyleColor(); // InputBg

        ImGui::PopStyleColor(4); // Buttons + Text
        ImGui::EndGroup();
    }

    void Terminal::render_log_window(const ImVec2& size) 
    {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, m_theme.window_bg);
        
        ImGuiWindowFlags flags = ImGuiWindowFlags_HorizontalScrollbar;
        if (!m_auto_wrap) flags |= ImGuiWindowFlags_AlwaysHorizontalScrollbar;

        ImGui::BeginChild("##LogWindow", ImVec2(size.x, size.y), false, flags);
        
        std::lock_guard<std::mutex> lock(m_mutex);
        
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 2));

        bool has_filter = (m_filter_buf[0] != '\0');

        if (!has_filter) 
        {
            ImGuiListClipper clipper;
            clipper.Begin((int)m_messages.size());
            while (clipper.Step()) 
            {
                for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) 
                {
                    const auto& msg = m_messages[i];
                    
                    // Skip messages with invalid severity
                    if (!is_valid_severity(static_cast<int>(msg.severity)))
                    {
                        continue;
                    }
                    
                    ImVec4 color = get_severity_color(msg.severity, m_theme);

                    ImGui::PushStyleColor(ImGuiCol_Text, color);
                    if (m_auto_wrap)
                    {
                        ImGui::PushTextWrapPos(ImGui::GetContentRegionAvail().x);
                    }
                    ImGui::TextUnformatted(msg.text.c_str());
                    if (m_auto_wrap) ImGui::PopTextWrapPos();
                    ImGui::PopStyleColor();
                }
            }
        } 
        else {
             for (const auto& msg : m_messages) 
             {
                // Skip messages with invalid severity
                if (!is_valid_severity(static_cast<int>(msg.severity)))
                {
                    continue;
                }
                
                if (!pass_filter(msg)) continue;

                ImVec4 color = get_severity_color(msg.severity, m_theme);

                ImGui::PushStyleColor(ImGuiCol_Text, color);
                if (m_auto_wrap) ImGui::PushTextWrapPos(ImGui::GetContentRegionAvail().x);
                ImGui::TextUnformatted(msg.text.c_str());
                if (m_auto_wrap) ImGui::PopTextWrapPos();
                ImGui::PopStyleColor();
            }
        }

        if (m_scroll_to_bottom)
        {
            ImGui::SetScrollHereY(1.0f);
            m_scroll_to_bottom = false;
        }

        ImGui::PopStyleVar();
        ImGui::EndChild();
        ImGui::PopStyleColor();
    }

    bool Terminal::pass_filter(const Message& msg) 
    {
        // Skip invalid severity messages
        if (!is_valid_severity(static_cast<int>(msg.severity)))
        {
            return false;
        }
        
        // Text filtering only
        if (m_filter_buf[0] == '\0') return true;
        return msg.text.find(m_filter_buf) != std::string::npos;
    }

    void Terminal::render_input_bar(const ImVec2& size) 
    {
        ImGui::Separator();
        ImGui::PushStyleColor(ImGuiCol_FrameBg, m_theme.input_bg);
        ImGui::PushStyleColor(ImGuiCol_Text, m_theme.input_text);

        ImGuiInputTextFlags flags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackHistory;
        
        auto callback = [](ImGuiInputTextCallbackData* data) -> int 
            {
            Terminal* term = (Terminal*)data->UserData;
            
            if (data->EventFlag == ImGuiInputTextFlags_CallbackHistory) 
            {
                if (data->EventKey == ImGuiKey_UpArrow) 
                {
                    if (!term->m_history.empty()) 
                    {
                        term->m_history_pos++;
                         if (term->m_history_pos >= (int)term->m_history.size()) term->m_history_pos = (int)term->m_history.size() - 1;
                    }
                } else if (data->EventKey == ImGuiKey_DownArrow) 
                {
                     term->m_history_pos--;
                     if (term->m_history_pos < -1) term->m_history_pos = -1;
                }
                
                if 
                (
                    term->m_history_pos >= 0 && 
                    term->m_history_pos < term->m_history.size()
                ) 
                {
                    int idx = (int)term->m_history.size() - 1 - term->m_history_pos;
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
        ImGui::PopStyleColor(2);
    }

    void Terminal::execute_command(std::string_view cmd) 
    {
        m_history.push_back(std::string(cmd));
        add_text(std::string("> ") + std::string(cmd), Severity::Debug);
        
        std::string command_str(cmd);

        if (command_str == "clear") 
        {
            clear();
            return;
        } 
        
        if (command_str == "help") 
        {
            add_text("Available commands: clear, help, [system commands]", Severity::Debug);
            return;
        }

        // Async execution for system commands
        // Use shared_from_this pattern by capturing 'this' and checking shutdown flag
        std::thread
        (
            [this, command_str]() 
            {
                // Early exit if terminal is shutting down
                if (is_shutting_down()) return;
                
                #ifdef _WIN32
                FILE* pipe = _popen((command_str + " 2>&1").c_str(), "r");
                #else
                FILE* pipe = popen((command_str + " 2>&1").c_str(), "r");
                #endif

                if (!pipe)
                {
                    if (!is_shutting_down())
                    {
                        #ifdef _WIN32
                        char error_msg[256];
                        strerror_s(error_msg, sizeof(error_msg), errno);
                        this->add_text(std::string("Failed to start command: ") + error_msg, Severity::Error);
                        #else
                        this->add_text(std::string("Failed to start command: ") + strerror(errno), Severity::Error);
                        #endif
                    }
                    return;
                }

                char buffer[128];
                while (fgets(buffer, sizeof(buffer), pipe)) 
                {
                    // Check if terminal is shutting down
                    if (is_shutting_down()) 
                    {
                        #ifdef _WIN32
                        _pclose(pipe);
                        #else
                        pclose(pipe);
                        #endif
                        return;
                    }
                    
                    // Remove newline
                    std::string res(buffer);
                    while (!res.empty() && (res.back() == '\n' || res.back() == '\r')) 
                    {
                        res.pop_back();
                    }
                    
                    if (!is_shutting_down())
                    {
                        this->add_text(res, Severity::Debug);
                    }
                }
                
                #ifdef _WIN32
                int return_code = _pclose(pipe);
                #else
                int return_code = pclose(pipe);
                #endif

                if (!is_shutting_down())
                {
                    if (return_code != 0) 
                    {
                         this->add_text
                         (
                             "Command exited with code " + 
                             std::to_string(return_code), 
                             Severity::Warn
                         );
                    } else 
                    {
                         this->add_text("Command finished.", Severity::Debug);
                    }
                }
            }
        ).detach();
    }

}
