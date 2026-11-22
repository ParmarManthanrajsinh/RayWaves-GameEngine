#include "InspectorPanel.h"
#include "../../Engine/MapManager.h"
#include "../../Engine/Profiler.h"
#include "../../Engine/ProjectManager.h"
#include "../EditorAssets.h"
#include "../GameEditor.h"
#include "../SceneCommands.h"
#include <algorithm>
#include <imgui.h>
#include <imgui_stdlib.h>
#include <memory>
#include <string>
#include <tinyfiledialogs.h>
#include <vector>

namespace
{
constexpr float c_LabelWidth = 92.0f;
constexpr ImU32 c_AxisXColor = IM_COL32(235, 90, 90, 255);
constexpr ImU32 c_AxisYColor = IM_COL32(100, 210, 130, 255);
constexpr ImU32 c_AxisZColor = IM_COL32(90, 140, 235, 255);

// Per-frame activation flags for a row that may contain several widgets. A
// Vector2 row has two DragFloats, so checking IsItemDeactivatedAfterEdit() once
// after the row would only see the last axis.
struct FRowState
{
    bool m_bActivated = false;
    bool m_bDeactivated = false;
    bool m_bDeactivatedAfterEdit = false;
};

void CaptureRow(FRowState* state)
{
    if (state == nullptr)
    {
        return;
    }
    state->m_bActivated |= ImGui::IsItemActivated();
    state->m_bDeactivated |= ImGui::IsItemDeactivated();
    state->m_bDeactivatedAfterEdit |= ImGui::IsItemDeactivatedAfterEdit();
}

// Left-aligned label column, so the widgets that follow never cover their own
// label (the old full-width InputFloat2 labels ended up clipped off-screen).
void PropertyLabel(const char* label)
{
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);
    ImGui::SameLine(c_LabelWidth);
}

bool AxisField(const char* id, const char* axis, float* value, ImU32 axis_color, float width,
    const char* format = "%.1f", FRowState* state = nullptr)
{
    ImGui::PushStyleColor(ImGuiCol_Text, axis_color);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(axis);
    ImGui::PopStyleColor();
    ImGui::SameLine(0.0f, 3.0f);
    ImGui::SetNextItemWidth(width);
    const bool b_changed = ImGui::DragFloat(id, value, 1.0f, 0.0f, 0.0f, format);
    CaptureRow(state);
    return b_changed;
}

bool Vector2Row(
    const char* label, const char* id_prefix, float* values, const char* format = "%.1f", FRowState* state = nullptr)
{
    PropertyLabel(label);
    const float field_width = std::max(54.0f, (ImGui::GetContentRegionAvail().x - 36.0f) * 0.5f);
    const std::string x_id = std::string("##") + id_prefix + "_x";
    const std::string y_id = std::string("##") + id_prefix + "_y";

    bool b_changed = AxisField(x_id.c_str(), "X", &values[0], c_AxisXColor, field_width, format, state);
    ImGui::SameLine();
    b_changed |= AxisField(y_id.c_str(), "Y", &values[1], c_AxisYColor, field_width, format, state);
    return b_changed;
}

bool Vector3Row(
    const char* label, const char* id_prefix, float* values, const char* format = "%.1f", FRowState* state = nullptr)
{
    PropertyLabel(label);
    const float field_width = std::max(46.0f, (ImGui::GetContentRegionAvail().x - 56.0f) / 3.0f);
    const std::string x_id = std::string("##") + id_prefix + "_x";
    const std::string y_id = std::string("##") + id_prefix + "_y";
    const std::string z_id = std::string("##") + id_prefix + "_z";

    bool b_changed = AxisField(x_id.c_str(), "X", &values[0], c_AxisXColor, field_width, format, state);
    ImGui::SameLine();
    b_changed |= AxisField(y_id.c_str(), "Y", &values[1], c_AxisYColor, field_width, format, state);
    ImGui::SameLine();
    b_changed |= AxisField(z_id.c_str(), "Z", &values[2], c_AxisZColor, field_width, format, state);
    return b_changed;
}

bool FloatRow(const char* label, const char* id, float* value, const char* format = "%.1f", FRowState* state = nullptr)
{
    PropertyLabel(label);
    ImGui::SetNextItemWidth(-1.0f);
    const bool b_changed = ImGui::DragFloat(id, value, 1.0f, 0.0f, 0.0f, format);
    CaptureRow(state);
    return b_changed;
}

bool BoolRow(const char* label, const char* id, bool* value, FRowState* state = nullptr)
{
    PropertyLabel(label);
    const bool b_changed = ImGui::Checkbox(id, value);
    CaptureRow(state);
    return b_changed;
}

template <typename TCommand, typename... TArgs>
TCommand* PushCommand(GameEditor* editor, TArgs&&... args)
{
    auto command = std::make_unique<TCommand>(std::forward<TArgs>(args)...);
    TCommand* raw = command.get();
    editor->GetCommandStack().Push(std::move(command));
    return raw;
}

void DrawEntityHeader(const GameEditor::FEditorEntityRef& entity)
{
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_HeaderActive]);
    ImGui::Text("%s  %s", ICON_FA_GAMEPAD, entity.m_Name.c_str());
    ImGui::PopStyleColor();

    ImGui::TextDisabled("%s", entity.m_Type.c_str());
    ImGui::SameLine();
    ImGui::TextDisabled("- gameplay object - id %llu", static_cast<unsigned long long>(entity.m_Id));
}

enum class EEntityField
{
    Position2D,
    Position3D,
    Size2D,
    Size3D,
    Rotation,
    Visible
};

bool b_EntityHasField(const GameEditor::FEditorEntityRef& entity, EEntityField field)
{
    switch (field)
    {
        case EEntityField::Position2D:
            return entity.m_Position != nullptr;
        case EEntityField::Position3D:
            return entity.m_Position3D != nullptr;
        case EEntityField::Size2D:
            return entity.m_Size != nullptr;
        case EEntityField::Size3D:
            return entity.m_Size3D != nullptr;
        case EEntityField::Rotation:
            return entity.m_Rotation != nullptr;
        case EEntityField::Visible:
            return entity.m_Visible != nullptr;
    }
    return false;
}

