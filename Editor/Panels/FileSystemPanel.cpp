#include "FileSystemPanel.h"
#include "../../Engine/MapManager.h"
#include "../../Engine/Profiler.h"
#include "../../Engine/ProjectManager.h"
#include "../EditorAssets.h"
#include "../EditorUtils.h"
#include "../EditorWidgets.h"
#include "../GameEditor.h"
#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <imgui.h>
#include <imgui_stdlib.h>
#include <tinyfiledialogs.h>
#include <vector>

namespace
{
std::string ToLower(std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(),
        [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
    return text;
}

bool b_MatchesFilter(std::string_view name, std::string_view filter)
{
    if (filter.empty())
    {
        return true;
    }
    return ToLower(std::string(name)).contains(ToLower(std::string(filter)));
}

// Scene files authored on Windows may store separators as '\'; the asset
// resolver normalizes them, so comparisons in this panel must too.
std::string NormalizeAssetKey(std::string text)
{
    std::replace(text.begin(), text.end(), '\\', '/');
    return text;
}

const char* IconForPath(const std::filesystem::path& path)
{
    if (EditorAssets::b_IsScene(path.string()))
    {
        return ICON_FA_FILE_LINES;
    }
    if (EditorAssets::b_IsImage(path.string()))
    {
        return ICON_FA_IMAGE;
    }
    if (EditorAssets::b_IsAudio(path.string()))
    {
        return ICON_FA_MUSIC;
    }
    if (EditorAssets::b_IsCode(path.string()))
    {
        return ICON_FA_CODE;
    }
    return ICON_FA_FILE;
}

void CreateSprite(GameEditor* editor, const std::string& relative_path, Vector2 position)
{
    Scene& scene = editor->GetScene();
    t_SceneObject& object = scene.AddObject("Sprite");
    object.m_TexturePath = relative_path;
    object.m_Position = position;
    scene.ReloadTexture(object.m_Id);
    editor->SelectSceneObject(object.m_Id);
}

void AssignTextureRelative(GameEditor* editor, const std::string& relative_path)
{
    if (relative_path.empty())
    {
        return;
    }

    if (t_SceneObject* object = editor->GetSelectedSceneObject())
    {
        object->m_TexturePath = relative_path;
        editor->GetScene().ReloadTexture(object->m_Id);
        return;
    }

    if (editor->m_SelectionKind == GameEditor::EEditorSelectionKind::None)
    {
        CreateSprite(editor, relative_path, editor->GetViewportCenterWorld());
    }
}

void AssignTexture(GameEditor* editor, const std::filesystem::path& path)
{
    AssignTextureRelative(editor, EditorAssets::ToRelativeAssetPath(path));
}

const char* const* ImageFileFilters()
{
    static const char* s_Filters[] = { "*.png", "*.jpg", "*.jpeg", "*.bmp", "*.tga", "*.gif", "*.qoi", "*.psd" };
    return s_Filters;
}

constexpr int c_ImageFilterCount = 8;

// Statting scene texture paths every frame is wasted work while a project sits
// idle; a coarse rescan keeps the panel responsive to external file changes
// without burning syscalls on each frame.
constexpr uint64_t c_MissingScanIntervalFrames = 30;

// A nested file change updates only its own directory's timestamp, never the
// root's, so the recursive source listing needs a slow periodic safety net.
constexpr uint64_t c_SourceCacheRefreshFrames = 120;
} // namespace

FileSystemPanel::~FileSystemPanel()
{
    UnloadThumbnails();
}

void FileSystemPanel::UnloadThumbnails()
{
    for (auto& [key, thumbnail] : m_Thumbnails)
    {
        if (IsTextureValid(thumbnail.m_Texture))
        {
            UnloadTexture(thumbnail.m_Texture);
        }
    }
    m_Thumbnails.clear();

    // Every caller of this function just imported, renamed, deleted, or
    // re-rooted an asset, so the directory snapshots and the missing-asset
    // report are stale by construction.
    InvalidateFileCaches();
}

void FileSystemPanel::InvalidateFileCaches()
{
    m_AssetFiles.m_bValid = false;
    m_SourceFiles.m_bValid = false;
    m_bMissingScanValid = false;
}

const std::vector<std::filesystem::path>& FileSystemPanel::GetCachedFiles(
    FDirectoryCache& cache, const std::filesystem::path& directory, bool b_recursive, uint64_t max_age_frames)
{
    std::error_code error;
    const auto write_time = std::filesystem::last_write_time(directory, error);

    bool b_refresh = !cache.m_bValid || cache.m_Path != directory;
    if (!b_refresh)
    {
        if (error)
        {
            // A root that vanished drops its stale listing; a root that is
            // simply absent stays cached as an empty result.
            b_refresh = cache.m_bExists || !cache.m_Files.empty();
        }
        else
        {
            b_refresh = !cache.m_bExists || cache.m_WriteTime != write_time;
        }
        if (!b_refresh && max_age_frames > 0)
        {
            b_refresh = (m_FrameCounter - cache.m_RefreshFrame) >= max_age_frames;
        }
    }

    if (b_refresh)
    {
        cache.m_Path = directory;
        cache.m_WriteTime = error ? std::filesystem::file_time_type{} : write_time;
        cache.m_bExists = !error;
        cache.m_Files = EditorAssets::ListFiles(directory, b_recursive);
        cache.m_RefreshFrame = m_FrameCounter;
        cache.m_bValid = true;
    }

    return cache.m_Files;
}

Texture2D* FileSystemPanel::GetThumbnail(const std::filesystem::path& path)
{
    const std::string key = path.string();
    std::error_code error;
    const auto write_time = std::filesystem::last_write_time(path, error);

    auto cached = m_Thumbnails.find(key);
    if (cached != m_Thumbnails.end())
    {
        if (error || cached->second.m_WriteTime == write_time)
        {
            return IsTextureValid(cached->second.m_Texture) ? &cached->second.m_Texture : nullptr;
        }
        if (IsTextureValid(cached->second.m_Texture))
        {
            UnloadTexture(cached->second.m_Texture);
        }
        m_Thumbnails.erase(cached);
    }

    FThumbnail thumbnail;
    thumbnail.m_WriteTime = write_time;
    thumbnail.m_Texture = LoadTexture(path.string().c_str());
    if (IsTextureValid(thumbnail.m_Texture))
    {
        SetTextureFilter(thumbnail.m_Texture, TEXTURE_FILTER_BILINEAR);
    }
    else
    {
        thumbnail.m_Texture = {};
    }

    auto [inserted, b_inserted] = m_Thumbnails.emplace(key, thumbnail);
    return IsTextureValid(inserted->second.m_Texture) ? &inserted->second.m_Texture : nullptr;
}

void FileSystemPanel::DrawLevels(GameEditor* editor, MapManager* manager, const std::filesystem::path& asset_root)
{
    // A plain GameMap (for example a single rw::Level) is not managed by a
    // MapManager, but it still owns one scene file keyed by its map name.
    std::vector<std::string> level_ids;
    if (manager != nullptr)
    {
        level_ids = manager->GetAvailableMaps();
    }
    else if (GameMap* active_map = editor->GetActiveMap())
    {
        level_ids.push_back(active_map->GetMapName());
    }

    if (level_ids.empty())
    {
        ImGui::TextDisabled("No levels registered.");
        return;
    }

    const bool b_dirty = editor->GetScene().b_IsDirty();
    bool b_any_level = false;

    for (const std::string& map_id : level_ids)
    {
        if (!b_MatchesFilter(map_id, m_Filter))
        {
            continue;
        }
        b_any_level = true;

        const std::filesystem::path scene_path = asset_root / (Scene::ToFileKey(map_id) + ".scene");
        const bool b_scene_exists = std::filesystem::exists(scene_path);
        const bool b_current = (manager != nullptr) ? manager->b_IsCurrentMap(map_id) : true;

        std::string label = std::string(b_current ? ICON_FA_CIRCLE : ICON_FA_FILE_LINES) + " " + map_id;
        if (b_current && b_dirty)
        {
            label += " *";
        }

        ImGui::PushID(map_id.c_str());
        if (ImGui::Selectable(label.c_str(), b_current, ImGuiSelectableFlags_None,
                ImVec2(ImGui::GetContentRegionAvail().x, EditorWidgets::c_RowHeight)))
        {
            editor->m_SelectedMapId = map_id;
        }

        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && !b_current &&
            manager != nullptr)
        {
            editor->InvalidateEditorEntities();
            manager->b_GotoMap(map_id);
        }

        if (ImGui::BeginPopupContextItem("##level_context"))
        {
            if (ImGui::MenuItem("Open", nullptr, false, (manager != nullptr) && !b_current))
            {
                editor->InvalidateEditorEntities();
                manager->b_GotoMap(map_id);
            }
            if (ImGui::MenuItem("Save Scene", nullptr, false, b_current))
            {
                editor->GetGameEngine().b_SaveActiveScene();
            }
            if (ImGui::MenuItem("Reveal", nullptr, false, b_scene_exists))
            {
                EditorUtils::OpenInExplorer(scene_path.parent_path());
            }
            if (ImGui::MenuItem("Delete Scene", nullptr, false, b_scene_exists))
            {
                if (tinyfd_messageBox("Delete Scene", "Delete this level's scene file?", "yesno", "warning", 0) == 1)
                {
                    std::error_code error;
                    std::filesystem::remove(scene_path, error);
                    InvalidateFileCaches();
                    if (b_current)
                    {
                        editor->GetScene().Clear();
                    }
                }
            }
            ImGui::EndPopup();
        }

        ImGui::PopID();
    }

