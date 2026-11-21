#include "Commands.h"
#include "AgentLock.h"
#include "AssetResolver.h"
#include "BuildDiagnostics.h"
#include "CliCommon.h"
#include "DllLoader.h"
#include "EngineVersion.h"
#include "Json.h"
#include "MapManager.h"
#include "PlatformInfo.h"
#include "PlatformNames.h"
#include "ProcessRunner.h"
#include "ProjectManager.h"
#include "Scene.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <iterator>
#include <set>
#include <sstream>

namespace fs = std::filesystem;

namespace
{
constexpr const char* c_MANIFEST = "project.raywaves";

std::string FindClangd();

struct FToolInfo
{
    bool m_bCMake = false;
    bool m_bNinja = false;
    bool m_bZig = false;
    bool m_bRaylib = false;
    fs::path m_CMakePath;
    fs::path m_RaylibDir;
};

FToolInfo DetectTools()
{
    FToolInfo tools;
    const fs::path tools_dir = ProjectManager::GetToolsDirectory();
    tools.m_CMakePath = tools_dir / "cmake" / "bin" / rw::platform::ExecutableName("cmake");
    tools.m_bCMake = fs::exists(tools.m_CMakePath);
    if (!tools.m_bCMake)
    {
        // Dev checkouts may use a system CMake while the generated project
        // still fetches the bundled copy on first use.
        const fs::path system_cmake = PlatformInfo::FindExecutable("cmake");
        if (!system_cmake.empty())
        {
            tools.m_CMakePath = system_cmake;
            tools.m_bCMake = true;
        }
    }
    tools.m_bNinja = fs::exists(tools_dir / "ninja" / rw::platform::ExecutableName("ninja"));
    if (rw::platform::b_IsWindows)
    {
        tools.m_bZig = fs::exists(tools_dir / "zig-cxx.bat") && fs::exists(tools_dir / "zig-cc.bat");
    }
    else
    {
        // Linux uses the system GCC/G++ toolchain rather than Zig.
        tools.m_bZig = !PlatformInfo::FindExecutable("gcc").empty() && !PlatformInfo::FindExecutable("g++").empty();
    }

    const fs::path exe_dir = GetSelfPath().parent_path();
    const fs::path candidates[] = { exe_dir / "raylib", exe_dir / "Core" / "raylib" };
    for (const fs::path& candidate : candidates)
    {
        const bool b_has_lib = fs::exists(candidate / "lib" / "libraylib.a") ||
            fs::exists(candidate / "lib" / "libraylib.dll.a") || fs::exists(candidate / "lib" / "libraylib.so") ||
            fs::exists(candidate / "libraylib.so");
        if (fs::exists(candidate / "include" / "raylib.h") && b_has_lib)
        {
            tools.m_bRaylib = true;
            tools.m_RaylibDir = candidate;
            break;
        }
    }

    return tools;
}

std::string SceneFilePath(const FProjectContext& project, const std::string& key)
{
    return (fs::path(project.m_Project.m_AssetPath) / (key + ".scene")).string();
}

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

std::vector<fs::path> ListSourceFiles(const fs::path& source_root)
{
    std::vector<fs::path> files;
    std::error_code error;
    if (!fs::is_directory(source_root, error))
    {
        return files;
    }

    for (const fs::directory_entry& entry : fs::recursive_directory_iterator(source_root, error))
    {
        if (!entry.is_regular_file(error))
        {
            continue;
        }
        const std::string extension = entry.path().extension().string();
        if (extension == ".cpp" || extension == ".h" || extension == ".hpp" || extension == ".cc")
        {
            files.push_back(entry.path());
        }
    }

    std::ranges::sort(files);
    return files;
}

std::string ReadText(const fs::path& path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open())
    {
        return {};
    }
    return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

struct FCheck
{
    std::string m_Id;
    std::string m_Status; // ok | warn | error
    std::string m_Message;
};

FCheck MakeCheck(std::string id, bool b_ok, std::string message, bool b_warning = false)
{
    return { std::move(id), b_ok ? "ok" : (b_warning ? "warn" : "error"), std::move(message) };
}

void AddProjectChecks(const FProjectContext& project, std::vector<FCheck>& checks)
{
    const t_Project& data = project.m_Project;
    checks.push_back(MakeCheck("manifest", true, (project.m_Dir / c_MANIFEST).string()));
    checks.push_back(MakeCheck("source_dir", fs::is_directory(data.m_SourcePath), data.m_SourcePath));
    checks.push_back(MakeCheck("asset_dir", fs::is_directory(data.m_AssetPath), data.m_AssetPath));
    checks.push_back(MakeCheck("entry_dll", fs::exists(data.m_DllPath), data.m_DllPath, /*warning=*/true));

    if (!data.m_EngineVersion.empty() && data.m_EngineVersion != RAYWAVES_VERSION)
    {
        checks.push_back(MakeCheck("engine_version", false,
            "Project targets " + data.m_EngineVersion + ", engine is " + RAYWAVES_VERSION, true));
    }
    else
    {
        checks.push_back(MakeCheck("engine_version", true, RAYWAVES_VERSION));
    }

    AgentLock::FInfo lock_info;
    if (AgentLock::b_Query(project.m_Dir, lock_info))
    {
        checks.push_back(MakeCheck("lock", true,
            "Held by pid " + std::to_string(lock_info.m_Pid) +
                (lock_info.m_Command.empty() ? "" : " (" + lock_info.m_Command + ")")));
    }
    else
    {
        checks.push_back(MakeCheck("lock", true, "Free"));
    }

    // Asset references and duplicate ids are the two scene mistakes that are
    // cheap to catch and expensive to debug from a screenshot.
    AssetResolver::SetProjectAssetPath(data.m_AssetPath);
    int scene_errors = 0;
    int duplicate_ids = 0;
    int missing_textures = 0;
    std::string scene_messages;

    for (const fs::path& scene_file : ListSceneFiles(data.m_AssetPath))
    {
        Scene scene;
        if (!scene.b_LoadFromFile(scene_file.string()))
        {
            ++scene_errors;
            scene_messages += scene_file.filename().string() + ": parse failed; ";
            continue;
        }

        std::set<uint32_t> ids;
        for (const t_SceneObject& object : scene.GetObjects())
        {
            if (!ids.insert(object.m_Id).second)
            {
                ++duplicate_ids;
            }
            if (!object.m_TexturePath.empty())
            {
                const std::string resolved = AssetResolver::Resolve(object.m_TexturePath);
                std::error_code error;
                if (!fs::exists(resolved, error))
                {
                    ++missing_textures;
                    scene_messages += scene_file.filename().string() + ": missing " + object.m_TexturePath + "; ";
                }
            }
        }
    }

    checks.push_back(MakeCheck("scenes", scene_errors == 0,
        scene_errors == 0 ? "Scene files parse" : std::to_string(scene_errors) + " scene file(s) failed to parse"));
    checks.push_back(MakeCheck("scene_ids", duplicate_ids == 0,
        duplicate_ids == 0 ? "Ids are unique" : std::to_string(duplicate_ids) + " duplicate id(s)", true));
    checks.push_back(MakeCheck("scene_textures", missing_textures == 0,
        missing_textures == 0 ? "All references exist"
                              : std::to_string(missing_textures) + " missing texture reference(s); " + scene_messages,
        true));

    int rw_game_count = 0;
    for (const fs::path& file : ListSourceFiles(data.m_SourcePath))
    {
        const std::string text = ReadText(file);
        size_t position = 0;
        while ((position = text.find("RW_GAME(", position)) != std::string::npos)
        {
            ++rw_game_count;
            position += 8;
        }
    }
    checks.push_back(MakeCheck("rw_game", rw_game_count == 1,
        rw_game_count == 1 ? "RW_GAME defined once" : std::to_string(rw_game_count) + " RW_GAME definition(s)"));
}

std::string ChecksJson(const std::vector<FCheck>& checks)
{
    std::vector<std::string> items;
    items.reserve(checks.size());
    for (const FCheck& check : checks)
    {
        items.push_back("{" + CliJson::Key("id") + CliJson::Quoted(check.m_Id) + "," + CliJson::Key("status") +
            CliJson::Quoted(check.m_Status) + "," + CliJson::Key("message") + CliJson::Quoted(check.m_Message) + "}");
    }
    return CliJson::Array(items);
}

bool b_IsError(const std::vector<FCheck>& checks)
{
    return std::ranges::any_of(checks, [](const FCheck& check) { return check.m_Status == "error"; });
}

void PrintChecks(const std::vector<FCheck>& checks)
{
    for (const FCheck& check : checks)
    {
        std::cout << "[" << check.m_Status << "] " << check.m_Id << ": " << check.m_Message << "\n";
    }
}

int InfoCommand(const FProjectContext& project, bool b_json)
{
    const t_Project& data = project.m_Project;
    const std::vector<fs::path> scenes = ListSceneFiles(data.m_AssetPath);
    const FToolInfo tools = DetectTools();

    if (b_json)
    {
        std::vector<std::string> scene_items;
        for (const fs::path& scene : scenes)
        {
            Scene loaded;
            const bool b_loaded = loaded.b_LoadFromFile(scene.string());
            scene_items.push_back("{" + CliJson::Key("map") + CliJson::Quoted(scene.stem().string()) + "," +
                CliJson::Key("path") + CliJson::Quoted(scene.string()) + "," + CliJson::Key("objects") +
                CliJson::Number(static_cast<int>(b_loaded ? loaded.GetObjects().size() : 0)) + "}");
        }

        std::cout << "{" << CliJson::Key("ok") << "true," << CliJson::Key("project") << "{" << CliJson::Key("name")
                  << CliJson::Quoted(data.m_Name) << "," << CliJson::Key("version") << CliJson::Quoted(data.m_Version)
                  << "," << CliJson::Key("engineVersion") << CliJson::Quoted(data.m_EngineVersion) << ","
                  << CliJson::Key("root") << CliJson::Quoted(project.m_Dir.string()) << "," << CliJson::Key("sourceDir")
                  << CliJson::Quoted(data.m_SourcePath) << "," << CliJson::Key("assetDir")
                  << CliJson::Quoted(data.m_AssetPath) << "," << CliJson::Key("entryDll")
                  << CliJson::Quoted(data.m_DllPath) << "," << CliJson::Key("dllExists")
                  << CliJson::Bool(fs::exists(data.m_DllPath)) << "," << CliJson::Key("sceneWidth")
                  << CliJson::Number(data.m_SceneWidth) << "," << CliJson::Key("sceneHeight")
                  << CliJson::Number(data.m_SceneHeight) << "," << CliJson::Key("targetFPS")
                  << CliJson::Number(data.m_TargetFPS) << "}," << CliJson::Key("scenes") << CliJson::Array(scene_items)
                  << "," << CliJson::Key("tools") << "{" << CliJson::Key("cmake") << CliJson::Bool(tools.m_bCMake)
                  << "," << CliJson::Key("ninja") << CliJson::Bool(tools.m_bNinja) << "," << CliJson::Key("zig")
                  << CliJson::Bool(tools.m_bZig) << "," << CliJson::Key("raylib") << CliJson::Bool(tools.m_bRaylib)
                  << "}}" << "\n";
    }
    else
    {
        std::cout << "Project: " << data.m_Name << " (" << project.m_Dir.string() << ")\n";
        std::cout << "  source: " << data.m_SourcePath << "\n";
        std::cout << "  assets: " << data.m_AssetPath << "\n";
        std::cout << "  dll:    " << data.m_DllPath << (fs::exists(data.m_DllPath) ? "" : " (missing)") << "\n";
        std::cout << "  scenes: " << scenes.size() << "\n";
        for (const fs::path& scene : scenes)
        {
            std::cout << "    - " << scene.stem().string() << "\n";
        }
    }
    return 0;
}

int DoctorCommand(const FProjectContext& project, bool b_json, bool b_fix)
{
    std::vector<FCheck> checks;
    AddProjectChecks(project, checks);

    const FToolInfo tools = DetectTools();
    const std::string setup_hint =
        rw::platform::b_IsWindows ? "Run Tools/setup_tools.ps1" : "Run Tools/setup_tools.sh or install build-essential";
    checks.push_back(MakeCheck("tool_cmake", tools.m_bCMake,
        tools.m_bCMake ? tools.m_CMakePath.string() : setup_hint + " or open the editor once"));
    checks.push_back(MakeCheck("tool_ninja", tools.m_bNinja, tools.m_bNinja ? "found" : setup_hint));
    checks.push_back(MakeCheck("tool_zig", tools.m_bZig, tools.m_bZig ? "found" : setup_hint));
    checks.push_back(MakeCheck("tool_raylib", tools.m_bRaylib,
        tools.m_bRaylib ? tools.m_RaylibDir.string() : "Build RayWaves once so raylib is staged"));

    bool b_writable = false;
    std::string writable_message = "Could not write .raywaves";
    std::error_code directory_error;
    const fs::path probe_dir = project.m_Dir / ".raywaves" / "agent";
    fs::create_directories(probe_dir, directory_error);
    if (!directory_error)
    {
        const fs::path probe = probe_dir / "write-test.tmp";
        std::ofstream file(probe, std::ios::trunc);
        if (file.is_open())
        {
            file << "ok";
            file.close();
            std::filesystem::remove(probe, directory_error);
            b_writable = true;
            writable_message = "Project is writable";
        }
    }
    checks.push_back(MakeCheck("writable", b_writable, writable_message));

    const bool b_has_clangd_config = fs::exists(project.m_Dir / ".clangd");
    const bool b_has_cpp_props = fs::exists(project.m_Dir / ".vscode" / "c_cpp_properties.json");
    checks.push_back(MakeCheck("ide_configs", b_has_clangd_config && b_has_cpp_props,
        (b_has_clangd_config && b_has_cpp_props) ? "clangd + VS Code configs present" : "Run: rwcli lsp <project>",
        true));
    const bool b_has_tooling = fs::exists(project.m_Dir / ".raywaves" / "tooling.ini");
    const bool b_has_wrapper = fs::exists(project.m_Dir / ".raywaves" / "bin" / "rwcli.cmd") ||
        fs::exists(project.m_Dir / ".raywaves" / "bin" / "rwcli");
    bool b_tooling_available = false;
    if (b_has_tooling)
    {
        const std::string tooling_text = ReadText(project.m_Dir / ".raywaves" / "tooling.ini");
        b_tooling_available = tooling_text.find("available=1") != std::string::npos;
    }
    std::string tooling_message = "project rwcli wrapper present";
    if (!b_has_tooling || !b_has_wrapper)
    {
        tooling_message = "Reopen in RayWaves or run rwcli lsp";
    }
    else if (!b_tooling_available)
    {
        tooling_message = "tooling.ini reports no rwcli; build the rwcli target or add it to PATH";
    }
    checks.push_back(
        MakeCheck("tooling", b_has_tooling && b_has_wrapper && b_tooling_available, tooling_message, true));
    const bool b_has_database = fs::exists(project.m_Dir / "compile_commands.json");
    checks.push_back(MakeCheck("compile_database", b_has_database,
        b_has_database ? "compile_commands.json present" : "Run: rwcli lsp <project>", true));
    const std::string clangd = FindClangd();
    checks.push_back(
        MakeCheck("clangd", !clangd.empty(), clangd.empty() ? "clangd not found; install LLVM" : clangd, true));

    if (b_fix)
    {
        std::string remove_error;
        AgentLock::b_RemoveStale(project.m_Dir, remove_error);
    }

    const bool b_failed = b_IsError(checks);
    if (b_json)
    {
        std::cout << "{" << CliJson::Key("ok") << CliJson::Bool(!b_failed) << "," << CliJson::Key("checks")
                  << ChecksJson(checks) << "}\n";
    }
    else
    {
        PrintChecks(checks);
    }
    return b_failed ? 3 : 0;
}

int ValidateCommand(const FProjectContext& project, bool b_json)
{
    std::vector<FCheck> checks;
    AddProjectChecks(project, checks);
    const bool b_failed = b_IsError(checks);

    if (b_json)
    {
        std::cout << "{" << CliJson::Key("ok") << CliJson::Bool(!b_failed) << "," << CliJson::Key("checks")
                  << ChecksJson(checks) << "}\n";
    }
    else
    {
        PrintChecks(checks);
    }
    return b_failed ? 3 : 0;
}

int MapsCommand(const FProjectContext& project, bool b_json)
{
    const t_Project& data = project.m_Project;
    std::vector<std::string> maps;
    bool b_from_dll = false;
    std::string error;

    if (fs::exists(data.m_DllPath))
    {
        DllHandle dll = LoadDll(data.m_DllPath.c_str());
        if (dll.handle != nullptr)
        {
            using CreateFn = GameMap* (*)();
            using DestroyFn = void (*)(GameMap*);
            auto create = reinterpret_cast<CreateFn>(GetDllSymbol(dll, "CreateGameMap"));
            auto destroy = reinterpret_cast<DestroyFn>(GetDllSymbol(dll, "DestroyGameMap"));
            if (create != nullptr && destroy != nullptr)
            {
                if (GameMap* map = create())
                {
                    if (map->b_IsMapManager())
                    {
                        auto* manager = static_cast<MapManager*>(map);
                        maps = manager->GetAvailableMaps();
                        b_from_dll = true;
                    }
                    else
                    {
                        maps.push_back(map->GetMapName());
                        b_from_dll = true;
                    }
                    destroy(map);
                }
            }
            UnloadDll(dll);
        }
    }

    if (!b_from_dll)
    {
        for (const fs::path& scene : ListSceneFiles(data.m_AssetPath))
        {
            maps.push_back(scene.stem().string());
        }
    }

    std::ranges::sort(maps);
    maps.erase(std::unique(maps.begin(), maps.end()), maps.end());

    if (b_json)
    {
        std::vector<std::string> items;
        for (const std::string& map : maps)
        {
            items.push_back(CliJson::Quoted(map));
        }
        std::cout << "{" << CliJson::Key("ok") << "true," << CliJson::Key("source")
                  << CliJson::Quoted(b_from_dll ? "dll" : "scenes") << "," << CliJson::Key("maps")
                  << CliJson::Array(items) << "}\n";
    }
    else
    {
        for (const std::string& map : maps)
        {
            std::cout << map << "\n";
        }
    }
    return 0;
}

int BuildCommand(const FProjectContext& project, const FArgs& args, bool b_json)
{
    const bool b_clean = args.b_Has("clean");
    const int jobs = args.GetInt("jobs", 0);
    const int timeout_seconds = args.GetInt("timeout", 600);

    std::string lock_error;
    if (!AgentLock::b_Acquire(project.m_Dir, "build", lock_error))
    {
        PrintError(b_json, "build_locked", lock_error);
        return 7;
    }

    struct FLockGuard
    {
        fs::path m_Path;
        ~FLockGuard() { AgentLock::Release(m_Path); }
    } lock_guard{ project.m_Dir };

    if (!ProjectManager::b_OpenProject(project.m_Dir.string(), false))
    {
        PrintError(b_json, "invalid_project", "ProjectManager could not open the project");
        return 3;
    }
    CleanupStaleShadowCopies(project.m_Dir);

    const FToolInfo tools = DetectTools();
    if (!tools.m_bCMake)
    {
        PrintError(b_json, "missing_toolchain",
            "cmake not found at " + tools.m_CMakePath.string() +
                (rw::platform::b_IsWindows ? ". Run Tools/setup_tools.ps1 first."
                                           : ". Run Tools/setup_tools.sh first."));
        return 3;
    }

    const fs::path source_dir = project.m_Dir / ".raywaves";
    const fs::path build_dir = source_dir / "build-agent";
    if (b_clean)
    {
        std::error_code error;
        fs::remove_all(build_dir, error);
    }

    const auto start_time = std::chrono::steady_clock::now();
    std::vector<std::string> tail;
    std::vector<BuildDiagnostics::t_Diagnostic> diagnostics;
    std::set<std::string> diagnostic_keys;

    auto capture = [&](std::string_view line, bool b_error)
    {
        (void)b_error;
        tail.push_back(std::string(line));
        if (tail.size() > 80)
        {
            tail.erase(tail.begin());
        }

        BuildDiagnostics::t_Diagnostic diagnostic;
        if (BuildDiagnostics::b_ParseLine(line, diagnostic))
        {
            const std::string key = diagnostic.m_File + ":" + std::to_string(diagnostic.m_Line) + ":" +
                std::to_string(diagnostic.m_Column) + ":" + diagnostic.m_Message;
            if (diagnostic_keys.insert(key).second)
            {
                diagnostics.push_back(std::move(diagnostic));
            }
        }
    };

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

    const std::string configure = ProcessRunner::QuoteArg(tools.m_CMakePath.string()) + " -G Ninja " +
        ProcessRunner::QuoteArg(source_dir.string()) + " -B " + ProcessRunner::QuoteArg(build_dir.string());
    bool b_configured = ProcessRunner::RunAndWaitCommandLine(configure, project.m_Dir.string(), capture, cancel);

    if (!b_configured && !b_timed_out.load())
    {
        const std::string fresh = ProcessRunner::QuoteArg(tools.m_CMakePath.string()) + " --fresh -G Ninja " +
            ProcessRunner::QuoteArg(source_dir.string()) + " -B " + ProcessRunner::QuoteArg(build_dir.string());
        b_configured = ProcessRunner::RunAndWaitCommandLine(fresh, project.m_Dir.string(), capture, cancel);
    }

    bool b_built = false;
    if (b_configured)
    {
        std::string build_cmd = ProcessRunner::QuoteArg(tools.m_CMakePath.string()) + " --build " +
            ProcessRunner::QuoteArg(build_dir.string()) + " --config Release";
        if (jobs > 0)
        {
            build_cmd += " --parallel " + std::to_string(jobs);
        }
        b_built = ProcessRunner::RunAndWaitCommandLine(build_cmd, project.m_Dir.string(), capture, cancel);
    }

    if (watchdog.joinable())
    {
        cancel->store(true);
        watchdog.join();
    }

    const auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start_time).count();

