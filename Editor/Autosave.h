#pragma once
#include <filesystem>
#include <string>
#include <string_view>

class Scene;

// Crash-recovery snapshots for the open project. A snapshot is deliberately
// separate from the project's Assets/<map>.scene: the author's save stays
// authoritative, and the snapshot is only consulted when they ask for it.
namespace Autosave
{
struct FSnapshot
{
    std::string m_MapId;
    std::string m_ProjectRoot;
    long long m_UnixSeconds = 0;
};

// <project>/.raywaves/autosave
std::filesystem::path GetSnapshotDirectory(const std::filesystem::path& project_root);

// Writes autosave.scene and then autosave.ini. The scene is written first so a
// half-finished snapshot never advertises metadata without a payload. Does not
// clear the scene's dirty flag.
bool b_Save(
    const std::filesystem::path& project_root, std::string_view map_id, const Scene& scene, std::string& out_error);

// Reads the snapshot metadata. A missing or incomplete snapshot is not an
// error; out_snapshot is cleared and the function returns false.
bool b_Query(const std::filesystem::path& project_root, FSnapshot& out_snapshot);

// Loads the snapshot scene and its metadata. out_scene is left untouched when
// the snapshot is absent or unreadable.
bool b_Load(
    const std::filesystem::path& project_root, Scene& out_scene, FSnapshot& out_snapshot, std::string& out_error);

// Removes the snapshot directory. Safe to call when no snapshot exists.
bool b_Clear(const std::filesystem::path& project_root);
} // namespace Autosave
