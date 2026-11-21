#include "AssetResolver.h"
#include "CliCommon.h"
#include "Commands.h"
#include "DllLoader.h"
#include "GameConfig.h"
#include "GameEngine.h"
#include "GameLogicHost.h"
#include "GameMap.h"
#include "GameState.h"
#include "Json.h"
#include "MapManager.h"
#include "PlatformInfo.h"
#include "PlatformNames.h"
#include "ProcessRunner.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <thread>

namespace fs = std::filesystem;

namespace
{
std::string EntityJson(const t_EditorEntity& entity)
{
    std::string out = "{";
    out += CliJson::Key("id") + CliJson::Number(static_cast<int>(entity.m_Id)) + ",";
    out += CliJson::Key("name") + CliJson::Quoted(entity.m_Name) + ",";
    out += CliJson::Key("type") + CliJson::Quoted(entity.m_Type) + ",";

    if (entity.m_Position != nullptr)
    {
        out += CliJson::Key("position") + "[" + CliJson::Number(entity.m_Position->x) + "," +
            CliJson::Number(entity.m_Position->y) + "],";
    }
    else if (entity.m_Position3D != nullptr)
    {
        out += CliJson::Key("position") + "[" + CliJson::Number(entity.m_Position3D->x) + "," +
            CliJson::Number(entity.m_Position3D->y) + "," + CliJson::Number(entity.m_Position3D->z) + "],";
    }

    if (entity.m_Size != nullptr)
    {
        out += CliJson::Key("size") + "[" + CliJson::Number(entity.m_Size->x) + "," +
            CliJson::Number(entity.m_Size->y) + "],";
    }
    else if (entity.m_Size3D != nullptr)
    {
        out += CliJson::Key("size") + "[" + CliJson::Number(entity.m_Size3D->x) + "," +
            CliJson::Number(entity.m_Size3D->y) + "," + CliJson::Number(entity.m_Size3D->z) + "],";
    }

    if (entity.m_Rotation != nullptr)
    {
        out += CliJson::Key("rotation") + CliJson::Number(*entity.m_Rotation) + ",";
    }
    if (entity.m_Visible != nullptr)
    {
        out += CliJson::Key("visible") + CliJson::Bool(*entity.m_Visible) + ",";
    }
    if (out.back() == ',')
    {
        out.pop_back();
    }
    out += "}";
    return out;
}

std::string StateJson(const StateBag& bag)
{
    std::string out = "{";
    bool b_needs_comma = false;
    const auto append = [&out, &b_needs_comma](std::string_view key, const std::string& value)
    {
        if (b_needs_comma)
        {
            out += ",";
        }
        b_needs_comma = true;
        out += CliJson::Quoted(key) + ":" + value;
    };

    for (const auto& [key, value] : bag.Floats())
    {
        append(key, CliJson::Number(value));
    }
    for (const auto& [key, value] : bag.Ints())
    {
        append(key, CliJson::Number(value));
    }
    for (const auto& [key, value] : bag.Bools())
    {
        append(key, CliJson::Bool(value));
    }
    for (const auto& [key, value] : bag.Strings())
    {
        append(key, CliJson::Quoted(value));
    }

    out += "}";
    return out;
}

bool b_WriteFile(const fs::path& path, std::string_view text)
{
    std::error_code error;
    fs::create_directories(path.parent_path(), error);
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file.is_open())
    {
        return false;
    }
    file << text;
    return file.good();
}

int FailWorker(const fs::path& result_path, std::string_view code, std::string_view message)
{
    const std::string json = ErrorJson(code, message);
    b_WriteFile(result_path, json);
    std::cerr << message << "\n";
    return 5;
}

bool b_ParseSize(const std::string& text, int& out_width, int& out_height)
{
    const size_t x = text.find_first_of("xX");
    if (x == std::string::npos)
    {
        return false;
    }
    const int width = std::atoi(text.substr(0, x).c_str());
    const int height = std::atoi(text.substr(x + 1).c_str());
    if (width <= 0 || height <= 0)
    {
        return false;
    }
    out_width = width;
    out_height = height;
    return true;
}

