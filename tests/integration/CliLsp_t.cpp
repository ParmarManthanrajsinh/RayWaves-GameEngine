#include "Engine/PlatformNames.h"
#include "ProcessRunner.h"
#include "doctest/doctest.h"
#include "support/TempDir.h"
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

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
} // namespace

TEST_CASE("rwcli lsp writes editor configs and a compilation database")
{
    TempDir temp("raywaves_lsp");
    const fs::path project = temp.Path() / "LspGame";
    WriteFile(project / "project.raywaves",
        "[project]\n"
        "name=LspGame\n"
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
    WriteFile(project / "GameLogic" / "Game.cpp", "#include \"rw.h\"\n");

    const std::string output = RunCli("lsp " + ProcessRunner::QuoteArg(project.string()) + " --json");
    CHECK(output.find("\"ok\":true") != std::string::npos);
    CHECK(output.find("\"files\":1") != std::string::npos);

    CHECK(fs::exists(project / ".clangd"));
    CHECK(fs::exists(project / ".vscode" / "c_cpp_properties.json"));
    CHECK(fs::exists(project / ".vscode" / "settings.json"));
    CHECK(fs::exists(project / "compile_commands.json"));
    CHECK(fs::exists(project / ".agents" / "skills" / "raywaves-game" / "SKILL.md"));
    CHECK(fs::exists(project / ".agents" / "skills" / "raywaves-game" / "reference.md"));
    CHECK(fs::exists(project / "AGENTS.md"));
    CHECK(fs::exists(project / ".raywaves" / "tooling.ini"));
    const fs::path wrapper = project / ".raywaves" / "bin" / (rw::platform::b_IsWindows ? "rwcli.cmd" : "rwcli");
    CHECK(fs::exists(wrapper));

    std::string wrapper_output;
    std::vector<std::string> wrapper_argv;
    if (rw::platform::b_IsWindows)
    {
        wrapper_argv = { "cmd.exe", "/C", wrapper.string(), "info", ".", "--json" };
    }
    else
    {
        wrapper_argv = { wrapper.string(), "info", ".", "--json" };
    }
    ProcessRunner::RunAndWaitCommand(wrapper_argv, project.string(),
        [&wrapper_output](std::string_view line, bool)
        {
            wrapper_output += line;
            wrapper_output += "\n";
        });
    CHECK(wrapper_output.find("\"ok\":true") != std::string::npos);

    const std::string clangd = [&]()
    {
        std::ifstream file(project / ".clangd");
        return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    }();
#if defined(_WIN32)
    CHECK(clangd.find("x86_64-windows-gnu") != std::string::npos);
    CHECK(clangd.find("libcxx") != std::string::npos);
#else
    // Linux uses the host libstdc++/glibc headers; Zig's target triple and
    // per-architecture libc includes would make clangd parse conflicting
    // definitions.
    CHECK(clangd.find("-std=c++23") != std::string::npos);
    CHECK(clangd.find("aarch64-linux") == std::string::npos);
#endif
}