    std::vector<std::string> diagnostic_items;
    for (const BuildDiagnostics::t_Diagnostic& diagnostic : diagnostics)
    {
        diagnostic_items.push_back("{" + CliJson::Key("file") + CliJson::Quoted(diagnostic.m_File) + "," +
            CliJson::Key("line") + CliJson::Number(diagnostic.m_Line) + "," + CliJson::Key("column") +
            CliJson::Number(diagnostic.m_Column) + "," + CliJson::Key("severity") +
            CliJson::Quoted(BuildDiagnostics::ToString(diagnostic.m_Severity)) + "," + CliJson::Key("message") +
            CliJson::Quoted(diagnostic.m_Message) + "}");
    }

    if (b_timed_out.load())
    {
        if (b_json)
        {
            std::cout << "{" << CliJson::Key("ok") << "false," << CliJson::Key("error") << "{" << CliJson::Key("code")
                      << CliJson::Quoted("build_timeout") << "," << CliJson::Key("message")
                      << CliJson::Quoted("Build timed out after " + std::to_string(timeout_seconds) + "s") << "},"
                      << CliJson::Key("diagnostics") << CliJson::Array(diagnostic_items) << ","
                      << CliJson::Key("rawTail") << CliJson::Quoted(JoinTail(tail, 40)) << "}\n";
        }
        else
        {
            std::cerr << "build timed out after " << timeout_seconds << "s\n";
        }
        return 6;
    }

