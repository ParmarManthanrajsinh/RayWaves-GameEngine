#include "vterm_screen.h"

#include <algorithm>
#include <cstring>
#include <utility>

namespace term
{
namespace
{
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

t_Rgb s_ToRgb(VTermScreen* screen, const VTermColor& color)
{
    VTermColor copy = color;
    if (!VTERM_COLOR_IS_RGB(&copy))
    {
        vterm_screen_convert_color_to_rgb(screen, &copy);
    }
    return t_Rgb{ copy.rgb.red, copy.rgb.green, copy.rgb.blue };
}

bool s_NextCodepoint(std::string_view text, size_t& index, uint32_t& codepoint)
{
    if (index >= text.size())
    {
        return false;
    }

    const unsigned char first = static_cast<unsigned char>(text[index]);
    if (first < 0x80)
    {
        codepoint = first;
        ++index;
        return true;
    }

    size_t length = 0;
    uint32_t value = 0;
    if ((first & 0xE0) == 0xC0)
    {
        length = 2;
        value = first & 0x1F;
    }
    else if ((first & 0xF0) == 0xE0)
    {
        length = 3;
        value = first & 0x0F;
    }
    else if ((first & 0xF8) == 0xF0)
    {
        length = 4;
        value = first & 0x07;
    }
    else
    {
        codepoint = 0xFFFD;
        ++index;
        return true;
    }

    if (index + length > text.size())
    {
        codepoint = 0xFFFD;
        ++index;
        return true;
    }

    for (size_t offset = 1; offset < length; ++offset)
    {
        const unsigned char continuation = static_cast<unsigned char>(text[index + offset]);
        if ((continuation & 0xC0) != 0x80)
        {
            codepoint = 0xFFFD;
            ++index;
            return true;
        }
        value = (value << 6) | (continuation & 0x3F);
    }

    index += length;
    codepoint = value;
    return true;
}

t_Cell s_ToCell(VTermScreen* screen, const VTermScreenCell& source)
{
    t_Cell cell;
    std::memcpy(cell.codepoints, source.chars, sizeof(cell.codepoints));
    cell.width = source.width;
    cell.b_Bold = source.attrs.bold != 0;
    cell.b_Underline = source.attrs.underline != 0;
    cell.b_Italic = source.attrs.italic != 0;
    cell.b_Blink = source.attrs.blink != 0;
    cell.b_Reverse = source.attrs.reverse != 0;
    cell.b_Conceal = source.attrs.conceal != 0;
    cell.b_Strike = source.attrs.strike != 0;
    cell.b_DefaultFg = VTERM_COLOR_IS_DEFAULT_FG(&source.fg) != 0;
    cell.b_DefaultBg = VTERM_COLOR_IS_DEFAULT_BG(&source.bg) != 0;
    cell.fg = s_ToRgb(screen, source.fg);
    cell.bg = s_ToRgb(screen, source.bg);
    return cell;
}

} // namespace

t_VTermScreen::t_VTermScreen(int rows, int cols, size_t scrollback_limit)
    : m_ScrollbackLimit(scrollback_limit), m_Rows(std::max(rows, 2)), m_Cols(std::max(cols, 2))
{
    m_Vt = vterm_new(m_Rows, m_Cols);
    vterm_set_utf8(m_Vt, 1);
    m_Screen = vterm_obtain_screen(m_Vt);
    m_State = vterm_obtain_state(m_Vt);

    static const VTermScreenCallbacks callbacks = {
        &t_VTermScreen::s_Damage,
        &t_VTermScreen::s_MoveRect,
        &t_VTermScreen::s_MoveCursor,
        &t_VTermScreen::s_SetTermProp,
        &t_VTermScreen::s_Bell,
        &t_VTermScreen::s_Resize,
        &t_VTermScreen::s_PushLine,
        &t_VTermScreen::s_PopLine,
        &t_VTermScreen::s_ClearScrollback,
        &t_VTermScreen::s_PushLine4,
    };

    vterm_screen_set_callbacks(m_Screen, &callbacks, this);
    vterm_screen_callbacks_has_pushline4(m_Screen);
    vterm_screen_enable_altscreen(m_Screen, 1);
    vterm_screen_enable_reflow(m_Screen, true);
    vterm_screen_set_damage_merge(m_Screen, VTERM_DAMAGE_SCROLL);
    vterm_state_set_bold_highbright(m_State, 1);
    vterm_screen_reset(m_Screen, 1);
}

t_VTermScreen::~t_VTermScreen()
{
    if (m_Vt != nullptr)
    {
        vterm_free(m_Vt);
        m_Vt = nullptr;
        m_Screen = nullptr;
        m_State = nullptr;
    }
}

void t_VTermScreen::Write(std::string_view bytes)
{
    if (bytes.empty() || m_Vt == nullptr)
    {
        return;
    }

    std::scoped_lock lock(m_Mutex);
    vterm_input_write(m_Vt, bytes.data(), bytes.size());
    m_bDirty.store(true);
}

void t_VTermScreen::Resize(int rows, int cols)
{
    rows = std::max(rows, 2);
    cols = std::max(cols, 2);

    std::scoped_lock lock(m_Mutex);
    if (rows == m_Rows && cols == m_Cols)
    {
        return;
    }

    m_Rows = rows;
    m_Cols = cols;
    vterm_set_size(m_Vt, rows, cols);
    ResizeScrollback(cols);
    m_CursorRow = std::clamp(m_CursorRow, 0, m_Rows - 1);
    m_CursorCol = std::clamp(m_CursorCol, 0, m_Cols - 1);
    m_bDirty.store(true);
}

void t_VTermScreen::Reset()
{
    std::scoped_lock lock(m_Mutex);
    ClearScrollback();
    if (m_Screen != nullptr)
    {
        vterm_screen_reset(m_Screen, 1);
    }
    m_CursorRow = 0;
    m_CursorCol = 0;
    m_bDirty.store(true);
}

void t_VTermScreen::KeyboardKey(VTermKey key, VTermModifier mods)
{
    std::scoped_lock lock(m_Mutex);
    vterm_keyboard_key(m_Vt, key, mods);
}

void t_VTermScreen::KeyboardUnichar(uint32_t codepoint, VTermModifier mods)
{
    std::scoped_lock lock(m_Mutex);
    vterm_keyboard_unichar(m_Vt, codepoint, mods);
}

void t_VTermScreen::PasteStart()
{
    std::scoped_lock lock(m_Mutex);
    vterm_keyboard_start_paste(m_Vt);
}

void t_VTermScreen::PasteEnd()
{
    std::scoped_lock lock(m_Mutex);
    vterm_keyboard_end_paste(m_Vt);
}

void t_VTermScreen::SendPaste(std::string_view text)
{
    PasteStart();

    size_t index = 0;
    uint32_t codepoint = 0;
    while (s_NextCodepoint(text, index, codepoint))
    {
        if (codepoint == '\r')
        {
            continue;
        }
        if (codepoint == '\n')
        {
            KeyboardUnichar('\r');
            continue;
        }
        KeyboardUnichar(codepoint);
    }

    PasteEnd();
}

void t_VTermScreen::SetDefaultColors(t_Rgb fg, t_Rgb bg)
{
    std::scoped_lock lock(m_Mutex);
    m_DefaultFg = fg;
    m_DefaultBg = bg;

    VTermColor foreground;
    VTermColor background;
    vterm_color_rgb(&foreground, fg.r, fg.g, fg.b);
    vterm_color_rgb(&background, bg.r, bg.g, bg.b);

    vterm_screen_set_default_colors(m_Screen, &foreground, &background);
    vterm_state_set_default_colors(m_State, &foreground, &background);
    m_bDirty.store(true);
}

void t_VTermScreen::SetPaletteColor(int index, t_Rgb color)
{
    if (index < 0 || index >= 16)
    {
        return;
    }

    std::scoped_lock lock(m_Mutex);
    VTermColor converted;
    vterm_color_rgb(&converted, color.r, color.g, color.b);
    vterm_state_set_palette_color(m_State, index, &converted);
    m_bDirty.store(true);
}

void t_VTermScreen::SetOutputCallback(std::function<void(std::string_view)> callback)
{
    std::scoped_lock lock(m_Mutex);
    m_Output = std::move(callback);
    vterm_output_set_callback(m_Vt, &t_VTermScreen::s_Output, this);
}

std::string t_VTermScreen::Title() const
{
    std::scoped_lock lock(m_Mutex);
    return m_Title;
}

bool t_VTermScreen::b_GetCell(int absolute_row, int col, t_Cell& out) const
{
    std::scoped_lock lock(m_Mutex);
    if (absolute_row < 0 || col < 0)
    {
        return false;
    }

    const int scrollback_size = static_cast<int>(m_Scrollback.size());
    if (absolute_row < scrollback_size)
    {
        const t_Line& line = m_Scrollback[static_cast<size_t>(absolute_row)];
        if (col >= static_cast<int>(line.cells.size()))
        {
            return false;
        }
        out = line.cells[static_cast<size_t>(col)];
        return true;
    }

    const int row = absolute_row - scrollback_size;
    if (row < 0 || row >= m_Rows || col >= m_Cols)
    {
        return false;
    }

    VTermScreenCell source{};
    const VTermPos position{ row, col };
    if (!vterm_screen_get_cell(m_Screen, position, &source))
    {
        return false;
    }

    out = s_ToCell(const_cast<VTermScreen*>(m_Screen), source);
    return true;
}

bool t_VTermScreen::b_IsContinuation(int absolute_row) const
{
    std::scoped_lock lock(m_Mutex);
    const int scrollback_size = static_cast<int>(m_Scrollback.size());
    if (absolute_row < 0)
    {
        return false;
    }
    if (absolute_row < scrollback_size)
    {
        return m_Scrollback[static_cast<size_t>(absolute_row)].b_Continuation;
    }

    const int row = absolute_row - scrollback_size;
    if (row < 0 || row >= m_Rows)
    {
        return false;
    }

    const VTermLineInfo* info = vterm_state_get_lineinfo(m_State, row);
    return info != nullptr && info->continuation != 0;
}

std::string t_VTermScreen::GetRowText(int absolute_row) const
{
    std::scoped_lock lock(m_Mutex);
    std::string text;
    for (int col = 0; col < m_Cols; ++col)
    {
        t_Cell cell;
        if (!b_GetCell(absolute_row, col, cell) || cell.width == 0)
        {
            continue;
        }

        for (uint32_t codepoint : cell.codepoints)
        {
            if (codepoint == 0)
            {
                break;
            }
            s_AppendUtf8(text, codepoint);
        }
    }

    while (!text.empty() && text.back() == ' ')
    {
        text.pop_back();
    }
    return text;
}

std::string t_VTermScreen::GetText(int start_row, int start_col, int end_row, int end_col) const
{
    std::scoped_lock lock(m_Mutex);
    if (start_row > end_row || (start_row == end_row && start_col > end_col))
    {
        std::swap(start_row, end_row);
        std::swap(start_col, end_col);
    }

    std::string text;
    for (int row = start_row; row <= end_row; ++row)
    {
        const int first_col = (row == start_row) ? std::max(start_col, 0) : 0;
        const int last_col = (row == end_row) ? std::min(end_col, m_Cols - 1) : m_Cols - 1;

        std::string line;
        for (int col = first_col; col <= last_col; ++col)
        {
            t_Cell cell;
            if (!b_GetCell(row, col, cell) || cell.width == 0)
            {
                continue;
            }

            bool b_HasText = false;
            for (uint32_t codepoint : cell.codepoints)
            {
                if (codepoint == 0)
                {
                    break;
                }
                s_AppendUtf8(line, codepoint);
                b_HasText = true;
            }
            if (!b_HasText)
            {
                line.push_back(' ');
            }
        }

        while (!line.empty() && line.back() == ' ')
        {
            line.pop_back();
        }

        text += line;
        if (row < end_row && !b_IsContinuation(row + 1))
        {
            text.push_back('\n');
        }
    }

    return text;
}

void t_VTermScreen::ClearScrollback()
{
    std::scoped_lock lock(m_Mutex);
    m_Scrollback.clear();
}

void t_VTermScreen::PushScrollbackLine(const VTermScreenCell* cells, int cols, bool continuation)
{
    t_Line line;
    line.b_Continuation = continuation;
    line.cells.reserve(static_cast<size_t>(m_Cols));

    for (int col = 0; col < m_Cols; ++col)
    {
        if (cells != nullptr && col < cols)
        {
            line.cells.push_back(s_ToCell(m_Screen, cells[col]));
        }
        else
        {
            line.cells.emplace_back();
        }
    }

    m_Scrollback.push_back(std::move(line));
    while (m_Scrollback.size() > m_ScrollbackLimit)
    {
        m_Scrollback.pop_front();
    }
}

bool t_VTermScreen::b_PopScrollbackLine(VTermScreenCell* cells, int cols)
{
    if (m_Scrollback.empty() || cells == nullptr)
    {
        return false;
    }

    const t_Line line = std::move(m_Scrollback.back());
    m_Scrollback.pop_back();

    for (int col = 0; col < cols; ++col)
    {
        cells[col] = VTermScreenCell{};
        cells[col].width = 1;
        cells[col].chars[0] = L' ';
        if (col < static_cast<int>(line.cells.size()))
        {
            const t_Cell& source = line.cells[static_cast<size_t>(col)];
            std::memcpy(cells[col].chars, source.codepoints, sizeof(cells[col].chars));
            cells[col].width = static_cast<char>(source.width);
            cells[col].attrs.bold = source.b_Bold ? 1 : 0;
            cells[col].attrs.underline = source.b_Underline ? 1 : 0;
            cells[col].attrs.italic = source.b_Italic ? 1 : 0;
            cells[col].attrs.blink = source.b_Blink ? 1 : 0;
            cells[col].attrs.reverse = source.b_Reverse ? 1 : 0;
            cells[col].attrs.conceal = source.b_Conceal ? 1 : 0;
            cells[col].attrs.strike = source.b_Strike ? 1 : 0;
            if (source.b_DefaultFg)
            {
                cells[col].fg.type = VTERM_COLOR_DEFAULT_FG;
            }
            else
            {
                vterm_color_rgb(&cells[col].fg, source.fg.r, source.fg.g, source.fg.b);
            }
            if (source.b_DefaultBg)
            {
                cells[col].bg.type = VTERM_COLOR_DEFAULT_BG;
            }
            else
            {
                vterm_color_rgb(&cells[col].bg, source.bg.r, source.bg.g, source.bg.b);
            }
        }
    }

    return true;
}

void t_VTermScreen::ResizeScrollback(int cols)
{
    for (t_Line& line : m_Scrollback)
    {
        if (static_cast<int>(line.cells.size()) < cols)
        {
            line.cells.resize(static_cast<size_t>(cols));
        }
        else if (static_cast<int>(line.cells.size()) > cols)
        {
            line.cells.resize(static_cast<size_t>(cols));
        }
    }
}

int t_VTermScreen::s_Damage(VTermRect, void* user)
{
    static_cast<t_VTermScreen*>(user)->m_bDirty.store(true);
    return 1;
}

int t_VTermScreen::s_MoveRect(VTermRect, VTermRect, void* user)
{
    static_cast<t_VTermScreen*>(user)->m_bDirty.store(true);
    return 1;
}

int t_VTermScreen::s_MoveCursor(VTermPos pos, VTermPos, int visible, void* user)
{
    auto* self = static_cast<t_VTermScreen*>(user);
    self->m_CursorRow = pos.row;
    self->m_CursorCol = pos.col;
    self->m_bCursorVisible = visible != 0;
    self->m_bDirty.store(true);
    return 1;
}

int t_VTermScreen::s_SetTermProp(VTermProp prop, VTermValue* value, void* user)
{
    auto* self = static_cast<t_VTermScreen*>(user);
    switch (prop)
    {
        case VTERM_PROP_TITLE:
            if (value->string.initial)
            {
                self->m_TitleAccum.clear();
            }
            if (value->string.str != nullptr && value->string.len > 0)
            {
                self->m_TitleAccum.append(value->string.str, value->string.len);
            }
            if (value->string.final)
            {
                self->m_Title = self->m_TitleAccum;
                self->m_TitleAccum.clear();
            }
            break;
        case VTERM_PROP_CURSORSHAPE:
            switch (value->number)
            {
                case VTERM_PROP_CURSORSHAPE_UNDERLINE:
                    self->m_CursorShape = ECursorShape::Underline;
                    break;
                case VTERM_PROP_CURSORSHAPE_BAR_LEFT:
                    self->m_CursorShape = ECursorShape::Bar;
                    break;
                default:
                    self->m_CursorShape = ECursorShape::Block;
                    break;
            }
            break;
        case VTERM_PROP_ALTSCREEN:
            self->m_bAltScreen = value->boolean != 0;
            break;
        default:
            break;
    }
    self->m_bDirty.store(true);
    return 1;
}

int t_VTermScreen::s_Bell(void* user)
{
    static_cast<t_VTermScreen*>(user)->m_bBell.store(true);
    return 1;
}

int t_VTermScreen::s_Resize(int rows, int cols, void* user)
{
    auto* self = static_cast<t_VTermScreen*>(user);
    self->m_Rows = std::max(rows, 2);
    self->m_Cols = std::max(cols, 2);
    self->ResizeScrollback(self->m_Cols);
    self->m_bDirty.store(true);
    return 1;
}

int t_VTermScreen::s_PushLine(int cols, const VTermScreenCell* cells, void* user)
{
    static_cast<t_VTermScreen*>(user)->PushScrollbackLine(cells, cols, false);
    return 1;
}

int t_VTermScreen::s_PushLine4(int cols, const VTermScreenCell* cells, bool continuation, void* user)
{
    static_cast<t_VTermScreen*>(user)->PushScrollbackLine(cells, cols, continuation);
    return 1;
}

int t_VTermScreen::s_PopLine(int cols, VTermScreenCell* cells, void* user)
{
    return static_cast<t_VTermScreen*>(user)->b_PopScrollbackLine(cells, cols) ? 1 : 0;
}

int t_VTermScreen::s_ClearScrollback(void* user)
{
    static_cast<t_VTermScreen*>(user)->ClearScrollback();
    return 1;
}

void t_VTermScreen::s_Output(const char* bytes, size_t len, void* user)
{
    auto* self = static_cast<t_VTermScreen*>(user);
    if (self->m_Output && bytes != nullptr && len > 0)
    {
        self->m_Output(std::string_view(bytes, len));
    }
}

} // namespace term
