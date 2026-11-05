#pragma once

#include "vterm_screen.h"

#include <functional>
#include <string>

#include <imgui.h>

namespace term
{

class t_TerminalView
{
public:
    void Draw(const char* id, t_VTermScreen& screen, bool interactive,
        const std::function<void(int rows, int cols)>& on_resize);

    void SetFont(ImFont* font) { m_Font = font; }
    void SetDefaultColors(ImU32 foreground, ImU32 background)
    {
        m_DefaultFg = foreground;
        m_DefaultBg = background;
    }

    bool b_KeyboardCaptured() const { return m_bKeyboardCaptured; }
    bool b_HasSelection() const;
    void ClearSelection();
    void CopySelection(t_VTermScreen& screen);
    void SelectAll(const t_VTermScreen& screen);
    void ScrollToBottom();
    void ScrollLines(int lines);

private:
    struct t_Position
    {
        int row = -1;
        int col = -1;

        bool b_Valid() const { return row >= 0 && col >= 0; }
    };

    void HandleKeyboard(t_VTermScreen& screen, bool focused);
    void HandleMouse(
        t_VTermScreen& screen, ImVec2 origin, float cell_width, float cell_height, bool hovered, bool interactive);
    void DrawSelection(t_VTermScreen& screen, ImDrawList* draw_list, ImVec2 origin, float cell_width, float cell_height,
        int first_visible_row, int rows);
    void DrawCursor(t_VTermScreen& screen, ImDrawList* draw_list, ImVec2 origin, float cell_width, float cell_height,
        int first_visible_row, int rows, bool focused);
    void DrawContextMenu(t_VTermScreen& screen, bool interactive);

    t_Position m_SelectionAnchor;
    t_Position m_SelectionHead;
    int m_ScrollOffset = 0;
    int m_LastScrollbackSize = 0;
    bool m_bKeyboardCaptured = false;
    bool m_bDragging = false;

    ImFont* m_Font = nullptr;
    ImU32 m_DefaultFg = IM_COL32(220, 220, 220, 255);
    ImU32 m_DefaultBg = IM_COL32(20, 20, 24, 255);
};

} // namespace term
