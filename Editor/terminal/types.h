#pragma once

#include <string>
#include <vector>
#include <optional>
#include <string_view>
#include <cstdint>
#include <imgui.h>

namespace term 
{
    enum class Severity : uint8_t
    {
        Debug,
        Warn,
        Error
    };

    struct Message 
    {
        std::string text;
        Severity severity;

        Message(std::string_view txt, Severity sev) : text(txt), severity(sev) {}
    };

    struct Theme 
    {
        // Window & Layout (Nuake-inspired)
        ImVec4 window_bg = ImVec4(0.098f, 0.098f, 0.110f, 1.0f); // #191919
        ImVec4 border_color = ImVec4(0.14f, 0.14f, 0.16f, 1.0f);
        float border_size = 1.0f;
        float window_padding = 6.0f;
        float item_spacing = 6.0f;
        float font_scale = 1.0f;

        // Text
        ImVec4 text_default = ImVec4(0.85f, 0.85f, 0.85f, 1.0f);
        
        // Severity Colors
        ImVec4 log_debug = ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
        ImVec4 log_warn = ImVec4(0.9f, 0.7f, 0.2f, 1.0f);
        ImVec4 log_error = ImVec4(0.95f, 0.35f, 0.35f, 1.0f);

        // Controls (Input, Buttons)
        ImVec4 input_bg = ImVec4(0.145f, 0.145f, 0.160f, 1.0f); // bg_frame
        ImVec4 input_text = ImVec4(0.85f, 0.85f, 0.85f, 1.0f);
        ImVec4 button_bg = ImVec4(0.145f, 0.145f, 0.160f, 1.0f);
        ImVec4 button_hover = ImVec4(0.22f, 0.22f, 0.24f, 1.0f);
        ImVec4 button_active = ImVec4(0.75f, 0.15f, 0.15f, 1.0f); // Accent
    };

}
