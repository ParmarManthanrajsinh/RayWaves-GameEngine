#include "terminal_view.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>

namespace term
{
namespace
{
struct t_KeyBinding
{
    ImGuiKey key;
    VTermKey term_key;
};

const t_KeyBinding c_KeyBindings[] = {
    { ImGuiKey_Enter, VTERM_KEY_ENTER },
    { ImGuiKey_KeypadEnter, VTERM_KEY_KP_ENTER },
    { ImGuiKey_Tab, VTERM_KEY_TAB },
    { ImGuiKey_Backspace, VTERM_KEY_BACKSPACE },
    { ImGuiKey_Escape, VTERM_KEY_ESCAPE },
    { ImGuiKey_UpArrow, VTERM_KEY_UP },
    { ImGuiKey_DownArrow, VTERM_KEY_DOWN },
    { ImGuiKey_LeftArrow, VTERM_KEY_LEFT },
    { ImGuiKey_RightArrow, VTERM_KEY_RIGHT },
    { ImGuiKey_Insert, VTERM_KEY_INS },
    { ImGuiKey_Delete, VTERM_KEY_DEL },
    { ImGuiKey_Home, VTERM_KEY_HOME },
    { ImGuiKey_End, VTERM_KEY_END },
    { ImGuiKey_PageUp, VTERM_KEY_PAGEUP },
    { ImGuiKey_PageDown, VTERM_KEY_PAGEDOWN },
    { ImGuiKey_Keypad0, VTERM_KEY_KP_0 },
    { ImGuiKey_Keypad1, VTERM_KEY_KP_1 },
    { ImGuiKey_Keypad2, VTERM_KEY_KP_2 },
    { ImGuiKey_Keypad3, VTERM_KEY_KP_3 },
    { ImGuiKey_Keypad4, VTERM_KEY_KP_4 },
    { ImGuiKey_Keypad5, VTERM_KEY_KP_5 },
    { ImGuiKey_Keypad6, VTERM_KEY_KP_6 },
    { ImGuiKey_Keypad7, VTERM_KEY_KP_7 },
    { ImGuiKey_Keypad8, VTERM_KEY_KP_8 },
    { ImGuiKey_Keypad9, VTERM_KEY_KP_9 },
};

const ImGuiKey c_FunctionKeys[] = {
    ImGuiKey_F1,
    ImGuiKey_F2,
    ImGuiKey_F3,
    ImGuiKey_F4,
    ImGuiKey_F5,
    ImGuiKey_F6,
    ImGuiKey_F7,
    ImGuiKey_F8,
    ImGuiKey_F9,
    ImGuiKey_F10,
    ImGuiKey_F11,
    ImGuiKey_F12,
};

VTermModifier s_Modifiers(const ImGuiIO& io)
{
    int mods = VTERM_MOD_NONE;
    if (io.KeyShift)
        mods |= VTERM_MOD_SHIFT;
    if (io.KeyAlt)
        mods |= VTERM_MOD_ALT;
    if (io.KeyCtrl)
        mods |= VTERM_MOD_CTRL;
    return static_cast<VTermModifier>(mods);
}

ImU32 s_Color(const t_Rgb& color)
{
    return IM_COL32(color.r, color.g, color.b, 255);
}

void s_AppendUtf8(std::string& out, uint32_t codepoint)
{
    if (codepoint == 0)
    {
        out.push_back(' ');
        return;
    }
    if (codepoint < 0x80)
    {
        out.push_back(static_cast<char>(codepoint));
    }
    else if (codepoint < 0x800)
    {
        out.push_back(static_cast<char>(0xC0 | (codepoint >> 6)));
        out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    }
    else if (codepoint < 0x10000)
    {
        out.push_back(static_cast<char>(0xE0 | (codepoint >> 12)));
        out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    }
    else
    {
        out.push_back(static_cast<char>(0xF0 | (codepoint >> 18)));
        out.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    }
}

std::string s_CellText(const t_Cell& cell)
{
    std::string text;
    for (uint32_t codepoint : cell.codepoints)
    {
        if (codepoint == 0)
        {
            break;
        }
        s_AppendUtf8(text, codepoint);
    }
    if (text.empty())
    {
        text.push_back(' ');
    }
    return text;
}

struct t_Run
{
    ImU32 foreground = 0;
    ImU32 background = 0;
    bool b_Underline = false;
    bool b_Strike = false;
    float start_x = 0.0f;
    float end_x = 0.0f;
    std::string text;
};

void s_FlushRun(t_Run& run, ImDrawList* draw_list, ImFont* font, float font_size, float y, float cell_height,
    ImU32 default_background)
{
    if (run.text.empty())
    {
        return;
    }

    if (run.background != default_background)
    {
        draw_list->AddRectFilled(ImVec2(run.start_x, y), ImVec2(run.end_x, y + cell_height), run.background);
    }

    draw_list->AddText(font, font_size, ImVec2(run.start_x, y), run.foreground, run.text.c_str());

    if (run.b_Underline)
    {
        draw_list->AddLine(ImVec2(run.start_x, y + cell_height - 1.5f), ImVec2(run.end_x, y + cell_height - 1.5f),
            run.foreground, 1.0f);
    }
    if (run.b_Strike)
    {
        draw_list->AddLine(ImVec2(run.start_x, y + cell_height * 0.5f), ImVec2(run.end_x, y + cell_height * 0.5f),
            run.foreground, 1.0f);
    }

    run = t_Run{};
}

bool s_IsSameStyle(const t_Run& run, ImU32 foreground, ImU32 background, bool underline, bool strike)
{
    return run.foreground == foreground && run.background == background && run.b_Underline == underline &&
        run.b_Strike == strike;
}
} // namespace

bool t_TerminalView::b_HasSelection() const
{
    return m_SelectionAnchor.b_Valid() && m_SelectionHead.b_Valid();
}

void t_TerminalView::ClearSelection()
{
    m_SelectionAnchor = {};
    m_SelectionHead = {};
    m_bDragging = false;
}

void t_TerminalView::CopySelection(t_VTermScreen& screen)
{
    if (!b_HasSelection())
    {
        return;
    }

    const std::string text =
        screen.GetText(m_SelectionAnchor.row, m_SelectionAnchor.col, m_SelectionHead.row, m_SelectionHead.col);

    if (!text.empty())
    {
        ImGui::SetClipboardText(text.c_str());
    }
}

void t_TerminalView::SelectAll(const t_VTermScreen& screen)
{
    const int total = static_cast<int>(screen.TotalLines());
    if (total <= 0)
    {
        return;
    }

    m_SelectionAnchor = { 0, 0 };
    m_SelectionHead = { total - 1, std::max(screen.Cols() - 1, 0) };
}

void t_TerminalView::ScrollToBottom()
{
    m_ScrollOffset = 0;
}

void t_TerminalView::ScrollLines(int lines)
{
    m_ScrollOffset += lines;
}

void t_TerminalView::Draw(
    const char* id, t_VTermScreen& screen, bool interactive, const std::function<void(int rows, int cols)>& on_resize)
{
    ImGui::PushFont(m_Font);
    const float font_size = ImGui::GetFontSize();
    const float cell_width = std::max(ImGui::CalcTextSize("M").x, 1.0f);
    const float cell_height = std::max(ImGui::GetTextLineHeight(), 1.0f);
    ImGui::PopFont();

    const ImVec2 available = ImGui::GetContentRegionAvail();
    const int wanted_cols = std::max(2, static_cast<int>(std::floor(available.x / cell_width)));
    const int wanted_rows = std::max(2, static_cast<int>(std::floor(available.y / cell_height)));

    if (wanted_cols != screen.Cols() || wanted_rows != screen.Rows())
    {
        if (on_resize)
        {
            on_resize(wanted_rows, wanted_cols);
        }
        else
        {
            screen.Resize(wanted_rows, wanted_cols);
        }
    }

    const int cols = screen.Cols();
    const int rows = screen.Rows();
    const ImVec2 grid_size(cols * cell_width, rows * cell_height);

    const ImVec2 origin = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton(id, grid_size,
        ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight | ImGuiButtonFlags_MouseButtonMiddle);

    const bool hovered = ImGui::IsItemHovered();
    const bool window_focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    const bool focused = interactive && window_focused;
    m_bKeyboardCaptured = focused;

    if (focused)
    {
        ImGui::SetNextFrameWantCaptureKeyboard(true);
    }

    if (interactive && focused)
    {
        HandleKeyboard(screen, focused);
    }
    else if (!interactive && window_focused)
    {
        if (ImGui::IsKeyPressed(ImGuiKey_A) && ImGui::GetIO().KeyCtrl)
        {
            SelectAll(screen);
        }
        else if (ImGui::IsKeyPressed(ImGuiKey_C) && ImGui::GetIO().KeyCtrl && b_HasSelection())
        {
            CopySelection(screen);
        }
    }

    HandleMouse(screen, origin, cell_width, cell_height, hovered, interactive);

    DrawContextMenu(screen, interactive);

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->AddRectFilled(origin, ImVec2(origin.x + grid_size.x, origin.y + grid_size.y), m_DefaultBg, 4.0f);
    draw_list->PushClipRect(origin, ImVec2(origin.x + grid_size.x, origin.y + grid_size.y), true);

    const int scrollback_size = static_cast<int>(screen.ScrollbackSize());
    if (screen.b_IsAltScreen())
    {
        m_ScrollOffset = 0;
    }
    if (scrollback_size > m_LastScrollbackSize && m_ScrollOffset > 0)
    {
        m_ScrollOffset += scrollback_size - m_LastScrollbackSize;
    }
    m_LastScrollbackSize = scrollback_size;
    m_ScrollOffset = std::clamp(m_ScrollOffset, 0, scrollback_size);

    const int first_visible_row = scrollback_size - m_ScrollOffset;
    const int total_lines = static_cast<int>(screen.TotalLines());

    for (int row = 0; row < rows; ++row)
    {
        const int absolute_row = first_visible_row + row;
        if (absolute_row < 0 || absolute_row >= total_lines)
        {
            continue;
        }

        const float y = origin.y + row * cell_height;
        const float row_end_x = origin.x + cols * cell_width;
        t_Run run;
        run.start_x = origin.x;
        run.end_x = origin.x;

        for (int col = 0; col < cols; ++col)
        {
            t_Cell cell;
            if (!screen.b_GetCell(absolute_row, col, cell) || cell.width == 0)
            {
                continue;
            }

            ImU32 foreground = cell.b_DefaultFg ? m_DefaultFg : s_Color(cell.fg);
            ImU32 background = cell.b_DefaultBg ? m_DefaultBg : s_Color(cell.bg);

            if (cell.b_Reverse)
            {
                std::swap(foreground, background);
            }
            if (cell.b_Conceal)
            {
                foreground = background;
            }

            const float cell_start_x = origin.x + col * cell_width;
            const int width = std::max(cell.width, 1);
            const float cell_end_x = std::min(cell_start_x + width * cell_width, row_end_x);

            if (!s_IsSameStyle(run, foreground, background, cell.b_Underline, cell.b_Strike))
            {
                s_FlushRun(run, draw_list, m_Font, font_size, y, cell_height, m_DefaultBg);
                run.foreground = foreground;
                run.background = background;
                run.b_Underline = cell.b_Underline;
                run.b_Strike = cell.b_Strike;
                run.start_x = cell_start_x;
                run.end_x = cell_start_x;
            }

            run.text += s_CellText(cell);
            run.end_x = cell_end_x;

            if (width > 1)
            {
                col += width - 1;
            }
        }

        run.end_x = std::min(run.end_x, row_end_x);
        s_FlushRun(run, draw_list, m_Font, font_size, y, cell_height, m_DefaultBg);
    }

    DrawSelection(screen, draw_list, origin, cell_width, cell_height, first_visible_row, rows);

    if (interactive)
    {
        DrawCursor(screen, draw_list, origin, cell_width, cell_height, first_visible_row, rows, focused);
    }

    draw_list->PopClipRect();
    draw_list->AddRect(origin, ImVec2(origin.x + grid_size.x, origin.y + grid_size.y),
        focused ? ImGui::GetColorU32(ImGuiCol_NavCursor) : ImGui::GetColorU32(ImGuiCol_Border), 4.0f);

    if (m_ScrollOffset > 0)
    {
        ImGui::SetCursorScreenPos(
            ImVec2(origin.x + grid_size.x - 74.0f, origin.y + grid_size.y - ImGui::GetFrameHeight() - 6.0f));
        if (ImGui::SmallButton("Bottom"))
        {
            ScrollToBottom();
        }
    }
}

void t_TerminalView::HandleKeyboard(t_VTermScreen& screen, bool)
{
    ImGuiIO& io = ImGui::GetIO();
    if (ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId))
    {
        return;
    }

