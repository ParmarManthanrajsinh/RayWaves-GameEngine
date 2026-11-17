#pragma once

#include "rlImGui/extras/IconsFontAwesome6.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_stdlib.h>

#include <string>
#include <string_view>

namespace EditorWidgets
{
// Shared list metrics so panels drawn side by side keep their rows aligned.
constexpr float c_RowHeight = 22.0f;
constexpr float c_RowIconWidth = 18.0f;

// Flat icon hit area for list rows. Fixed width keeps the label column stable
// whether a row carries an icon or not.
inline bool RowIconButton(const char* id, const char* icon, std::string_view tooltip, bool b_dimmed = false)
{
    ImGui::PushID(id);

    const ImVec2 slot(c_RowIconWidth, c_RowIconWidth);
    const ImVec2 slot_min = ImGui::GetCursorScreenPos();
    const bool b_clicked = ImGui::InvisibleButton("##row_icon", slot);
    const bool b_hovered = ImGui::IsItemHovered();

    const ImU32 color =
        ImGui::GetColorU32(b_hovered ? ImGuiCol_Text : (b_dimmed ? ImGuiCol_TextDisabled : ImGuiCol_Text));
    const ImVec2 text_size = ImGui::CalcTextSize(icon);
    ImGui::GetWindowDrawList()->AddText(
        ImVec2(slot_min.x + ((slot.x - text_size.x) * 0.5f), slot_min.y + ((slot.y - text_size.y) * 0.5f)), color,
        icon);

    if (b_hovered)
    {
        ImGui::SetTooltip("%.*s", static_cast<int>(tooltip.size()), tooltip.data());
    }

    ImGui::PopID();
    return b_clicked;
}

// Reserves the icon column for rows that have no icon, so labels stay aligned
// with rows that do.
inline void RowIconGutter()
{
    ImGui::Dummy(ImVec2(c_RowIconWidth, c_RowIconWidth));
}

// Non-interactive icon in the row icon column.
inline void RowIcon(const char* icon, bool b_dimmed = false)
{
    const ImVec2 slot(c_RowIconWidth, c_RowIconWidth);
    const ImVec2 slot_min = ImGui::GetCursorScreenPos();
    ImGui::Dummy(slot);

    const ImU32 color = ImGui::GetColorU32(b_dimmed ? ImGuiCol_TextDisabled : ImGuiCol_Text);
    const ImVec2 text_size = ImGui::CalcTextSize(icon);
    ImGui::GetWindowDrawList()->AddText(
        ImVec2(slot_min.x + ((slot.x - text_size.x) * 0.5f), slot_min.y + ((slot.y - text_size.y) * 0.5f)), color,
        icon);
}

// Dim, right-aligned count drawn on the row that was submitted last. Use after
// a tree node or selectable header.
inline void RowCountBadge(int count)
{
    const ImVec2 rect_min = ImGui::GetItemRectMin();
    const ImVec2 rect_max = ImGui::GetItemRectMax();
    const std::string text = std::to_string(count);
    const ImVec2 text_size = ImGui::CalcTextSize(text.c_str());
    const float text_x = rect_max.x - text_size.x - ImGui::GetStyle().FramePadding.x - 10.0f;
    const float text_y = rect_min.y + ((rect_max.y - rect_min.y - text_size.y) * 0.5f);

    ImGui::GetWindowDrawList()->AddText(
        ImVec2(text_x, text_y), ImGui::GetColorU32(ImGuiCol_TextDisabled), text.c_str());
}

// Filter field with an inline clear button. Ctrl+F focuses the field while the
// owning panel has focus.
inline void ClearableFilter(const char* id, std::string& filter, const char* hint = "Filter...")
{
    ImGui::PushID(id);

    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !ImGui::GetIO().WantTextInput &&
        ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_F))
    {
        ImGui::SetKeyboardFocusHere();
    }

    const bool b_has_text = !filter.empty();
    const float clear_width = ImGui::GetFrameHeight() + ImGui::GetStyle().ItemSpacing.x;
    ImGui::SetNextItemWidth(b_has_text ? -clear_width : -1.0f);
    ImGui::InputTextWithHint("##filter", hint, &filter);

    if (b_has_text)
    {
        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_XMARK, ImVec2(ImGui::GetFrameHeight(), 0.0f)))
        {
            filter.clear();
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
        {
            ImGui::SetTooltip("Clear filter");
        }
    }

    ImGui::PopID();
}

// Compact icon-only button with a tooltip. Use for panel toolbars.
inline bool IconButton(const char* id, const char* icon, std::string_view tooltip,
    const ImVec2& size = ImVec2(24.0f, 24.0f), bool b_active = false)
{
    if (id != nullptr)
    {
        ImGui::PushID(id);
    }

    if (b_active)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
    }

    const bool b_clicked = ImGui::Button(icon, size);

    if (b_active)
    {
        ImGui::PopStyleColor();
    }

    if (id != nullptr)
    {
        ImGui::PopID();
    }

    if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
    {
        ImGui::SetTooltip("%.*s", static_cast<int>(tooltip.size()), tooltip.data());
    }

    return b_clicked;
}

// Icon-only button that lives on the right side of a tab bar (VS Code style).
// Must be called between BeginTabBar() and EndTabBar().
inline bool TabIconButton(const char* id, const char* icon, std::string_view tooltip, bool b_active = false)
{
    ImGui::PushID(id);

    if (b_active)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
    }

    ImGui::SetNextItemWidth(ImGui::GetFontSize() + ImGui::GetStyle().FramePadding.x * 2.0f + 2.0f);
    const bool b_clicked = ImGui::TabItemButton(icon, ImGuiTabItemFlags_Trailing);

    if (b_active)
    {
        ImGui::PopStyleColor();
    }

    ImGui::PopID();

    if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
    {
        ImGui::SetTooltip("%.*s", static_cast<int>(tooltip.size()), tooltip.data());
    }

    return b_clicked;
}

// Passive right-aligned text on a tab bar row. Reserves a trailing slot so it
// does not overlap the action buttons. Must be called between BeginTabBar()
// and EndTabBar().
inline void TabStatusText(const char* id, std::string_view text)
{
    if (text.empty())
    {
        return;
    }

    const char* text_begin = text.data();
    const char* text_end = text.data() + text.size();
    const float text_width = ImGui::CalcTextSize(text_begin, text_end).x;

    // Extra width keeps a gap between the status text and the next icon.
    ImGui::TabItemSpacing(id, ImGuiTabItemFlags_Trailing, text_width + 12.0f);

    const ImVec2 rect_min = ImGui::GetItemRectMin();
    const ImVec2 rect_max = ImGui::GetItemRectMax();
    const float text_y = rect_min.y + (rect_max.y - rect_min.y - ImGui::GetTextLineHeight()) * 0.5f;

    ImGui::GetWindowDrawList()->AddText(
        ImVec2(rect_min.x, text_y), ImGui::GetColorU32(ImGuiCol_TextDisabled), text_begin, text_end);
}
} // namespace EditorWidgets
