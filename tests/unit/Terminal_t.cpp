#include "doctest/doctest.h"

#include "terminal/shell_detect.h"
#include "terminal/vterm_screen.h"

#include <string>

TEST_CASE("Terminal screen: writes text and tracks cursor")
{
    term::t_VTermScreen screen(10, 20);
    screen.Write("hello");

    CHECK(screen.Rows() == 10);
    CHECK(screen.Cols() == 20);
    CHECK(screen.CursorRow() == 0);
    CHECK(screen.CursorCol() == 5);

    term::t_Cell cell;
    REQUIRE(screen.b_GetCell(0, 0, cell));
    CHECK(cell.codepoints[0] == 'h');

    REQUIRE(screen.b_GetCell(0, 4, cell));
    CHECK(cell.codepoints[0] == 'o');
}

TEST_CASE("Terminal screen: scrolls lines into scrollback")
{
    term::t_VTermScreen screen(3, 10);
    screen.Write("one\r\ntwo\r\nthree\r\nfour");

    CHECK(screen.ScrollbackSize() == 1);

    term::t_Cell cell;
    REQUIRE(screen.b_GetCell(0, 0, cell));
    CHECK(cell.codepoints[0] == 'o');

    CHECK(screen.GetRowText(0) == "one");
    CHECK(screen.GetRowText(1) == "two");
    CHECK(screen.GetRowText(2) == "three");
    CHECK(screen.GetRowText(3) == "four");
}

TEST_CASE("Terminal screen: parses SGR colors")
{
    term::t_VTermScreen screen(5, 10);
    screen.Write("\x1b[31mR\x1b[0mN");

    term::t_Cell red;
    REQUIRE(screen.b_GetCell(0, 0, red));
    CHECK_FALSE(red.b_DefaultFg);
    CHECK(red.fg.r > red.fg.g);

    term::t_Cell normal;
    REQUIRE(screen.b_GetCell(0, 1, normal));
    CHECK(normal.b_DefaultFg);
}

TEST_CASE("Terminal screen: resizes without losing text")
{
    term::t_VTermScreen screen(5, 10);
    screen.Write("hello");

    screen.Resize(3, 20);
    CHECK(screen.Rows() == 3);
    CHECK(screen.Cols() == 20);

    term::t_Cell cell;
    REQUIRE(screen.b_GetCell(0, 0, cell));
    CHECK(cell.codepoints[0] == 'h');

    screen.Resize(8, 6);
    CHECK(screen.Rows() == 8);
    CHECK(screen.Cols() == 6);
    CHECK(screen.GetRowText(0) == "hello");
}

TEST_CASE("Terminal screen: extracts selection text")
{
    term::t_VTermScreen screen(5, 20);
    screen.Write("abc\r\ndef");

    CHECK(screen.GetText(0, 0, 1, 2) == "abc\ndef");
    CHECK(screen.GetText(1, 0, 0, 0) == "abc\nd");
}

TEST_CASE("Terminal screen: encodes keyboard input")
{
    term::t_VTermScreen screen(5, 10);
    std::string output;
    screen.SetOutputCallback([&output](std::string_view bytes) { output.append(bytes); });

    screen.KeyboardUnichar('a');
    screen.KeyboardUnichar('c', VTERM_MOD_CTRL);

    CHECK(output == "a\x03");
}

TEST_CASE("Terminal screen: sends paste through the output callback")
{
    term::t_VTermScreen screen(5, 10);
    std::string output;
    screen.SetOutputCallback([&output](std::string_view bytes) { output.append(bytes); });

    screen.SendPaste("ab\ncd");

    CHECK(output == "ab\rcd");
}

TEST_CASE("Terminal shell detection: prefers pwsh then cmd")
{
    const auto exists = [](const std::wstring& name) { return name == L"pwsh.exe" || name == L"cmd.exe"; };

    const std::vector<term::t_ShellSpec> shells = term::DetectShells(exists);
    REQUIRE(shells.size() == 2);
    CHECK(shells[0].display_name == "PowerShell 7");
    CHECK(shells[1].display_name == "Command Prompt");
    CHECK(term::DefaultShell(exists).display_name == "PowerShell 7");
}

TEST_CASE("Terminal shell detection: falls back to cmd")
{
    const auto only_cmd = [](const std::wstring& name) { return name == L"cmd.exe"; };

    const std::vector<term::t_ShellSpec> shells = term::DetectShells(only_cmd);
    REQUIRE(shells.size() == 1);
    CHECK(shells[0].display_name == "Command Prompt");
}
