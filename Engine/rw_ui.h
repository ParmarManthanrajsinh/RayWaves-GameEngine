#pragma once
#include "rw_input.h"
#include <raylib.h>
#include <string>
#include <string_view>
namespace rw
{
// Replay-aware immediate-mode widgets for small in-game overlays.
//
// Every hit/click decision reads the mouse through rw::GetMousePosition,
// rw::IsMouseButtonDown, rw::IsMouseButtonPressed and rw::IsMouseButtonReleased.
// That keeps buttons working under `rwcli run --input`, where raylib's raw mouse
// state is not driven by the replay script. Drawing still goes through raylib.
//
// The widgets remember only the id of the active press. A click completes when
// the same widget is pressed and released, and cancels if the release happens
// elsewhere. Everything else (hover, labels, panels) is recomputed from input.
struct UITheme
{
    Color Background{ 32, 32, 38, 255 };
    Color Text{ 235, 235, 240, 255 };
    Color Accent{ 76, 132, 255, 255 };
    Color Border{ 72, 72, 84, 255 };
    float FontSize = 20.0f;
};

class UI
{
public:
    explicit UI(UITheme theme = {}) : m_Theme(theme) {}

    void SetTheme(const UITheme& theme) { m_Theme = theme; }
    const UITheme& GetTheme() const { return m_Theme; }

    // Inclusive hit test: a point exactly on an edge counts as a hit. This is
    // the same rule raylib's CheckCollisionPointRec uses, kept here so the
    // widgets do not depend on raylib for input math and stay testable.
    static bool b_HitTest(Rectangle rect, Vector2 point)
    {
        return point.x >= rect.x && point.x <= rect.x + rect.width && point.y >= rect.y &&
            point.y <= rect.y + rect.height;
    }

    // A completed click is a press and a release that both land inside the
    // rectangle. Pure so tests can cover the edge cases without a window.
    static bool b_CompletedClick(Rectangle rect, Vector2 press, Vector2 release)
    {
        return b_HitTest(rect, press) && b_HitTest(rect, release);
    }

    void Label(std::string_view text, Vector2 position) const
    {
        const std::string value(text);
        DrawTextEx(GetFontDefault(), value.c_str(), position, m_Theme.FontSize, c_TextSpacing, m_Theme.Text);
    }

    // Draws a button and returns true on the frame a click completes inside it.
    //
    // A click is a press and a release on the same button. Releasing outside
    // cancels it. The active id is the only retained UI state.
    bool Button(std::string_view label, Rectangle rect) const
    {
        const Vector2 mouse = rw::GetMousePosition();
        const bool b_hovered = b_HitTest(rect, mouse);
        const std::string widget = WidgetId("button", label, rect);

        if (b_hovered && rw::IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        {
            m_ActiveWidget = widget;
        }

        const bool b_pressed = (m_ActiveWidget == widget) && b_hovered && rw::IsMouseButtonDown(MOUSE_BUTTON_LEFT);
        bool b_clicked = false;
        if ((m_ActiveWidget == widget) && rw::IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
        {
            b_clicked = b_hovered;
            m_ActiveWidget.clear();
        }

        Color fill = m_Theme.Background;
        Color text_color = m_Theme.Text;
        Color border = m_Theme.Border;
        if (b_pressed)
        {
            fill = ColorBrightness(m_Theme.Accent, -0.25f);
            text_color = m_Theme.Background;
            border = m_Theme.Accent;
        }
        else if (b_hovered)
        {
            fill = m_Theme.Accent;
            text_color = m_Theme.Background;
            border = m_Theme.Accent;
        }

        DrawRectangleRec(rect, fill);
        DrawRectangleLinesEx(rect, 1.0f, border);

        const std::string value(label);
        const Vector2 size = MeasureTextEx(GetFontDefault(), value.c_str(), m_Theme.FontSize, c_TextSpacing);
        const Vector2 origin{ rect.x + ((rect.width - size.x) * 0.5f), rect.y + ((rect.height - size.y) * 0.5f) };
        DrawTextEx(GetFontDefault(), value.c_str(), origin, m_Theme.FontSize, c_TextSpacing, text_color);
        return b_clicked;
    }

    // Draws a checkbox with its label and toggles `value` when the same box is
    // pressed and released. Returns true on the frame the value changed.
    bool Checkbox(std::string_view label, Vector2 position, bool& value) const
    {
        const std::string text(label);
        const float box_size = m_Theme.FontSize;
        const Rectangle box{ position.x, position.y, box_size, box_size };

        const Vector2 text_size = MeasureTextEx(GetFontDefault(), text.c_str(), m_Theme.FontSize, c_TextSpacing);
        const Rectangle hit_area{ box.x, box.y, box_size + c_Padding + text_size.x, box_size };
        const std::string widget = WidgetId("checkbox", label, hit_area);

        const Vector2 mouse = rw::GetMousePosition();
        const bool b_hovered = b_HitTest(hit_area, mouse);
        if (b_hovered && rw::IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        {
            m_ActiveWidget = widget;
        }

        bool b_clicked = false;
        if ((m_ActiveWidget == widget) && rw::IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
        {
            b_clicked = b_hovered;
            m_ActiveWidget.clear();
        }
        if (b_clicked)
        {
            value = !value;
        }

        DrawRectangleRec(box, value ? m_Theme.Accent : m_Theme.Background);
        DrawRectangleLinesEx(box, 1.0f, (b_hovered || value) ? m_Theme.Accent : m_Theme.Border);
        if (value)
        {
            const float inset = box_size * 0.25f;
            const Rectangle inner{ box.x + inset, box.y + inset, box.width - (inset * 2.0f),
                box.height - (inset * 2.0f) };
            DrawRectangleRec(inner, m_Theme.Background);
        }

        const Vector2 text_origin{ box.x + box_size + c_Padding, box.y + ((box_size - text_size.y) * 0.5f) };
        DrawTextEx(GetFontDefault(), text.c_str(), text_origin, m_Theme.FontSize, c_TextSpacing, m_Theme.Text);

        if (b_clicked)
        {
            return true;
        }
        return false;
    }

    void Panel(Rectangle rect) const
    {
        DrawRectangleRec(rect, m_Theme.Background);
        DrawRectangleLinesEx(rect, 1.0f, m_Theme.Border);
    }

private:
    static constexpr float c_TextSpacing = 1.0f;
    static constexpr float c_Padding = 6.0f;

    static std::string WidgetId(const char* kind, std::string_view label, Rectangle rect)
    {
        std::string id(kind);
        id.push_back(':');
        id.append(label);
        id.push_back(':');
        id.append(std::to_string(rect.x));
        id.push_back(',');
        id.append(std::to_string(rect.y));
        id.push_back(',');
        id.append(std::to_string(rect.width));
        id.push_back(',');
        id.append(std::to_string(rect.height));
        return id;
    }

    UITheme m_Theme;
    mutable std::string m_ActiveWidget;
};
} // namespace rw