    if (b_json)
    {
        std::cout << "{" << CliJson::Key("ok") << CliJson::Bool(b_built) << "," << CliJson::Key("durationMs")
                  << CliJson::Number(static_cast<double>(duration)) << "," << CliJson::Key("buildDir")
                  << CliJson::Quoted(build_dir.string()) << "," << CliJson::Key("diagnostics")
                  << CliJson::Array(diagnostic_items) << "," << CliJson::Key("rawTail")
                  << CliJson::Quoted(JoinTail(tail, 40)) << "}\n";
    }
    else
    {
        for (const std::string& line : tail)
        {
            std::cerr << line << "\n";
        }
        std::cout << (b_built ? "build ok" : "build failed") << "\n";
    }

    return b_built ? 0 : 4;
}

int SceneCommand(const FProjectContext& project, const FArgs& args, bool b_json, const std::string& subcommand)
{
    const std::string map_option = args.Get("map");
    if (subcommand == "list")
    {
        std::vector<std::string> scenes_json;
        std::vector<fs::path> scenes;
        if (!map_option.empty())
        {
            scenes.push_back(fs::path(SceneFilePath(project, Scene::ToFileKey(map_option))));
        }
        else
        {
            scenes = ListSceneFiles(project.m_Project.m_AssetPath);
        }

        for (const fs::path& scene_path : scenes)
        {
            Scene scene;
            if (!scene.b_LoadFromFile(scene_path.string()))
            {
                continue;
            }
            std::vector<std::string> objects;
            for (const t_SceneObject& object : scene.GetObjects())
            {
                objects.push_back("{" + CliJson::Key("id") + CliJson::Number(static_cast<int>(object.m_Id)) + "," +
                    CliJson::Key("name") + CliJson::Quoted(object.m_Name) + "," + CliJson::Key("texture") +
                    CliJson::Quoted(object.m_TexturePath) + "," + CliJson::Key("position") + "[" +
                    CliJson::Number(object.m_Position.x) + "," + CliJson::Number(object.m_Position.y) + "]," +
                    CliJson::Key("layer") + CliJson::Number(object.m_Layer) + "," + CliJson::Key("visible") +
                    CliJson::Bool(object.m_bVisible) + "}");
            }
            scenes_json.push_back("{" + CliJson::Key("map") + CliJson::Quoted(scene_path.stem().string()) + "," +
                CliJson::Key("objects") + CliJson::Array(objects) + "}");
        }

        if (b_json)
        {
            std::cout << "{" << CliJson::Key("ok") << "true," << CliJson::Key("scenes") << CliJson::Array(scenes_json)
                      << "}\n";
        }
        else
        {
            for (const std::string& scene : scenes_json)
            {
                std::cout << scene << "\n";
            }
        }
        return 0;
    }

    std::string key;
    std::string key_error;
    if (!b_ResolveSceneKey(project, map_option, key, key_error))
    {
        PrintError(b_json, "scene_not_found", key_error);
        return 3;
    }

    const fs::path scene_path(SceneFilePath(project, key));
    Scene scene;
    if (fs::exists(scene_path))
    {
        if (!scene.b_LoadFromFile(scene_path.string()))
        {
            PrintError(b_json, "scene_not_found", "Could not parse " + scene_path.string());
            return 3;
        }
    }

    std::string lock_error;
    if (!AgentLock::b_Acquire(project.m_Dir, "scene " + subcommand, lock_error))
    {
        PrintError(b_json, "scene_conflict", lock_error + ". Close the editor or wait for the running command.");
        return 7;
    }
    struct FLockGuard
    {
        fs::path m_Path;
        ~FLockGuard() { AgentLock::Release(m_Path); }
    } lock_guard{ project.m_Dir };

    const int id = args.GetInt("id", 0);
    t_SceneObject* target = (id > 0) ? scene.FindObject(static_cast<uint32_t>(id)) : nullptr;
    int result_id = 0;
    std::string result_name;

    if (subcommand == "add")
    {
        t_SceneObject& object = scene.AddObject(args.Get("name", "Object"));
        object.m_TexturePath = args.Get("texture", "");
        const std::string position = args.Get("position", "");
        if (!position.empty())
        {
            const size_t comma = position.find(',');
            if (comma != std::string::npos)
            {
                object.m_Position.x = static_cast<float>(std::atof(position.substr(0, comma).c_str()));
                object.m_Position.y = static_cast<float>(std::atof(position.substr(comma + 1).c_str()));
            }
        }
        const std::string scale = args.Get("scale", "");
        if (!scale.empty())
        {
            const size_t comma = scale.find(',');
            if (comma == std::string::npos)
            {
                const float uniform = static_cast<float>(std::atof(scale.c_str()));
                object.m_Scale = { uniform, uniform };
            }
            else
            {
                object.m_Scale.x = static_cast<float>(std::atof(scale.substr(0, comma).c_str()));
                object.m_Scale.y = static_cast<float>(std::atof(scale.substr(comma + 1).c_str()));
            }
        }
        object.m_Rotation = args.GetFloat("rotation", object.m_Rotation);
        object.m_Layer = args.GetInt("layer", object.m_Layer);
        object.m_bVisible = args.Get("visible", "1") != "0";
        result_id = static_cast<int>(object.m_Id);
        result_name = object.m_Name;
    }
    else if (subcommand == "set")
    {
        if (target == nullptr)
        {
            PrintError(b_json, "scene_not_found", "No object with id " + std::to_string(id));
            return 3;
        }
        if (args.b_Has("name"))
        {
            target->m_Name = args.Get("name");
        }
        if (args.b_Has("texture"))
        {
            target->m_TexturePath = args.Get("texture");
        }
        if (args.b_Has("position"))
        {
            const std::string position = args.Get("position");
            const size_t comma = position.find(',');
            if (comma != std::string::npos)
            {
                target->m_Position.x = static_cast<float>(std::atof(position.substr(0, comma).c_str()));
                target->m_Position.y = static_cast<float>(std::atof(position.substr(comma + 1).c_str()));
            }
        }
        if (args.b_Has("rotation"))
        {
            target->m_Rotation = args.GetFloat("rotation", target->m_Rotation);
        }
        if (args.b_Has("layer"))
        {
            target->m_Layer = args.GetInt("layer", target->m_Layer);
        }
        if (args.b_Has("visible"))
        {
            target->m_bVisible = args.Get("visible", "1") != "0";
        }
        result_id = static_cast<int>(target->m_Id);
        result_name = target->m_Name;
    }
    else if (subcommand == "remove")
    {
        if (target == nullptr || !scene.b_RemoveObject(static_cast<uint32_t>(id)))
        {
            PrintError(b_json, "scene_not_found", "No object with id " + std::to_string(id));
            return 3;
        }
        result_id = id;
    }
    else if (subcommand == "duplicate")
    {
        if (target == nullptr)
        {
            PrintError(b_json, "scene_not_found", "No object with id " + std::to_string(id));
            return 3;
        }
        t_SceneObject* copy = scene.DuplicateObject(static_cast<uint32_t>(id));
        if (copy == nullptr)
        {
            PrintError(b_json, "scene_not_found", "Duplicate failed");
            return 3;
        }
        copy->m_Position.x += 16.0f;
        copy->m_Position.y += 16.0f;
        result_id = static_cast<int>(copy->m_Id);
        result_name = copy->m_Name;
    }
    else
    {
        PrintError(b_json, "usage", "Unknown scene subcommand: " + subcommand);
        return 2;
    }

    if (!args.b_Has("dry-run"))
    {
        std::error_code error;
        fs::create_directories(scene_path.parent_path(), error);
        if (!scene.b_SaveToFile(scene_path.string()))
        {
            PrintError(b_json, "scene_not_found", "Could not write " + scene_path.string());
            return 3;
        }
    }

    if (b_json)
    {
        std::cout << "{" << CliJson::Key("ok") << "true," << CliJson::Key("map") << CliJson::Quoted(key) << ","
                  << CliJson::Key("id") << CliJson::Number(result_id) << "," << CliJson::Key("name")
                  << CliJson::Quoted(result_name) << "," << CliJson::Key("saved")
                  << CliJson::Bool(!args.b_Has("dry-run")) << "}\n";
    }
    else
    {
        std::cout << subcommand << " ok: " << key << " id=" << result_id << "\n";
    }
    return 0;
}

