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
inline ImVec4 GetAccentColor()        { return ImVec4(0.839f, 0.188f, 0.192f, 1.00f); } // #D63031
inline ImVec4 GetAccentHoverColor()   { return ImVec4(0.939f, 0.288f, 0.292f, 1.00f); }
inline ImVec4 GetAccentActiveColor()  { return ImVec4(0.739f, 0.088f, 0.092f, 1.00f); }
inline ImU32  GetAccentU32()          { return IM_COL32(214, 48, 49, 255); }
inline ImU32  GetAccentDimU32()       { return IM_COL32(214, 48, 49, 100); }

// ============================================================================
//  Theme Presets
// ============================================================================
struct FThemePreset
{
    std::string Name;
    ImVec4 BgBase, BgPanel, BgFrame, BgHeader, BgElevated;
    ImVec4 Accent, AccentHover, AccentActive, AccentDim;
    ImVec4 TextPrimary, TextSecondary;
    ImVec4 Hover, Active, Select;
    ImVec4 Border, BorderLight;
};

inline const std::vector<FThemePreset>& GetThemePresets()
{
    static std::vector<FThemePreset> presets = {
        {
            "Charcoal",
            ImVec4(0.082f, 0.094f, 0.118f, 1.00f), ImVec4(0.102f, 0.110f, 0.137f, 1.00f), ImVec4(0.125f, 0.137f, 0.165f, 1.00f), ImVec4(0.145f, 0.157f, 0.188f, 1.00f), ImVec4(0.165f, 0.177f, 0.208f, 1.00f),
            GetAccentColor(), GetAccentHoverColor(), GetAccentActiveColor(), ImVec4(0.839f, 0.188f, 0.192f, 0.40f),
            ImVec4(0.90f, 0.90f, 0.92f, 1.00f), ImVec4(0.60f, 0.62f, 0.65f, 1.00f),
            ImVec4(0.22f, 0.23f, 0.26f, 1.00f), ImVec4(0.839f, 0.188f, 0.192f, 0.60f), ImVec4(0.839f, 0.188f, 0.192f, 0.30f),
            ImVec4(0.18f, 0.20f, 0.23f, 1.00f), ImVec4(0.24f, 0.26f, 0.29f, 0.50f)
        },
        {
            "Light",
            ImVec4(0.95f, 0.95f, 0.95f, 1.00f), ImVec4(0.90f, 0.90f, 0.90f, 1.00f), ImVec4(0.85f, 0.85f, 0.85f, 1.00f), ImVec4(0.80f, 0.80f, 0.80f, 1.00f), ImVec4(0.88f, 0.88f, 0.88f, 1.00f),
            GetAccentColor(), GetAccentHoverColor(), GetAccentActiveColor(), ImVec4(0.839f, 0.188f, 0.192f, 0.40f),
            ImVec4(0.10f, 0.10f, 0.10f, 1.00f), ImVec4(0.30f, 0.30f, 0.30f, 1.00f),
            ImVec4(0.75f, 0.75f, 0.75f, 1.00f), ImVec4(0.839f, 0.188f, 0.192f, 0.60f), ImVec4(0.839f, 0.188f, 0.192f, 0.30f),
            ImVec4(0.70f, 0.70f, 0.70f, 1.00f), ImVec4(0.60f, 0.60f, 0.60f, 0.50f)
        }
    };
    return presets;
}

