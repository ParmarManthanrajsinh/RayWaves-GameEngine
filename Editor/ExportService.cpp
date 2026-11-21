#include "ExportService.h"
#include "../Engine/PlatformInfo.h"
#include "../Engine/PlatformNames.h"
#include "../Engine/ProjectManager.h"
#include "../Runtime/AgentLock.h"
#include "../Runtime/DllLoader.h"
#include "EditorUtils.h"
#include "ProcessRunner.h"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

namespace fs = std::filesystem;

namespace
{
bool b_IsRaylibRuntimeName(const std::string& filename)
{
    return filename == "libraylib.dll" || filename == "raylib.dll" || filename.rfind("libraylib.so", 0) == 0 ||
        filename.rfind("raylib.so", 0) == 0;
}

struct FLockGuard
{
    fs::path m_Root;
    ~FLockGuard() { AgentLock::Release(m_Root); }
};
} // namespace

namespace ExportService
{
void ClampSettings(FExportSettings& settings)
{
    settings.m_WindowWidth = std::max(settings.m_WindowWidth, 320);
    settings.m_WindowHeight = std::max(settings.m_WindowHeight, 240);
    settings.m_WindowWidth = std::min(settings.m_WindowWidth, 7680);
    settings.m_WindowHeight = std::min(settings.m_WindowHeight, 4320);
    settings.m_TargetFPS = std::max(settings.m_TargetFPS, 0);
    settings.m_TargetFPS = std::min(settings.m_TargetFPS, 1000);
}

std::string ResolveExportDir(std::string_view export_path, std::string_view project_root)
{
    fs::path export_dir = fs::path(export_path);
    if (export_dir.is_relative())
    {
        export_dir = fs::path(project_root) / export_dir;
    }
    return export_dir.string();
}

std::string SanitizeGameName(std::string_view name)
{
    std::string result;
    result.reserve(name.size());

    for (const char c : name)
    {
        const unsigned char uc = static_cast<unsigned char>(c);
        if (std::isalnum(uc) || c == '_' || c == '-' || c == ' ' || c == '.')
        {
            result += c;
        }
        else
        {
            result += '_';
        }
    }

    const auto first = result.find_first_not_of(" .");
    result.erase(0, first == std::string::npos ? result.size() : first);
    const auto last = result.find_last_not_of(" .");
    if (last != std::string::npos)
    {
        result.erase(last + 1);
    }
    else
    {
        result.clear();
    }

    if (result.empty() || result == "." || result == "..")
    {
        return "Game";
    }

    std::string stem = result.substr(0, result.find('.'));
    std::ranges::transform(stem, stem.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    static constexpr const char* reserved[] = { "con", "prn", "aux", "nul", "com1", "com2", "com3", "com4", "com5",
        "com6", "com7", "com8", "com9", "lpt1", "lpt2", "lpt3", "lpt4", "lpt5", "lpt6", "lpt7", "lpt8", "lpt9" };
    for (const char* device : reserved)
    {
        if (stem == device)
        {
            result = "_" + result;
            break;
        }
    }

    return result;
}

bool b_WriteGameConfig(
    const std::string& config_path, const FExportSettings& settings, int scene_width, int scene_height, int scene_fps)
{
    std::ofstream config_file(config_path);
    if (!config_file.is_open())
    {
        return false;
    }

    std::ostringstream ss;
    ss << "# Game Configuration File\n"
       << "# Window Settings\n"
       << "width=" << settings.m_WindowWidth << "\n"
       << "height=" << settings.m_WindowHeight << "\n"
       << "b_Fullscreen=" << (settings.m_bFullscreen ? "true" : "false") << "\n"
       << "b_Resizable=" << (settings.m_bResizable ? "true" : "false") << "\n"
       << "b_Vsync=" << (settings.m_bVSync ? "true" : "false") << "\n"
       << "target_fps=" << settings.m_TargetFPS << "\n"
       << "scene_width=" << scene_width << "\n"
       << "scene_height=" << scene_height << "\n"
       << "scene_fps=" << scene_fps << "\n"
       << "title=" << settings.m_GameName << "\n";

    config_file << ss.str();
    return true;
}

bool b_ValidateExportFolder(std::string_view out_dir, LogSink log)
{
    bool b_Ok = true;

    log(std::string("Validation working directory: ") + fs::current_path().string());
    log(std::string("Checking export directory: ").append(out_dir));

    auto require = [&](const fs::path& p)
    {
        bool b_Exists = fs::exists(p);
        log(std::string("Checking: ") + p.string() + " - " + (b_Exists ? "EXISTS" : "MISSING"));
        if (!b_Exists)
            b_Ok = false;
    };

    bool b_FoundGameExe = false;
    std::error_code ec;
    if (fs::exists(out_dir, ec) && !ec)
    {
        for (const auto& ENTRY : fs::directory_iterator(out_dir, ec))
        {
            if (ec || !ENTRY.is_regular_file())
            {
                continue;
            }

            const fs::path path = ENTRY.path();
            const std::string filename = path.filename().string();
            bool b_is_executable = false;
            if (rw::platform::b_IsWindows)
            {
                b_is_executable = path.extension() == ".exe";
            }
            else
            {
                const fs::file_status status = ENTRY.status(ec);
                b_is_executable = !ec && (status.permissions() & fs::perms::owner_exec) != fs::perms::none;
            }

            if (b_is_executable && filename != rw::platform::c_GameLogicModuleName && !b_IsRaylibRuntimeName(filename))
            {
                b_FoundGameExe = true;
                log(std::string("Found game executable: ") + filename);
                break;
            }
        }
    }
    if (!b_FoundGameExe)
    {
        log("Missing: Game executable");
        b_Ok = false;
    }

    require(fs::path(out_dir) / rw::platform::c_GameLogicModuleName);

    bool b_FoundRaylib = false;
    std::error_code raylib_ec;
    if (fs::exists(out_dir, raylib_ec) && !raylib_ec)
    {
        for (const auto& entry : fs::directory_iterator(out_dir, raylib_ec))
        {
            if (raylib_ec || !entry.is_regular_file())
            {
                continue;
            }
            const std::string filename = entry.path().filename().string();
            if (b_IsRaylibRuntimeName(filename))
            {
                b_FoundRaylib = true;
                log(std::string("Found raylib runtime: ") + filename);
                break;
            }
        }
    }
    if (!b_FoundRaylib)
    {
        log("Missing: raylib runtime library");
        b_Ok = false;
    }

    fs::path assets_path = fs::path(out_dir) / "Assets";
    if (fs::exists(assets_path))
    {
        log("Found Assets folder in export");
    }
    else
    {
        log("No Assets folder found - this is OK if game has no assets");
    }

    return b_Ok;
}

FExportResult RunExport(const FExportSettings& settings, const std::shared_ptr<std::atomic<bool>>& cancel, LogSink log)
{
    FExportResult result;

    auto fail = [&](std::string_view message)
    {
        log(message);
        result.m_bSuccess = false;
        return result;
    };

    auto is_cancelled = [&]() { return (cancel != nullptr) && cancel->load(); };

    try
    {
        if (!ProjectManager::b_HasOpenProject())
        {
            return fail("ERROR: No project is currently open!");
        }

        if (is_cancelled())
            return result;

        fs::create_directories(settings.m_ExportPath);
        log("Starting export process...");

        fs::path current_path = fs::current_path();
        // Snapshot under the lock: the UI thread can keep editing the
        // live project (icon, name, scene settings) while this worker
        // runs, and the reference from GetCurrent() escapes that lock.
        t_Project proj = ProjectManager::GetCurrentCopy();

        // Export builds the project and writes GameLogic next to the source,
        // exactly like an editor compile. Take the same lock so the two
        // processes cannot link the module at the same time.
        std::string lock_error;
        if (!AgentLock::b_Acquire(proj.m_RootPath, "export", lock_error))
        {
            log("ERROR: " + lock_error);
            return result;
        }
        FLockGuard lock_guard{ fs::path(proj.m_RootPath) };

        // Resolve next to the running host first. Export must work from
        // zig-debug, zig-release, and packaged layouts without guessing a
        // hardcoded build directory.
        std::string host_exe = GetHostExePath();
        fs::path host_dir = host_exe.empty() ? current_path : fs::path(host_exe).parent_path();

        const std::string runtime_name = rw::platform::ExecutableName("runtime");
        const std::string game_host_name = rw::platform::ExecutableName("game");
        const bool b_IsDistribution = fs::exists(host_dir / "Core" / runtime_name);
        fs::path game_exe = b_IsDistribution ? (host_dir / "Core" / runtime_name) : (host_dir / game_host_name);

        auto find_raylib_files = [](const fs::path& directory)
        {
            std::vector<fs::path> files;
            std::error_code error;
            if (!fs::is_directory(directory, error))
            {
                return files;
            }
            for (const fs::directory_entry& entry : fs::directory_iterator(directory, error))
            {
                if (error || !entry.is_regular_file())
                {
                    continue;
                }
                const std::string filename = entry.path().filename().string();
                if (b_IsRaylibRuntimeName(filename))
                {
                    files.push_back(entry.path());
                }
            }
            return files;
        };

        std::vector<fs::path> raylib_files = find_raylib_files(host_dir);

        if (!fs::exists(game_exe))
            game_exe = current_path / game_host_name;
        if (raylib_files.empty())
            raylib_files = find_raylib_files(current_path);

        if (!fs::exists(game_exe))
        {
            return fail("ERROR: runtime/game executable not found! Please build the engine runtime first.");
        }

        if (raylib_files.empty())
        {
            return fail(std::string("ERROR: ") + rw::platform::c_RaylibRuntimeName + " not found!");
        }

        // 1. Build the project GameLogic module using CMake.
        log("Building project GameLogic (Release)...");
        const fs::path raywaves_dir = fs::path(proj.m_RootPath) / ".raywaves";
        const fs::path build_dir = raywaves_dir / "build";

        auto forward_output = [&log](std::string_view line, bool)
        {
            if (!line.empty())
            {
                log(line);
            }
        };

        fs::path cmake_exe =
            ProjectManager::GetToolsDirectory() / "cmake" / "bin" / rw::platform::ExecutableName("cmake");
        if (!fs::exists(cmake_exe))
        {
            const fs::path system_cmake = PlatformInfo::FindExecutable("cmake");
            if (!system_cmake.empty())
            {
                cmake_exe = system_cmake;
            }
        }
        if (!fs::exists(cmake_exe))
        {
            log("Downloading CMake (first-time setup)...");
            std::vector<std::string> setup_argv;
            if (rw::platform::b_IsWindows)
            {
                setup_argv = { "powershell", "-ExecutionPolicy", "Bypass", "-File",
                    (ProjectManager::GetToolsDirectory() / "setup_tools.ps1").string(), "-SkipZig", "-SkipRcEdit",
                    "-SkipNinja" };
            }
            else
            {
                setup_argv = { "bash", (ProjectManager::GetToolsDirectory() / "setup_tools.sh").string(), "--skip-zig",
                    "--skip-rcedit", "--skip-ninja" };
            }
            ProcessRunner::RunAndWaitCommand(setup_argv, proj.m_RootPath, forward_output, cancel);
            if (is_cancelled())
                return result;
        }

        // Blocking, but cancellable: the runner kills the whole build tree
        // when cancel is set, so Close() cannot hang on an export.
        bool b_configured = ProcessRunner::RunAndWaitCommand(
            { cmake_exe.string(), "-G", "Ninja", raywaves_dir.string(), "-B", build_dir.string() }, proj.m_RootPath,
            forward_output, cancel);
        if (!b_configured && !is_cancelled())
        {
            b_configured = ProcessRunner::RunAndWaitCommand(
                { cmake_exe.string(), "--fresh", "-G", "Ninja", raywaves_dir.string(), "-B", build_dir.string() },
                proj.m_RootPath, forward_output, cancel);
        }
        const bool b_BuildOk = b_configured &&
            ProcessRunner::RunAndWaitCommand(
                { cmake_exe.string(), "--build", build_dir.string(), "--config", "Release" }, proj.m_RootPath,
                forward_output, cancel);
        if (is_cancelled())
            return result;
        if (!b_BuildOk)
        {
            return fail("ERROR: Build failed!");
        }

        const fs::path game_logic_module = proj.m_DllPath;
        if (!fs::exists(game_logic_module))
        {
            return fail(
                std::string("ERROR: ") + rw::platform::c_GameLogicModuleName + " was not produced by the build.");
        }

        fs::path export_dir = ResolveExportDir(settings.m_ExportPath, proj.m_RootPath);
        fs::create_directories(export_dir);

        const std::string safe_game_name = SanitizeGameName(settings.m_GameName);
        const std::string game_exe_name = safe_game_name + rw::platform::c_ExecutableSuffix;
        log("Creating game executable: " + game_exe_name);
        fs::copy_file(game_exe, export_dir / game_exe_name, fs::copy_options::overwrite_existing);
        if (!rw::platform::b_IsWindows)
        {
            std::error_code permission_error;
            fs::permissions(export_dir / game_exe_name,
                fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec, fs::perm_options::add,
                permission_error);
        }

        log("Creating game configuration...");

        FExportSettings safe_settings = settings;
        safe_settings.m_GameName = safe_game_name;

        fs::path config_path = export_dir / "config.ini";
        if (!b_WriteGameConfig(
                config_path.string(), safe_settings, proj.m_SceneWidth, proj.m_SceneHeight, proj.m_TargetFPS))
        {
            return fail("ERROR: Could not write game configuration.");
        }

        log(std::string("Copying ") + rw::platform::c_GameLogicModuleName + "...");
        fs::copy_file(
            game_logic_module, export_dir / rw::platform::c_GameLogicModuleName, fs::copy_options::overwrite_existing);

        for (const fs::path& raylib_file : raylib_files)
        {
            log(std::string("Copying ") + raylib_file.filename().string() + "...");
            fs::copy_file(raylib_file, export_dir / raylib_file.filename(), fs::copy_options::overwrite_existing);
        }

        fs::path assets_dir = proj.m_AssetPath;
        if (fs::exists(assets_dir))
        {
            log("Copying project assets...");

            fs::path export_assets_dir = export_dir / "Assets";
            fs::create_directories(export_assets_dir);

            for (const auto& ENTRY : fs::directory_iterator(assets_dir))
            {
                if (ENTRY.is_directory())
                {
                    fs::path dest = export_assets_dir / ENTRY.path().filename();
                    fs::copy(ENTRY.path(), dest, fs::copy_options::recursive | fs::copy_options::overwrite_existing);

                    log("Copied asset folder: " + ENTRY.path().filename().string());
                }
                else if (ENTRY.is_regular_file())
                {
                    fs::path dest = export_assets_dir / ENTRY.path().filename();
                    fs::copy_file(ENTRY.path(), dest, fs::copy_options::overwrite_existing);

                    log("Copied asset file: " + ENTRY.path().filename().string());
                }
            }
        }
        else
        {
            log("No Assets folder found - skipping asset copy");
        }

        fs::path icon_path = proj.m_IconPath;
        if (icon_path.empty())
        {
            fs::path root = ProjectManager::GetEngineRootDirectory();
            fs::path defaultIcon = root / "Core" / "EngineContent" / "raylib.ico";
            if (!fs::exists(defaultIcon))
                defaultIcon = root / "EngineContent" / "raylib.ico";
            icon_path = defaultIcon;
        }
        else if (icon_path.is_relative())
        {
            icon_path = fs::path(proj.m_RootPath) / icon_path;
        }

        if (!rw::platform::b_IsWindows)
        {
            log("Icon embedding is Windows-only; skipped on this platform.");
        }
        else
        {
            const std::string custom_icon = icon_path.string();
            if (!custom_icon.empty() && fs::exists(custom_icon))
            {
                const fs::path rceditExe = ProjectManager::GetToolsDirectory() / "rcedit.exe";
                if (fs::exists(rceditExe))
                {
                    const std::string exePath = (export_dir / game_exe_name).string();
                    const bool b_IconOk =
                        ProcessRunner::RunAndWaitCommand({ rceditExe.string(), exePath, "--set-icon", custom_icon },
                            proj.m_RootPath, forward_output, cancel);
                    log(b_IconOk ? "rcedit: icon applied" : "rcedit: icon update failed");
                }
                else
                {
                    log("WARNING: rcedit.exe not found at: " + rceditExe.string());
                }
            }
            else
            {
                log("WARNING: No icon found. customIcon=" + custom_icon +
                    " exists=" + (fs::exists(custom_icon) ? "true" : "false"));
            }
        }

        log(std::string("Process completed. Validating export folder: ") + export_dir.string());

        bool b_Ok = b_ValidateExportFolder(export_dir.string(), log);
        result.m_bSuccess = b_Ok;

        if (!b_Ok)
        {
            log("Export validation failed - check export folder contents");
        }
        else
        {
            log("Export completed successfully!");
        }
    }
    catch (const std::exception& e)
    {
        return fail(std::string("CRITICAL ERROR: ") + e.what());
    }

    return result;
}
} // namespace ExportService