    if (!b_any_level)
    {
        ImGui::TextDisabled(level_ids.empty() ? "No levels registered." : "No levels match the filter.");
    }

    // Scene files with no matching registered map stay reachable from a
    // separate section instead of cluttering the level list.
    std::vector<std::filesystem::path> unmatched_scenes;
    for (const std::filesystem::path& file : GetCachedFiles(m_AssetFiles, asset_root, false))
    {
        if (!EditorAssets::b_IsScene(file.string()))
        {
            continue;
        }

        const std::string stem = file.stem().string();
        bool b_known_map = false;
        for (const std::string& map_id : level_ids)
        {
            if (Scene::ToFileKey(map_id) == stem)
            {
                b_known_map = true;
                break;
            }
        }

        if (b_known_map || !b_MatchesFilter(stem, m_Filter))
        {
            continue;
        }

        unmatched_scenes.push_back(file);
    }

    if (!unmatched_scenes.empty())
    {
        const std::string unmatched_header =
            std::string("Unmatched Scenes (") + std::to_string(unmatched_scenes.size()) + ")";
        if (ImGui::CollapsingHeader(unmatched_header.c_str()))
        {
            for (const std::filesystem::path& file : unmatched_scenes)
            {
                ImGui::PushID(file.string().c_str());
                ImGui::Selectable((std::string(ICON_FA_FILE_LINES) + " " + file.stem().string()).c_str(), false,
                    ImGuiSelectableFlags_None, ImVec2(ImGui::GetContentRegionAvail().x, EditorWidgets::c_RowHeight));
                if (ImGui::BeginPopupContextItem("##orphan_scene_context"))
                {
                    if (ImGui::MenuItem("Reveal"))
                    {
                        EditorUtils::OpenInExplorer(file.parent_path());
                    }
                    if (ImGui::MenuItem("Delete Scene"))
                    {
                        if (tinyfd_messageBox("Delete Scene", "Delete this scene file?", "yesno", "warning", 0) == 1)
                        {
                            std::error_code error;
                            std::filesystem::remove(file, error);
                            InvalidateFileCaches();
                        }
                    }
                    ImGui::EndPopup();
                }
                ImGui::PopID();
            }
        }
    }
}