// ============================================================================
//  SetEngineTheme — call once after rlImGuiSetup
// ============================================================================
inline void SetEngineTheme
(
    const FThemePreset& preset = GetThemePresets()[0],
    float gui_scale = 1.0f,
    std::string_view base_font_path = "Assets/EngineContent/Roboto-Regular.ttf",
    std::string_view mono_font_path = "Assets/EngineContent/Consolas-Regular.ttf",
    std::string_view icon_font_path = "Assets/EngineContent/fa-solid-900.ttf"
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
    if (std::filesystem::exists(base_font_path))
    {
        if (!io.Fonts->AddFontFromFileTTF(base_font_path.data(), 17.0f * gui_scale, &font_config))
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
    std::string icon_path = std::string(icon_font_path);
    if (std::filesystem::exists(icon_path))
    {
        io.Fonts->AddFontFromFileTTF(icon_path.c_str(), 17.0f * gui_scale, &icons_config, icons_ranges);
    }

    // Font 1: Roboto 24px (large headers)
    if (std::filesystem::exists(base_font_path))
    {
        if (!io.Fonts->AddFontFromFileTTF(base_font_path.data(), 24.0f * gui_scale, &font_config))
            io.Fonts->AddFontDefault();
    }
    else
    {
        io.Fonts->AddFontDefault();
    }

    // Font 2: Monospace 15px (terminal)
    if (std::filesystem::exists(mono_font_path))
    {
        if (!io.Fonts->AddFontFromFileTTF(mono_font_path.data(), 15.0f * gui_scale, &font_config))
            io.Fonts->AddFontDefault();
    }
    else
    {
        io.Fonts->AddFontDefault();
    }

    // Font 3: Monospace 13px (message log, small code)
    if (std::filesystem::exists(mono_font_path))
    {
        if (!io.Fonts->AddFontFromFileTTF(mono_font_path.data(), 13.0f * gui_scale, &font_config))
            io.Fonts->AddFontDefault();
    }
    else
    {
        io.Fonts->AddFontDefault();
    }

    rlImGuiReloadFonts();

    // ════════════════════════════════════════════════════════════════════
    //  APPLY COLORS
    // ════════════════════════════════════════════════════════════════════
    ImGuiStyle style;
    if (preset.BgBase.x > 0.5f)
        ImGui::StyleColorsLight(&style);
    else
        ImGui::StyleColorsDark(&style);
    
    style.Colors[ImGuiCol_Text]                 = preset.TextPrimary;
    style.Colors[ImGuiCol_TextDisabled]         = preset.TextSecondary;
    style.Colors[ImGuiCol_WindowBg]             = preset.BgBase;
    style.Colors[ImGuiCol_ChildBg]              = ImVec4(0, 0, 0, 0); // Transparent children
    style.Colors[ImGuiCol_PopupBg]              = ImVec4(preset.BgBase.x, preset.BgBase.y, preset.BgBase.z, 0.96f);
    style.Colors[ImGuiCol_Border]               = preset.Border;
    style.Colors[ImGuiCol_BorderShadow]         = ImVec4(0, 0, 0, 0);

    // Frames & Controls
    style.Colors[ImGuiCol_FrameBg]              = preset.BgFrame;
    style.Colors[ImGuiCol_FrameBgHovered]       = preset.Hover;
    style.Colors[ImGuiCol_FrameBgActive]        = preset.Active;
    style.Colors[ImGuiCol_CheckMark]            = preset.AccentActive;
    style.Colors[ImGuiCol_CheckboxSelectedBg]   = preset.BgFrame;
    style.Colors[ImGuiCol_SliderGrab]           = preset.Accent;
    style.Colors[ImGuiCol_SliderGrabActive]     = preset.AccentActive;

    // Buttons
    style.Colors[ImGuiCol_Button]               = preset.BgFrame;
    style.Colors[ImGuiCol_ButtonHovered]        = preset.Hover;
    style.Colors[ImGuiCol_ButtonActive]         = preset.Accent;

    // Tabs
    style.Colors[ImGuiCol_Tab]                  = preset.BgPanel;
    style.Colors[ImGuiCol_TabHovered]           = preset.Hover;
    style.Colors[ImGuiCol_TabActive]            = preset.BgBase;
    style.Colors[ImGuiCol_TabUnfocused]         = preset.BgPanel;
    style.Colors[ImGuiCol_TabUnfocusedActive]   = preset.BgBase;

    // Headers (collapsing, tree nodes, selectables)
    style.Colors[ImGuiCol_Header]               = preset.BgHeader;
    style.Colors[ImGuiCol_HeaderHovered]        = preset.Hover;
    style.Colors[ImGuiCol_HeaderActive]         = preset.Accent;

    // Scrollbar — very subtle, only visible on hover
    style.Colors[ImGuiCol_ScrollbarBg]          = ImVec4(0, 0, 0, 0.02f);
    style.Colors[ImGuiCol_ScrollbarGrab]        = ImVec4(0.30f, 0.30f, 0.32f, 0.60f);
    style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.40f, 0.40f, 0.42f, 0.80f);
    style.Colors[ImGuiCol_ScrollbarGrabActive]  = preset.Accent;

    // Selection & highlights
    style.Colors[ImGuiCol_TextSelectedBg]       = preset.Select;
    style.Colors[ImGuiCol_DragDropTarget]       = preset.Accent;
    style.Colors[ImGuiCol_NavHighlight]         = preset.Accent;

    // Title bars — blend with window bg
    style.Colors[ImGuiCol_TitleBg]              = preset.BgBase;
    style.Colors[ImGuiCol_TitleBgActive]        = preset.BgPanel;
    style.Colors[ImGuiCol_TitleBgCollapsed]     = preset.BgBase;

    // Menu bar
    style.Colors[ImGuiCol_MenuBarBg]            = preset.BgBase;

    // Separators
    style.Colors[ImGuiCol_Separator]            = preset.Border;
    style.Colors[ImGuiCol_SeparatorHovered]     = preset.AccentHover;
    style.Colors[ImGuiCol_SeparatorActive]      = preset.Accent;

    // Resize grips
    style.Colors[ImGuiCol_ResizeGrip]           = ImVec4(preset.Accent.x, preset.Accent.y, preset.Accent.z, 0.20f);
    style.Colors[ImGuiCol_ResizeGripHovered]    = ImVec4(preset.AccentHover.x, preset.AccentHover.y, preset.AccentHover.z, 0.60f);
    style.Colors[ImGuiCol_ResizeGripActive]     = preset.Accent;

    // Docking
    style.Colors[ImGuiCol_DockingPreview]       = preset.AccentDim;
    style.Colors[ImGuiCol_DockingEmptyBg]       = preset.BgBase;

    // Modal overlay
    style.Colors[ImGuiCol_NavWindowingHighlight]= ImVec4(1.0f, 1.0f, 1.0f, 0.70f);
    style.Colors[ImGuiCol_NavWindowingDimBg]    = ImVec4(0.0f, 0.0f, 0.0f, 0.60f);
    style.Colors[ImGuiCol_ModalWindowDimBg]     = ImVec4(0.0f, 0.0f, 0.0f, 0.70f);

    // Plots
    style.Colors[ImGuiCol_PlotLines]            = preset.Accent;
    style.Colors[ImGuiCol_PlotLinesHovered]     = preset.AccentActive;
    style.Colors[ImGuiCol_PlotHistogram]        = preset.AccentHover;
    style.Colors[ImGuiCol_PlotHistogramHovered]  = preset.AccentActive;

    // Tables
    style.Colors[ImGuiCol_TableHeaderBg]        = preset.BgHeader;
    style.Colors[ImGuiCol_TableBorderStrong]    = preset.Border;
    style.Colors[ImGuiCol_TableBorderLight]     = preset.BorderLight;
    style.Colors[ImGuiCol_TableRowBg]           = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    style.Colors[ImGuiCol_TableRowBgAlt]        = ImVec4(preset.TextPrimary.x, preset.TextPrimary.y, preset.TextPrimary.z, 0.05f);

    // ════════════════════════════════════════════════════════════════════
    //  GEOMETRY — compact, professional (scaled)
    // ════════════════════════════════════════════════════════════════════

    // First reset to unscaled base
    style.WindowPadding     = ImVec2(6.0f, 6.0f);
    style.FramePadding      = ImVec2(6.0f, 4.0f);
    style.ItemSpacing       = ImVec2(6.0f, 4.0f);
    style.ItemInnerSpacing  = ImVec2(4.0f, 4.0f);
    style.CellPadding       = ImVec2(4.0f, 2.0f);
    style.IndentSpacing     = 16.0f;
    style.ScrollbarSize     = 10.0f;
    style.GrabMinSize       = 8.0f;
    style.WindowBorderSize  = 1.0f;
    style.ChildBorderSize   = 0.0f;
    style.PopupBorderSize   = 1.0f;
    style.FrameBorderSize   = 0.0f;
    style.TabBorderSize     = 0.0f;
    style.WindowRounding    = 2.0f;
    style.FrameRounding     = 2.0f;
    style.ScrollbarRounding = 2.0f;
    style.GrabRounding      = 2.0f;
    style.TabRounding       = 2.0f;
    style.ChildRounding     = 1.0f;
    style.PopupRounding     = 3.0f;

    // Apply scale
    style.ScaleAllSizes(gui_scale);

    // Alignment
    style.WindowTitleAlign  = ImVec2(0.0f, 0.5f); // Left-aligned titles (like reference)
    style.ButtonTextAlign   = ImVec2(0.5f, 0.5f);

    // Window menu button
    style.WindowMenuButtonPosition = ImGuiDir_None; // Hide the collapse arrow

    ImGui::GetStyle() = style;
}
