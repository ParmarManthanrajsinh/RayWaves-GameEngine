#include "Engine/PlatformNames.h"
#include "doctest/doctest.h"
#include <string>

TEST_CASE("platform names follow the host conventions")
{
    if (rw::platform::b_IsWindows)
    {
        CHECK(rw::platform::ExecutableName("rwcli") == "rwcli.exe");
        CHECK(rw::platform::SharedLibraryName("GameLogic") == "GameLogic.dll");
        CHECK(std::string(rw::platform::c_GameLogicModuleName) == "GameLogic.dll");
        CHECK(std::string(rw::platform::c_RaylibRuntimeName) == "libraylib.dll");
        CHECK(rw::platform::c_PathListSeparator == ';');
    }
    else
    {
        CHECK(rw::platform::ExecutableName("rwcli") == "rwcli");
        CHECK(rw::platform::SharedLibraryName("GameLogic") == "GameLogic.so");
        CHECK(std::string(rw::platform::c_GameLogicModuleName) == "GameLogic.so");
        CHECK(std::string(rw::platform::c_RaylibRuntimeName) == "libraylib.so");
        CHECK(rw::platform::c_PathListSeparator == ':');
    }
}