void FileSystemPanel::DrawAssets(GameEditor* editor, const std::filesystem::path& asset_root)
{
    bool b_any = false;

    for (const std::filesystem::path& file : GetCachedFiles(m_AssetFiles, asset_root, false))
    {
        if (EditorAssets::b_IsScene(file.string()) || !b_MatchesFilter(file.filename().string(), m_Filter))
        {
            continue;
        }
        b_any = true;

        const std::string path_key = file.string();
        ImGui::PushID(path_key.c_str());

        if (EditorAssets::b_IsImage(file.string()))
        {
            if (Texture2D* thumbnail = GetThumbnail(file))
            {
                const float thumb_size = EditorWidgets::c_RowIconWidth;
                const ImVec2 thumb_min = ImGui::GetCursorScreenPos();
                ImGui::Dummy(ImVec2(thumb_size, thumb_size));
                ImGui::GetWindowDrawList()->AddImageRounded(static_cast<ImTextureID>(thumbnail->id), thumb_min,
                    ImVec2(thumb_min.x + thumb_size, thumb_min.y + thumb_size), ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f),
                    IM_COL32_WHITE, 2.0f);
            }
            else
            {
                EditorWidgets::RowIcon(ICON_FA_IMAGE);
            }
        }
        else
        {
            EditorWidgets::RowIcon(IconForPath(file));
        }
        ImGui::SameLine(0.0f, 6.0f);

        const std::string filename = file.filename().string();
        if (m_RenamingAssetPath == path_key)
        {
            if (m_bRenameFocusPending)
            {
                ImGui::SetKeyboardFocusHere();
                m_bRenameFocusPending = false;
            }
            ImGui::SetNextItemWidth(-1.0f);
            const bool b_commit = ImGui::InputText("##asset_rename", &m_RenameBuffer,
                ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
            if (b_commit)
            {
                CommitRename(editor, file);
            }
            else if (ImGui::IsItemDeactivated())
            {
                m_RenamingAssetPath.clear();
            }
            ImGui::PopID();
            continue;
        }

        ImGui::Selectable(filename.c_str(), false, ImGuiSelectableFlags_None,
            ImVec2(ImGui::GetContentRegionAvail().x, EditorWidgets::c_RowHeight));

        if (ImGui::IsItemHovered())
        {
            const std::string relative = EditorAssets::ToRelativeAssetPath(file);
            std::error_code size_error;
            const auto file_size = std::filesystem::file_size(file, size_error);
            if (size_error)
            {
                ImGui::SetTooltip("%s", relative.c_str());
            }
            else
            {
                ImGui::SetTooltip("%s\n%.1f KB", relative.c_str(), static_cast<double>(file_size) / 1024.0);
            }
        }

        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) &&
            EditorAssets::b_IsImage(file.string()))
        {
            AssignTexture(editor, file);
        }

        if (ImGui::BeginDragDropSource())
        {
            const std::string relative = EditorAssets::ToRelativeAssetPath(file);
            ImGui::SetDragDropPayload("RAYWAVES_ASSET", relative.c_str(), relative.size() + 1);
            ImGui::Text("%s", file.filename().string().c_str());
            ImGui::EndDragDropSource();
        }

        if (ImGui::BeginPopupContextItem("##asset_context"))
        {
            if (EditorAssets::b_IsImage(file.string()))
            {
                if (ImGui::MenuItem("Assign to Selected", nullptr, false,
                        editor->m_SelectionKind == GameEditor::EEditorSelectionKind::SceneObject))
                {
                    AssignTexture(editor, file);
                }
                if (ImGui::MenuItem("Create Sprite"))
                {
                    const std::string relative = EditorAssets::ToRelativeAssetPath(file);
                    CreateSprite(editor, relative, editor->GetViewportCenterWorld());
                }
            }
            if (ImGui::MenuItem("Rename"))
            {
                StartRename(file);
            }
            if (ImGui::MenuItem("Reveal"))
            {
                EditorUtils::OpenInExplorer(file.parent_path());
            }
            if (ImGui::MenuItem("Delete"))
            {
                if (tinyfd_messageBox("Delete Asset", "Delete this file from Assets?", "yesno", "warning", 0) == 1)
                {
                    std::error_code error;
                    std::filesystem::remove(file, error);
                    if (m_RenamingAssetPath == path_key)
                    {
                        m_RenamingAssetPath.clear();
                        m_bRenameFocusPending = false;
                    }
                    UnloadThumbnails();
                }
            }
            ImGui::EndPopup();
        }

        ImGui::PopID();
    }

    if (!b_any)
    {
        ImGui::TextDisabled(m_Filter.empty() ? "No assets yet." : "No matching assets.");
    }
}