SceneCommands::FEntityTransformEntry MakeEntityEntry(GameEditor::FEditorEntityRef& entity, EEntityField field)
{
    SceneCommands::FEntityTransformEntry entry;
    switch (field)
    {
        case EEntityField::Position2D:
            entry.m_Position = entity.m_Position;
            break;
        case EEntityField::Position3D:
            entry.m_Position3D = entity.m_Position3D;
            break;
        case EEntityField::Size2D:
            entry.m_Size = entity.m_Size;
            break;
        case EEntityField::Size3D:
            entry.m_Size3D = entity.m_Size3D;
            break;
        case EEntityField::Rotation:
            entry.m_Rotation = entity.m_Rotation;
            break;
        case EEntityField::Visible:
            entry.m_Visible = entity.m_Visible;
            break;
    }
    return entry;
}

// All pointers at once: batch trackers need a complete before-state so editing
// one field never restores zeroes into the others on undo.
SceneCommands::FEntityTransformEntry MakeFullEntityEntry(GameEditor::FEditorEntityRef& entity)
{
    SceneCommands::FEntityTransformEntry entry;
    entry.m_Position = entity.m_Position;
    entry.m_Position3D = entity.m_Position3D;
    entry.m_Size = entity.m_Size;
    entry.m_Size3D = entity.m_Size3D;
    entry.m_Rotation = entity.m_Rotation;
    entry.m_Visible = entity.m_Visible;
    return entry;
}

void WriteEntityField(
    GameEditor::FEditorEntityRef& entity, EEntityField field, const SceneCommands::FEntityTransform& value)
{
    switch (field)
    {
        case EEntityField::Position2D:
            if (entity.m_Position != nullptr)
            {
                *entity.m_Position = value.m_Position;
            }
            break;
        case EEntityField::Position3D:
            if (entity.m_Position3D != nullptr)
            {
                *entity.m_Position3D = value.m_Position3D;
            }
            break;
        case EEntityField::Size2D:
            if (entity.m_Size != nullptr)
            {
                *entity.m_Size = value.m_Size;
            }
            break;
        case EEntityField::Size3D:
            if (entity.m_Size3D != nullptr)
            {
                *entity.m_Size3D = value.m_Size3D;
            }
            break;
        case EEntityField::Rotation:
            if (entity.m_Rotation != nullptr)
            {
                *entity.m_Rotation = value.m_Rotation;
            }
            break;
        case EEntityField::Visible:
            if (entity.m_Visible != nullptr)
            {
                *entity.m_Visible = value.m_bVisible;
            }
            break;
    }
}

bool b_EntityTransformEqual(
    EEntityField field, const SceneCommands::FEntityTransform& left, const SceneCommands::FEntityTransform& right)
{
    switch (field)
    {
        case EEntityField::Position2D:
            return left.m_Position.x == right.m_Position.x && left.m_Position.y == right.m_Position.y;
        case EEntityField::Position3D:
            return left.m_Position3D.x == right.m_Position3D.x && left.m_Position3D.y == right.m_Position3D.y &&
                left.m_Position3D.z == right.m_Position3D.z;
        case EEntityField::Size2D:
            return left.m_Size.x == right.m_Size.x && left.m_Size.y == right.m_Size.y;
        case EEntityField::Size3D:
            return left.m_Size3D.x == right.m_Size3D.x && left.m_Size3D.y == right.m_Size3D.y &&
                left.m_Size3D.z == right.m_Size3D.z;
        case EEntityField::Rotation:
            return left.m_Rotation == right.m_Rotation;
        case EEntityField::Visible:
            return left.m_bVisible == right.m_bVisible;
    }
    return true;
}

bool b_AnyEntityTransformChanged(const std::vector<SceneCommands::FEntityTransformEntry>& entries)
{
    for (const SceneCommands::FEntityTransformEntry& entry : entries)
    {
        const bool b_visible_only = entry.m_Visible != nullptr;
        const SceneCommands::FEntityTransform after = SceneCommands::ReadTransform(entry);
        if (b_visible_only)
        {
            if (entry.m_Before.m_bVisible != after.m_bVisible)
            {
                return true;
            }
            continue;
        }

        if (entry.m_Position != nullptr &&
            (entry.m_Before.m_Position.x != after.m_Position.x || entry.m_Before.m_Position.y != after.m_Position.y))
        {
            return true;
        }
        if (entry.m_Position3D != nullptr && entry.m_Before.m_Position3D.x != after.m_Position3D.x)
        {
            return true;
        }
        if (entry.m_Position3D != nullptr && entry.m_Before.m_Position3D.y != after.m_Position3D.y)
        {
            return true;
        }
        if (entry.m_Position3D != nullptr && entry.m_Before.m_Position3D.z != after.m_Position3D.z)
        {
            return true;
        }
        if (entry.m_Size != nullptr &&
            (entry.m_Before.m_Size.x != after.m_Size.x || entry.m_Before.m_Size.y != after.m_Size.y))
        {
            return true;
        }
        if (entry.m_Size3D != nullptr && entry.m_Before.m_Size3D.x != after.m_Size3D.x)
        {
            return true;
        }
        if (entry.m_Size3D != nullptr && entry.m_Before.m_Size3D.y != after.m_Size3D.y)
        {
            return true;
        }
        if (entry.m_Size3D != nullptr && entry.m_Before.m_Size3D.z != after.m_Size3D.z)
        {
            return true;
        }
        if (entry.m_Rotation != nullptr && entry.m_Before.m_Rotation != after.m_Rotation)
        {
            return true;
        }
    }
    return false;
}

