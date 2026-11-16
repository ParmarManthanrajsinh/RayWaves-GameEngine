#pragma once

#include <imgui.h>
#include <imgui_internal.h>

#include <string_view>

namespace EditorWidgets
{
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
