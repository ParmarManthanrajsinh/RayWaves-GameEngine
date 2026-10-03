#include "doctest/doctest.h"
#include "EditorUtils.h"
#include <string>

// The project path is interpolated into `sh -c` build commands
// (`cd "<path>" && cmake ...`). EditorUtils::IsShellSafe is the gate that
// keeps shell metacharacters out of that string, so it carries the security
// load the old cmd.exe quoting rules used to.
TEST_CASE("GameEditor: POSIX project paths pass the shell gate")
{
    CHECK(EditorUtils::IsShellSafe("/home/dev/MyGame/.raywaves"));
    CHECK(EditorUtils::IsShellSafe("/opt/games/slime-quest_2/.raywaves"));
    CHECK(EditorUtils::IsShellSafe("/tmp/with space/Project/.raywaves"));
    CHECK(EditorUtils::IsShellSafe("/srv/a.b.c/x-y+z/.raywaves"));
}

TEST_CASE("GameEditor: shell metacharacters are rejected")
{
    CHECK_FALSE(EditorUtils::IsShellSafe("/tmp/a&b"));
    CHECK_FALSE(EditorUtils::IsShellSafe("/tmp/a|b"));
    CHECK_FALSE(EditorUtils::IsShellSafe("/tmp/a;b"));
    CHECK_FALSE(EditorUtils::IsShellSafe("/tmp/a$b"));
    CHECK_FALSE(EditorUtils::IsShellSafe("/tmp/a\"b"));
    CHECK_FALSE(EditorUtils::IsShellSafe("/tmp/a`b"));
    CHECK_FALSE(EditorUtils::IsShellSafe("/tmp/a'b"));
    CHECK_FALSE(EditorUtils::IsShellSafe("/tmp/a<b"));
    CHECK_FALSE(EditorUtils::IsShellSafe("/tmp/a>b"));
    CHECK_FALSE(EditorUtils::IsShellSafe("/tmp/ab%"));
    CHECK_FALSE(EditorUtils::IsShellSafe("/tmp/a!b"));
    CHECK_FALSE(EditorUtils::IsShellSafe("/tmp/a^b"));
    CHECK_FALSE(EditorUtils::IsShellSafe("/tmp/a(b)"));
    CHECK_FALSE(EditorUtils::IsShellSafe("/tmp/a@b"));
    CHECK_FALSE(EditorUtils::IsShellSafe("/tmp/a#b"));
    // Backslash is a live escape under sh, unlike under cmd.exe.
    CHECK_FALSE(EditorUtils::IsShellSafe("/tmp/a\\b"));
    // Embedded control characters
    CHECK_FALSE(EditorUtils::IsShellSafe("/tmp/a\nb"));
    CHECK_FALSE(EditorUtils::IsShellSafe("/tmp/a\rb"));
}

TEST_CASE("GameEditor: empty path is rejected")
{
    CHECK_FALSE(EditorUtils::IsShellSafe(""));
}