    const VTermModifier mods = s_Modifiers(io);
    const bool ctrl = io.KeyCtrl;
    const bool shift = io.KeyShift;
    const int page = std::max(screen.Rows(), 1);
    const int scrollback_size = static_cast<int>(screen.ScrollbackSize());

    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_C))
    {
        if (shift)
        {
            CopySelection(screen);
            return;
        }
        if (b_HasSelection())
        {
            CopySelection(screen);
        }
        else
        {
            screen.KeyboardUnichar('c', VTERM_MOD_CTRL);
        }
        return;
    }

    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_V))
    {
        const char* clipboard = ImGui::GetClipboardText();
        if (clipboard != nullptr)
        {
            screen.SendPaste(clipboard);
        }
        return;
    }

    if (ctrl && shift && ImGui::IsKeyPressed(ImGuiKey_A))
    {
        SelectAll(screen);
        return;
    }

    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Home))
    {
        m_ScrollOffset = scrollback_size;
        return;
    }
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_End))
    {
        ScrollToBottom();
        return;
    }

    if (shift && ImGui::IsKeyPressed(ImGuiKey_PageUp))
    {
        m_ScrollOffset = std::clamp(m_ScrollOffset + page, 0, scrollback_size);
        return;
    }
    if (shift && ImGui::IsKeyPressed(ImGuiKey_PageDown))
    {
        m_ScrollOffset = std::clamp(m_ScrollOffset - page, 0, scrollback_size);
        return;
    }

    if (!screen.b_IsAltScreen())
    {
        if (ImGui::IsKeyPressed(ImGuiKey_PageUp, true))
        {
            m_ScrollOffset = std::clamp(m_ScrollOffset + page, 0, scrollback_size);
            return;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_PageDown, true))
        {
            m_ScrollOffset = std::clamp(m_ScrollOffset - page, 0, scrollback_size);
            return;
        }
    }

    for (const t_KeyBinding& binding : c_KeyBindings)
    {
        const bool repeat = binding.key == ImGuiKey_Backspace || binding.key == ImGuiKey_UpArrow ||
            binding.key == ImGuiKey_DownArrow || binding.key == ImGuiKey_LeftArrow ||
            binding.key == ImGuiKey_RightArrow;

        if (ImGui::IsKeyPressed(binding.key, repeat))
        {
            screen.KeyboardKey(binding.term_key, mods);
            return;
        }
    }

    for (int index = 0; index < 12; ++index)
    {
        if (ImGui::IsKeyPressed(c_FunctionKeys[index], false))
        {
            screen.KeyboardKey(static_cast<VTermKey>(VTERM_KEY_FUNCTION(index + 1)), mods);
            return;
        }
    }

    if (ctrl || io.KeyAlt)
    {
        // Ctrl+Shift+T/W are handled by the terminal tab bar. Every other
        // Ctrl/Alt letter belongs to the shell (PSReadLine, readline).
        for (int key = ImGuiKey_A; key <= ImGuiKey_Z; ++key)
        {
            if (ctrl && shift && (key == ImGuiKey_T || key == ImGuiKey_W))
            {
                continue;
            }
            if (ImGui::IsKeyPressed(static_cast<ImGuiKey>(key)))
            {
                const char letter = static_cast<char>('a' + (key - ImGuiKey_A));
                screen.KeyboardUnichar(static_cast<uint32_t>(letter), mods);
                return;
            }
        }

        if (ImGui::IsKeyPressed(ImGuiKey_Space))
        {
            screen.KeyboardUnichar(' ', mods);
        }
        return;
    }

    for (ImWchar character : io.InputQueueCharacters)
    {
        if (character < 32 || character == 127)
        {
            continue;
        }
        screen.KeyboardUnichar(static_cast<uint32_t>(character));
    }
}

