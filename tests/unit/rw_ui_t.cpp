#include "Engine/rw_ui.h"
#include "doctest/doctest.h"
namespace
{
constexpr Rectangle c_Rect{ 10.0f, 20.0f, 100.0f, 40.0f };
} // namespace

TEST_CASE("rw::UI: b_HitTest covers inside, edges, and outside")
{
    CHECK(rw::UI::b_HitTest(c_Rect, { 50.0f, 40.0f }));

    // Inclusive edges.
    CHECK(rw::UI::b_HitTest(c_Rect, { 10.0f, 20.0f }));
    CHECK(rw::UI::b_HitTest(c_Rect, { 110.0f, 60.0f }));
    CHECK(rw::UI::b_HitTest(c_Rect, { 10.0f, 60.0f }));
    CHECK(rw::UI::b_HitTest(c_Rect, { 110.0f, 20.0f }));

    CHECK_FALSE(rw::UI::b_HitTest(c_Rect, { 9.9f, 40.0f }));
    CHECK_FALSE(rw::UI::b_HitTest(c_Rect, { 110.1f, 40.0f }));
    CHECK_FALSE(rw::UI::b_HitTest(c_Rect, { 50.0f, 19.9f }));
    CHECK_FALSE(rw::UI::b_HitTest(c_Rect, { 50.0f, 60.1f }));
}

TEST_CASE("rw::UI: b_HitTest treats a zero-size rect as a point")
{
    const Rectangle point_rect{ 5.0f, 5.0f, 0.0f, 0.0f };
    CHECK(rw::UI::b_HitTest(point_rect, { 5.0f, 5.0f }));
    CHECK_FALSE(rw::UI::b_HitTest(point_rect, { 5.1f, 5.0f }));
}

TEST_CASE("rw::UI: b_CompletedClick requires press and release inside")
{
    CHECK(rw::UI::b_CompletedClick(c_Rect, { 50.0f, 40.0f }, { 60.0f, 40.0f }));
    CHECK(rw::UI::b_CompletedClick(c_Rect, { 10.0f, 20.0f }, { 110.0f, 60.0f }));

    CHECK_FALSE(rw::UI::b_CompletedClick(c_Rect, { 50.0f, 40.0f }, { 200.0f, 40.0f }));
    CHECK_FALSE(rw::UI::b_CompletedClick(c_Rect, { 200.0f, 40.0f }, { 50.0f, 40.0f }));
    CHECK_FALSE(rw::UI::b_CompletedClick(c_Rect, { 0.0f, 0.0f }, { 200.0f, 200.0f }));
}

TEST_CASE("rw::UI: default theme is sane")
{
    const rw::UI ui;
    const rw::UITheme& theme = ui.GetTheme();
    CHECK(theme.FontSize == doctest::Approx(20.0f));
    CHECK(theme.Background.a == 255);
    CHECK(theme.Text.a == 255);
    CHECK(theme.Accent.a == 255);
    CHECK(theme.Border.a == 255);
}

TEST_CASE("rw::UI: SetTheme round-trips through GetTheme")
{
    rw::UI ui;
    rw::UITheme theme;
    theme.Background = { 1, 2, 3, 255 };
    theme.Text = { 4, 5, 6, 255 };
    theme.Accent = { 7, 8, 9, 255 };
    theme.Border = { 10, 11, 12, 255 };
    theme.FontSize = 32.0f;

    ui.SetTheme(theme);
    const rw::UITheme& stored = ui.GetTheme();
    CHECK(stored.Background.r == 1);
    CHECK(stored.Background.g == 2);
    CHECK(stored.Background.b == 3);
    CHECK(stored.Text.r == 4);
    CHECK(stored.Accent.b == 9);
    CHECK(stored.Border.g == 11);
    CHECK(stored.FontSize == doctest::Approx(32.0f));
}

TEST_CASE("rw::UI: constructor stores a custom theme")
{
    rw::UITheme theme;
    theme.FontSize = 14.0f;
    theme.Accent = { 200, 100, 50, 255 };

    const rw::UI ui(theme);
    CHECK(ui.GetTheme().FontSize == doctest::Approx(14.0f));
    CHECK(ui.GetTheme().Accent.r == 200);
    CHECK(ui.GetTheme().Accent.g == 100);
    CHECK(ui.GetTheme().Accent.b == 50);
}