// Drives one entity transform field. The widget writes the live pointer; the
// tracker holds the value from before activation and records one command when
// the user lets go.
void EditEntityField(GameEditor* editor, GameEditor::FEditorEntityRef& entity, InspectorPanel::FEntityFieldEdit& edit,
    EEntityField field, const char* label, const char* id, const char* format, bool b_bool_field)
{
    if (edit.m_bEditing && (edit.m_EntityId != entity.m_Id || edit.m_Field != static_cast<int>(field)))
    {
        edit.m_bEditing = false;
    }

    SceneCommands::FEntityTransformEntry entry = MakeEntityEntry(entity, field);
    if (!edit.m_bEditing)
    {
        edit.m_EntityId = entity.m_Id;
        edit.m_Field = static_cast<int>(field);
        edit.m_Before = SceneCommands::ReadTransform(entry);
    }

    FRowState row;
    if (b_bool_field)
    {
        BoolRow(label, id, entity.m_Visible, &row);
    }
    else if (field == EEntityField::Rotation)
    {
        FloatRow(label, id, entity.m_Rotation, format, &row);
    }
    else if (field == EEntityField::Position3D || field == EEntityField::Size3D)
    {
        Vector3Row(label, id, &(field == EEntityField::Position3D ? entity.m_Position3D->x : entity.m_Size3D->x),
            format, &row);
    }
    else
    {
        Vector2Row(
            label, id, &(field == EEntityField::Position2D ? entity.m_Position->x : entity.m_Size->x), format, &row);
    }

    if (row.m_bActivated)
    {
        edit.m_bEditing = true;
    }

    if (edit.m_bEditing && row.m_bDeactivatedAfterEdit)
    {
        entry = MakeEntityEntry(entity, field);
        entry.m_Before = edit.m_Before;
        entry.m_After = SceneCommands::ReadTransform(entry);
        if (b_AnyEntityTransformChanged({ entry }))
        {
            const char* command_name = (field == EEntityField::Size2D || field == EEntityField::Size3D)
                ? "Edit Size"
                : (field == EEntityField::Rotation
                          ? "Edit Rotation"
                          : (field == EEntityField::Visible ? "Change Visibility" : "Move Entity"));
            PushCommand<SceneCommands::EntityTransformCommand>(editor, std::vector{ entry }, command_name);
        }
        edit.m_bEditing = false;
    }
    else if (edit.m_bEditing && row.m_bDeactivated)
    {
        edit.m_bEditing = false;
    }
}

void DrawEntityProperties(GameEditor* editor, InspectorPanel& panel, GameEditor::FEditorEntityRef& entity)
{
    if ((entity.m_Properties == nullptr) || (entity.m_PropertyCount == 0))
    {
        return;
    }

    ImGui::Spacing();
    ImGui::SeparatorText("Properties");
    ImGui::PushID(static_cast<int>(entity.m_Id));
    for (uint32_t index = 0; index < entity.m_PropertyCount; ++index)
    {
        t_EntityProperty& property = entity.m_Properties[index];
        if ((property.m_Name == nullptr) || (property.m_Data == nullptr))
        {
            continue;
        }

        InspectorPanel::FEntityPropertyEdit& edit = panel.m_EntityPropertyEdit;
        if (edit.m_bEditing && (edit.m_EntityId != entity.m_Id || edit.m_PropertyIndex != index))
        {
            edit.m_bEditing = false;
        }
        if (!edit.m_bEditing)
        {
            edit.m_EntityId = entity.m_Id;
            edit.m_PropertyIndex = index;
            edit.m_Before = SceneCommands::ReadPropertyValue(property);
        }

        ImGui::PushID(static_cast<int>(index));
        FRowState row;
        switch (property.m_Type)
        {
            case EEntityPropertyType::Float:
            {
                PropertyLabel(property.m_Name);
                ImGui::SetNextItemWidth(-1.0f);
                ImGui::DragFloat("##value", static_cast<float*>(property.m_Data), 0.1f, property.m_Min, property.m_Max);
                CaptureRow(&row);
                break;
            }
            case EEntityPropertyType::Int:
            {
                PropertyLabel(property.m_Name);
                ImGui::SetNextItemWidth(-1.0f);
                ImGui::DragInt("##value", static_cast<int*>(property.m_Data), 1.0f, static_cast<int>(property.m_Min),
                    static_cast<int>(property.m_Max));
                CaptureRow(&row);
                break;
            }
            case EEntityPropertyType::Bool:
            {
                BoolRow(property.m_Name, "##value", static_cast<bool*>(property.m_Data), &row);
                break;
            }
            case EEntityPropertyType::Vector2:
            {
                Vector2Row(property.m_Name, property.m_Name, &static_cast<Vector2*>(property.m_Data)->x, "%.2f", &row);
                break;
            }
        }

        if (row.m_bActivated)
        {
            edit.m_bEditing = true;
        }
        if (edit.m_bEditing && row.m_bDeactivatedAfterEdit)
        {
            SceneCommands::FEntityPropertyEntry entry;
            entry.m_Property = &property;
            entry.m_Before = edit.m_Before;
            entry.m_After = SceneCommands::ReadPropertyValue(property);
            PushCommand<SceneCommands::EntityPropertyCommand>(
                editor, std::vector{ entry }, std::string("Edit ") + property.m_Name);
            edit.m_bEditing = false;
        }
        else if (edit.m_bEditing && row.m_bDeactivated)
        {
            edit.m_bEditing = false;
        }
        ImGui::PopID();
    }
    ImGui::PopID();
}

