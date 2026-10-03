#include "ExportService.h"
#include "../Engine/ProjectManager.h"
#include "EditorUtils.h"
#include "../Engine/Platform/PlatformDesktop.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <sys/wait.h>

namespace fs = std::filesystem;

namespace ExportService
{
    namespace
    {
        // Export validation runs against shipped binaries, so check the file
        // itself (ELF magic) instead of trusting an extension.
        bool b_IsElfExecutable(const fs::path& p)
        {
            std::error_code ec;
            if (!fs::is_regular_file(p, ec) || ec)
            {
                return false;
            }

            std::ifstream file(p, std::ios::binary);
            char magic[4] = {};
            file.read(magic, sizeof(magic));
            if (!file || magic[0] != 0x7f || magic[1] != 'E' ||
                magic[2] != 'L' || magic[3] != 'F')
            {
                return false;
            }

            auto perms = fs::status(p, ec).permissions();
            return !ec && (perms & fs::perms::owner_exec) != fs::perms::none;
        }

        void MakeExecutable(const fs::path& p)
        {
            std::error_code ec;
            auto perms = fs::status(p, ec).permissions();
            if (ec)
            {
                return;
            }
            fs::permissions(
                p,
                perms | fs::perms::owner_exec | fs::perms::group_exec |
                    fs::perms::others_exec,
                ec);
        }
    }

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
                if (!ec && ENTRY.is_regular_file() && b_IsElfExecutable(ENTRY.path()))
                {
                    b_FoundGameExe = true;
                    log(std::string("Found game executable: ") + ENTRY.path().filename().string());
                    break;
                }
            }
        }
        if (!b_FoundGameExe)
        {
            log("Missing: Game executable (ELF file)");
            b_Ok = false;
        }

        require(fs::path(out_dir) / "GameLogic.so");
        require(fs::path(out_dir) / "libraylib.so");

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

            bool b_IsDistribution =
                fs::exists(current_path / "Core" / "runtime") &&
                !fs::exists(current_path / "Game" / "game.cpp");
            fs::path game_exe;
            if (b_IsDistribution)
            {
                game_exe = current_path / "Core" / "runtime";
            }
            else
            {
                game_exe = current_path / "build" / "linux-release" / "game";
                if (!fs::exists(game_exe))
                    game_exe = current_path / "build" / "linux-debug" / "game";
                if (!fs::exists(game_exe))
                    game_exe = current_path / "game"; // Generic fallback
            }

            fs::path raylib_so = game_exe.parent_path() / "libraylib.so";
            if (!fs::exists(raylib_so)) raylib_so = current_path / "libraylib.so";
            if (!fs::exists(raylib_so)) raylib_so = current_path / "build" / "linux-release" / "libraylib.so";
            if (!fs::exists(raylib_so)) raylib_so = current_path / "build" / "linux-debug" / "libraylib.so";

            if (!fs::exists(game_exe))
            {
                return fail("ERROR: runtime/game not found! Please build the engine runtime first.");
            }

            if (!fs::exists(raylib_so))
            {
                return fail("ERROR: libraylib.so not found!");
            }

            // 1. Build the project GameLogic using CMake
            log("Building project GameLogic (Release)...");
            fs::path raywaves_dir = fs::path(proj.m_RootPath) / ".raywaves";
            std::string path_str = raywaves_dir.string();
            if (!EditorUtils::IsShellSafe(path_str))
            {
                return fail("ERROR: Project path contains unsafe characters!");
            }

            // System cmake from PATH: popen already runs through `sh -c`.
            // CMake caches contain absolute paths, so fall back to --fresh.
            std::string build_cmd = "cd \"" + path_str + "\" && (cmake -G Ninja . -B build || cmake --fresh -G Ninja . -B build) && cmake --build build --config Release";

            FILE* pipe = popen(build_cmd.c_str(), "r");
            if (!pipe)
            {
                return fail("ERROR: Failed to start build process!");
            }
            std::array<char, 1024> buffer{};
            while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr)
            {
                std::string line = buffer.data();
                line.erase(line.find_last_not_of(" \n\r\t") + 1);
                if (!line.empty()) {
                    log(line);
                }
            }
            int status = pclose(pipe);
            if (status == -1 || !WIFEXITED(status) || WEXITSTATUS(status) != 0)
            {
                return fail("ERROR: Build failed!");
            }

            fs::path game_logic_dll = fs::path(proj.m_RootPath) / "GameLogic.so";
            if (!fs::exists(game_logic_dll))
            {
                return fail("ERROR: GameLogic.so was not produced by the build.");
            }

            fs::path export_dir = ResolveExportDir(settings.m_ExportPath, proj.m_RootPath);
            fs::create_directories(export_dir);

            std::string game_exe_name = settings.m_GameName;
            log("Creating game executable: " + game_exe_name);
            fs::copy_file(game_exe, export_dir / game_exe_name, fs::copy_options::overwrite_existing);
            MakeExecutable(export_dir / game_exe_name);

            log("Creating game configuration...");

            fs::path config_path = export_dir / "config.ini";
            if (!b_WriteGameConfig(config_path.string(), settings,
                                   proj.m_SceneWidth, proj.m_SceneHeight, proj.m_TargetFPS))
            {
                return fail("ERROR: Could not write game configuration.");
            }

            log("Copying GameLogic.so...");
            fs::copy_file(game_logic_dll, export_dir / "GameLogic.so", fs::copy_options::overwrite_existing);

            log("Copying libraylib.so...");
            fs::copy_file(raylib_so, export_dir / "libraylib.so", fs::copy_options::overwrite_existing);

            // Launcher: pin the export dir on LD_LIBRARY_PATH so the ELF
            // loads its bundled libraylib.so regardless of rpath state.
            fs::path run_sh = export_dir / "run.sh";
            {
                std::ofstream sh(run_sh);
                if (!sh.is_open())
                {
                    return fail("ERROR: Could not write run.sh.");
                }
                sh << "#!/bin/sh\n"
                   << "# Generated by RayWaves export. Launches the game\n"
                   << "# next to this script with bundled libraries.\n"
                   << "DIR=\"$(CDPATH= cd -- \"$(dirname -- \"$0\")\" && pwd)\"\n"
                   << "export LD_LIBRARY_PATH=\"$DIR${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}\"\n"
                   << "exec \"$DIR/" << game_exe_name << "\" \"$@\"\n";
            }
            MakeExecutable(run_sh);
            log("Created run.sh launcher");

            // Icon for the desktop entry: project icon if set, else the
            // engine default PNG (the .ico era is gone; XDG wants PNG).
            fs::path icon_src = proj.m_IconPath;
            if (icon_src.empty() || !fs::exists(icon_src))
            {
                fs::path root = ProjectManager::GetEngineRootDirectory();
                icon_src = root / "Core" / "EngineContent" / "icon.png";
                if (!fs::exists(icon_src)) icon_src = root / "EngineContent" / "icon.png";
            }

            fs::path export_icon = export_dir / (game_exe_name + ".png");
            if (fs::exists(icon_src))
            {
                fs::copy_file(icon_src, export_icon,
                              fs::copy_options::overwrite_existing);
                log("Copied game icon: " + export_icon.filename().string());
            }
            else
            {
                log("WARNING: no icon found at " + icon_src.string() +
                    "; install.sh will skip the icon.");
            }

            // Desktop entry template: install.sh substitutes @EXEC@ with the
            // real install path, so the export stays relocatable. Empty MIME
            // type keeps the game from claiming .raywaves from the editor.
            std::string icon_name = game_exe_name;
            fs::path desktop_in = export_dir / (game_exe_name + ".desktop.in");
            {
                std::ofstream entry(desktop_in);
                if (!entry.is_open())
                {
                    return fail("ERROR: Could not write desktop entry template.");
                }
                entry << platform::MakeDesktopEntry(game_exe_name, "@EXEC@",
                                                    icon_name, "");
            }

            // install.sh: per-user, no root - copies the payload, installs
            // icon + .desktop entry, refreshes the desktop database.
            fs::path install_sh = export_dir / "install.sh";
            {
                std::ofstream sh(install_sh);
                if (!sh.is_open())
                {
                    return fail("ERROR: Could not write install.sh.");
                }
                sh << "#!/bin/sh\n"
                   << "# Generated by RayWaves export. Installs this game for\n"
                   << "# the current user (no root needed).\n"
                   << "set -eu\n"
                   << "DIR=\"$(CDPATH= cd -- \"$(dirname -- \"$0\")\" && pwd)\"\n"
                   << "GAME_NAME='" << game_exe_name << "'\n"
                   << "DATA_HOME=\"${XDG_DATA_HOME:-$HOME/.local/share}\"\n"
                   << "DEST=\"$DATA_HOME/RayWavesGames/$GAME_NAME\"\n"
                   << "APPS=\"$DATA_HOME/applications\"\n"
                   << "ICONS_DIR=\"$DATA_HOME/" 
                   << platform::InstalledIconRelPath(icon_name) << "\"\n"
                   << "\n"
                   << "echo \"Installing to $DEST ...\"\n"
                   << "mkdir -p \"$DEST\"\n"
                   << "for entry in \"$DIR\"/*; do\n"
                   << "    [ -e \"$entry\" ] || continue\n"
                   << "    case \"$(basename \"$entry\")\" in\n"
                   << "        install.sh|*.desktop.in) continue ;;\n"
                   << "    esac\n"
                   << "    cp -a \"$entry\" \"$DEST/\"\n"
                   << "done\n"
                   << "\n"
                   << "if [ -f \"$DIR/" << game_exe_name << ".png\" ]; then\n"
                   << "    mkdir -p \"$(dirname \"$ICONS_DIR\")\"\n"
                   << "    cp -f \"$DIR/" << game_exe_name << ".png\" \"$ICONS_DIR\"\n"
                   << "fi\n"
                   << "\n"
                   << "if [ -f \"$DIR/" << game_exe_name << ".desktop.in\" ]; then\n"
                   << "    mkdir -p \"$APPS\"\n"
                   << "    sed \"s|^Exec=.*|Exec=$DEST/run.sh|\" \"$DIR/"
                   << game_exe_name << ".desktop.in\" > \"$APPS/" 
                   << game_exe_name << ".desktop\"\n"
                   << "    update-desktop-database \"$APPS\" 2>/dev/null || true\n"
                   << "fi\n"
                   << "\n"
                   << "echo \"Installed. Launch with: $DEST/run.sh\"\n"
                   << "echo \"Desktop entry: $APPS/" << game_exe_name 
                   << ".desktop\"\n";
            }
            MakeExecutable(install_sh);
            log("Created install.sh installer");

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

            log("Icon embedding skipped: Linux desktops take the icon from the .desktop entry at install time.");

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

                // Tarball for distribution: same folder, ready to ship.
                if (!is_cancelled())
                {
                    fs::path tarball = export_dir.parent_path() /
                                      (game_exe_name + ".tar.gz");
                    std::string tar_cmd =
                        "tar -C \"" + export_dir.parent_path().string() +
                        "\" -czf \"" + tarball.string() + "\" \"" +
                        export_dir.filename().string() + "\"";
                    if (std::system(tar_cmd.c_str()) == 0)
                    {
                        log("Created archive: " + tarball.string());
                    }
                    else
                    {
                        log("WARNING: tar failed; export folder left as-is.");
                    }
                }
            }
        }
        catch (const std::exception& e)
        {
            return fail(std::string("CRITICAL ERROR: ") + e.what());
        }

        return result;
    }
}