void FileSystemPanel::StartRename(const std::filesystem::path& asset)
{
    m_RenamingAssetPath = asset.string();
    m_RenameBuffer = asset.filename().string();
    m_bRenameFocusPending = true;
}

void FileSystemPanel::CommitRename(GameEditor* editor, const std::filesystem::path& asset)
{
    m_RenamingAssetPath.clear();
    m_bRenameFocusPending = false;

    const std::string old_relative = EditorAssets::ToRelativeAssetPath(asset);
    if (old_relative.empty())
    {
        editor->GetTerminal().add_text(
            "Rename failed: asset is outside the project's Assets folder.", term::Severity::Error);
        return;
    }

    std::filesystem::path new_path;
    std::string error;
    if (!EditorAssets::b_RenameAsset(asset, m_RenameBuffer, new_path, error))
    {
        editor->GetTerminal().add_text("Rename failed: " + error, term::Severity::Error);
        tinyfd_messageBox("Rename Asset", error.c_str(), "ok", "error", 0);
        return;
    }

    const std::string new_relative = EditorAssets::ToRelativeAssetPath(new_path);
    if (new_relative.empty() || new_relative == old_relative)
    {
        UnloadThumbnails();
        return;
    }

    // The scene stores relative texture paths, so a rename has to retarget every
    // object that referenced the old path before geometry can reload.
    Scene& scene = editor->GetScene();
    const std::string old_key = NormalizeAssetKey(old_relative);
    int updated_count = 0;
    for (t_SceneObject& object : scene.GetObjects())
    {
        if (NormalizeAssetKey(object.m_TexturePath) != old_key)
        {
            continue;
        }
        object.m_TexturePath = new_relative;
        scene.ReloadTexture(object.m_Id);
        ++updated_count;
    }

    UnloadThumbnails();

    std::string message = "Renamed '" + old_relative + "' -> '" + new_relative + "'.";
    if (updated_count > 0)
    {
        message += " Updated " + std::to_string(updated_count) + " scene object(s).";
    }
    if (asset.extension() != new_path.extension())
    {
        message += " The extension changed.";
    }
    message += " GameLogic code references are not updated.";
    editor->GetTerminal().add_text(message, term::Severity::Warn);
}

