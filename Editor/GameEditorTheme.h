#pragma once
#include <string>
#include <imgui.h>
#include "../Editor/rlImGui/extras/IconsFontAwesome6.h"
#include <rlImGui.h>

inline void SetEngineTheme
(
    std::string_view path = "Assets/EngineContent/Roboto-Regular.ttf"
)
{
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    io.Fonts->Clear();

    ImFontConfig font_config;
    font_config.OversampleH = 2;
    font_config.OversampleV = 1;
    font_config.PixelSnapH = true;
    font_config.RasterizerMultiply = 1.15f;

    // Load base font (size 20)
    io.Fonts->AddFontFromFileTTF(path.data(), 20.0f, &font_config);

    // Merge FontAwesome icons
    ImFontConfig icons_config;
    icons_config.MergeMode = true;
    icons_config.PixelSnapH = true;
    icons_config.OversampleH = 2;
    icons_config.OversampleV = 1;
    icons_config.RasterizerMultiply = 1.15f;
    
    static const ImWchar icons_ranges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
    
    // Try to load FontAwesome Solid from the same directory structure
    // We assume the file is in Assets/EngineContent/
    std::string icon_path = "Assets/EngineContent/" FONT_ICON_FILE_NAME_FAS;
    
    io.Fonts->AddFontFromFileTTF(icon_path.c_str(), 22.0f, &icons_config, icons_ranges);

    // Load larger size (26)
    // Note: We are not merging icons for the large font here for simplicity, 
    // but if needed we would repeat the merge process.
    io.Fonts->AddFontFromFileTTF(path.data(), 26.0f, &font_config);

    rlImGuiReloadFonts();

    ImGuiStyle& style = ImGui::GetStyle();

    // === MODERN RED THEME ===
    // Deep charcoal blacks
    ImVec4 bg_0 = ImVec4(0.08f, 0.08f, 0.09f, 1.00f); // Main window
    ImVec4 bg_1 = ImVec4(0.12f, 0.12f, 0.13f, 1.00f); // Panels
    ImVec4 bg_2 = ImVec4(0.16f, 0.16f, 0.17f, 1.00f); // Frames
    ImVec4 bg_3 = ImVec4(0.20f, 0.20f, 0.21f, 1.00f); // Headers

    // Modern Red Accents
    ImVec4 accent_primary = ImVec4(0.83f, 0.18f, 0.18f, 1.00f); // Vibrant Ruby Red
    ImVec4 accent_secondary = ImVec4(0.94f, 0.33f, 0.31f, 1.00f); // Lighter Red
    ImVec4 accent_highlight = ImVec4(1.00f, 0.40f, 0.40f, 1.00f); // Bright Red

    // Text
    ImVec4 text_primary = ImVec4(0.90f, 0.90f, 0.90f, 1.00f); 
    ImVec4 text_disabled = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);

    // Hover/active/select overlays
    ImVec4 hover = ImVec4(0.24f, 0.24f, 0.25f, 1.00f); 
    ImVec4 active = ImVec4(0.83f, 0.18f, 0.18f, 0.70f);
    ImVec4 select = ImVec4(0.83f, 0.18f, 0.18f, 0.40f);

    // === Apply Colors ===
    style.Colors[ImGuiCol_Text] = text_primary;
    style.Colors[ImGuiCol_TextDisabled] = text_disabled;
    style.Colors[ImGuiCol_WindowBg] = bg_0;
    style.Colors[ImGuiCol_ChildBg] = bg_1;
    style.Colors[ImGuiCol_PopupBg] = bg_0;
    style.Colors[ImGuiCol_Border] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);

    // Frames & Controls
    style.Colors[ImGuiCol_FrameBg] = bg_2;
    style.Colors[ImGuiCol_FrameBgHovered] = hover;
    style.Colors[ImGuiCol_FrameBgActive] = active;
    style.Colors[ImGuiCol_CheckMark] = accent_highlight;
    style.Colors[ImGuiCol_CheckboxSelectedBg] = bg_0;
    style.Colors[ImGuiCol_SliderGrab] = accent_primary;
    style.Colors[ImGuiCol_SliderGrabActive] = accent_highlight;

    // Buttons
    style.Colors[ImGuiCol_Button] = bg_2;
    style.Colors[ImGuiCol_ButtonHovered] = hover;
    style.Colors[ImGuiCol_ButtonActive] = accent_primary;

    // Tabs
    style.Colors[ImGuiCol_Tab] = bg_2;
    style.Colors[ImGuiCol_TabHovered] = hover;
    style.Colors[ImGuiCol_TabActive] = accent_primary;
    style.Colors[ImGuiCol_TabUnfocused] = bg_2;
    style.Colors[ImGuiCol_TabUnfocusedActive] = accent_secondary;

    // Headers
    style.Colors[ImGuiCol_Header] = bg_3;
    style.Colors[ImGuiCol_HeaderHovered] = hover;
    style.Colors[ImGuiCol_HeaderActive] = accent_primary;

    // Scrollbars
    style.Colors[ImGuiCol_ScrollbarBg] = bg_0;
    style.Colors[ImGuiCol_ScrollbarGrab] = bg_3;
    style.Colors[ImGuiCol_ScrollbarGrabHovered] = hover;  
    style.Colors[ImGuiCol_ScrollbarGrabActive] = accent_primary;

    // Highlights
    style.Colors[ImGuiCol_TextSelectedBg] = select;
    style.Colors[ImGuiCol_DragDropTarget] = accent_primary;
    style.Colors[ImGuiCol_NavHighlight] = accent_primary;

    // === Modern Styling ===
    style.WindowRounding = 4.0f;
    style.FrameRounding = 4.0f;
    style.ScrollbarRounding = 4.0f;
    style.GrabRounding = 4.0f;
    style.TabRounding = 4.0f;

    style.WindowBorderSize = 1.0f;
    style.ChildBorderSize = 1.0f;
    style.PopupBorderSize = 1.0f;
    style.FrameBorderSize = 1.0f;
    style.TabBorderSize = 1.0f;

    style.WindowPadding = ImVec2(10.0f, 10.0f);
    style.FramePadding = ImVec2(10.0f, 6.0f);
    style.ItemSpacing = ImVec2(10.0f, 6.0f);
    style.ItemInnerSpacing = ImVec2(6.0f, 4.0f);
    style.ScrollbarSize = 14.0f;
    style.GrabMinSize = 12.0f;

    style.WindowTitleAlign = ImVec2(0.5f, 0.5f);
    style.ButtonTextAlign = ImVec2(0.5f, 0.5f);

    // --- PERFECT BLACK FOCUS / SELECTION / INTERACTION FIX ---
    style.Colors[ImGuiCol_NavHighlight] = ImVec4(0.83f, 0.18f, 0.18f, 0.80f); 
    style.Colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.0f, 1.0f, 1.0f, 0.70f); 
    style.Colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.60f); 
    style.Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.70f); 

    // Window title bars - smooth state transitions
    style.Colors[ImGuiCol_TitleBg] = bg_0;
    style.Colors[ImGuiCol_TitleBgActive] = bg_1;
    style.Colors[ImGuiCol_TitleBgCollapsed] = bg_0;

    // Menu bar
    style.Colors[ImGuiCol_MenuBarBg] = bg_1;

    // === RESIZING ELEMENTS & BLOCKING FEATURES ===
    style.Colors[ImGuiCol_ResizeGrip] = ImVec4(0.83f, 0.18f, 0.18f, 0.50f); 
    style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(1.00f, 0.40f, 0.40f, 0.80f); 
    style.Colors[ImGuiCol_ResizeGripActive] = accent_primary; 

    // Docking elements
    style.Colors[ImGuiCol_DockingPreview] = ImVec4(0.83f, 0.18f, 0.18f, 0.50f); 
    style.Colors[ImGuiCol_DockingEmptyBg] = bg_0; 

    // Plot colors (if using ImPlot)
    style.Colors[ImGuiCol_PlotLines] = accent_primary;
    style.Colors[ImGuiCol_PlotLinesHovered] = accent_highlight;
    style.Colors[ImGuiCol_PlotHistogram] = accent_secondary;
    style.Colors[ImGuiCol_PlotHistogramHovered] = accent_highlight;

    // Table elements
    style.Colors[ImGuiCol_TableHeaderBg] = bg_3;
    style.Colors[ImGuiCol_TableBorderStrong] = ImVec4(0.30f, 0.30f, 0.30f, 1.00f); 
    style.Colors[ImGuiCol_TableBorderLight] = ImVec4(0.25f, 0.25f, 0.25f, 0.50f); 
    style.Colors[ImGuiCol_TableRowBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f); 
    style.Colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.0f, 1.0f, 1.0f, 0.05f); 
}