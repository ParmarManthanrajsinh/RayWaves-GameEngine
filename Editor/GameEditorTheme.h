#pragma once
#include <string>
#include <imgui.h>
#include <rlImGui.h>
#include <filesystem>
#include "../Editor/rlImGui/extras/IconsFontAwesome6.h"

// ============================================================================
//  Font indices — use these to push fonts in draw code
// ============================================================================
enum EEditorFont : int
{
    Font_Default    = 0,   // Roboto 17px  + FA icons merged
    Font_Large      = 1,   // Roboto 24px  (section titles / big FPS)
    Font_Mono       = 2,   // Consolas 15px (terminal / code)
    Font_MonoSmall  = 3,   // Consolas 13px (message log)
};

// ============================================================================
//  Accent color accessors  (so other code can reference the theme)
// ============================================================================
inline ImVec4 GetAccentColor()        { return ImVec4(0.75f, 0.15f, 0.15f, 1.00f); }
inline ImVec4 GetAccentHoverColor()   { return ImVec4(0.88f, 0.28f, 0.28f, 1.00f); }
inline ImVec4 GetAccentActiveColor()  { return ImVec4(0.95f, 0.35f, 0.35f, 1.00f); }
inline ImU32  GetAccentU32()          { return IM_COL32(191, 38, 38, 255); }
inline ImU32  GetAccentDimU32()       { return IM_COL32(191, 38, 38, 100); }