void FileSystemPanel::RefreshMissingAssets(GameEditor* editor, const std::filesystem::path& asset_root)
{
    m_MissingAssets.clear();
    m_MissingScanRoot = asset_root;
    m_MissingScanFrame = m_FrameCounter;
    m_bMissingScanValid = true;

    if (asset_root.empty())
    {
        return;
    }

    for (const t_SceneObject& object : editor->GetScene().GetObjects())
    {
        if (object.m_TexturePath.empty())
        {
            continue;
        }

        std::error_code error;
        const std::filesystem::path candidate =
            asset_root / std::filesystem::path(NormalizeAssetKey(object.m_TexturePath));
        if (!std::filesystem::is_regular_file(candidate, error))
        {
            m_MissingAssets.push_back(
                { object.m_Id, object.m_Name.empty() ? ("Object " + std::to_string(object.m_Id)) : object.m_Name,
                    object.m_TexturePath });
        }
    }
}

void FileSystemPanel::DrawMissingAssets(GameEditor* editor, const std::filesystem::path& asset_root)
{
    if (!m_bMissingScanValid || m_MissingScanRoot != asset_root ||
        m_FrameCounter - m_MissingScanFrame >= c_MissingScanIntervalFrames)
    {
        RefreshMissingAssets(editor, asset_root);
    }

    const size_t missing_count = m_MissingAssets.size();
    const std::string header = std::string(ICON_FA_TRIANGLE_EXCLAMATION) + " Missing Assets (" +
        std::to_string(missing_count) + ")###missing_assets";
    const ImGuiTreeNodeFlags header_flags =
        missing_count == 0 ? ImGuiTreeNodeFlags_None : ImGuiTreeNodeFlags_DefaultOpen;
    if (!ImGui::CollapsingHeader(header.c_str(), header_flags))
    {
        return;
    }

    if (asset_root.empty())
    {
        ImGui::TextDisabled("No asset folder is configured for this project.");
        return;
    }

    if (missing_count == 0)
    {
        ImGui::TextDisabled("Every scene texture path resolves.");
        return;
    }

    ImGui::TextDisabled("Current scene references only; source code is not scanned.");
    if (ImGui::SmallButton(ICON_FA_FOLDER_OPEN " Reveal Assets"))
    {
        EditorUtils::OpenInExplorer(asset_root);
    }

    // A resolved entry is removed from the snapshot immediately so the row does
    // not linger until the next periodic rescan. IDs are looked up fresh each
    // action: a target that disappeared since the snapshot is skipped safely.
    size_t resolved_index = m_MissingAssets.size();
    for (size_t index = 0; index < m_MissingAssets.size(); ++index)
    {
        const FMissingAssetEntry& entry = m_MissingAssets[index];
        ImGui::PushID(static_cast<int>(entry.m_ObjectId));
        ImGui::Text("%s: %s", entry.m_ObjectName.c_str(), entry.m_Path.c_str());

        if (ImGui::SmallButton(ICON_FA_MAGNIFYING_GLASS " Locate..."))
        {
            const char* selected = tinyfd_openFileDialog("Locate Missing Asset", asset_root.string().c_str(),
                c_ImageFilterCount, ImageFileFilters(), "Image files", 0);
            if (selected != nullptr)
            {
                std::string relative;
                if (EditorAssets::b_ImportFile(selected, relative))
                {
                    if (t_SceneObject* object = editor->GetScene().FindObject(entry.m_ObjectId))
                    {
                        object->m_TexturePath = relative;
                        editor->GetScene().ReloadTexture(object->m_Id);
                    }
                    UnloadThumbnails();
                    resolved_index = index;
                }
                else
                {
                    editor->GetTerminal().add_text(
                        std::string("Failed to import located asset: ") + selected, term::Severity::Error);
                }
            }
        }

        ImGui::SameLine();
        if (ImGui::SmallButton(ICON_FA_ERASER " Clear"))
        {
            if (t_SceneObject* object = editor->GetScene().FindObject(entry.m_ObjectId))
            {
                object->m_TexturePath.clear();
                editor->GetScene().ReloadTexture(object->m_Id);
                resolved_index = index;
            }
        }
        ImGui::PopID();
    }

    if (resolved_index < m_MissingAssets.size())
    {
        m_MissingAssets.erase(m_MissingAssets.begin() + static_cast<std::ptrdiff_t>(resolved_index));
    }
}

