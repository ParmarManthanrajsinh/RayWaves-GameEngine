#pragma once

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

struct Theme
{
    ImVec4 window_bg = ImVec4(0.098f, 0.098f, 0.110f, 1.0f);
    ImVec4 border_color = ImVec4(0.14f, 0.14f, 0.16f, 1.0f);
    float border_size = 1.0f;
    float window_padding = 6.0f;
    float item_spacing = 6.0f;
    float font_scale = 1.0f;

    ImVec4 text_default = ImVec4(0.85f, 0.85f, 0.85f, 1.0f);
    ImVec4 log_debug = ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
    ImVec4 log_warn = ImVec4(0.9f, 0.7f, 0.2f, 1.0f);
    ImVec4 log_error = ImVec4(0.95f, 0.35f, 0.35f, 1.0f);

    ImVec4 ansi_30 = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
    ImVec4 ansi_31 = ImVec4(0.8f, 0.2f, 0.2f, 1.0f);
    ImVec4 ansi_32 = ImVec4(0.2f, 0.8f, 0.2f, 1.0f);
    ImVec4 ansi_33 = ImVec4(0.8f, 0.8f, 0.2f, 1.0f);
    ImVec4 ansi_34 = ImVec4(0.2f, 0.2f, 0.8f, 1.0f);
    ImVec4 ansi_35 = ImVec4(0.8f, 0.2f, 0.8f, 1.0f);
    ImVec4 ansi_36 = ImVec4(0.2f, 0.8f, 0.8f, 1.0f);
    ImVec4 ansi_37 = ImVec4(0.8f, 0.8f, 0.8f, 1.0f);
    ImVec4 ansi_90 = ImVec4(0.4f, 0.4f, 0.4f, 1.0f);
    ImVec4 ansi_91 = ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
    ImVec4 ansi_92 = ImVec4(0.3f, 1.0f, 0.3f, 1.0f);
    ImVec4 ansi_93 = ImVec4(1.0f, 1.0f, 0.3f, 1.0f);
    ImVec4 ansi_94 = ImVec4(0.3f, 0.3f, 1.0f, 1.0f);
    ImVec4 ansi_95 = ImVec4(1.0f, 0.3f, 1.0f, 1.0f);
    ImVec4 ansi_96 = ImVec4(0.3f, 1.0f, 1.0f, 1.0f);
    ImVec4 ansi_97 = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);

    ImVec4 selection_bg = ImVec4(0.2f, 0.4f, 0.8f, 0.4f);
};
} // namespace term