// ============================================================================
//  SetEngineTheme — call once after rlImGuiSetup
// ============================================================================
inline void SetEngineTheme
(
    std::string_view path      = "Assets/EngineContent/Roboto-Regular.ttf",
    std::string_view mono_path = "Assets/EngineContent/Consolas-Regular.ttf"
)
{
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    io.Fonts->Clear();

    // ── Base font config ────────────────────────────────────────────────
    ImFontConfig font_config;
    font_config.OversampleH = 2;
    font_config.OversampleV = 1;
    font_config.PixelSnapH  = true;
    font_config.RasterizerMultiply = 1.10f;
    font_config.Flags |= ImFontFlags_NoLoadError;

    // Font 0: Roboto 17px (main UI)
    if (std::filesystem::exists(path))
    {
        if (!io.Fonts->AddFontFromFileTTF(path.data(), 17.0f, &font_config))
            io.Fonts->AddFontDefault();
    }
    else
    {
        io.Fonts->AddFontDefault();
    }

    // Merge FontAwesome icons into Font 0
    ImFontConfig icons_config;
    icons_config.MergeMode   = true;
    icons_config.PixelSnapH  = true;
    icons_config.OversampleH = 2;
    icons_config.OversampleV = 1;
    icons_config.RasterizerMultiply = 1.10f;
    icons_config.Flags |= ImFontFlags_NoLoadError;

    static const ImWchar icons_ranges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
    std::string icon_path = "Assets/EngineContent/" FONT_ICON_FILE_NAME_FAS;
    if (std::filesystem::exists(icon_path))
    {
        io.Fonts->AddFontFromFileTTF(icon_path.c_str(), 17.0f, &icons_config, icons_ranges);
    }

    // Font 1: Roboto 24px (large headers)
    if (std::filesystem::exists(path))
    {
        if (!io.Fonts->AddFontFromFileTTF(path.data(), 24.0f, &font_config))
            io.Fonts->AddFontDefault();
    }
    else
    {
        io.Fonts->AddFontDefault();
    }

    // Font 2: Monospace 15px (terminal)
    if (std::filesystem::exists(mono_path))
    {
        if (!io.Fonts->AddFontFromFileTTF(mono_path.data(), 15.0f, &font_config))
            io.Fonts->AddFontDefault();
    }
    else
    {
        io.Fonts->AddFontDefault();
    }

    // Font 3: Monospace 13px (message log, small code)
    if (std::filesystem::exists(mono_path))
    {
        if (!io.Fonts->AddFontFromFileTTF(mono_path.data(), 13.0f, &font_config))
            io.Fonts->AddFontDefault();
    }
    else
    {
        io.Fonts->AddFontDefault();
    }

    rlImGuiReloadFonts();

    // ════════════════════════════════════════════════════════════════════
    //  COLOR PALETTE — Nuake-inspired deep charcoal
    // ════════════════════════════════════════════════════════════════════
    ImGuiStyle& style = ImGui::GetStyle();

    // Background layers (darkest → lightest)
    ImVec4 bg_base     = ImVec4(0.098f, 0.098f, 0.110f, 1.00f); // #191919 — main window
    ImVec4 bg_panel    = ImVec4(0.118f, 0.118f, 0.133f, 1.00f); // #1E1E22 — child/panel
    ImVec4 bg_frame    = ImVec4(0.145f, 0.145f, 0.160f, 1.00f); // #252529 — input frames
    ImVec4 bg_header   = ImVec4(0.165f, 0.165f, 0.180f, 1.00f); // #2A2A2E — headers
    ImVec4 bg_elevated = ImVec4(0.200f, 0.200f, 0.215f, 1.00f); // #333337 — elevated

    // Accent (subdued ruby red)
    ImVec4 accent         = GetAccentColor();
    ImVec4 accent_hover   = GetAccentHoverColor();
    ImVec4 accent_active  = GetAccentActiveColor();
    ImVec4 accent_dim     = ImVec4(0.75f, 0.15f, 0.15f, 0.40f);

    // Text
    ImVec4 text_primary   = ImVec4(0.85f, 0.85f, 0.85f, 1.00f);
    ImVec4 text_secondary = ImVec4(0.55f, 0.55f, 0.55f, 1.00f);

    // Interactive overlays
    ImVec4 hover  = ImVec4(0.22f, 0.22f, 0.24f, 1.00f);
    ImVec4 active = ImVec4(0.75f, 0.15f, 0.15f, 0.60f);
    ImVec4 select = ImVec4(0.75f, 0.15f, 0.15f, 0.30f);

    // Border
    ImVec4 border       = ImVec4(0.14f, 0.14f, 0.16f, 1.00f);
    ImVec4 border_light = ImVec4(0.20f, 0.20f, 0.22f, 0.50f);

    // ════════════════════════════════════════════════════════════════════
    //  APPLY COLORS
    // ════════════════════════════════════════════════════════════════════

    // Window
    style.Colors[ImGuiCol_Text]                 = text_primary;
    style.Colors[ImGuiCol_TextDisabled]         = text_secondary;
    style.Colors[ImGuiCol_WindowBg]             = bg_base;
    style.Colors[ImGuiCol_ChildBg]              = ImVec4(0, 0, 0, 0); // Transparent children
    style.Colors[ImGuiCol_PopupBg]              = ImVec4(0.10f, 0.10f, 0.12f, 0.96f);
    style.Colors[ImGuiCol_Border]               = border;
    style.Colors[ImGuiCol_BorderShadow]         = ImVec4(0, 0, 0, 0);

    // Frames & Controls
    style.Colors[ImGuiCol_FrameBg]              = bg_frame;
    style.Colors[ImGuiCol_FrameBgHovered]       = hover;
    style.Colors[ImGuiCol_FrameBgActive]        = active;
    style.Colors[ImGuiCol_CheckMark]            = accent_active;
    style.Colors[ImGuiCol_CheckboxSelectedBg]   = bg_frame;
    style.Colors[ImGuiCol_SliderGrab]           = accent;
    style.Colors[ImGuiCol_SliderGrabActive]     = accent_active;

    // Buttons
    style.Colors[ImGuiCol_Button]               = bg_frame;
    style.Colors[ImGuiCol_ButtonHovered]        = hover;
    style.Colors[ImGuiCol_ButtonActive]         = accent;

    // Tabs
    style.Colors[ImGuiCol_Tab]                  = bg_panel;
    style.Colors[ImGuiCol_TabHovered]           = hover;
    style.Colors[ImGuiCol_TabActive]            = bg_base;
    style.Colors[ImGuiCol_TabUnfocused]         = bg_panel;
    style.Colors[ImGuiCol_TabUnfocusedActive]   = bg_base;

    // Headers (collapsing, tree nodes, selectables)
    style.Colors[ImGuiCol_Header]               = bg_header;
    style.Colors[ImGuiCol_HeaderHovered]        = hover;
    style.Colors[ImGuiCol_HeaderActive]         = accent;

    // Scrollbar — very subtle, only visible on hover
    style.Colors[ImGuiCol_ScrollbarBg]          = ImVec4(0, 0, 0, 0.02f);
    style.Colors[ImGuiCol_ScrollbarGrab]        = ImVec4(0.30f, 0.30f, 0.32f, 0.60f);
    style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.40f, 0.40f, 0.42f, 0.80f);
    style.Colors[ImGuiCol_ScrollbarGrabActive]  = accent;

    // Selection & highlights
    style.Colors[ImGuiCol_TextSelectedBg]       = select;
    style.Colors[ImGuiCol_DragDropTarget]       = accent;
    style.Colors[ImGuiCol_NavHighlight]         = accent;

    // Title bars — blend with window bg
    style.Colors[ImGuiCol_TitleBg]              = bg_base;
    style.Colors[ImGuiCol_TitleBgActive]        = ImVec4(0.12f, 0.12f, 0.14f, 1.00f);
    style.Colors[ImGuiCol_TitleBgCollapsed]     = bg_base;

    // Menu bar
    style.Colors[ImGuiCol_MenuBarBg]            = bg_base;

    // Separators
    style.Colors[ImGuiCol_Separator]            = border;
    style.Colors[ImGuiCol_SeparatorHovered]     = accent_hover;
    style.Colors[ImGuiCol_SeparatorActive]      = accent;

    // Resize grips
    style.Colors[ImGuiCol_ResizeGrip]           = ImVec4(0.75f, 0.15f, 0.15f, 0.20f);
    style.Colors[ImGuiCol_ResizeGripHovered]    = ImVec4(0.88f, 0.28f, 0.28f, 0.60f);
    style.Colors[ImGuiCol_ResizeGripActive]     = accent;

    // Docking
    style.Colors[ImGuiCol_DockingPreview]       = accent_dim;
    style.Colors[ImGuiCol_DockingEmptyBg]       = bg_base;

    // Modal overlay
    style.Colors[ImGuiCol_NavWindowingHighlight]= ImVec4(1.0f, 1.0f, 1.0f, 0.70f);
    style.Colors[ImGuiCol_NavWindowingDimBg]    = ImVec4(0.0f, 0.0f, 0.0f, 0.60f);
    style.Colors[ImGuiCol_ModalWindowDimBg]     = ImVec4(0.0f, 0.0f, 0.0f, 0.70f);

    // Plots
    style.Colors[ImGuiCol_PlotLines]            = accent;
    style.Colors[ImGuiCol_PlotLinesHovered]     = accent_active;
    style.Colors[ImGuiCol_PlotHistogram]        = accent_hover;
    style.Colors[ImGuiCol_PlotHistogramHovered]  = accent_active;

    // Tables
    style.Colors[ImGuiCol_TableHeaderBg]        = bg_header;
    style.Colors[ImGuiCol_TableBorderStrong]    = border;
    style.Colors[ImGuiCol_TableBorderLight]     = border_light;
    style.Colors[ImGuiCol_TableRowBg]           = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    style.Colors[ImGuiCol_TableRowBgAlt]        = ImVec4(1.0f, 1.0f, 1.0f, 0.02f);

    // ════════════════════════════════════════════════════════════════════
    //  GEOMETRY — compact, professional
    // ════════════════════════════════════════════════════════════════════

    // Minimal rounding — clean, flat aesthetic
    style.WindowRounding    = 2.0f;
    style.FrameRounding     = 2.0f;
    style.ScrollbarRounding = 2.0f;
    style.GrabRounding      = 2.0f;
    style.TabRounding       = 2.0f;
    style.ChildRounding     = 1.0f;
    style.PopupRounding     = 3.0f;

    // Thin borders
    style.WindowBorderSize  = 1.0f;
    style.ChildBorderSize   = 0.0f;
    style.PopupBorderSize   = 1.0f;
    style.FrameBorderSize   = 0.0f;
    style.TabBorderSize     = 0.0f;

    // Compact spacing
    style.WindowPadding     = ImVec2(6.0f, 6.0f);
    style.FramePadding      = ImVec2(6.0f, 4.0f);
    style.ItemSpacing       = ImVec2(6.0f, 4.0f);
    style.ItemInnerSpacing  = ImVec2(4.0f, 4.0f);
    style.CellPadding       = ImVec2(4.0f, 2.0f);
    style.IndentSpacing     = 16.0f;

    // Subtle scrollbar
    style.ScrollbarSize     = 10.0f;
    style.GrabMinSize       = 8.0f;

    // Alignment
    style.WindowTitleAlign  = ImVec2(0.0f, 0.5f); // Left-aligned titles (like reference)
    style.ButtonTextAlign   = ImVec2(0.5f, 0.5f);

    // Window menu button
    style.WindowMenuButtonPosition = ImGuiDir_None; // Hide the collapse arrow
}