void FileSystemPanel::HandleFileDrop(GameEditor* editor)
{
    // Raylib holds dropped paths until UnloadDroppedFiles() clears them, and this
    // panel is the only consumer. Drops queued while the panel is hidden or
    // collapsed are therefore left untouched: Draw() is not reached then, so the
    // FileSystem panel is the editor's drop target by construction.
    if (!IsFileDropped())
    {
        return;
    }

    const FilePathList dropped = LoadDroppedFiles();
    const unsigned int dropped_count = dropped.count;
    std::vector<std::string> imported;

    for (unsigned int index = 0; index < dropped_count; ++index)
    {
        const char* raw = (dropped.paths != nullptr) ? dropped.paths[index] : nullptr;
        if (raw == nullptr)
        {
            continue;
        }

        const std::filesystem::path source(raw);
        std::error_code error;
        if (!std::filesystem::is_regular_file(source, error))
        {
            editor->GetTerminal().add_text(
                "Skipped dropped path (not a file): " + source.string(), term::Severity::Warn);
            continue;
        }

        std::string relative;
        if (!EditorAssets::b_ImportFile(source, relative))
        {
            editor->GetTerminal().add_text("Failed to import dropped file: " + source.string(), term::Severity::Error);
            continue;
        }
        imported.push_back(relative);
    }

    UnloadDroppedFiles(dropped);

    if (imported.empty())
    {
        return;
    }

    std::string summary = "Imported " + std::to_string(imported.size()) + " dropped asset(s):";
    for (const std::string& relative : imported)
    {
        summary += ' ';
        summary += relative;
    }
    editor->GetTerminal().add_text(summary, term::Severity::Debug);
    UnloadThumbnails();

    if (dropped_count != 1 || imported.size() != 1 || !EditorAssets::b_IsImage(imported.front()))
    {
        return;
    }

    // A single image is applied to the selection when one exists; otherwise it
    // becomes a sprite the user can see and move.
    if (editor->m_SelectionKind == GameEditor::EEditorSelectionKind::SceneObject)
    {
        AssignTextureRelative(editor, imported.front());
    }
    else
    {
        CreateSprite(editor, imported.front(), editor->GetViewportCenterWorld());
    }
}