void DrawSceneObjectInspector(GameEditor* editor, InspectorPanel& panel, t_SceneObject& object, Scene& scene)
{
    InspectorPanel::FSceneFieldEdit& edit = panel.m_SceneFieldEdit;
    bool b_changed = false;

    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_HeaderActive]);
    ImGui::Text("%s  %s", ICON_FA_IMAGE, object.m_Name.empty() ? "Object" : object.m_Name.c_str());
    ImGui::PopStyleColor();
    ImGui::TextDisabled("Scene sprite - id %u - layer %d", object.m_Id, object.m_Layer);
    ImGui::Spacing();

    const auto begin_field = [&](SceneCommands::ESceneObjectField field)
    {
        if (edit.m_bEditing && (edit.m_ObjectId != object.m_Id || edit.m_Field != field))
        {
            edit.m_bEditing = false;
        }
        if (!edit.m_bEditing)
        {
            edit.m_ObjectId = object.m_Id;
            edit.m_Field = field;
            edit.m_Before = SceneCommands::ReadField(object, field);
        }
    };

    const auto commit_field = [&](SceneCommands::ESceneObjectField field, bool b_finished)
    {
        if (!edit.m_bEditing || edit.m_ObjectId != object.m_Id || edit.m_Field != field || !b_finished)
        {
            return;
        }

        const SceneCommands::FSceneObjectFieldValue after = SceneCommands::ReadField(object, field);
        if (!SceneCommands::b_Equal(edit.m_Before, after))
        {
            PushCommand<SceneCommands::SceneObjectFieldCommand>(
                editor, scene, object.m_Id, field, edit.m_Before, after);
        }
        edit.m_bEditing = false;
    };

    PropertyLabel("Name");
    begin_field(SceneCommands::ESceneObjectField::Name);
    ImGui::SetNextItemWidth(-1.0f);
    b_changed |= ImGui::InputTextWithHint("##object_name", "Object name", &object.m_Name);
    if (ImGui::IsItemActivated())
    {
        edit.m_bEditing = true;
    }
    commit_field(SceneCommands::ESceneObjectField::Name, ImGui::IsItemDeactivatedAfterEdit());
    if (ImGui::IsItemDeactivated())
    {
        edit.m_bEditing = false;
    }

    PropertyLabel("Type / Tag");
    begin_field(SceneCommands::ESceneObjectField::Type);
    ImGui::SetNextItemWidth(-1.0f);
    b_changed |= ImGui::InputTextWithHint("##object_type", "spawn, enemy, decor", &object.m_Type);
    if (ImGui::IsItemActivated())
    {
        edit.m_bEditing = true;
    }
    commit_field(SceneCommands::ESceneObjectField::Type, ImGui::IsItemDeactivatedAfterEdit());
    if (ImGui::IsItemDeactivated())
    {
        edit.m_bEditing = false;
    }

    ImGui::Spacing();

    ImGui::SeparatorText("Appearance");

    const bool b_has_texture = IsTextureValid(object.m_Texture);
    if (b_has_texture)
    {
        const float preview_height = 64.0f;
        const float aspect = (object.m_Texture.height > 0)
            ? static_cast<float>(object.m_Texture.width) / static_cast<float>(object.m_Texture.height)
            : 1.0f;
        const ImVec2 preview_size{ preview_height * aspect, preview_height };
        ImGui::Image(static_cast<ImTextureID>(object.m_Texture.id), preview_size, ImVec2(0, 0), ImVec2(1, 1));
        ImGui::SameLine();
        ImGui::BeginGroup();
        ImGui::TextDisabled("%d x %d px", object.m_Texture.width, object.m_Texture.height);
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextDisabled("%s", object.m_TexturePath.c_str());
        ImGui::PopTextWrapPos();
        ImGui::EndGroup();
    }
    else if (!object.m_TexturePath.empty())
    {
        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.4f, 1.0f), ICON_FA_TRIANGLE_EXCLAMATION " Texture not found");
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextDisabled("%s", object.m_TexturePath.c_str());
        ImGui::PopTextWrapPos();
    }
    else
    {
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextDisabled("No texture - drawn as a placeholder.");
        ImGui::PopTextWrapPos();
    }

    ImGui::Spacing();

    PropertyLabel("Texture");
    begin_field(SceneCommands::ESceneObjectField::TexturePath);
    ImGui::SetNextItemWidth(-1.0f);
    std::string texture_path = object.m_TexturePath;
    if (ImGui::InputTextWithHint("##texture_path", "Assets/crate.png", &texture_path))
    {
        object.m_TexturePath = texture_path;
        b_changed = true;
    }
    if (ImGui::IsItemActivated())
    {
        edit.m_bEditing = true;
    }
    commit_field(SceneCommands::ESceneObjectField::TexturePath, ImGui::IsItemDeactivatedAfterEdit());
    if (ImGui::IsItemDeactivated())
    {
        edit.m_bEditing = false;
    }

    const auto set_texture = [&](const std::string& path)
    {
        if (path == object.m_TexturePath)
        {
            return;
        }
        PushCommand<SceneCommands::SceneObjectFieldCommand>(editor, scene, object.m_Id,
            SceneCommands::ESceneObjectField::TexturePath,
            SceneCommands::FSceneObjectFieldValue::Text(object.m_TexturePath),
            SceneCommands::FSceneObjectFieldValue::Text(path), "Set Texture");
    };

    const float button_width = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
    if (ImGui::Button(ICON_FA_FOLDER_OPEN " Browse", ImVec2(button_width, 0.0f)))
    {
        const std::string asset_root = ProjectManager::GetCurrentCopy().m_AssetPath;
        const char* filters[] = { "*.png", "*.jpg", "*.jpeg", "*.bmp", "*.tga", "*.gif", "*.qoi", "*.psd" };
        const char* selected =
            tinyfd_openFileDialog("Select Texture", asset_root.c_str(), 8, filters, "Image files", 0);
        if (selected != nullptr)
        {
            std::string relative;
            if (EditorAssets::b_ImportFile(selected, relative))
            {
                set_texture(relative);
                b_changed = true;
            }
        }
    }

    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_XMARK " Clear", ImVec2(button_width, 0.0f)))
    {
        set_texture("");
        b_changed = true;
    }

    ImGui::SeparatorText("Transform");

    begin_field(SceneCommands::ESceneObjectField::Position);
    FRowState position_row;
    b_changed |= Vector2Row("Position", "object_position", &object.m_Position.x, "%.1f", &position_row);
    if (position_row.m_bActivated)
    {
        edit.m_bEditing = true;
    }
    commit_field(SceneCommands::ESceneObjectField::Position, position_row.m_bDeactivatedAfterEdit);
    if (position_row.m_bDeactivated)
    {
        edit.m_bEditing = false;
    }

    begin_field(SceneCommands::ESceneObjectField::Rotation);
    FRowState rotation_row;
    b_changed |= FloatRow("Rotation", "##object_rotation", &object.m_Rotation, "%.1f deg", &rotation_row);
    if (rotation_row.m_bActivated)
    {
        edit.m_bEditing = true;
    }
    commit_field(SceneCommands::ESceneObjectField::Rotation, rotation_row.m_bDeactivatedAfterEdit);
    if (rotation_row.m_bDeactivated)
    {
        edit.m_bEditing = false;
    }

    begin_field(SceneCommands::ESceneObjectField::Scale);
    FRowState scale_row;
    b_changed |= Vector2Row("Scale", "object_scale", &object.m_Scale.x, "%.2f", &scale_row);
    if (scale_row.m_bActivated)
    {
        edit.m_bEditing = true;
    }
    commit_field(SceneCommands::ESceneObjectField::Scale, scale_row.m_bDeactivatedAfterEdit);
    if (scale_row.m_bDeactivated)
    {
        edit.m_bEditing = false;
    }

    if (ImGui::CollapsingHeader("Sprite Region"))
    {
        float source[4] = { object.m_Source.x, object.m_Source.y, object.m_Source.width, object.m_Source.height };
        PropertyLabel("Source");
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::DragFloat4("##object_source", source, 1.0f, 0.0f, 0.0f, "%.0f"))
        {
            object.m_Source = { source[0], source[1], source[2], source[3] };
            b_changed = true;
        }
    }

    ImGui::SeparatorText("Rendering");

    PropertyLabel("Tint");
    float tint[4] = { static_cast<float>(object.m_Tint.r) / 255.0f, static_cast<float>(object.m_Tint.g) / 255.0f,
        static_cast<float>(object.m_Tint.b) / 255.0f, static_cast<float>(object.m_Tint.a) / 255.0f };
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::ColorEdit4("##object_tint", tint, ImGuiColorEditFlags_AlphaBar))
    {
        object.m_Tint =
            Color{ static_cast<unsigned char>(tint[0] * 255.0f), static_cast<unsigned char>(tint[1] * 255.0f),
                static_cast<unsigned char>(tint[2] * 255.0f), static_cast<unsigned char>(tint[3] * 255.0f) };
        b_changed = true;
    }

    begin_field(SceneCommands::ESceneObjectField::Layer);
    FRowState layer_row;
    PropertyLabel("Layer");
    ImGui::SetNextItemWidth(-1.0f);
    b_changed |= ImGui::DragInt("##object_layer", &object.m_Layer, 0.2f);
    CaptureRow(&layer_row);
    if (layer_row.m_bActivated)
    {
        edit.m_bEditing = true;
    }
    commit_field(SceneCommands::ESceneObjectField::Layer, layer_row.m_bDeactivatedAfterEdit);
    if (layer_row.m_bDeactivated)
    {
        edit.m_bEditing = false;
    }

    begin_field(SceneCommands::ESceneObjectField::Visible);
    FRowState visible_row;
    b_changed |= BoolRow("Visible", "##object_visible", &object.m_bVisible, &visible_row);
    if (visible_row.m_bActivated)
    {
        edit.m_bEditing = true;
    }
    commit_field(SceneCommands::ESceneObjectField::Visible, visible_row.m_bDeactivatedAfterEdit);
    if (visible_row.m_bDeactivated)
    {
        edit.m_bEditing = false;
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.55f, 0.16f, 0.18f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.68f, 0.22f, 0.24f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.78f, 0.26f, 0.28f, 1.0f));
    if (ImGui::Button(ICON_FA_TRASH_CAN " Delete Object", ImVec2(-1.0f, 0.0f)))
    {
        if (tinyfd_messageBox("Delete Object", "Delete the selected object?", "yesno", "warning", 0) == 1)
        {
            PushCommand<SceneCommands::SceneObjectRemoveCommand>(editor, scene, object.m_Id);
            editor->ClearSelection();
            ImGui::PopStyleColor(3);
            return;
        }
    }
    ImGui::PopStyleColor(3);

    if (b_changed)
    {
        scene.MarkDirty();
    }
}