std::vector<fs::path> ListCppFiles(const fs::path& source_root)
{
    std::vector<fs::path> files;
    std::error_code error;
    if (!fs::is_directory(source_root, error))
    {
        return files;
    }

    for (const fs::directory_entry& entry : fs::recursive_directory_iterator(source_root, error))
    {
        if (entry.is_regular_file(error) && entry.path().extension() == ".cpp")
        {
            files.push_back(entry.path());
        }
    }
    std::ranges::sort(files);
    return files;
}

std::string FindClangd()
{
    std::filesystem::path found = PlatformInfo::FindExecutable(rw::platform::b_IsWindows ? "clangd.exe" : "clangd");
    if (!found.empty())
    {
        return found.string();
    }

    if (rw::platform::b_IsWindows)
    {
        const fs::path common = "C:/Program Files/LLVM/bin/clangd.exe";
        if (fs::exists(common))
        {
            return common.string();
        }
    }
    else
    {
        for (const char* candidate : { "/usr/bin/clangd", "/usr/local/bin/clangd" })
        {
            if (fs::exists(candidate))
            {
                return candidate;
            }
        }
    }
    return {};
}

int LspCommand(const FProjectContext& project, const FArgs& args, bool b_json)
{
    const bool b_force = args.b_Has("force");
    const bool b_check = args.b_Has("check");

    if (!ProjectManager::b_OpenProject(project.m_Dir.string(), false))
    {
        PrintError(b_json, "invalid_project", "ProjectManager could not open the project");
        return 3;
    }
    CleanupStaleShadowCopies(project.m_Dir);
    ProjectManager::b_WriteIdeConfigs(b_force);

    const FToolInfo tools = DetectTools();
    if (!tools.m_bCMake)
    {
        PrintError(b_json, "missing_toolchain",
            "cmake not found at " + tools.m_CMakePath.string() +
                (rw::platform::b_IsWindows ? ". Run Tools/setup_tools.ps1 first."
                                           : ". Run Tools/setup_tools.sh first."));
        return 3;
    }

    const fs::path source_dir = project.m_Dir / ".raywaves";
    const fs::path build_dir = source_dir / "build-ide";
    const std::string configure = ProcessRunner::QuoteArg(tools.m_CMakePath.string()) + " -G Ninja " +
        ProcessRunner::QuoteArg(source_dir.string()) + " -B " + ProcessRunner::QuoteArg(build_dir.string()) +
        " -DCMAKE_EXPORT_COMPILE_COMMANDS=ON";
    const std::string fresh = ProcessRunner::QuoteArg(tools.m_CMakePath.string()) + " --fresh -G Ninja " +
        ProcessRunner::QuoteArg(source_dir.string()) + " -B " + ProcessRunner::QuoteArg(build_dir.string()) +
        " -DCMAKE_EXPORT_COMPILE_COMMANDS=ON";

    std::vector<std::string> configure_tail;
    auto capture = [&configure_tail](std::string_view line, bool)
    {
        configure_tail.push_back(std::string(line));
        if (configure_tail.size() > 20)
        {
            configure_tail.erase(configure_tail.begin());
        }
    };

    bool b_configured = ProcessRunner::RunAndWaitCommandLine(configure, project.m_Dir.string(), capture);
    if (!b_configured)
    {
        b_configured = ProcessRunner::RunAndWaitCommandLine(fresh, project.m_Dir.string(), capture);
    }
    if (!b_configured)
    {
        if (b_json)
        {
            std::cout << "{\"ok\":false,\"error\":{\"code\":\"configure_failed\",\"message\":" +
                    CliJson::Quoted(JoinTail(configure_tail, 12)) + "}}\n";
        }
        else
        {
            std::cerr << "configure failed:\n" << JoinTail(configure_tail, 12) << "\n";
        }
        return 4;
    }

    const fs::path database_source = build_dir / "compile_commands.json";
    const fs::path database_dest = project.m_Dir / "compile_commands.json";
    std::error_code copy_error;
    if (!fs::exists(database_source))
    {
        PrintError(b_json, "compile_database_missing",
            "CMake did not produce " + database_source.string() + ". Check the configure log.");
        return 3;
    }
    fs::copy_file(database_source, database_dest, fs::copy_options::overwrite_existing, copy_error);
    if (copy_error)
    {
        std::error_code remove_error;
        fs::remove(database_dest, remove_error);
        PrintError(b_json, "compile_database_write",
            "Could not copy " + database_source.string() + ": " + copy_error.message());
        return 3;
    }

    const std::string clangd = FindClangd();
    std::vector<std::string> diagnostics;
    const std::vector<fs::path> sources = ListCppFiles(project.m_Project.m_SourcePath);

    if (b_check && clangd.empty())
    {
        PrintError(b_json, "clangd_missing",
            rw::platform::b_IsWindows ? "clangd.exe not found in PATH or C:/Program Files/LLVM/bin"
                                      : "clangd not found in PATH or /usr/bin");
        return 3;
    }

    if (b_check)
    {
        for (const fs::path& source : sources)
        {
            const std::string command = ProcessRunner::QuoteArg(clangd) +
                " --check=" + ProcessRunner::QuoteArg(source.string()) +
                " --compile-commands-dir=" + ProcessRunner::QuoteArg(project.m_Dir.string());
            ProcessRunner::RunAndWaitCommandLine(command, project.m_Dir.string(),
                [&diagnostics, &source](std::string_view line, bool)
                {
                    if (line.rfind("E[", 0) != 0)
                    {
                        return;
                    }
                    if (line.find(" tweak: ") != std::string_view::npos ||
                        line.find("Failed to resolve path") != std::string_view::npos)
                    {
                        return;
                    }
                    diagnostics.push_back("{" + CliJson::Key("file") + CliJson::Quoted(source.string()) + "," +
                        CliJson::Key("message") + CliJson::Quoted(line) + "}");
                });
        }
    }

    if (b_json)
    {
        std::cout << "{" << CliJson::Key("ok") << CliJson::Bool(diagnostics.empty()) << "," << CliJson::Key("clangd")
                  << CliJson::Quoted(clangd) << "," << CliJson::Key("files")
                  << CliJson::Number(static_cast<int>(sources.size())) << "," << CliJson::Key("compileCommands")
                  << CliJson::Quoted(database_dest.string()) << "," << CliJson::Key("diagnostics")
                  << CliJson::Array(diagnostics) << "}\n";
    }
    else
    {
        std::cout << "wrote .clangd, .vscode/, compile_commands.json\n";
        if (b_check)
        {
            std::cout << sources.size() << " files, " << diagnostics.size() << " diagnostics\n";
        }
    }

    return diagnostics.empty() ? 0 : 3;
}

