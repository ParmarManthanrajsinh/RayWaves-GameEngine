#include "BuildDiagnostics.h"
#include "doctest/doctest.h"

using BuildDiagnostics::ESeverity;
using BuildDiagnostics::t_Diagnostic;

TEST_CASE("clang error with column")
{
    t_Diagnostic diagnostic;
    REQUIRE(BuildDiagnostics::b_ParseLine(
        "D:/Games/My Game/GameLogic/Player.cpp:42:7: error: use of undeclared identifier 'speed'", diagnostic));
    CHECK(diagnostic.m_Severity == ESeverity::Error);
    CHECK(diagnostic.m_File == "D:/Games/My Game/GameLogic/Player.cpp");
    CHECK(diagnostic.m_Line == 42);
    CHECK(diagnostic.m_Column == 7);
}

TEST_CASE("clang warning")
{
    t_Diagnostic diagnostic;
    REQUIRE(BuildDiagnostics::b_ParseLine(
        "C:\\Projects\\RayWaves\\GameLogic\\Level.cpp:10:3: warning: unused variable 'value'", diagnostic));
    CHECK(diagnostic.m_Severity == ESeverity::Warning);
    CHECK(diagnostic.m_File == "C:\\Projects\\RayWaves\\GameLogic\\Level.cpp");
    CHECK(diagnostic.m_Line == 10);
    CHECK(diagnostic.m_Column == 3);
}

TEST_CASE("ninja failure line is not a location diagnostic")
{
    t_Diagnostic diagnostic;
    CHECK_FALSE(BuildDiagnostics::b_ParseLine("FAILED: CMakeFiles/GameLogic.dir/Game.cpp.obj", diagnostic));
}

TEST_CASE("msvc error with line and column")
{
    t_Diagnostic diagnostic;
    REQUIRE(BuildDiagnostics::b_ParseLine(
        "C:\\Projects\\Game\\Player.cpp(42,7): error C2039: 'Speed': is not a member of 'Player'", diagnostic));
    CHECK(diagnostic.m_Severity == ESeverity::Error);
    CHECK(diagnostic.m_File == "C:\\Projects\\Game\\Player.cpp");
    CHECK(diagnostic.m_Line == 42);
    CHECK(diagnostic.m_Column == 7);
}

TEST_CASE("msvc warning with line only")
{
    t_Diagnostic diagnostic;
    REQUIRE(BuildDiagnostics::b_ParseLine(
        "C:\\Projects\\Game\\Player.cpp(10): warning C4101: 'x': unreferenced", diagnostic));
    CHECK(diagnostic.m_Severity == ESeverity::Warning);
    CHECK(diagnostic.m_File == "C:\\Projects\\Game\\Player.cpp");
    CHECK(diagnostic.m_Line == 10);
    CHECK(diagnostic.m_Column == 0);
}

TEST_CASE("plain output is not a diagnostic")
{
    t_Diagnostic diagnostic;
    CHECK_FALSE(BuildDiagnostics::b_ParseLine("[3/9] Building CXX object Game.cpp.obj", diagnostic));
    CHECK_FALSE(BuildDiagnostics::b_ParseLine("-- Configuring done", diagnostic));
}