void FileSystemPanel::DrawCode(const std::filesystem::path& source_root)
{
    bool b_any = false;
    for (const std::filesystem::path& file :
        GetCachedFiles(m_SourceFiles, source_root, true, c_SourceCacheRefreshFrames))
    {
        std::error_code error;
        const std::filesystem::path relative = std::filesystem::relative(file, source_root, error);
        const std::string name = error ? file.filename().string() : relative.generic_string();
        if (!b_MatchesFilter(name, m_Filter))
        {
            continue;
        }
        b_any = true;

        ImGui::PushID(file.string().c_str());
        EditorWidgets::RowIcon(IconForPath(file));
        ImGui::SameLine(0.0f, 6.0f);
        ImGui::Selectable(name.c_str(), false, ImGuiSelectableFlags_None,
            ImVec2(ImGui::GetContentRegionAvail().x, EditorWidgets::c_RowHeight));

        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        {
            EditorUtils::OpenInDefaultApp(file);
        }

        if (ImGui::BeginPopupContextItem("##code_context"))
        {
            if (ImGui::MenuItem("Open"))
            {
                EditorUtils::OpenInDefaultApp(file);
            }
            if (ImGui::MenuItem("Reveal"))
            {
                EditorUtils::OpenInExplorer(file.parent_path());
            }
            ImGui::EndPopup();
        }
        ImGui::PopID();
    }

    if (!b_any)
    {
        ImGui::TextDisabled(m_Filter.empty() ? "No source files yet." : "No source files match the filter.");
    }
}

void FileSystemPanel::Draw(GameEditor* editor)
{
    SCOPED_TIMER("panel_file_system");
    ++m_FrameCounter;
    if (!ImGui::Begin(ICON_FA_FOLDER_TREE " FileSystem", &editor->m_bShowFileSystem))
    {
        ImGui::End();
        return;
    }

    if (!ProjectManager::b_HasOpenProject())
    {
        ImGui::TextDisabled("Open a project to browse its files.");
        ImGui::End();
        return;
    }

    // The panel window is open, so it owns OS drops for this frame. A collapsed
    // window returns false from Begin() above and drops stay queued.
    HandleFileDrop(editor);

    const t_Project project = ProjectManager::GetCurrentCopy();
    const std::filesystem::path asset_root = project.m_AssetPath;
    const std::filesystem::path source_root = project.m_SourcePath;

    if (m_LastAssetRoot != asset_root.string())
    {
        UnloadThumbnails();
        m_LastAssetRoot = asset_root.string();
    }

    MapManager* manager = editor->GetMapManager();

    if (EditorWidgets::IconButton("fs_import", ICON_FA_FILE_IMPORT, "Import asset", ImVec2(28.0f, 24.0f)))
    {
        const char* filters[] = { "*.png", "*.jpg", "*.jpeg", "*.bmp", "*.tga", "*.gif", "*.qoi", "*.psd", "*.wav",
            "*.ogg", "*.mp3", "*.flac", "*.ttf", "*.otf", "*.fnt" };
        const char* selected =
            tinyfd_openFileDialog("Import Asset", asset_root.string().c_str(), 15, filters, "Assets", 0);
        if (selected != nullptr)
        {
            std::string relative;
            if (EditorAssets::b_ImportFile(selected, relative))
            {
                UnloadThumbnails();
            }
        }
    }

    ImGui::SameLine();
    if (EditorWidgets::IconButton("fs_open_folder", ICON_FA_FOLDER_OPEN, "Open asset folder", ImVec2(28.0f, 24.0f)))
    {
        EditorUtils::OpenInExplorer(asset_root);
    }

    ImGui::SameLine();
    EditorWidgets::ClearableFilter("fs_filter", m_Filter);

    if (ImGui::BeginTabBar("##fs_tabs"))
    {
        if (ImGui::BeginTabItem(ICON_FA_FILE_LINES " Levels"))
        {
            ImGui::Spacing();
            DrawLevels(editor, manager, asset_root);
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem(ICON_FA_IMAGES " Assets"))
        {
            ImGui::Spacing();
            DrawAssets(editor, asset_root);
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem(ICON_FA_CODE " Code"))
        {
            ImGui::Spacing();
            DrawCode(source_root);
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::Separator();
    DrawMissingAssets(editor, asset_root);

    ImGui::End();
}