void DrawEntityInspector(GameEditor* editor, InspectorPanel& panel, GameEditor::FEditorEntityRef& entity)
{
    DrawEntityHeader(entity);
    ImGui::Spacing();

    if (editor->b_IsPlaying)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.78f, 0.35f, 1.0f));
        ImGui::TextUnformatted(ICON_FA_PAUSE "  Playback running - stop to edit");
        ImGui::PopStyleColor();
    }

    ImGui::SeparatorText("Transform");

    if (editor->b_IsPlaying)
    {
        ImGui::BeginDisabled();
    }

    InspectorPanel::FEntityFieldEdit& edit = panel.m_EntityFieldEdit;
    if (entity.m_Position != nullptr)
    {
        EditEntityField(editor, entity, edit, EEntityField::Position2D, "Position", "entity_position", "%.1f", false);
    }
    else if (entity.m_Position3D != nullptr)
    {
        EditEntityField(editor, entity, edit, EEntityField::Position3D, "Position", "entity_position3d", "%.1f", false);
    }

    if (entity.m_Size != nullptr)
    {
        EditEntityField(editor, entity, edit, EEntityField::Size2D, "Size", "entity_size", "%.1f", false);
    }
    else if (entity.m_Size3D != nullptr)
    {
        EditEntityField(editor, entity, edit, EEntityField::Size3D, "Size", "entity_size3d", "%.1f", false);
    }
    else
    {
        PropertyLabel("Size");
        ImGui::TextDisabled("not exposed");
    }

    if (entity.m_Rotation != nullptr)
    {
        EditEntityField(
            editor, entity, edit, EEntityField::Rotation, "Rotation", "##entity_rotation", "%.1f deg", false);
    }

    if (entity.m_Visible != nullptr)
    {
        EditEntityField(editor, entity, edit, EEntityField::Visible, "Visible", "##entity_visible", "%.1f", true);
    }

    if (editor->b_IsPlaying)
    {
        ImGui::EndDisabled();
    }

    ImGui::Spacing();
    ImGui::SeparatorText("Gameplay");

    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextDisabled("These values point straight into your GameLogic objects.");
    ImGui::TextDisabled("The editor only writes them; gameplay code decides what they mean.");
    ImGui::PopTextWrapPos();

    DrawEntityProperties(editor, panel, entity);

    std::string exposed;
    const auto add_exposed = [&exposed](const char* name)
    {
        if (!exposed.empty())
        {
            exposed += ", ";
        }
        exposed += name;
    };
    if ((entity.m_Position != nullptr) || (entity.m_Position3D != nullptr))
    {
        add_exposed("position");
    }
    if ((entity.m_Size != nullptr) || (entity.m_Size3D != nullptr))
    {
        add_exposed("size");
    }
    if (entity.m_Rotation != nullptr)
    {
        add_exposed("rotation");
    }
    if (entity.m_Visible != nullptr)
    {
        add_exposed("visibility");
    }
    for (uint32_t index = 0; index < entity.m_PropertyCount; ++index)
    {
        if (entity.m_Properties[index].m_Name != nullptr)
        {
            add_exposed(entity.m_Properties[index].m_Name);
        }
    }

    ImGui::Spacing();
    ImGui::TextDisabled("Exposed: %s", exposed.empty() ? "nothing" : exposed.c_str());
}

