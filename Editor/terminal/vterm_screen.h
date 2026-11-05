#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include <vterm.h>

namespace term
{

struct t_Rgb
{
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
};

enum class ECursorShape
{
    Block,
    Underline,
    Bar
};

struct t_Cell
{
    uint32_t codepoints[6] = {};
    int width = 1;
    bool b_Bold = false;
    bool b_Underline = false;
    bool b_Italic = false;
    bool b_Blink = false;
    bool b_Reverse = false;
    bool b_Conceal = false;
    bool b_Strike = false;
    bool b_DefaultFg = true;
    bool b_DefaultBg = true;
    t_Rgb fg;
    t_Rgb bg;
};

struct t_Line
{
    std::vector<t_Cell> cells;
    bool b_Continuation = false;
};

// Owns a libvterm instance and exposes a UI-agnostic view of the screen plus
// a bounded scrollback. All mutation and cell reads must be serialised: callers
// that read many cells should hold Mutex() for the duration of the frame.
class t_VTermScreen
{
public:
    t_VTermScreen(int rows, int cols, size_t scrollback_limit = 5000);
    ~t_VTermScreen();

    t_VTermScreen(const t_VTermScreen&) = delete;
    t_VTermScreen& operator=(const t_VTermScreen&) = delete;

    void Write(std::string_view bytes);
    void Resize(int rows, int cols);
    void Reset();

    void KeyboardKey(VTermKey key, VTermModifier mods = VTERM_MOD_NONE);
    void KeyboardUnichar(uint32_t codepoint, VTermModifier mods = VTERM_MOD_NONE);
    void PasteStart();
    void PasteEnd();
    void SendPaste(std::string_view text);

    void SetDefaultColors(t_Rgb fg, t_Rgb bg);
    void SetPaletteColor(int index, t_Rgb color);
    void SetOutputCallback(std::function<void(std::string_view)> callback);

    int Rows() const
    {
        std::scoped_lock lock(m_Mutex);
        return m_Rows;
    }
    int Cols() const
    {
        std::scoped_lock lock(m_Mutex);
        return m_Cols;
    }

    int CursorRow() const
    {
        std::scoped_lock lock(m_Mutex);
        return m_CursorRow;
    }
    int CursorCol() const
    {
        std::scoped_lock lock(m_Mutex);
        return m_CursorCol;
    }
    bool b_CursorVisible() const
    {
        std::scoped_lock lock(m_Mutex);
        return m_bCursorVisible;
    }
    ECursorShape CursorShape() const
    {
        std::scoped_lock lock(m_Mutex);
        return m_CursorShape;
    }

    std::string Title() const;
    bool b_IsAltScreen() const
    {
        std::scoped_lock lock(m_Mutex);
        return m_bAltScreen;
    }

    bool b_TakeDirty() { return m_bDirty.exchange(false); }
    bool b_TakeBell() { return m_bBell.exchange(false); }

    std::recursive_mutex& Mutex() { return m_Mutex; }

    size_t ScrollbackSize() const
    {
        std::scoped_lock lock(m_Mutex);
        return m_Scrollback.size();
    }
    size_t TotalLines() const
    {
        std::scoped_lock lock(m_Mutex);
        return m_Scrollback.size() + static_cast<size_t>(m_Rows);
    }
    const t_Line& ScrollbackLine(size_t index) const { return m_Scrollback[index]; }

    bool b_GetCell(int absolute_row, int col, t_Cell& out) const;
    bool b_IsContinuation(int absolute_row) const;

    // Copy a linear cell range. Rows are joined with '\n' unless libvterm
    // marked the line as a wrapped continuation.
    std::string GetText(int start_row, int start_col, int end_row, int end_col) const;
    std::string GetRowText(int absolute_row) const;

    void ClearScrollback();

private:
    void PushScrollbackLine(const VTermScreenCell* cells, int cols, bool continuation);
    bool b_PopScrollbackLine(VTermScreenCell* cells, int cols);
    void ResizeScrollback(int cols);

    static int s_Damage(VTermRect rect, void* user);
    static int s_MoveRect(VTermRect dest, VTermRect src, void* user);
    static int s_MoveCursor(VTermPos pos, VTermPos oldpos, int visible, void* user);
    static int s_SetTermProp(VTermProp prop, VTermValue* value, void* user);
    static int s_Bell(void* user);
    static int s_Resize(int rows, int cols, void* user);
    static int s_PushLine(int cols, const VTermScreenCell* cells, void* user);
    static int s_PushLine4(int cols, const VTermScreenCell* cells, bool continuation, void* user);
    static int s_PopLine(int cols, VTermScreenCell* cells, void* user);
    static int s_ClearScrollback(void* user);
    static void s_Output(const char* bytes, size_t len, void* user);

    VTerm* m_Vt = nullptr;
    VTermScreen* m_Screen = nullptr;
    VTermState* m_State = nullptr;

    mutable std::recursive_mutex m_Mutex;
    std::deque<t_Line> m_Scrollback;
    size_t m_ScrollbackLimit = 5000;

    int m_Rows = 0;
    int m_Cols = 0;
    int m_CursorRow = 0;
    int m_CursorCol = 0;
    bool m_bCursorVisible = true;
    ECursorShape m_CursorShape = ECursorShape::Block;
    bool m_bAltScreen = false;

    std::string m_Title;
    std::string m_TitleAccum;
    std::function<void(std::string_view)> m_Output;

    t_Rgb m_DefaultFg{ 220, 220, 220 };
    t_Rgb m_DefaultBg{ 20, 20, 24 };

    std::atomic<bool> m_bDirty{ false };
    std::atomic<bool> m_bBell{ false };
};

} // namespace term
