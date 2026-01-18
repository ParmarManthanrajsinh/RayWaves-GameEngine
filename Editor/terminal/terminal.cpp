#include <algorithm>
#include <iostream>
#include <vector>
#include <array>
#include <mutex>
#include <thread>
#include <memory>
#include <imgui_internal.h>
#include "terminal.h"

namespace tterm 
{

    // Output Streams
    static LogStreamBuf* s_cout_buf = nullptr;
    static LogStreamBuf* s_cerr_buf = nullptr;

    Terminal* Terminal::s_Instance = nullptr;

    Terminal::Terminal() 
    {
        s_Instance = this;
    }

    Terminal::~Terminal() 
    {
        if (s_Instance == this) s_Instance = nullptr;
        
        // Restore streams
        if (m_old_cout) std::cout.rdbuf(m_old_cout);
        if (m_old_cerr) std::cerr.rdbuf(m_old_cerr);
        delete s_cout_buf;
        delete s_cerr_buf;
    }

    void Terminal::InitCapture() 
    {
        // 1. Raylib Capture
        SetTraceLogCallback(RaylibLogCallback);

        // 2. Std Output Capture
        m_old_cout = std::cout.rdbuf();
        s_cout_buf = new LogStreamBuf(this, Severity::Info);
        std::cout.rdbuf(s_cout_buf);

        m_old_cerr = std::cerr.rdbuf();
        s_cerr_buf = new LogStreamBuf(this, Severity::Error);
        std::cerr.rdbuf(s_cerr_buf);
    }

    void Terminal::RaylibLogCallback(int logLevel, const char* text, va_list args) 
    {
        if (!s_Instance) return;

        // Fix 1: Thread-safe buffer (on stack), no static
        char buffer[1024];
        vsnprintf(buffer, sizeof(buffer), text, args);

        Severity s = Severity::Info;
        switch (logLevel) 
        {
            case LOG_TRACE: s = Severity::Trace; break;
            case LOG_DEBUG: s = Severity::Debug; break;
            case LOG_INFO: s = Severity::Info; break;
            case LOG_WARNING: s = Severity::Warn; break;
            case LOG_ERROR: s = Severity::Error; break;
            case LOG_FATAL: s = Severity::Critical; break;
            default: break;
        }
        
        s_Instance->add_text(buffer, s);
    }

    void Terminal::add_text(std::string_view text, Severity severity) 
    {
        Message msg(text, severity);
        add_message(msg);
    }

    void Terminal::add_message(const Message& msg) 
    {
        // Fix 2: Mutex lock
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

    void Terminal::show(const char* window_title, bool* p_open) 
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
        if (!ImGui::Begin(window_title, p_open)) 
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
        ImGui::Checkbox("Auto-scroll", &m_auto_scroll);
        ImGui::SameLine();
        ImGui::Checkbox("Wrap", &m_auto_wrap);
        ImGui::SameLine();

        ImGui::SetNextItemWidth(150.0f);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, m_theme.input_bg);
        ImGui::InputTextWithHint
        (
            "##Filter", 
            "Filter...", 
            m_filter_buf, 
            sizeof(m_filter_buf)
        );
        ImGui::PopStyleColor(); // InputBg
        
        ImGui::SameLine();
        ImGui::SetNextItemWidth(100.0f);
        const char* levels[] = 
        {
            "Trace", "Debug", "Info", "Warn", "Error", "Critical", "None" 
        };
        int current_level = static_cast<int>(m_min_log_level);
        if (ImGui::Combo("##LogLevel", &current_level, levels, IM_ARRAYSIZE(levels))) 
        {
            m_min_log_level = static_cast<Severity>(current_level);
        }

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

        // Fix 3: ImGuiListClipper integration
        // Only use clipper if NOT filtering (or if we had a filtered list cache)
        // If sorting or filtering is active, clipping is harder.
        // For complexity simplicity: if has_filter, draw normally. If no filter, use Clipper.
        
        if (!has_filter) 
        {
            ImGuiListClipper clipper;
            clipper.Begin((int)m_messages.size());
            while (clipper.Step()) 
            {
                for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) 
                {
                    const auto& msg = m_messages[i];
                    
                    if (msg.severity < m_min_log_level) continue; // Note: Clipper + variable height skip is slightly visible glitching if many skipped.
                    // Ideally we should have a `std::vector<Message*> visible_msgs` but that requires recomputing on change.
                    // For now, this is "good enough" optimization basics.
                    
                    ImVec4 color = m_theme.text_default;
                    switch (msg.severity) 
                    {
                        case Severity::Trace: color = m_theme.log_trace; break;
                        case Severity::Debug: color = m_theme.log_debug; break;
                        case Severity::Info: color = m_theme.log_info; break;
                        case Severity::Warn: color = m_theme.log_warn; break;
                        case Severity::Error: color = m_theme.log_error; break;
                        case Severity::Critical: color = m_theme.log_critical; break;
                        default: break;
                    }

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
            // Unoptimized path for filtered results
             for (const auto& msg : m_messages) 
             {
                if (!pass_filter(msg)) continue;

                ImVec4 color = m_theme.text_default;
                switch (msg.severity)
                {
                    case Severity::Trace: color = m_theme.log_trace; break;
                    case Severity::Debug: color = m_theme.log_debug; break;
                    case Severity::Info: color = m_theme.log_info; break;
                    case Severity::Warn: color = m_theme.log_warn; break;
                    case Severity::Error: color = m_theme.log_error; break;
                    case Severity::Critical: color = m_theme.log_critical; break;
                    default: break;
                }

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
        if (msg.severity < m_min_log_level) return false;
        if (m_filter_buf[0] == '\0') return true;
        // Simple case insensitive? No, case sensitive for now
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
        add_text(std::string("> ") + std::string(cmd), Severity::Info);
        
        std::string command_str(cmd);

        if (command_str == "clear") 
        {
            clear();
            return;
        } 
        
        if (command_str == "help") 
        {
            add_text("Available commands: clear, help, [system commands]", Severity::Info);
            return;
        }

        // Fix 4: Async execution for system commands
        // Prevent UI blocking by running popen in a detached thread
        std::thread
        (
            [this, command_str]() 
            {
                #ifdef _WIN32
                #define popen _popen
                #define pclose _pclose
                #endif

                // Note: popen combines stdout/stderr usually? No, "r" only stdout.
                // To get stderr, need "2>&1" in command.
                std::string full_cmd = command_str + " 2>&1";

                FILE* pipe = popen(full_cmd.c_str(), "r");
                if (!pipe)
                {
                    this->add_text("Failed to start command.", Severity::Error);
                    return;
                }

                char buffer[128];
                while (fgets(buffer, sizeof(buffer), pipe)) 
                {
                    // Remove newline
                    std::string res(buffer);
                    while (!res.empty() && (res.back() == '\n' || res.back() == '\r')) 
                    {
                        res.pop_back();
                    }
                    // Log from background thread (add_text is thread-safe now)
                    this->add_text(res, Severity::Info);
                }
                
                int return_code = pclose(pipe);
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
                     this->add_text("Command finished.", Severity::Trace);
                }

            }
        ).detach();
    }

}