void t_TerminalView::HandleMouse(
    t_VTermScreen& screen, ImVec2 origin, float cell_width, float cell_height, bool hovered, bool interactive)
{
    ImGuiIO& io = ImGui::GetIO();
    const int scrollback_size = static_cast<int>(screen.ScrollbackSize());
    const int total_lines = static_cast<int>(screen.TotalLines());
    const int first_visible_row = scrollback_size - m_ScrollOffset;

    if (hovered && io.MouseWheel != 0.0f)
    {
        if (screen.b_IsAltScreen())
        {
            const int notches = static_cast<int>(std::abs(io.MouseWheel) * 3.0f);
            const VTermKey key = io.MouseWheel > 0.0f ? VTERM_KEY_UP : VTERM_KEY_DOWN;
            for (int index = 0; index < notches; ++index)
            {
                screen.KeyboardKey(key);
            }
        }
        else
        {
            m_ScrollOffset = std::clamp(m_ScrollOffset + static_cast<int>(io.MouseWheel * 3.0f), 0, scrollback_size);
        }
    }

    if (interactive && hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Middle))
    {
        const char* clipboard = ImGui::GetClipboardText();
        if (clipboard != nullptr)
        {
            screen.SendPaste(clipboard);
        }
    }

    if (!hovered && !m_bDragging)
    {
        return;
    }

    const int column =
        std::clamp(static_cast<int>(std::floor((io.MousePos.x - origin.x) / cell_width)), 0, screen.Cols() - 1);
    const int visible_row =
        std::clamp(static_cast<int>(std::floor((io.MousePos.y - origin.y) / cell_height)), 0, screen.Rows() - 1);
    const int absolute_row = std::clamp(first_visible_row + visible_row, 0, std::max(total_lines - 1, 0));

    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        m_SelectionAnchor = { absolute_row, column };
        m_SelectionHead = m_SelectionAnchor;
        m_bDragging = true;
    }
    else if (m_bDragging && ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        m_SelectionHead = { absolute_row, column };
    }

    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        m_bDragging = false;
    }
}