std::string ScreenshotJson(const fs::path& path, bool b_requested)
{
    if (!b_requested)
    {
        return "null";
    }

    std::error_code size_error;
    const auto bytes = fs::file_size(path, size_error);
    Image image = LoadImage(path.string().c_str());
    const bool b_valid = IsImageValid(image) && image.width > 0 && image.height > 0;
    if (IsImageValid(image))
    {
        UnloadImage(image);
    }

    return "{" + CliJson::Key("path") + CliJson::Quoted(path.string()) + "," + CliJson::Key("bytes") +
        CliJson::Number(static_cast<double>(size_error ? 0 : bytes)) + "," + CliJson::Key("nonEmpty") +
        CliJson::Bool(b_valid) + "}";
}

void AppendOption(std::string& command, const FArgs& args, const char* name)
{
    if (args.b_Has(name))
    {
        command += std::string(" --") + name + " " + ProcessRunner::QuoteArg(args.Get(name));
    }
}

void AppendFlag(std::string& command, const FArgs& args, const char* name)
{
    if (args.b_Has(name))
    {
        command += std::string(" --") + name;
    }
}
} // namespace

namespace Cli
{
int RunCommand(const FProjectContext& project, const FArgs& args, bool b_json)
{
    const fs::path result_path = project.m_Dir / ".raywaves" / "agent" /
        ("run-" + std::to_string(PlatformInfo::GetCurrentProcessId()) + ".json");
    std::error_code error;
    fs::create_directories(result_path.parent_path(), error);
    fs::remove(result_path, error);

    if (!fs::exists(project.m_Project.m_DllPath))
    {
        PrintError(b_json, "dll_missing",
            std::string(rw::platform::c_GameLogicModuleName) + " not found. Run: rwcli build <project>");
        return 3;
    }

    CleanupStaleShadowCopies(project.m_Dir);

    std::string command = ProcessRunner::QuoteArg(GetSelfPath().string()) + " __run-worker --project " +
        ProcessRunner::QuoteArg(project.m_Dir.string()) + " --result " + ProcessRunner::QuoteArg(result_path.string());

    AppendOption(command, args, "frames");
    AppendOption(command, args, "dt");
    AppendOption(command, args, "seed");
    AppendOption(command, args, "screenshot");
    AppendOption(command, args, "screenshot-frame");
    AppendOption(command, args, "state");
    AppendOption(command, args, "size");
    AppendOption(command, args, "timeout");
    AppendOption(command, args, "input");
    AppendFlag(command, args, "visible");
    AppendFlag(command, args, "audio");

    const int timeout_seconds = args.GetInt("timeout", 60);
    auto cancel = std::make_shared<std::atomic<bool>>(false);
    std::atomic<bool> b_timed_out{ false };
    std::thread watchdog;
    if (timeout_seconds > 0)
    {
        watchdog = std::thread(
            [cancel, &b_timed_out, timeout_seconds]()
            {
                const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(timeout_seconds);
                while (std::chrono::steady_clock::now() < deadline)
                {
                    if (cancel->load())
                    {
                        return;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
                b_timed_out.store(true);
                cancel->store(true);
            });
    }

    std::vector<std::string> tail;
    int exit_code = 0;
    const bool b_success = ProcessRunner::RunAndWaitCommandLine(
        command, project.m_Dir.string(),
        [&tail](std::string_view line, bool b_error)
        {
            (void)b_error;
            tail.push_back(std::string(line));
            if (tail.size() > 40)
            {
                tail.erase(tail.begin());
            }
        },
        cancel, &exit_code);

    if (watchdog.joinable())
    {
        cancel->store(true);
        watchdog.join();
    }

    if (fs::exists(result_path))
    {
        std::ifstream file(result_path, std::ios::binary);
        const std::string result((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        fs::remove(result_path, error);
        if (!result.empty())
        {
            std::cout << result;
            if (result.back() != '\n')
            {
                std::cout << "\n";
            }
            if (exit_code == 6)
            {
                return 6;
            }
            return b_success ? 0 : 5;
        }
    }

    if (b_timed_out.load())
    {
        if (b_json)
        {
            std::cout << ErrorJson("run_timeout", "Run timed out after " + std::to_string(timeout_seconds) + "s")
                      << "\n";
        }
        return 6;
    }

    const std::string message = "GameLogic process produced no result. Logs:\n" + JoinTail(tail, 30);
    if (b_json)
    {
        std::cout << "{\"ok\":false,\"error\":{\"code\":\"run_crashed\",\"message\":" + CliJson::Quoted(message) +
                ",\"logs\":" + CliJson::Quoted(JoinTail(tail, 30)) + "}}\n";
    }
    else
    {
        std::cerr << message << "\n";
    }
    return 8;
}

int RunWorker(const FArgs& args)
{
    const fs::path result_path(args.Get("result"));
    if (result_path.empty())
    {
        std::cerr << "worker: missing --result\n";
        return 2;
    }

    FProjectContext project;
    std::string error;
    if (!b_ResolveProject(args.Get("project"), project, error))
    {
        return FailWorker(result_path, "invalid_project", error);
    }

    std::error_code chdir_error;
    fs::current_path(project.m_Dir, chdir_error);
    if (chdir_error)
    {
        return FailWorker(
            result_path, "invalid_project", "Could not enter project directory: " + chdir_error.message());
    }

    fs::path input_path;
    if (args.b_Has("input"))
    {
        input_path = args.Get("input");
        if (input_path.is_relative())
        {
            input_path = project.m_Dir / input_path;
        }
        if (!fs::exists(input_path))
        {
            return FailWorker(result_path, "input_missing", "Input script not found: " + input_path.string());
        }
        if (!PlatformInfo::b_SetEnvironmentVariable("RAYWAVES_AGENT_INPUT", input_path.string()))
        {
            return FailWorker(result_path, "input_missing", "Could not set RAYWAVES_AGENT_INPUT");
        }
    }

    const int frames = std::max(args.GetInt("frames", 60), 1);
    const float dt = (args.GetFloat("dt", 1.0f / 60.0f) > 0.0f) ? args.GetFloat("dt", 1.0f / 60.0f) : (1.0f / 60.0f);
    const int seed = args.GetInt("seed", 1);
    const int requested_shot_frame = args.GetInt("screenshot-frame", 0);
    const int shot_frame = (requested_shot_frame > 0) ? std::min(requested_shot_frame, frames) : frames;
    const int timeout_seconds = args.GetInt("timeout", 60);

    const bool b_visible = args.b_Has("visible");
    const bool b_audio = args.b_Has("audio");
    const bool b_want_screenshot = args.b_Has("screenshot");

    fs::path screenshot_path;
    std::string raylib_screenshot_path;
    if (b_want_screenshot)
    {
        screenshot_path = args.Get("screenshot");
        if (screenshot_path.is_relative())
        {
            screenshot_path = project.m_Dir / screenshot_path;
        }
        std::error_code screenshot_error;
        fs::create_directories(screenshot_path.parent_path(), screenshot_error);

        // raylib prefixes screenshots with its storage base path (the working
        // directory at InitWindow), so hand it a project-relative name.
        raylib_screenshot_path = fs::relative(screenshot_path, project.m_Dir, screenshot_error).string();
        if (screenshot_error)
        {
            raylib_screenshot_path = screenshot_path.string();
        }
    }

    t_WindowConfig config;
    config.width = project.m_Project.m_SceneWidth > 0 ? project.m_Project.m_SceneWidth : 1280;
    config.height = project.m_Project.m_SceneHeight > 0 ? project.m_Project.m_SceneHeight : 720;
    config.title = "RayWaves Agent Run";
    config.b_Resizable = false;
    config.b_Vsync = false;
    config.target_fps = 0;
    config.b_Hidden = !b_visible;
    config.b_NoAudio = !b_audio;

    int width = 0;
    int height = 0;
    if (b_ParseSize(args.Get("size"), width, height))
    {
        config.width = width;
        config.height = height;
    }

    AssetResolver::SetProjectAssetPath(project.m_Project.m_AssetPath);

    GameEngine engine;
    if (!engine.LaunchWindow(config))
    {
        return FailWorker(result_path, "run_window", "Could not create the render window");
    }

    SetTargetFPS(0);
    SetExitKey(KEY_NULL);
    SetRandomSeed(static_cast<unsigned int>(seed));

    GameLogicHost host(engine);
    if (!host.b_Load(project.m_Project.m_DllPath, error))
    {
        CloseWindow();
        return FailWorker(result_path, "dll_load", error);
    }

    GameMap* active_map = (host.GetMapManager() != nullptr) ? host.GetMapManager()->GetCurrentMap() : host.GetMap();
    if (active_map == nullptr)
    {
        host.Unload();
        CloseWindow();
        return FailWorker(result_path, "dll_load", "GameLogic created no active map");
    }

    const auto start_time = std::chrono::steady_clock::now();
    bool b_timed_out = false;
    bool b_screenshot_taken = false;

    for (int frame = 0; frame < frames; ++frame)
    {
        engine.SetViewportSize(GetScreenWidth(), GetScreenHeight());
        engine.UpdateMap(dt);

        BeginDrawing();
        ClearBackground(BLACK);
        engine.DrawMap();
        EndDrawing();

        if (b_want_screenshot && !b_screenshot_taken && (frame + 1) >= shot_frame)
        {
            TakeScreenshot(raylib_screenshot_path.c_str());
            b_screenshot_taken = true;
        }

        if (timeout_seconds > 0)
        {
            const auto elapsed =
                std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start_time)
                    .count();
            if (elapsed > (static_cast<int64_t>(timeout_seconds) * 1000))
            {
                b_timed_out = true;
                break;
            }
        }
    }

    const auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start_time).count();

    if (b_want_screenshot && !b_screenshot_taken)
    {
        TakeScreenshot(raylib_screenshot_path.c_str());
    }

    StateBag state;
    active_map->SaveState(state);

    if (args.b_Has("state"))
    {
        fs::path state_path = args.Get("state");
        if (state_path.is_relative())
        {
            state_path = project.m_Dir / state_path;
        }
        b_WriteFile(state_path, StateJson(state));
    }

    std::vector<std::string> entity_items;
    for (const t_EditorEntity& entity : active_map->GetEditorEntities())
    {
        entity_items.push_back(EntityJson(entity));
    }

    const std::string result = "{" + CliJson::Key("ok") + CliJson::Bool(!b_timed_out) + "," + CliJson::Key("map") +
        CliJson::Quoted(active_map->GetMapName()) + "," + CliJson::Key("frames") + CliJson::Number(frames) + "," +
        CliJson::Key("dt") + CliJson::Number(dt) + "," + CliJson::Key("durationMs") +
        CliJson::Number(static_cast<double>(duration)) + "," + CliJson::Key("seed") + CliJson::Number(seed) + "," +
        CliJson::Key("input") + (input_path.empty() ? "null" : CliJson::Quoted(input_path.string())) + "," +
        CliJson::Key("screenshot") + ScreenshotJson(screenshot_path, b_want_screenshot) + "," +
        CliJson::Key("entities") + CliJson::Array(entity_items) + "," + CliJson::Key("state") + StateJson(state) + "}";

    host.Unload();
    if (!config.b_NoAudio)
    {
        CloseAudioDevice();
    }
    CloseWindow();

    b_WriteFile(result_path, result);
    return b_timed_out ? 6 : 0;
}
} // namespace Cli
