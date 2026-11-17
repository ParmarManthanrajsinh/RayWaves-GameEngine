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
#include <imgui.h>
#include <tinyfiledialogs.h>

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

void AssignTexture(GameEditor* editor, const std::filesystem::path& path)
{
    const std::string relative = EditorAssets::ToRelativeAssetPath(path);
    if (relative.empty())
    {
        return;
    }

    if (editor->m_SelectionKind == GameEditor::EEditorSelectionKind::SceneObject)
    {
        if (t_SceneObject* object = editor->GetSelectedSceneObject())
        {
            object->m_TexturePath = relative;
            editor->GetScene().ReloadTexture(object->m_Id);
        }
        return;
    }

    if (editor->m_SelectionKind == GameEditor::EEditorSelectionKind::None)
    {
        CreateSprite(editor, relative, editor->GetViewportCenterWorld());
    }
}
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
    for (const std::filesystem::path& file : EditorAssets::ListFiles(asset_root, false))
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

    for (const std::filesystem::path& file : EditorAssets::ListFiles(asset_root, false))
    {
        if (EditorAssets::b_IsScene(file.string()) || !b_MatchesFilter(file.filename().string(), m_Filter))
        {
            continue;
        }
        b_any = true;

        ImGui::PushID(file.string().c_str());

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

void FileSystemPanel::DrawCode(const std::filesystem::path& source_root)
{
    bool b_any = false;
    for (const std::filesystem::path& file : EditorAssets::ListFiles(source_root, true))
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

    ImGui::End();
}
