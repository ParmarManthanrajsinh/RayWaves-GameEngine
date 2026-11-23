#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <string_view>

struct t_Project;

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
    // Remove the resolved export directory before writing. The service refuses
    // to clean an empty path, the project root, or an ancestor of it.
    bool m_bCleanOutput = false;
};

struct FExportResult
{
    bool m_bSuccess = false;
    // Absolute export directory the run targeted. Empty only when the
    // project had no usable root path.
    std::string m_OutputPath;
};

namespace ExportService
{
using LogSink = std::function<void(std::string_view line)>;
// Optional pipeline progress. `progress` is 0..1 and `stage` is a short
// human-readable milestone ("Building GameLogic", "Copying assets", ...).
using ProgressSink = std::function<void(float progress, std::string_view stage)>;

// Clamp dimensions/fps to sane ranges (pure, unit-tested).
void ClampSettings(FExportSettings& settings);

// Resolve the export dir: relative paths anchor at the project root,
// absolute paths pass through (pure, unit-tested).
std::string ResolveExportDir(std::string_view export_path, std::string_view project_root);

// Produce a safe file name for the exported executable. Replaces path
// separators and shell-sensitive characters, trims dots/spaces, and
// avoids Windows reserved device names (pure, unit-tested).
std::string SanitizeGameName(std::string_view name);

// Write config.ini for the exported game (unit-tested).
bool b_WriteGameConfig(
    const std::string& config_path, const FExportSettings& settings, int scene_width, int scene_height, int scene_fps);

// True for editor-side backup/temp files that must never ship in an export.
bool b_IsTransientAssetFile(std::string_view filename);

// Check a finished export folder for the required files (unit-tested).
bool b_ValidateExportFolder(std::string_view out_dir, LogSink log);

// Full export on the caller's thread. Explicit project overload: rwcli and
// other hosts pass a resolved snapshot so exporting never depends on the
// open-project singleton. Honors cancel and reports progress when a sink is
// provided.
FExportResult RunExport(const t_Project& project, const FExportSettings& settings,
    const std::shared_ptr<std::atomic<bool>>& cancel, LogSink log, ProgressSink progress = {});

// Editor overload: resolves the open project via ProjectManager and fails
// cleanly when no project is open.
FExportResult RunExport(const FExportSettings& settings, const std::shared_ptr<std::atomic<bool>>& cancel, LogSink log,
    ProgressSink progress = {});
} // namespace ExportService