void PrintGeneralHelp()
{
    std::cout << "rwcli " << RAYWAVES_VERSION
              << "\n"
                 "Agent CLI for RayWaves projects.\n\n"
                 "Usage:\n"
                 "  rwcli <command> <projectDir> [options]\n"
                 "  rwcli help [command]\n"
                 "  rwcli --version\n\n"
                 "Commands:\n"
                 "  doctor    toolchain, manifest, scenes, lock, IDE config\n"
                 "  info      project paths, scenes, tool availability\n"
                 "  maps      map ids from the DLL or scene filenames\n"
                 "  validate  manifest, scene ids, textures, RW_GAME\n"
                 "  build     configure + build GameLogic\n"
                 "  run       headless fixed-frame run + screenshot/state\n"
                 "  lsp       write .clangd/.vscode, compile_commands.json\n"
                 "  scene     list|add|set|remove|duplicate scene objects\n"
                 "  assets    list|import assets\n\n"
                 "Options: --json, --force, --clean, --jobs N, --timeout S\n"
                 "From a project, use .\\.raywaves\\bin\\rwcli.cmd <command> . --json\n";
}

void PrintCommandHelp(std::string_view command)
{
    if (command == "doctor")
    {
        std::cout << "rwcli doctor <project> [--fix] [--json]\n"
                     "Checks manifest, dirs, scenes, textures, RW_GAME, tools, lock, IDE config.\n";
    }
    else if (command == "info")
    {
        std::cout << "rwcli info <project> [--json]\n"
                     "Prints manifest paths, scene list, and tool availability.\n";
    }
    else if (command == "maps")
    {
        std::cout << "rwcli maps <project> [--json]\n"
                     "Loads GameLogic.dll for map ids; falls back to Assets/*.scene stems.\n";
    }
    else if (command == "validate")
    {
        std::cout << "rwcli validate <project> [--json]\n"
                     "Fails on invalid manifest/dirs/scenes/RW_GAME; warns on textures and version drift.\n";
    }
    else if (command == "build")
    {
        std::cout
            << "rwcli build <project> [--clean] [--jobs N] [--timeout S] [--json]\n"
               "Builds in .raywaves/build-agent under .raywaves/agent.lock.\n"
               "Result: {\"ok\":bool,\"diagnostics\":[{file,line,column,severity,message}],\"rawTail\":\"...\"}\n";
    }
    else if (command == "run")
    {
        std::cout << "rwcli run <project> [--frames N] [--dt S] [--seed N]\n"
                     "  [--screenshot path] [--screenshot-frame N] [--state path]\n"
                     "  [--size WxH] [--timeout S] [--input file] [--visible] [--audio] [--json]\n"
                     "Runs in a child process with a hidden window. Result includes map, entities, state,\n"
                     "screenshot {path,bytes,nonEmpty}, and logs on failure.\n";
    }
    else if (command == "lsp")
    {
        std::cout << "rwcli lsp <project> [--check] [--force] [--json]\n"
                     "Writes .clangd, .vscode/, AGENTS.md, project skill, tooling.ini, rwcli wrapper.\n"
                     "Configures .raywaves/build-ide and copies compile_commands.json to the root.\n"
                     "--check runs clangd --check on GameLogic/**/*.cpp and returns diagnostics[].\n";
    }
    else if (command == "scene")
    {
        std::cout << "rwcli scene list <project> [--map M] [--json]\n"
                     "rwcli scene add <project> --map M --name NAME [--texture T] [--position X,Y]\n"
                     "  [--scale S|SX,SY] [--rotation DEG] [--layer N] [--json]\n"
                     "rwcli scene set <project> --map M --id N [--position X,Y] [--rotation DEG]\n"
                     "  [--layer N] [--visible 0|1] [--name NAME] [--texture T] [--json]\n"
                     "rwcli scene duplicate <project> --map M --id N [--json]\n"
                     "rwcli scene remove <project> --map M --id N [--json]\n"
                     "Add --dry-run to validate without writing.\n";
    }
    else if (command == "assets")
    {
        std::cout << "rwcli assets list <project> [--json]\n"
                     "rwcli assets import <project> <file> [--force] [--json]\n"
                     "Imports into Assets/ with unsafe characters replaced by underscores.\n";
    }
    else
    {
        PrintGeneralHelp();
    }
}

