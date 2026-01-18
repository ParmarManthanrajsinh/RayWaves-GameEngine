#pragma once

#include <string>
#include <vector>
#include <optional>
#include <string_view>
#include <imgui.h>

namespace tterm {

    enum class Severity {
        Trace,
        Debug,
        Info,
        Warn,
        Error,
        Critical,
        None
    };

    enum class AutocompletePos {
        Above,
        Below,
        Hidden
    };

    struct ColorRange {
        size_t start;
        size_t end;
        ImVec4 color;
    };

    struct Message {
        std::string text;
        Severity severity;
        std::vector<ColorRange> color_ranges;

        Message(std::string_view txt, Severity sev) : text(txt), severity(sev) {}
        Message(std::string_view txt, Severity sev, const std::vector<ColorRange>& ranges) 
            : text(txt), severity(sev), color_ranges(ranges) {}
    };

    struct Theme {
        // Window & Layout
        ImVec4 window_bg = ImVec4(0.08f, 0.08f, 0.08f, 1.0f);
        ImVec4 border_color = ImVec4(0.25f, 0.25f, 0.25f, 1.0f);
        float border_size = 1.0f;
        float window_padding = 4.0f;
        float item_spacing = 3.0f;
        float font_scale = 0.85f;

        // Text
        ImVec4 text_default = ImVec4(0.9f, 0.9f, 0.9f, 1.0f);
        
        // Severity Colors
        ImVec4 log_trace = ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
        ImVec4 log_debug = ImVec4(0.4f, 0.7f, 1.0f, 1.0f);
        ImVec4 log_info = ImVec4(0.9f, 0.9f, 0.9f, 1.0f);
        ImVec4 log_warn = ImVec4(1.0f, 0.8f, 0.2f, 1.0f);
        ImVec4 log_error = ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
        ImVec4 log_critical = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);

        // Controls (Input, Buttons)
        ImVec4 input_bg = ImVec4(0.12f, 0.12f, 0.12f, 1.0f);
        ImVec4 input_text = ImVec4(0.9f, 0.9f, 0.9f, 1.0f);
        ImVec4 button_bg = ImVec4(0.2f, 0.2f, 0.2f, 1.0f);
        ImVec4 button_hover = ImVec4(0.3f, 0.3f, 0.3f, 1.0f);
        ImVec4 button_active = ImVec4(0.15f, 0.15f, 0.15f, 1.0f);
        
        // Auto-completeWindow
        ImVec4 autocomplete_bg = ImVec4(0.15f, 0.15f, 0.15f, 0.95f);
        ImVec4 autocomplete_text = ImVec4(0.85f, 0.85f, 0.85f, 1.0f);
        ImVec4 autocomplete_selected = ImVec4(0.3f, 0.4f, 0.6f, 1.0f);
        
        AutocompletePos autocomplete_position = AutocompletePos::Above;

        // Highlight
        ImVec4 filter_match_highlight = ImVec4(1.0f, 1.0f, 0.2f, 0.4f);
    };

}
