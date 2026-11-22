#include "Autosave.h"
#include "../Engine/PlatformInfo.h"
#include "../Engine/Scene.h"
#include <charconv>
#include <chrono>
#include <fstream>
#include <system_error>
#include <utility>

namespace
{
constexpr const char* c_SceneFileName = "autosave.scene";
constexpr const char* c_MetadataFileName = "autosave.ini";

std::string_view Trim(std::string_view value)
{
    const size_t start = value.find_first_not_of(" \t\r\n");
    if (start == std::string_view::npos)
    {
        return {};
    }
    const size_t end = value.find_last_not_of(" \t\r\n");
    return value.substr(start, (end - start) + 1);
}

bool b_WriteMetadata(const std::filesystem::path& path, const Autosave::FSnapshot& snapshot, std::string& out_error)
{
    std::string contents;
    contents += "map_id=";
    contents += snapshot.m_MapId;
    contents += "\nproject_root=";
    contents += snapshot.m_ProjectRoot;
    contents += "\nunix_seconds=";
    contents += std::to_string(snapshot.m_UnixSeconds);
    contents += "\n";

    return PlatformInfo::b_WriteFileAtomic(path, contents, out_error);
}

bool b_ReadMetadata(const std::filesystem::path& path, Autosave::FSnapshot& out_snapshot)
{
    std::ifstream file(path);
    if (!file.is_open())
    {
        return false;
    }

    Autosave::FSnapshot snapshot;
    std::string line;
    while (std::getline(file, line))
    {
        const std::string_view view = Trim(line);
        if (view.empty() || view.front() == '#' || view.front() == ';')
        {
            continue;
        }

        const size_t equal = view.find('=');
        if (equal == std::string_view::npos)
        {
            continue;
        }

        const std::string_view key = Trim(view.substr(0, equal));
        const std::string_view value = Trim(view.substr(equal + 1));
        if (key == "map_id")
        {
            snapshot.m_MapId = std::string(value);
        }
        else if (key == "project_root")
        {
            snapshot.m_ProjectRoot = std::string(value);
        }
        else if (key == "unix_seconds")
        {
            long long seconds = 0;
            const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), seconds);
            (void)end;
            if (error == std::errc{})
            {
                snapshot.m_UnixSeconds = seconds;
            }
        }
    }

    // A snapshot without a map id cannot be restored safely, so treat it as
    // absent rather than handing the editor a half-valid record.
    if (snapshot.m_MapId.empty())
    {
        return false;
    }

    out_snapshot = std::move(snapshot);
    return true;
}
} // namespace

namespace Autosave
{
std::filesystem::path GetSnapshotDirectory(const std::filesystem::path& project_root)
{
    if (project_root.empty())
    {
        return {};
    }
    return project_root / ".raywaves" / "autosave";
}

bool b_Save(
    const std::filesystem::path& project_root, std::string_view map_id, const Scene& scene, std::string& out_error)
{
    out_error.clear();

    if (project_root.empty())
    {
        out_error = "Cannot autosave: no project root.";
        return false;
    }
    if (map_id.empty())
    {
        out_error = "Cannot autosave: no active map.";
        return false;
    }

    const std::filesystem::path directory = GetSnapshotDirectory(project_root);
    std::error_code directory_error;
    std::filesystem::create_directories(directory, directory_error);
    if (directory_error)
    {
        out_error = "Cannot create autosave directory " + directory.string() + ": " + directory_error.message();
        return false;
    }

    // Metadata is the commit marker. Remove the old marker before rewriting the
    // scene so a crash mid-save cannot pair a new scene with an old map id.
    std::error_code marker_error;
    std::filesystem::remove(directory / c_MetadataFileName, marker_error);

    // Scene::b_SaveToFile is non-const and clears the dirty flag. The snapshot
    // must not make the author's unsaved work look saved, so restore the flag
    // afterwards.
    Scene& mutable_scene = const_cast<Scene&>(scene);
    const bool b_was_dirty = mutable_scene.b_IsDirty();
    const std::filesystem::path scene_path = directory / c_SceneFileName;
    const bool b_scene_saved = mutable_scene.b_SaveToFile(scene_path.string());
    if (b_was_dirty)
    {
        mutable_scene.MarkDirty();
    }
    if (!b_scene_saved)
    {
        out_error = "Cannot write autosave scene " + scene_path.string();
        return false;
    }

    FSnapshot snapshot;
    snapshot.m_MapId = std::string(map_id);
    snapshot.m_ProjectRoot = project_root.string();
    snapshot.m_UnixSeconds =
        std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();

    // Metadata is the commit marker: it is written last and atomically, so a
    // reader never sees a new map id paired with an old scene.
    return b_WriteMetadata(directory / c_MetadataFileName, snapshot, out_error);
}

bool b_Query(const std::filesystem::path& project_root, FSnapshot& out_snapshot)
{
    out_snapshot = {};

    const std::filesystem::path directory = GetSnapshotDirectory(project_root);
    if (directory.empty())
    {
        return false;
    }

    std::error_code scene_error;
    if (!std::filesystem::is_regular_file(directory / c_SceneFileName, scene_error))
    {
        return false;
    }

    FSnapshot snapshot;
    if (!b_ReadMetadata(directory / c_MetadataFileName, snapshot))
    {
        return false;
    }

    out_snapshot = std::move(snapshot);
    return true;
}

bool b_Load(
    const std::filesystem::path& project_root, Scene& out_scene, FSnapshot& out_snapshot, std::string& out_error)
{
    out_error.clear();
    out_snapshot = {};

    FSnapshot snapshot;
    if (!b_Query(project_root, snapshot))
    {
        out_error = "No autosave snapshot exists for this project.";
        return false;
    }

    const std::filesystem::path scene_path = GetSnapshotDirectory(project_root) / c_SceneFileName;
    if (!out_scene.b_LoadFromFile(scene_path.string()))
    {
        out_error = "Cannot read autosave scene " + scene_path.string();
        return false;
    }

    out_snapshot = std::move(snapshot);
    return true;
}

bool b_Clear(const std::filesystem::path& project_root)
{
    const std::filesystem::path directory = GetSnapshotDirectory(project_root);
    if (directory.empty())
    {
        return false;
    }

    std::error_code error;
    std::filesystem::remove_all(directory, error);
    return !error;
}
} // namespace Autosave
