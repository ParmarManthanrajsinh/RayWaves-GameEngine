#include "SceneHierarchyPanel.h"
#include "../../Engine/MapManager.h"
#include "../../Engine/Profiler.h"
#include "../../Engine/ProjectManager.h"
#include "../EditorWidgets.h"
#include "../GameEditor.h"
#include <cstdio>
#include <imgui.h>
#include <string>
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
        return editor->m_SelectionKind == GameEditor::EEditorSelectionKind::SceneObject &&
            editor->m_SelectedSceneObjectId == item.m_SceneId;
    }
    if (item.m_Kind == GameEditor::EEditorSelectionKind::GameplayEntity)
    {
        return editor->m_SelectionKind == GameEditor::EEditorSelectionKind::GameplayEntity &&
            editor->m_SelectedEditorEntityId == item.m_EntityId;
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

    if (editor->m_SelectedSceneObjectId != m_LastSceneId || editor->m_SelectedEditorEntityId != m_LastEntityId)
    {
        m_LastSceneId = editor->m_SelectedSceneObjectId;
        m_LastEntityId = editor->m_SelectedEditorEntityId;
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
    ImGui::BeginChild("##hierarchy_objects", ImVec2(0, 0), ImGuiChildFlags_None);

    // Empty space doubles as the add surface, so the header stays free of
    // toolbar buttons.
    if (ImGui::BeginPopupContextWindow(
            "##scene_hierarchy_context", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
    {
        if (ImGui::MenuItem(ICON_FA_PLUS " Add Object"))
        {
            t_SceneObject& object = scene.AddObject("Object");
            object.m_Position = editor->GetViewportCenterWorld();
            editor->SelectSceneObject(object.m_Id);
        }
        ImGui::EndPopup();
    }

    std::vector<FHierarchyItem> visible_items;
    bool b_selected_drawn = false;

    const auto start_rename = [this](const t_SceneObject& object)
    {
        m_RenamingSceneId = object.m_Id;
        std::snprintf(m_RenameBuffer, sizeof(m_RenameBuffer), "%s", object.m_Name.c_str());
    };

    const auto finish_rename = [this, &scene](t_SceneObject& object)
    {
        if (m_RenameBuffer[0] != '\0')
        {
            object.m_Name = m_RenameBuffer;
            scene.MarkDirty();
        }
        m_RenamingSceneId = 0;
    };

    const std::string gameplay_group = std::string(ICON_FA_GAMEPAD) + " Gameplay###gameplay_group";
    const bool b_gameplay_open =
        ImGui::TreeNodeEx(gameplay_group.c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth);
    EditorWidgets::RowCountBadge(static_cast<int>(editor->GetEditorEntities().size()));
    if (b_gameplay_open)
    {
        if (editor->GetEditorEntities().empty())
        {
            ImGui::PushTextWrapPos(0.0f);
            ImGui::TextDisabled("No gameplay entities.");
            ImGui::PopTextWrapPos();
        }

        for (const GameEditor::FEditorEntityRef& entity : editor->GetEditorEntities())
        {
            ImGui::PushID(static_cast<int>(entity.m_Id));

            if (entity.m_Visible != nullptr)
            {
                if (EditorWidgets::RowIconButton("visibility", *entity.m_Visible ? ICON_FA_EYE : ICON_FA_EYE_SLASH,
                        "Toggle visibility", !*entity.m_Visible))
                {
                    *entity.m_Visible = !*entity.m_Visible;
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
                editor->SelectEditorEntity(entity.m_Id);
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
        }
        ImGui::TreePop();
    }

    const std::string editor_objects_group = std::string(ICON_FA_IMAGE) + " Editor Objects###editor_objects_group";
    const bool b_editor_objects_open = ImGui::TreeNodeEx(
        editor_objects_group.c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth);
    EditorWidgets::RowCountBadge(static_cast<int>(scene.GetObjects().size()));

    if (b_editor_objects_open)
    {
        if (scene.GetObjects().empty())
        {
            ImGui::PushTextWrapPos(0.0f);
            ImGui::TextDisabled("No editor objects. Drag a sprite here or right-click to add one.");
            ImGui::PopTextWrapPos();
        }
        for (t_SceneObject& object : scene.GetObjects())
        {
            ImGui::PushID(static_cast<int>(object.m_Id));

            if (EditorWidgets::RowIconButton("visibility", object.m_bVisible ? ICON_FA_EYE : ICON_FA_EYE_SLASH,
                    "Toggle visibility", !object.m_bVisible))
            {
                object.m_bVisible = !object.m_bVisible;
                scene.MarkDirty();
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
                const std::string label =
                    std::string(ICON_FA_IMAGE) + " " + (object.m_Name.empty() ? std::string("Object") : object.m_Name);
                if (ImGui::Selectable(label.c_str(), b_selected, ImGuiSelectableFlags_AllowDoubleClick,
                        ImVec2(ImGui::GetContentRegionAvail().x, EditorWidgets::c_RowHeight)))
                {
                    editor->SelectSceneObject(object.m_Id);
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

            bool b_deleted = false;
            if (ImGui::BeginPopupContextItem("##scene_object_context"))
            {
                if (ImGui::MenuItem("Rename"))
                {
                    start_rename(object);
                }
                if (ImGui::MenuItem("Duplicate"))
                {
                    if (t_SceneObject* copy = scene.DuplicateObject(object.m_Id))
                    {
                        copy->m_Position.x += 16.0f;
                        copy->m_Position.y += 16.0f;
                        editor->SelectSceneObject(copy->m_Id);
                    }
                }
                if (ImGui::MenuItem("Delete"))
                {
                    if (tinyfd_messageBox(
                            "Delete Object", "Delete the selected scene object?", "yesno", "warning", 0) == 1)
                    {
                        scene.b_RemoveObject(object.m_Id);
                        editor->ClearSelection();
                        b_deleted = true;
                    }
                }
                ImGui::EndPopup();
            }

            if (b_deleted)
            {
                ImGui::PopID();
                break;
            }

            visible_items.push_back(item);
            ImGui::PopID();
        }
        ImGui::TreePop();
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
            editor->m_SelectionKind == GameEditor::EEditorSelectionKind::SceneObject)
        {
            if (t_SceneObject* object = editor->GetSelectedSceneObject())
            {
                if (tinyfd_messageBox("Delete Object", "Delete the selected scene object?", "yesno", "warning", 0) == 1)
                {
                    scene.b_RemoveObject(object->m_Id);
                    editor->ClearSelection();
                }
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
