#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <string_view>

// Export settings snapshot. Copied by value onto the worker thread so the
// panel's live m_ExportState can keep changing underneath it.
struct FExportSettings
{
    std::string m_GameName = "MyGame";
    std::string m_ExportPath;
    int m_WindowWidth = 1280;
    int m_WindowHeight = 720;
    int m_TargetFPS = 60;
    bool m_bFullscreen = false;
    bool m_bResizable = true;
    bool m_bVSync = true;
};

struct FExportResult
{
    bool m_bSuccess = false;
};

namespace ExportService
{
    using LogSink = std::function<void(std::string_view line)>;

    // Clamp dimensions/fps to sane ranges (pure, unit-tested).
    void ClampSettings(FExportSettings& settings);

    // Resolve the export dir: relative paths anchor at the project root,
    // absolute paths pass through (pure, unit-tested).
    std::string ResolveExportDir(std::string_view export_path, std::string_view project_root);

    // Write config.ini for the exported game (unit-tested).
    bool b_WriteGameConfig(const std::string& config_path, const FExportSettings& settings,
                           int scene_width, int scene_height, int scene_fps);

    // Check a finished export folder for the required files (unit-tested).
    bool b_ValidateExportFolder(std::string_view out_dir, LogSink log);

    // Full export on the caller's thread (the panel runs it on its worker
    // thread). Reads the open project via ProjectManager and honors cancel.
    FExportResult RunExport(const FExportSettings& settings,
                            const std::shared_ptr<std::atomic<bool>>& cancel,
                            LogSink log);
}