void t_TerminalView::DrawSelection(t_VTermScreen& screen, ImDrawList* draw_list, ImVec2 origin, float cell_width,
    float cell_height, int first_visible_row, int rows)
{
    if (!b_HasSelection())
    {
        return;
    }

    t_Position start = m_SelectionAnchor;
    t_Position end = m_SelectionHead;
    if (start.row > end.row || (start.row == end.row && start.col > end.col))
    {
        std::swap(start, end);
    }

    const ImU32 selection_color = IM_COL32(80, 120, 200, 100);

    for (int row = 0; row < rows; ++row)
    {
        const int absolute_row = first_visible_row + row;
        if (absolute_row < start.row || absolute_row > end.row)
        {
            continue;
        }

        const int start_col = (absolute_row == start.row) ? start.col : 0;
        const int end_col = (absolute_row == end.row) ? end.col : screen.Cols() - 1;
        if (start_col > end_col)
        {
            continue;
        }

        const float y = origin.y + row * cell_height;
        draw_list->AddRectFilled(ImVec2(origin.x + start_col * cell_width, y),
            ImVec2(origin.x + (end_col + 1) * cell_width, y + cell_height), selection_color);
    }
}

void t_TerminalView::DrawCursor(t_VTermScreen& screen, ImDrawList* draw_list, ImVec2 origin, float cell_width,
    float cell_height, int first_visible_row, int rows, bool focused)
{
    if (!screen.b_CursorVisible())
    {
        return;
    }

    const int absolute_row = static_cast<int>(screen.ScrollbackSize()) + screen.CursorRow();
    const int visible_row = absolute_row - first_visible_row;
    if (visible_row < 0 || visible_row >= rows)
    {
        return;
    }

    const int column = std::clamp(screen.CursorCol(), 0, screen.Cols() - 1);
    const float x = origin.x + column * cell_width;
    const float y = origin.y + visible_row * cell_height;

    const bool blink_on = !focused || std::fmod(ImGui::GetTime(), 1.06) < 0.53;
    if (!blink_on)
    {
        return;
    }

    const ImU32 color = focused ? m_DefaultFg : IM_COL32(160, 160, 160, 180);

    switch (screen.CursorShape())
    {
        case ECursorShape::Underline:
            draw_list->AddRectFilled(ImVec2(x, y + cell_height - 2.0f), ImVec2(x + cell_width, y + cell_height), color);
            break;
        case ECursorShape::Bar:
            draw_list->AddRectFilled(ImVec2(x, y), ImVec2(x + 2.0f, y + cell_height), color);
            break;
        case ECursorShape::Block:
        default:
            draw_list->AddRect(ImVec2(x, y), ImVec2(x + cell_width, y + cell_height), color, 0.0f, 0, 1.5f);
            break;
    }
}

void t_TerminalView::DrawContextMenu(t_VTermScreen& screen, bool interactive)
{
    if (!ImGui::BeginPopupContextItem("##terminal_context"))
    {
        return;
    }

    if (ImGui::MenuItem("Copy", "Ctrl+C", false, b_HasSelection()))
    {
        CopySelection(screen);
    }
    if (interactive && ImGui::MenuItem("Paste", "Ctrl+V"))
    {
        const char* clipboard = ImGui::GetClipboardText();
        if (clipboard != nullptr)
        {
            screen.SendPaste(clipboard);
        }
    }
    if (ImGui::MenuItem("Select All", "Ctrl+Shift+A"))
    {
        SelectAll(screen);
    }
    if (ImGui::MenuItem("Scroll to Bottom"))
    {
        ScrollToBottom();
    }

    ImGui::EndPopup();
}

} // namespace term
