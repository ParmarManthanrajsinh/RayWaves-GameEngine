#pragma once

#include "types.h"
#include <vector>
#include <string>
#include <string_view>
#include <atomic>
#include <deque>
#include <functional>
#include <optional>
#include <mutex>
#include <thread>
#include <iostream>
#include <sstream>
#include <imgui.h>
#include <raylib.h>

#ifdef IMTERM_ENABLE_REGEX
#include <regex>
#endif

namespace tterm 
{

    class Terminal 
    {
    public:
        Terminal();
        ~Terminal();

        // Main render function
        void show(std::string_view window_title = "Terminal", bool* p_open = nullptr);

        // API
        void add_text(std::string_view text, Severity severity = Severity::Debug);
        void add_message(const Message& msg);
        void clear();

        Theme& theme() { return m_theme; }
        
        // Output Capture Setup
        void InitCapture(); // Hides std::cout and hooks Raylib

    public:
        // Static access for C-style callbacks (Raylib)
        static Terminal* s_Instance;
        static void RaylibLogCallback(int logLevel, const char* text, va_list args);

    private:
        // Thread safety
        std::mutex m_mutex;

        // Logs
        std::deque<Message> m_messages;
        size_t m_max_log_size = 5000;

        // UI State
        char m_input_buf[1024] = "";
        char m_filter_buf[128] = "";
        bool m_auto_scroll = true;
        bool m_auto_wrap = true;
        bool m_scroll_to_bottom = false;

        // History
        std::vector<std::string> m_history;
        int m_history_pos = -1;

        // Theme
        Theme m_theme;

        // Capture State
        std::streambuf* m_old_cout = nullptr;
        std::streambuf* m_old_cerr = nullptr;

        // Helper Methods
        void render_settings_bar(const ImVec2& size);
        void render_log_window(const ImVec2& size);
        void render_input_bar(const ImVec2& size);
        void execute_command(std::string_view cmd);
        bool pass_filter(const Message& msg);
        
        // Validation helpers
        static bool is_valid_severity(int severity_value);
        static ImVec4 get_severity_color(Severity severity, const Theme& theme);
    };

    // Custom stream buffer to capture std::cout/cerr
    class LogStreamBuf : public std::stringbuf 
    {
    public:
        LogStreamBuf(Terminal* term, Severity severity) 
            : m_term(term), 
              m_severity(severity) {}
        
        virtual int sync() override 
        {
            std::string text = this->str();
            if (!text.empty()) 
            {
                // Remove trailing newlines often sent by endl
                if (text.back() == '\n')
                {
                    text.pop_back();
                }
                if (!text.empty()) m_term->add_text(text, m_severity);
                
                this->str(""); // Clear buffer
            }
            return 0;
        }
    private:
        Terminal* m_term;
        Severity m_severity;
    };

} 