std::string SanitizeAssetName(std::string_view name)
{
    std::string sanitized;
    sanitized.reserve(name.size());
    for (const char character : name)
    {
        const bool b_safe = (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') ||
            (character >= '0' && character <= '9') || character == '.' || character == '_' || character == '-';
        sanitized += b_safe ? character : '_';
    }
    if (sanitized.empty())
    {
        sanitized = "asset";
    }
    return sanitized;
}

std::string AssetKind(const fs::path& path)
{
    const std::string extension = path.extension().string();
    if (extension == ".png" || extension == ".jpg" || extension == ".jpeg" || extension == ".bmp" ||
        extension == ".tga" || extension == ".gif" || extension == ".qoi" || extension == ".psd")
    {
        return "image";
    }
    if (extension == ".wav" || extension == ".ogg" || extension == ".mp3" || extension == ".flac")
    {
        return "audio";
    }
    if (extension == ".ttf" || extension == ".otf" || extension == ".fnt")
    {
        return "font";
    }
    if (extension == ".scene")
    {
        return "scene";
    }
    return "other";
}

int AssetsCommand(const FProjectContext& project, const FArgs& args, bool b_json, const std::string& subcommand)
{
    const fs::path asset_root(project.m_Project.m_AssetPath);

    if (subcommand == "list")
    {
        std::vector<std::string> items;
        std::error_code error;
        if (fs::is_directory(asset_root, error))
        {
            for (const fs::directory_entry& entry : fs::directory_iterator(asset_root, error))
            {
                if (!entry.is_regular_file(error))
                {
                    continue;
                }
                std::error_code size_error;
                const auto size = fs::file_size(entry.path(), size_error);
                items.push_back("{" + CliJson::Key("name") + CliJson::Quoted(entry.path().filename().string()) + "," +
                    CliJson::Key("kind") + CliJson::Quoted(AssetKind(entry.path())) + "," + CliJson::Key("bytes") +
                    CliJson::Number(static_cast<double>(size_error ? 0 : size)) + "}");
            }
        }

        if (b_json)
        {
            std::cout << "{" << CliJson::Key("ok") << "true," << CliJson::Key("assets") << CliJson::Array(items)
                      << "}\n";
        }
        else
        {
            for (const std::string& item : items)
            {
                std::cout << item << "\n";
            }
        }
        return 0;
    }

    if (subcommand == "import")
    {
        if (args.m_Positional.size() < 4)
        {
            PrintError(b_json, "usage", "assets import <projectDir> <file>");
            return 2;
        }

        const fs::path source(args.m_Positional[3]);
        if (!fs::is_regular_file(source))
        {
            PrintError(b_json, "usage", "Source file does not exist: " + source.string());
            return 2;
        }

        std::error_code error;
        fs::create_directories(asset_root, error);
        const fs::path destination = asset_root / SanitizeAssetName(source.filename().string());
        const bool b_exists = fs::exists(destination, error);
        if (b_exists && !args.b_Has("force"))
        {
            PrintError(b_json, "asset_exists", "Already exists: " + destination.string() + " (pass --force)");
            return 3;
        }

        fs::copy_file(source, destination, fs::copy_options::overwrite_existing, error);
        if (error)
        {
            PrintError(b_json, "asset_copy", error.message());
            return 3;
        }

        if (b_json)
        {
            std::cout << "{" << CliJson::Key("ok") << "true," << CliJson::Key("asset")
                      << CliJson::Quoted(destination.string()) << "}\n";
        }
        else
        {
            std::cout << "imported " << destination.string() << "\n";
        }
        return 0;
    }

    PrintError(b_json, "usage", "Unknown assets subcommand: " + subcommand);
    return 2;
}
} // namespace