bool b_AllEntitiesHave(
    GameEditor* editor, const std::vector<uint64_t>& ids, bool (*condition)(const GameEditor::FEditorEntityRef&))
{
    for (const uint64_t id : ids)
    {
        const GameEditor::FEditorEntityRef* found = nullptr;
        for (const GameEditor::FEditorEntityRef& entity : editor->GetEditorEntities())
        {
            if (entity.m_Id == id)
            {
                found = &entity;
                break;
            }
        }
        if (found == nullptr || !condition(*found))
        {
            return false;
        }
    }
    return !ids.empty();
}

void DrawSceneObjectBatchInspector(
    GameEditor* editor, InspectorPanel& panel, const std::vector<uint32_t>& ids, Scene& scene)
{
    InspectorPanel::FSceneBatchEdit& edit = panel.m_SceneBatchEdit;
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_HeaderActive]);
    ImGui::Text("%s  %zu objects selected", ICON_FA_IMAGE, ids.size());
    ImGui::PopStyleColor();
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextDisabled("Edits apply to the whole selection as one undo step.");
    ImGui::PopTextWrapPos();
    ImGui::Spacing();

    if (!edit.m_bEditing)
    {
        edit.m_Ids = ids;
        edit.m_BeforeTransforms.clear();
        edit.m_BeforeTransforms.reserve(ids.size());
        for (const uint32_t id : ids)
        {
            if (const t_SceneObject* object = scene.FindObject(id))
            {
                edit.m_BeforeTransforms.push_back(SceneCommands::ReadTransform(*object));
            }
        }
    }

    const t_SceneObject* primary = scene.FindObject(ids.back());
    if (primary == nullptr)
    {
        return;
    }

    const auto apply_position = [&](Vector2 value)
    {
        for (const uint32_t id : ids)
        {
            if (t_SceneObject* object = scene.FindObject(id))
            {
                object->m_Position = value;
            }
        }
    };
    const auto apply_rotation = [&](float value)
    {
        for (const uint32_t id : ids)
        {
            if (t_SceneObject* object = scene.FindObject(id))
            {
                object->m_Rotation = value;
            }
        }
    };
    const auto apply_scale = [&](Vector2 value)
    {
        for (const uint32_t id : ids)
        {
            if (t_SceneObject* object = scene.FindObject(id))
            {
                object->m_Scale = value;
            }
        }
    };

    const auto commit_transforms = [&](const char* command_name)
    {
        std::vector<SceneCommands::FSceneObjectTransformEntry> entries;
        entries.reserve(ids.size());
        for (std::size_t index = 0; index < ids.size(); ++index)
        {
            const t_SceneObject* object = scene.FindObject(ids[index]);
            if (object == nullptr)
            {
                continue;
            }

            SceneCommands::FSceneObjectTransformEntry entry;
            entry.m_Id = ids[index];
            entry.m_After = SceneCommands::ReadTransform(*object);
            if (index < edit.m_BeforeTransforms.size())
            {
                entry.m_Before = edit.m_BeforeTransforms[index];
            }
            entries.push_back(entry);
        }
        if (!entries.empty())
        {
            PushCommand<SceneCommands::SceneObjectsTransformCommand>(editor, scene, std::move(entries), command_name);
        }
        edit.m_bEditing = false;
    };

    SceneCommands::FSceneObjectTransform primary_transform = SceneCommands::ReadTransform(*primary);

    FRowState position_row;
    Vector2 batch_position = primary_transform.m_Position;
    if (Vector2Row("Position", "batch_position", &batch_position.x, "%.1f", &position_row))
    {
        apply_position(batch_position);
        scene.MarkDirty();
    }
    if (position_row.m_bActivated)
    {
        edit.m_bEditing = true;
        edit.m_Field = 0;
    }
    if (edit.m_bEditing && edit.m_Field == 0 && position_row.m_bDeactivatedAfterEdit)
    {
        commit_transforms("Move Objects");
    }
    else if (edit.m_bEditing && edit.m_Field == 0 && position_row.m_bDeactivated)
    {
        edit.m_bEditing = false;
    }

    FRowState scale_row;
    Vector2 batch_scale = primary_transform.m_Scale;
    if (Vector2Row("Size", "batch_scale", &batch_scale.x, "%.2f", &scale_row))
    {
        apply_scale(batch_scale);
        scene.MarkDirty();
    }
    if (scale_row.m_bActivated)
    {
        edit.m_bEditing = true;
        edit.m_Field = 1;
    }
    if (edit.m_bEditing && edit.m_Field == 1 && scale_row.m_bDeactivatedAfterEdit)
    {
        commit_transforms("Scale Objects");
    }
    else if (edit.m_bEditing && edit.m_Field == 1 && scale_row.m_bDeactivated)
    {
        edit.m_bEditing = false;
    }

    FRowState rotation_row;
    float batch_rotation = primary_transform.m_Rotation;
    if (FloatRow("Rotation", "##batch_rotation", &batch_rotation, "%.1f deg", &rotation_row))
    {
        apply_rotation(batch_rotation);
        scene.MarkDirty();
    }
    if (rotation_row.m_bActivated)
    {
        edit.m_bEditing = true;
        edit.m_Field = 2;
    }
    if (edit.m_bEditing && edit.m_Field == 2 && rotation_row.m_bDeactivatedAfterEdit)
    {
        commit_transforms("Rotate Objects");
    }
    else if (edit.m_bEditing && edit.m_Field == 2 && rotation_row.m_bDeactivated)
    {
        edit.m_bEditing = false;
    }

    bool batch_visible = primary->m_bVisible;
    PropertyLabel("Visible");
    FRowState visible_row;
    if (ImGui::Checkbox("##batch_visible", &batch_visible))
    {
        scene.MarkDirty();
    }
    CaptureRow(&visible_row);
    if (visible_row.m_bActivated)
    {
        edit.m_bEditing = true;
        edit.m_Field = 3;
    }
    if (edit.m_bEditing && edit.m_Field == 3 && visible_row.m_bDeactivatedAfterEdit)
    {
        PushCommand<SceneCommands::SceneObjectsVisibleCommand>(editor, scene, ids, batch_visible);
        edit.m_bEditing = false;
    }
    else if (edit.m_bEditing && edit.m_Field == 3 && visible_row.m_bDeactivated)
    {
        edit.m_bEditing = false;
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.55f, 0.16f, 0.18f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.68f, 0.22f, 0.24f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.78f, 0.26f, 0.28f, 1.0f));
    if (ImGui::Button(ICON_FA_TRASH_CAN " Delete Objects", ImVec2(-1.0f, 0.0f)))
    {
        if (tinyfd_messageBox("Delete Objects", "Delete all selected scene objects?", "yesno", "warning", 0) == 1)
        {
            PushCommand<SceneCommands::SceneObjectRemoveCommand>(editor, scene, ids);
            editor->ClearSelection();
        }
    }
    ImGui::PopStyleColor(3);
}

