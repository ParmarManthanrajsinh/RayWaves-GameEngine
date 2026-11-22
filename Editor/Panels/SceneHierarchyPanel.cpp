#include "SceneHierarchyPanel.h"
#include "../../Engine/MapManager.h"
#include "../../Engine/Profiler.h"
#include "../../Engine/ProjectManager.h"
#include "../EditorWidgets.h"
#include "../GameEditor.h"
#include "../SceneCommands.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <imgui.h>
#include <initializer_list>
#include <memory>
#include <string>
#include <string_view>
#include <tinyfiledialogs.h>
#include <vector>

namespace
{
struct FHierarchyItem
{
    GameEditor::EEditorSelectionKind m_Kind = GameEditor::EEditorSelectionKind::None;
    uint32_t m_SceneId = 0;
    uint64_t m_EntityId = 0;
};

bool b_IsSelected(GameEditor* editor, const FHierarchyItem& item)
{
    if (item.m_Kind == GameEditor::EEditorSelectionKind::SceneObject)
    {
        return editor->b_IsSceneObjectSelected(item.m_SceneId);
    }
    if (item.m_Kind == GameEditor::EEditorSelectionKind::GameplayEntity)
    {
        return editor->b_IsEditorEntitySelected(item.m_EntityId);
    }
    return false;
}

void SelectItem(GameEditor* editor, const FHierarchyItem& item)
{
    if (item.m_Kind == GameEditor::EEditorSelectionKind::SceneObject)
    {
        editor->SelectSceneObject(item.m_SceneId);
    }
    else if (item.m_Kind == GameEditor::EEditorSelectionKind::GameplayEntity)
    {
        editor->SelectEditorEntity(item.m_EntityId);
    }
}

// Ctrl toggles membership, Shift extends the set. A plain click replaces it.
void SelectClickedItem(GameEditor* editor, const FHierarchyItem& item)
{
    const ImGuiIO& io = ImGui::GetIO();
    if (item.m_Kind == GameEditor::EEditorSelectionKind::SceneObject)
    {
        if (io.KeyCtrl)
        {
            editor->ToggleSceneObjectSelection(item.m_SceneId);
        }
        else if (io.KeyShift)
        {
            editor->AddSceneObjectToSelection(item.m_SceneId);
        }
        else
        {
            editor->SelectSceneObject(item.m_SceneId);
        }
    }
    else if (item.m_Kind == GameEditor::EEditorSelectionKind::GameplayEntity)
    {
        if (io.KeyCtrl)
        {
            editor->ToggleEditorEntitySelection(item.m_EntityId);
        }
        else if (io.KeyShift)
        {
            editor->AddEditorEntityToSelection(item.m_EntityId);
        }
        else
        {
            editor->SelectEditorEntity(item.m_EntityId);
        }
    }
}

GameEditor::FEditorEntityRef* FindEntity(GameEditor* editor, uint64_t id)
{
    for (GameEditor::FEditorEntityRef& entity : editor->GetEditorEntities())
    {
        if (entity.m_Id == id)
        {
            return &entity;
        }
    }
    return nullptr;
}

// If the clicked row is part of a multi-selection, act on the whole set.
std::vector<uint32_t> AffectedSceneIds(GameEditor* editor, uint32_t clicked_id)
{
    if (editor->m_SelectionKind == GameEditor::EEditorSelectionKind::SceneObject &&
        editor->b_IsSceneObjectSelected(clicked_id) && editor->GetSelectionCount() > 1)
    {
        return editor->GetSelectedSceneObjectIds();
    }
    return { clicked_id };
}

std::string ToLower(std::string_view text)
{
    std::string lowered(text);
    std::ranges::transform(
        lowered, lowered.begin(), [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
    return lowered;
}

// Query is expected to be lower case already; an empty query matches everything
// so callers can skip the search special case.
bool b_MatchesSearch(std::string_view query, std::initializer_list<std::string_view> fields)
{
    if (query.empty())
    {
        return true;
    }

    for (const std::string_view field : fields)
    {
        if (ToLower(field).find(query) != std::string::npos)
        {
            return true;
        }
    }
    return false;
}
} // namespace

void SceneHierarchyPanel::Draw(GameEditor* editor)
{
    SCOPED_TIMER("panel_scene_hierarchy");
    if (!ImGui::Begin(ICON_FA_SITEMAP " Scene", &editor->m_bShowHierarchy))
    {
        ImGui::End();
        return;
    }

    if (!ProjectManager::b_HasOpenProject())
    {
        ImGui::TextDisabled("Open a project to edit its scene.");
        ImGui::End();
        return;
    }

    Scene& scene = editor->GetScene();
    MapManager* manager = editor->GetMapManager();

    if (editor->m_SelectedSceneObjectId != m_LastSceneId || editor->m_SelectedEditorEntityId != m_LastEntityId ||
        editor->GetSelectionCount() != m_LastSelectionCount)
    {
        m_LastSceneId = editor->m_SelectedSceneObjectId;
        m_LastEntityId = editor->m_SelectedEditorEntityId;
        m_LastSelectionCount = editor->GetSelectionCount();
        m_bScrollToSelection = true;
    }

    std::string current_map;
    if (manager != nullptr)
    {
        current_map = std::string(manager->GetCurrentMapId());
    }
    else if (editor->GetActiveMap() != nullptr)
    {
        current_map = editor->GetActiveMap()->GetMapName();
    }

    std::string map_label = current_map.empty() ? "(no map)" : current_map;
    if (scene.b_IsDirty())
    {
        map_label += " *";
    }

    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::BeginCombo("##scene_map", map_label.c_str()))
    {
        if (manager != nullptr)
        {
            for (const std::string& map_id : manager->GetAvailableMaps())
            {
                const bool b_current = manager->b_IsCurrentMap(map_id);
                if (ImGui::Selectable(map_id.c_str(), b_current) && !b_current)
                {
                    editor->InvalidateEditorEntities();
                    manager->b_GotoMap(map_id);
                }
            }
        }
        else
        {
            ImGui::Selectable(map_label.c_str(), true);
        }
        ImGui::EndCombo();
    }

    ImGui::Separator();

    // Fixed above the scrolling list so the filter stays reachable in deep
    // hierarchies.
    EditorWidgets::ClearableFilter("hierarchy_search", m_Search, "Search name or type...");
    ImGui::Separator();

    ImGui::BeginChild("##hierarchy_objects", ImVec2(0, 0), ImGuiChildFlags_None);

    // Empty space doubles as the add surface, so the header stays free of
    // toolbar buttons.
    if (ImGui::BeginPopupContextWindow(
            "##scene_hierarchy_context", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
    {
        if (ImGui::MenuItem(ICON_FA_PLUS " Add Object"))
        {
            auto command = std::make_unique<SceneCommands::SceneObjectAddCommand>(
                scene, "Object", editor->GetViewportCenterWorld());
            auto* raw = command.get();
            editor->GetCommandStack().Push(std::move(command));
            editor->SelectSceneObject(raw->Id());
        }
        ImGui::EndPopup();
    }

    std::vector<FHierarchyItem> visible_items;
    bool b_selected_drawn = false;

    const auto start_rename = [this](const t_SceneObject& object)
    {
        m_RenamingSceneId = object.m_Id;
        m_RenameOriginal = object.m_Name;
        std::snprintf(m_RenameBuffer, sizeof(m_RenameBuffer), "%s", object.m_Name.c_str());
    };

    const auto finish_rename = [this, &scene, editor](t_SceneObject& object)
    {
        if (m_RenameBuffer[0] != '\0' && m_RenameOriginal != m_RenameBuffer)
        {
            editor->GetCommandStack().Push(std::make_unique<SceneCommands::SceneObjectFieldCommand>(scene, object.m_Id,
                SceneCommands::ESceneObjectField::Name, SceneCommands::FSceneObjectFieldValue::Text(m_RenameOriginal),
                SceneCommands::FSceneObjectFieldValue::Text(m_RenameBuffer), "Rename Object"));
        }
        m_RenamingSceneId = 0;
        m_RenameOriginal.clear();
    };

    const std::string query = ToLower(m_Search);
    const bool b_searching = !query.empty();

    std::vector<const GameEditor::FEditorEntityRef*> filtered_entities;
    for (const GameEditor::FEditorEntityRef& entity : editor->GetEditorEntities())
    {
        if (b_MatchesSearch(query, { entity.m_Name, entity.m_Type }))
        {
            filtered_entities.push_back(&entity);
        }
    }

    std::vector<t_SceneObject*> filtered_objects;
    for (t_SceneObject& object : scene.GetObjects())
    {
        if (b_MatchesSearch(query, { object.m_Name, object.m_Type }))
        {
            filtered_objects.push_back(&object);
        }
    }

    const auto draw_entity_row = [&](const GameEditor::FEditorEntityRef& entity)
    {
        ImGui::PushID(static_cast<int>(entity.m_Id));

        if (entity.m_Visible != nullptr)
        {
            if (EditorWidgets::RowIconButton("visibility", *entity.m_Visible ? ICON_FA_EYE : ICON_FA_EYE_SLASH,
                    "Toggle visibility", !*entity.m_Visible))
            {
                const bool b_after = !*entity.m_Visible;
                std::vector<uint64_t> ids{ entity.m_Id };
                if (editor->m_SelectionKind == GameEditor::EEditorSelectionKind::GameplayEntity &&
                    editor->b_IsEditorEntitySelected(entity.m_Id) && editor->GetSelectionCount() > 1)
                {
                    ids = editor->GetSelectedEditorEntityIds();
                }

                std::vector<SceneCommands::FEntityTransformEntry> entries;
                entries.reserve(ids.size());
                for (const uint64_t id : ids)
                {
                    GameEditor::FEditorEntityRef* target = FindEntity(editor, id);
                    if (target == nullptr || target->m_Visible == nullptr)
                    {
                        continue;
                    }

                    SceneCommands::FEntityTransformEntry entry;
                    entry.m_Visible = target->m_Visible;
                    entry.m_Before.m_bVisible = *target->m_Visible;
                    entry.m_After.m_bVisible = b_after;
                    entries.push_back(entry);
                }
                if (!entries.empty())
                {
                    editor->GetCommandStack().Push(std::make_unique<SceneCommands::EntityTransformCommand>(
                        std::move(entries), "Change Visibility"));
                }
            }
        }
        else
        {
            EditorWidgets::RowIconGutter();
        }
        ImGui::SameLine(0.0f, 4.0f);

        std::string label = std::string(ICON_FA_GAMEPAD) + " " + entity.m_Name;
        if (entity.m_Type != entity.m_Name)
        {
            label += "  (" + entity.m_Type + ")";
        }

        const FHierarchyItem item{ GameEditor::EEditorSelectionKind::GameplayEntity, 0, entity.m_Id };
        const bool b_selected = b_IsSelected(editor, item);
        if (ImGui::Selectable(label.c_str(), b_selected, ImGuiSelectableFlags_AllowDoubleClick,
                ImVec2(ImGui::GetContentRegionAvail().x, EditorWidgets::c_RowHeight)))
        {
            SelectClickedItem(editor, item);
        }
        if (b_selected && m_bScrollToSelection)
        {
            ImGui::SetScrollHereY(0.5f);
            m_bScrollToSelection = false;
            b_selected_drawn = true;
        }

        if (ImGui::IsItemHovered())
        {
            if (entity.m_Position != nullptr)
            {
                ImGui::SetTooltip(
                    "%s\nposition %.1f, %.1f", entity.m_Type.c_str(), entity.m_Position->x, entity.m_Position->y);
            }
            else if (entity.m_Position3D != nullptr)
            {
                ImGui::SetTooltip("%s\nposition %.1f, %.1f, %.1f", entity.m_Type.c_str(), entity.m_Position3D->x,
                    entity.m_Position3D->y, entity.m_Position3D->z);
            }
        }

        visible_items.push_back(item);
        ImGui::PopID();
    };

    // Returns true when the object list changed and the caller must stop
    // iterating its cached pointers.
    const auto draw_object_row = [&](t_SceneObject& object) -> bool
    {
        ImGui::PushID(static_cast<int>(object.m_Id));

        if (EditorWidgets::RowIconButton("visibility", object.m_bVisible ? ICON_FA_EYE : ICON_FA_EYE_SLASH,
                "Toggle visibility", !object.m_bVisible))
        {
            editor->GetCommandStack().Push(std::make_unique<SceneCommands::SceneObjectsVisibleCommand>(
                scene, AffectedSceneIds(editor, object.m_Id), !object.m_bVisible));
        }
        ImGui::SameLine(0.0f, 4.0f);

        const FHierarchyItem item{ GameEditor::EEditorSelectionKind::SceneObject, object.m_Id, 0 };
        const bool b_selected = b_IsSelected(editor, item);

        if (m_RenamingSceneId == object.m_Id)
        {
            ImGui::SetNextItemWidth(-1.0f);
            const bool b_commit = ImGui::InputText("##rename", m_RenameBuffer, sizeof(m_RenameBuffer),
                ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
            if (b_commit || ImGui::IsItemDeactivated())
            {
                finish_rename(object);
            }
        }
        else
        {
            std::string label =
                std::string(ICON_FA_IMAGE) + " " + (object.m_Name.empty() ? std::string("Object") : object.m_Name);
            if (!object.m_Type.empty())
            {
                label += "  (" + object.m_Type + ")";
            }
            if (ImGui::Selectable(label.c_str(), b_selected, ImGuiSelectableFlags_AllowDoubleClick,
                    ImVec2(ImGui::GetContentRegionAvail().x, EditorWidgets::c_RowHeight)))
            {
                SelectClickedItem(editor, item);
            }
            if (b_selected && m_bScrollToSelection)
            {
                ImGui::SetScrollHereY(0.5f);
                m_bScrollToSelection = false;
                b_selected_drawn = true;
            }
            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            {
                start_rename(object);
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("layer %d\n%s", object.m_Layer,
                    object.m_TexturePath.empty() ? "(no texture)" : object.m_TexturePath.c_str());
            }
        }

        bool b_list_changed = false;
        if (ImGui::BeginPopupContextItem("##scene_object_context"))
        {
            if (ImGui::MenuItem("Rename"))
            {
                start_rename(object);
            }
            if (ImGui::MenuItem("Duplicate"))
            {
                auto command = std::make_unique<SceneCommands::SceneObjectDuplicateCommand>(
                    scene, AffectedSceneIds(editor, object.m_Id));
                auto* raw = command.get();
                editor->GetCommandStack().Push(std::move(command));
                editor->SelectSceneObjects(raw->CreatedIds());
                // Inserting invalidates the cached object pointers.
                b_list_changed = true;
            }
            if (ImGui::MenuItem("Delete"))
            {
                const std::vector<uint32_t> ids = AffectedSceneIds(editor, object.m_Id);
                const std::string prompt = ids.size() > 1
                    ? "Delete the " + std::to_string(ids.size()) + " selected scene objects?"
                    : std::string("Delete the selected scene object?");
                if (tinyfd_messageBox("Delete Object", prompt.c_str(), "yesno", "warning", 0) == 1)
                {
                    editor->GetCommandStack().Push(
                        std::make_unique<SceneCommands::SceneObjectRemoveCommand>(scene, ids));
                    editor->ClearSelection();
                    b_list_changed = true;
                }
            }
            ImGui::EndPopup();
        }

        if (b_list_changed)
        {
            ImGui::PopID();
            return true;
        }

        visible_items.push_back(item);
        ImGui::PopID();
        return false;
    };

    // Groups the editor objects by layer. The eye toggles the whole layer as one
    // undo step. Returns true when a row changed the object list.
    const auto draw_layer_group = [&](int layer) -> bool
    {
        ImGui::PushID(layer);

        std::vector<uint32_t> layer_ids;
        bool b_any_visible = false;
        for (const t_SceneObject* object : filtered_objects)
        {
            if (object->m_Layer != layer)
            {
                continue;
            }
            layer_ids.push_back(object->m_Id);
            b_any_visible = b_any_visible || object->m_bVisible;
        }

        if (EditorWidgets::RowIconButton("layer_visibility", b_any_visible ? ICON_FA_EYE : ICON_FA_EYE_SLASH,
                "Toggle layer visibility", !b_any_visible))
        {
            editor->GetCommandStack().Push(std::make_unique<SceneCommands::SceneObjectsVisibleCommand>(
                scene, layer_ids, !b_any_visible, "Change Layer Visibility"));
        }
        ImGui::SameLine(0.0f, 4.0f);

        const std::string label = std::string(ICON_FA_LAYER_GROUP) + " Layer " + std::to_string(layer) + "###layer";
        const bool b_open =
            ImGui::TreeNodeEx(label.c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth);
        ImGui::SameLine();
        ImGui::TextDisabled("(%d)", static_cast<int>(layer_ids.size()));

        bool b_list_changed = false;
        if (b_open)
        {
            for (t_SceneObject* object : filtered_objects)
            {
                if (object->m_Layer != layer)
                {
                    continue;
                }
                if (draw_object_row(*object))
                {
                    b_list_changed = true;
                    break;
                }
            }
            ImGui::TreePop();
        }

        ImGui::PopID();
        return b_list_changed;
    };

    if (b_searching && filtered_entities.empty() && filtered_objects.empty())
    {
        ImGui::Spacing();
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextDisabled("No matches for \"%s\".", m_Search.c_str());
        ImGui::PopTextWrapPos();
    }
    else
    {
        // A search hides empty groups so the result list stays short.
        if (!b_searching || !filtered_entities.empty())
        {
            const std::string gameplay_group = std::string(ICON_FA_GAMEPAD) + " Gameplay###gameplay_group";
            const bool b_gameplay_open = ImGui::TreeNodeEx(
                gameplay_group.c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth);
            EditorWidgets::RowCountBadge(static_cast<int>(filtered_entities.size()));
            if (b_gameplay_open)
            {
                if (filtered_entities.empty())
                {
                    ImGui::PushTextWrapPos(0.0f);
                    ImGui::TextDisabled("No gameplay entities.");
                    ImGui::PopTextWrapPos();
                }

                for (const GameEditor::FEditorEntityRef* entity : filtered_entities)
                {
                    draw_entity_row(*entity);
                }
                ImGui::TreePop();
            }
        }

        if (!b_searching || !filtered_objects.empty())
        {
            const std::string editor_objects_group =
                std::string(ICON_FA_IMAGE) + " Editor Objects###editor_objects_group";
            const bool b_editor_objects_open = ImGui::TreeNodeEx(
                editor_objects_group.c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth);
            EditorWidgets::RowCountBadge(static_cast<int>(filtered_objects.size()));

            if (b_editor_objects_open)
            {
                if (filtered_objects.empty())
                {
                    ImGui::PushTextWrapPos(0.0f);
                    ImGui::TextDisabled("No editor objects. Drag a sprite here or right-click to add one.");
                    ImGui::PopTextWrapPos();
                }

                if (b_searching)
                {
                    for (t_SceneObject* object : filtered_objects)
                    {
                        if (draw_object_row(*object))
                        {
                            break;
                        }
                    }
                }
                else
                {
                    std::vector<int> layers;
                    layers.reserve(filtered_objects.size());
                    for (const t_SceneObject* object : filtered_objects)
                    {
                        layers.push_back(object->m_Layer);
                    }
                    std::ranges::sort(layers);
                    layers.erase(std::unique(layers.begin(), layers.end()), layers.end());

                    for (const int layer : layers)
                    {
                        if (draw_layer_group(layer))
                        {
                            break;
                        }
                    }
                }
                ImGui::TreePop();
            }
        }
    }

    if (!b_selected_drawn)
    {
        m_bScrollToSelection = false;
    }

    if (!ImGui::GetIO().WantTextInput && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
        !visible_items.empty())
    {
        const bool b_up = ImGui::IsKeyPressed(ImGuiKey_UpArrow);
        const bool b_down = ImGui::IsKeyPressed(ImGuiKey_DownArrow);
        if (b_up || b_down)
        {
            int current = -1;
            for (int index = 0; index < static_cast<int>(visible_items.size()); ++index)
            {
                if (b_IsSelected(editor, visible_items[index]))
                {
                    current = index;
                    break;
                }
            }

            int next = current;
            if (b_up)
            {
                next = (current <= 0) ? 0 : current - 1;
            }
            else
            {
                next = (current < 0 || current + 1 >= static_cast<int>(visible_items.size()))
                    ? static_cast<int>(visible_items.size()) - 1
                    : current + 1;
            }
            SelectItem(editor, visible_items[next]);
        }

        if (ImGui::IsKeyPressed(ImGuiKey_Delete) &&
            editor->m_SelectionKind == GameEditor::EEditorSelectionKind::SceneObject &&
            !editor->GetSelectedSceneObjectIds().empty())
        {
            const std::vector<uint32_t> ids = editor->GetSelectedSceneObjectIds();
            const std::string prompt = ids.size() > 1
                ? "Delete the " + std::to_string(ids.size()) + " selected scene objects?"
                : std::string("Delete the selected scene object?");
            if (tinyfd_messageBox("Delete Object", prompt.c_str(), "yesno", "warning", 0) == 1)
            {
                editor->GetCommandStack().Push(std::make_unique<SceneCommands::SceneObjectRemoveCommand>(scene, ids));
                editor->ClearSelection();
            }
        }

        if (ImGui::IsKeyPressed(ImGuiKey_F2) &&
            editor->m_SelectionKind == GameEditor::EEditorSelectionKind::SceneObject)
        {
            if (t_SceneObject* object = editor->GetSelectedSceneObject())
            {
                start_rename(*object);
            }
        }
    }

    ImGui::EndChild();
    ImGui::End();
}
