#include "doctest/doctest.h"
#include <string>

TEST_CASE("GameEditor: fallback build cmd quoting")
{
    std::string rawExe = R"(D:\tools\cmake.exe)";
    std::string appDir = R"(C:\project)";
    std::string cmd = "\"\"" + rawExe + "\" --build \"" + appDir + "\" --target GameLogic\"";

    CHECK(cmd ==
        "\"\"D:\\tools\\cmake.exe\" --build \"C:\\project\" --target GameLogic\"");
}

TEST_CASE("GameEditor: fallback build cmd leading quote count")
{
    std::string rawExe = R"(C:\tools\cmake.exe)";
    std::string appDir = R"(D:\game)";
    std::string cmd = "\"\"" + rawExe + "\" --build \"" + appDir + "\" --target GameLogic\"";

    size_t leadingQuotes = 0;
    for (char c : cmd)
    {
        if (c == '\"')
            ++leadingQuotes;
        else
            break;
    }
    REQUIRE(leadingQuotes == 2);
}

TEST_CASE("GameEditor: fallback build cmd exactly one quote after exe path")
{
    std::string rawExe = R"(C:\path\to\cmake.exe)";
    std::string appDir = R"(D:\project)";
    std::string cmd = "\"\"" + rawExe + "\" --build \"" + appDir + "\" --target GameLogic\"";

    auto exeEnd = cmd.find("cmake.exe");
    REQUIRE(exeEnd != std::string::npos);
    exeEnd += 9;
    REQUIRE(exeEnd < cmd.length());
    REQUIRE(cmd[exeEnd] == '\"');
    REQUIRE(cmd[exeEnd + 1] == ' ');
}
