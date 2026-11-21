#include "Engine/PlatformNames.h"
#include "ProcessRunner.h"
#include "doctest/doctest.h"
#include "support/TempDir.h"
#include <fstream>
#include <string>

namespace fs = std::filesystem;

namespace
{
std::string RunCli(const std::string& arguments)
{
    const fs::path exe = fs::current_path() / rw::platform::ExecutableName("rwcli");
    const std::string command = ProcessRunner::QuoteArg(exe.string()) + " " + arguments;

    std::string output;
    ProcessRunner::RunAndWaitCommandLine(command, fs::current_path().string(),
        [&output](std::string_view line, bool)
        {
            output += line;
            output += "\n";
        });
    return output;
}

void WriteFile(const fs::path& path, std::string_view text)
{
    fs::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file << text;
}

std::string ReadFile(const fs::path& path)
{
    std::ifstream file(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}
} // namespace

TEST_CASE("rwcli reports project info and edits scenes")
{
    TempDir temp("raywaves_cli");
    const fs::path project = temp.Path() / "CliGame";
    fs::create_directories(project / "GameLogic");
    fs::create_directories(project / "Assets");

    WriteFile(project / "project.raywaves",
        "[project]\n"
        "name=CliGame\n"
        "version=1.0.0\n"
        "engineVersion=0.1.0\n"
        "sourceDir=GameLogic\n"
        "assetDir=Assets\n"
        "entryDll=GameLogic.dll\n"
        "\n"
        "[editor]\n"
        "sceneWidth=640\n"
        "sceneHeight=360\n"
        "targetFPS=60\n");

    const std::string info = RunCli("info " + ProcessRunner::QuoteArg(project.string()) + " --json");
    CHECK(info.find("\"ok\":true") != std::string::npos);
    CHECK(info.find("CliGame") != std::string::npos);

    const std::string add = RunCli("scene add " + ProcessRunner::QuoteArg(project.string()) +
        " --map TestMap --name Crate --texture crate.png --position 10,20 --json");
    CHECK(add.find("\"ok\":true") != std::string::npos);
    CHECK(add.find("\"id\":1") != std::string::npos);

    const fs::path scene_file = project / "Assets" / "TestMap.scene";
    REQUIRE(fs::exists(scene_file));
    const std::string scene_text = ReadFile(scene_file);
    CHECK(scene_text.find("name=Crate") != std::string::npos);
    CHECK(scene_text.find("texture=crate.png") != std::string::npos);

    const std::string list = RunCli("scene list " + ProcessRunner::QuoteArg(project.string()) + " --json");
    CHECK(list.find("\"Crate\"") != std::string::npos);

    const std::string remove =
        RunCli("scene remove " + ProcessRunner::QuoteArg(project.string()) + " --map TestMap --id 1 --json");
    CHECK(remove.find("\"ok\":true") != std::string::npos);
    CHECK(ReadFile(scene_file).find("name=Crate") == std::string::npos);
}

TEST_CASE("rwcli rejects a folder without a manifest")
{
    TempDir temp("raywaves_cli_missing");
    const std::string output = RunCli("info " + ProcessRunner::QuoteArg(temp.Path().string()) + " --json");
    CHECK(output.find("\"ok\":false") != std::string::npos);
    CHECK(output.find("project.raywaves") != std::string::npos);
}
