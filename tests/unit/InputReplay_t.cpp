#include "PlatformInfo.h"
#include "doctest/doctest.h"
#include "rw.h"
#include "support/TempDir.h"
#include <cstdlib>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

TEST_CASE("agent input script replays key down and up")
{
    TempDir temp("raywaves_input");
    const fs::path script = temp.Path() / "input.txt";
    {
        std::ofstream file(script);
        file << "# frame,key,state\n";
        file << "0,KEY_RIGHT,down\n";
        file << "2,KEY_RIGHT,up\n";
        file << "0,mouse,640,360\n";
    }

    PlatformInfo::b_SetEnvironmentVariable("RAYWAVES_AGENT_INPUT", script.string());

    rw::input_detail::State() = rw::input_detail::FState{};
    rw::input_detail::BeginFrame(); // frame 0

    CHECK(rw::IsKeyDown(KEY_RIGHT));
    CHECK(rw::IsKeyPressed(KEY_RIGHT));
    CHECK(rw::GetMousePosition().x == 640.0f);
    CHECK(rw::GetMousePosition().y == 360.0f);

    rw::input_detail::BeginFrame(); // frame 1
    CHECK(rw::IsKeyDown(KEY_RIGHT));
    CHECK_FALSE(rw::IsKeyPressed(KEY_RIGHT));

    rw::input_detail::BeginFrame(); // frame 2
    CHECK_FALSE(rw::IsKeyDown(KEY_RIGHT));
    CHECK(rw::IsKeyReleased(KEY_RIGHT));

    rw::input_detail::State() = rw::input_detail::FState{};
    PlatformInfo::b_SetEnvironmentVariable("RAYWAVES_AGENT_INPUT", "");
}
