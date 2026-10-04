#pragma once
#include <filesystem>
#include <string>
#include <string_view>

namespace EditorUtils
{
    bool OpenInExplorer(const std::filesystem::path& path);
    bool OpenURL(std::string_view url);
    bool IsShellSafe(std::string_view s);

    // Recover a usable CWD after the launch directory was deleted or moved
    // while the editor runs. Without this every popen shell prints
    // "getcwd: cannot access" and fs::current_path() throws.
    void EnsureValidCwd();

    // Default directory for native file dialogs: project root, else CWD,
    // else "/". Empty default makes KDE's file dialog log
    // "kf.kio.core: Invalid URL: QUrl("")".
    std::string DefaultDialogDir();

    // Editor's GameConfig location: XDG_CONFIG_HOME/RayWaves/config.ini.
    // The old CWD-relative "config.ini" scattered a copy into every
    // launch directory; it stays only as a read-fallback for migration.
    std::string GameConfigPath();
}
