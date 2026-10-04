#include "ExportService.h"
#include "../Engine/ProjectManager.h"
#include "EditorUtils.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

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

    bool b_WriteGameConfig(const std::string& config_path, const FExportSettings& settings,
                           int scene_width, int scene_height, int scene_fps)
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
            if (!b_Exists) b_Ok = false;
        };

        bool b_FoundGameExe = false;
        std::error_code ec;
        if (fs::exists(out_dir, ec) && !ec)
        {
            for (const auto& ENTRY : fs::directory_iterator(out_dir, ec))
            {
                if (!ec && ENTRY.is_regular_file() && ENTRY.path().extension() == ".exe")
                {
                    b_FoundGameExe = true;
                    log(std::string("Found game executable: ") + ENTRY.path().filename().string());
                    break;
                }
            }
        }
        if (!b_FoundGameExe)
        {
            log("Missing: Game executable (.exe file)");
            b_Ok = false;
        }

        require(fs::path(out_dir) / "GameLogic.dll");
        require(fs::path(out_dir) / "libraylib.dll");

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

    FExportResult RunExport(const FExportSettings& settings,
                            const std::shared_ptr<std::atomic<bool>>& cancel,
                            LogSink log)
    {
        FExportResult result;

        auto fail = [&](std::string_view message)
        {
            log(message);
            result.m_bSuccess = false;
            return result;
        };

        auto is_cancelled = [&]()
        {
            return (cancel != nullptr) && cancel->load();
        };

        try
        {
            if (!ProjectManager::b_HasOpenProject())
            {
                return fail("ERROR: No project is currently open!");
            }

            if (is_cancelled()) return result;

            fs::create_directories(settings.m_ExportPath);
            log("Starting export process...");

            fs::path current_path = fs::current_path();
            const auto& proj = ProjectManager::GetCurrent();

            bool b_IsDistribution = fs::exists(current_path / "Core" / "runtime.exe") && !fs::exists(current_path / "Game" / "game.cpp");
            fs::path game_exe = b_IsDistribution ? (current_path / "Core" / "runtime.exe") : (current_path / "build" / "zig-release" / "game.exe"); // Fallback for source environment
            fs::path raylib_dll = b_IsDistribution ? (current_path / "libraylib.dll") : (current_path / "build" / "zig-release" / "libraylib.dll");

            if (!fs::exists(game_exe)) game_exe = current_path / "game.exe"; // Generic fallback
            if (!fs::exists(raylib_dll)) raylib_dll = current_path / "libraylib.dll";

            if (!fs::exists(game_exe))
            {
                return fail("ERROR: runtime.exe/game.exe not found! Please build the engine runtime first.");
            }

            if (!fs::exists(raylib_dll))
            {
                return fail("ERROR: libraylib.dll not found!");
            }

            // 1. Build the Project DLL using CMake
            log("Building project GameLogic (Release)...");
            fs::path raywaves_dir = fs::path(proj.m_RootPath) / ".raywaves";
            std::string path_str = raywaves_dir.string();
            if (!EditorUtils::IsShellSafe(path_str))
            {
                return fail("ERROR: Project path contains unsafe characters!");
            }

            fs::path cmakeExe = ProjectManager::GetToolsDirectory() / "cmake" / "bin" / "cmake.exe";
            if (!fs::exists(cmakeExe))
            {
                log("Downloading CMake (first-time setup)...");
                std::string fetchCmd = "powershell -ExecutionPolicy Bypass -File \""
                    + (ProjectManager::GetToolsDirectory() / "setup_zig.ps1").string()
                    + "\" -SkipZig -SkipRcEdit -SkipNinja";
                std::system(fetchCmd.c_str());
            }

            std::string cmakePath = "\"" + cmakeExe.string() + "\"";
            std::string build_cmd = "cd /d \"" + path_str + "\" && (" + cmakePath + " -G Ninja . -B build || " + cmakePath + " --fresh -G Ninja . -B build) && " + cmakePath + " --build build --config Release";

            FILE* pipe = _popen(build_cmd.c_str(), "r");
            if (pipe)
            {
                std::array<char, 1024> buffer{};
                while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr)
                {
                    std::string line = buffer.data();
                    line.erase(line.find_last_not_of(" \n\r\t") + 1);
                    if (!line.empty()) {
                        log(line);
                    }
                }
                int build_result = _pclose(pipe);
                if (build_result != 0)
                {
                    return fail("ERROR: Build failed!");
                }
            }

            fs::path game_logic_dll = fs::path(proj.m_RootPath) / "GameLogic.dll";
            if (!fs::exists(game_logic_dll))
            {
                return fail("ERROR: GameLogic.dll was not produced by the build.");
            }

            fs::path export_dir = ResolveExportDir(settings.m_ExportPath, proj.m_RootPath);
            fs::create_directories(export_dir);

            std::string game_exe_name = settings.m_GameName + ".exe";
            log("Creating game executable: " + game_exe_name);
            fs::copy_file(game_exe, export_dir / game_exe_name, fs::copy_options::overwrite_existing);

            log("Creating game configuration...");

            fs::path config_path = export_dir / "config.ini";
            if (!b_WriteGameConfig(config_path.string(), settings,
                                   proj.m_SceneWidth, proj.m_SceneHeight, proj.m_TargetFPS))
            {
                return fail("ERROR: Could not write game configuration.");
            }

            log("Copying GameLogic.dll...");
            fs::copy_file(game_logic_dll, export_dir / "GameLogic.dll", fs::copy_options::overwrite_existing);

            log("Copying libraylib.dll...");
            fs::copy_file(raylib_dll, export_dir / "libraylib.dll", fs::copy_options::overwrite_existing);

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

            std::string customIcon = proj.m_IconPath;
            if (customIcon.empty())
            {
                fs::path root = ProjectManager::GetEngineRootDirectory();
                fs::path defaultIcon = root / "Core" / "EngineContent" / "raylib.ico";
                if (!fs::exists(defaultIcon)) defaultIcon = root / "EngineContent" / "raylib.ico";
                customIcon = defaultIcon.string();
            }

            if (!customIcon.empty() && fs::exists(customIcon))
            {
                fs::path rceditExe = ProjectManager::GetToolsDirectory() / "rcedit.exe";
                if (fs::exists(rceditExe))
                {
                    std::string exePath = (export_dir / game_exe_name).string();
                    // cmd.exe /c strips first+last " when string starts with ". Wrap entire command in outer quotes.
                    std::string cmd = "\"\"" + rceditExe.string() + "\" \"" + exePath + "\" --set-icon \"" + customIcon + "\"\"";
                    log("Icon cmd: " + cmd);
                    int rc = std::system(cmd.c_str());
                    log("rcedit exit code: " + std::to_string(rc));
                }
                else
                {
                    log("WARNING: rcedit.exe not found at: " + rceditExe.string());
                }
            }
            else
            {
                log("WARNING: No icon found. customIcon=" + customIcon + " exists=" + (fs::exists(customIcon) ? "true" : "false"));
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
}