void DrawEntityBatchInspector(GameEditor* editor, InspectorPanel& panel, const std::vector<uint64_t>& ids)
{
    InspectorPanel::FEntityBatchEdit& edit = panel.m_EntityBatchEdit;

    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_HeaderActive]);
    ImGui::Text("%s  %zu gameplay objects selected", ICON_FA_GAMEPAD, ids.size());
    ImGui::PopStyleColor();
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextDisabled("Only fields shared by every selected object are shown.");
    ImGui::PopTextWrapPos();
    ImGui::Spacing();

    const bool b_all_position2d = b_AllEntitiesHave(
        editor, ids, [](const GameEditor::FEditorEntityRef& entity) { return entity.m_Position != nullptr; });
    const bool b_all_position3d = !b_all_position2d &&
        b_AllEntitiesHave(
            editor, ids, [](const GameEditor::FEditorEntityRef& entity) { return entity.m_Position3D != nullptr; });
    const bool b_all_size2d = b_AllEntitiesHave(
        editor, ids, [](const GameEditor::FEditorEntityRef& entity) { return entity.m_Size != nullptr; });
    const bool b_all_size3d = !b_all_size2d &&
        b_AllEntitiesHave(
            editor, ids, [](const GameEditor::FEditorEntityRef& entity) { return entity.m_Size3D != nullptr; });
    const bool b_all_rotation = b_AllEntitiesHave(
        editor, ids, [](const GameEditor::FEditorEntityRef& entity) { return entity.m_Rotation != nullptr; });
    const bool b_all_visible = b_AllEntitiesHave(
        editor, ids, [](const GameEditor::FEditorEntityRef& entity) { return entity.m_Visible != nullptr; });

    if (!b_all_position2d && !b_all_position3d)
    {
        ImGui::TextDisabled("No shared position field.");
    }

    if (!edit.m_bEditing)
    {
        edit.m_Ids = ids;
        edit.m_BeforeTransforms.clear();
        edit.m_BeforeTransforms.reserve(ids.size());
        for (const uint64_t id : ids)
        {
            for (GameEditor::FEditorEntityRef& entity : editor->GetEditorEntities())
            {
                if (entity.m_Id != id)
                {
                    continue;
                }
                edit.m_BeforeTransforms.push_back(SceneCommands::ReadTransform(MakeFullEntityEntry(entity)));
                break;
            }
        }
    }

    const auto find_entity = [editor](uint64_t id) -> GameEditor::FEditorEntityRef*
    {
        for (GameEditor::FEditorEntityRef& entity : editor->GetEditorEntities())
        {
            if (entity.m_Id == id)
            {
                return &entity;
            }
        }
        return nullptr;
    };

    const auto commit_field = [&](EEntityField field, const char* command_name)
    {
        std::vector<SceneCommands::FEntityTransformEntry> entries;
        entries.reserve(ids.size());
        for (std::size_t index = 0; index < ids.size(); ++index)
        {
            GameEditor::FEditorEntityRef* entity = find_entity(ids[index]);
            if (entity == nullptr)
            {
                continue;
            }
            SceneCommands::FEntityTransformEntry entry = MakeEntityEntry(*entity, field);
            if (index < edit.m_BeforeTransforms.size())
            {
                entry.m_Before = edit.m_BeforeTransforms[index];
            }
            entry.m_After = SceneCommands::ReadTransform(entry);
            entries.push_back(entry);
        }
        if (!entries.empty())
        {
            PushCommand<SceneCommands::EntityTransformCommand>(editor, std::move(entries), command_name);
        }
        edit.m_bEditing = false;
    };

    const auto apply_field = [&](EEntityField field, const SceneCommands::FEntityTransform& value)
    {
        for (const uint64_t id : ids)
        {
            if (GameEditor::FEditorEntityRef* entity = find_entity(id))
            {
                WriteEntityField(*entity, field, value);
            }
        }
    };

    if (b_all_position2d || b_all_position3d)
    {
        const EEntityField field = b_all_position2d ? EEntityField::Position2D : EEntityField::Position3D;
        FRowState row;
        if (b_all_position2d)
        {
            Vector2 value{};
            for (GameEditor::FEditorEntityRef& entity : editor->GetEditorEntities())
            {
                if (entity.m_Id == ids.back() && entity.m_Position != nullptr)
                {
                    value = *entity.m_Position;
                    break;
                }
            }
            if (Vector2Row("Position", "batch_entity_position", &value.x, "%.1f", &row))
            {
                SceneCommands::FEntityTransform transform;
                transform.m_Position = value;
                apply_field(field, transform);
            }
        }
        else
        {
            Vector3 value{};
            for (GameEditor::FEditorEntityRef& entity : editor->GetEditorEntities())
            {
                if (entity.m_Id == ids.back() && entity.m_Position3D != nullptr)
                {
                    value = *entity.m_Position3D;
                    break;
                }
            }
            if (Vector3Row("Position", "batch_entity_position3d", &value.x, "%.1f", &row))
            {
                SceneCommands::FEntityTransform transform;
                transform.m_Position3D = value;
                apply_field(field, transform);
            }
        }
        if (row.m_bActivated)
        {
            edit.m_bEditing = true;
            edit.m_Field = static_cast<int>(field);
        }
        if (edit.m_bEditing && edit.m_Field == static_cast<int>(field) && row.m_bDeactivatedAfterEdit)
        {
            commit_field(field, "Move Entities");
        }
        else if (edit.m_bEditing && edit.m_Field == static_cast<int>(field) && row.m_bDeactivated)
        {
            edit.m_bEditing = false;
        }
    }

    if (b_all_size2d || b_all_size3d)
    {
        const EEntityField field = b_all_size2d ? EEntityField::Size2D : EEntityField::Size3D;
        FRowState row;
        if (b_all_size2d)
        {
            Vector2 value{};
            for (GameEditor::FEditorEntityRef& entity : editor->GetEditorEntities())
            {
                if (entity.m_Id == ids.back() && entity.m_Size != nullptr)
                {
                    value = *entity.m_Size;
                    break;
                }
            }
            if (Vector2Row("Size", "batch_entity_size", &value.x, "%.1f", &row))
            {
                SceneCommands::FEntityTransform transform;
                transform.m_Size = value;
                apply_field(field, transform);
            }
        }
        else
        {
            Vector3 value{};
            for (GameEditor::FEditorEntityRef& entity : editor->GetEditorEntities())
            {
                if (entity.m_Id == ids.back() && entity.m_Size3D != nullptr)
                {
                    value = *entity.m_Size3D;
                    break;
                }
            }
            if (Vector3Row("Size", "batch_entity_size3d", &value.x, "%.1f", &row))
            {
                SceneCommands::FEntityTransform transform;
                transform.m_Size3D = value;
                apply_field(field, transform);
            }
        }
        if (row.m_bActivated)
        {
            edit.m_bEditing = true;
            edit.m_Field = static_cast<int>(field);
        }
        if (edit.m_bEditing && edit.m_Field == static_cast<int>(field) && row.m_bDeactivatedAfterEdit)
        {
            commit_field(field, "Edit Size");
        }
        else if (edit.m_bEditing && edit.m_Field == static_cast<int>(field) && row.m_bDeactivated)
        {
            edit.m_bEditing = false;
        }
    }

    if (b_all_rotation)
    {
        float value = 0.0f;
        for (GameEditor::FEditorEntityRef& entity : editor->GetEditorEntities())
        {
            if (entity.m_Id == ids.back() && entity.m_Rotation != nullptr)
            {
                value = *entity.m_Rotation;
                break;
            }
        }
        FRowState row;
        if (FloatRow("Rotation", "##batch_entity_rotation", &value, "%.1f deg", &row))
        {
            SceneCommands::FEntityTransform transform;
            transform.m_Rotation = value;
            apply_field(EEntityField::Rotation, transform);
        }
        if (row.m_bActivated)
        {
            edit.m_bEditing = true;
            edit.m_Field = static_cast<int>(EEntityField::Rotation);
        }
        if (edit.m_bEditing && edit.m_Field == static_cast<int>(EEntityField::Rotation) && row.m_bDeactivatedAfterEdit)
        {
            commit_field(EEntityField::Rotation, "Rotate Entities");
        }
        else if (edit.m_bEditing && edit.m_Field == static_cast<int>(EEntityField::Rotation) && row.m_bDeactivated)
        {
            edit.m_bEditing = false;
        }
    }

    if (b_all_visible)
    {
        bool value = true;
        for (GameEditor::FEditorEntityRef& entity : editor->GetEditorEntities())
        {
            if (entity.m_Id == ids.back() && entity.m_Visible != nullptr)
            {
                value = *entity.m_Visible;
                break;
            }
        }
        PropertyLabel("Visible");
        FRowState row;
        ImGui::Checkbox("##batch_entity_visible", &value);
        CaptureRow(&row);
        if (row.m_bActivated)
        {
            edit.m_bEditing = true;
            edit.m_Field = static_cast<int>(EEntityField::Visible);
        }
        if (row.m_bActivated || row.m_bDeactivatedAfterEdit)
        {
            SceneCommands::FEntityTransform transform;
            transform.m_bVisible = value;
            apply_field(EEntityField::Visible, transform);
        }
        if (edit.m_bEditing && edit.m_Field == static_cast<int>(EEntityField::Visible) && row.m_bDeactivatedAfterEdit)
        {
            commit_field(EEntityField::Visible, "Change Visibility");
        }
        else if (edit.m_bEditing && edit.m_Field == static_cast<int>(EEntityField::Visible) && row.m_bDeactivated)
        {
            edit.m_bEditing = false;
        }
    }
}
} // namespace