namespace Cli
{
int Run(const FArgs& args)
{
    const bool b_json = args.b_Has("json");

    if (args.b_Has("version") || (!args.m_Positional.empty() && args.m_Positional[0] == "version"))
    {
        std::cout << "rwcli " << RAYWAVES_VERSION << "\n";
        return 0;
    }

    if (args.m_Positional.empty() || args.m_Positional[0] == "help" || args.b_Has("help"))
    {
        const bool b_topic_from_help = (args.m_Positional.size() > 1 && args.m_Positional[0] == "help");
        const bool b_topic_from_command = (!args.m_Positional.empty() && args.m_Positional[0] != "help");
        if (b_topic_from_help)
        {
            PrintCommandHelp(args.m_Positional[1]);
        }
        else if (b_topic_from_command)
        {
            PrintCommandHelp(args.m_Positional[0]);
        }
        else
        {
            PrintGeneralHelp();
        }
        return 0;
    }

    const std::string command = args.m_Positional[0];
    if (command == "doctor" || command == "info" || command == "maps" || command == "validate" || command == "build" ||
        command == "run" || command == "lsp")
    {
        const std::string project_path = args.m_Positional.size() > 1 ? args.m_Positional[1] : "";
        FProjectContext project;
        std::string error;
        if (!b_ResolveProject(project_path, project, error))
        {
            PrintError(b_json, "invalid_project", error);
            return 3;
        }

        if (command == "info")
        {
            return InfoCommand(project, b_json);
        }
        if (command == "doctor")
        {
            return DoctorCommand(project, b_json, args.b_Has("fix"));
        }
        if (command == "maps")
        {
            return MapsCommand(project, b_json);
        }
        if (command == "validate")
        {
            return ValidateCommand(project, b_json);
        }
        if (command == "build")
        {
            return BuildCommand(project, args, b_json);
        }
        if (command == "lsp")
        {
            return LspCommand(project, args, b_json);
        }
        return Cli::RunCommand(project, args, b_json);
    }

    if (command == "scene" || command == "assets")
    {
        if (args.m_Positional.size() < 2)
        {
            PrintError(b_json, "usage", command + " needs a subcommand");
            return 2;
        }
        const std::string subcommand = args.m_Positional[1];
        const std::string project_path = args.m_Positional.size() > 2 ? args.m_Positional[2] : "";
        FProjectContext project;
        std::string error;
        if (!b_ResolveProject(project_path, project, error))
        {
            PrintError(b_json, "invalid_project", error);
            return 3;
        }

        if (command == "assets")
        {
            return AssetsCommand(project, args, b_json, subcommand);
        }
        return SceneCommand(project, args, b_json, subcommand);
    }

    PrintError(b_json, "usage", "Unknown command: " + command);
    return 2;
}
} // namespace Cli