void InspectorPanel::Draw(GameEditor* editor)
{
    SCOPED_TIMER("panel_inspector");
    if (!ImGui::Begin(ICON_FA_SLIDERS " Inspector", &editor->m_bShowInspector))
    {
        ImGui::End();
        return;
    }

    if (!ProjectManager::b_HasOpenProject())
    {
        ImGui::TextDisabled("Open a project to inspect its scene.");
        ImGui::End();
        return;
    }

    if (editor->GetSelectionCount() > 1)
    {
        if (editor->m_SelectionKind == GameEditor::EEditorSelectionKind::SceneObject)
        {
            DrawSceneObjectBatchInspector(editor, *this, editor->GetSelectedSceneObjectIds(), editor->GetScene());
        }
        else if (editor->m_SelectionKind == GameEditor::EEditorSelectionKind::GameplayEntity)
        {
            DrawEntityBatchInspector(editor, *this, editor->GetSelectedEditorEntityIds());
        }
        ImGui::End();
        return;
    }

    if (GameEditor::FEditorEntityRef* entity = editor->GetSelectedEditorEntity())
    {
        DrawEntityInspector(editor, *this, *entity);
        ImGui::End();
        return;
    }

    if (t_SceneObject* object = editor->GetSelectedSceneObject())
    {
        DrawSceneObjectInspector(editor, *this, *object, editor->GetScene());
        ImGui::End();
        return;
    }

    MapManager* manager = editor->GetMapManager();
    if (manager != nullptr)
    {
        ImGui::Text("Current level: %s", std::string(manager->GetCurrentMapId()).c_str());
    }
    else
    {
        ImGui::Text("Current level: (none)");
    }

    ImGui::Separator();
    ImGui::Spacing();
    ImGui::TextDisabled(ICON_FA_HAND_POINTER "  Nothing selected");
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextDisabled("Pick a gameplay object in the Scene panel, or a sprite in the Viewport.");
    ImGui::PopTextWrapPos();

    ImGui::End();
}
