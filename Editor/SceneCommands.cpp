#include "SceneCommands.h"
#include <algorithm>
#include <utility>

namespace SceneCommands
{
namespace
{
// Snapshots are value-only. Dropping the handle keeps every command free of
// GPU ownership; Scene::InsertObjectAt also resets it on the way back in.
t_SceneObject s_MakeSnapshot(const t_SceneObject& object)
{
    t_SceneObject snapshot = object;
    snapshot.m_Texture = {};
    snapshot.m_bTextureMissing = false;
    return snapshot;
}

bool s_bContainsId(const std::vector<uint32_t>& ids, uint32_t id)
{
    return std::ranges::find(ids, id) != ids.end();
}
} // namespace

std::string_view FieldName(ESceneObjectField field)
{
    switch (field)
    {
        case ESceneObjectField::Name:
            return "Name";
        case ESceneObjectField::Type:
            return "Type";
        case ESceneObjectField::TexturePath:
            return "Texture";
        case ESceneObjectField::Position:
            return "Position";
        case ESceneObjectField::Rotation:
            return "Rotation";
        case ESceneObjectField::Scale:
            return "Scale";
        case ESceneObjectField::Layer:
            return "Layer";
        case ESceneObjectField::Visible:
            return "Visibility";
    }
    return "Field";
}

FSceneObjectFieldValue FSceneObjectFieldValue::Text(std::string value)
{
    FSceneObjectFieldValue result;
    result.m_Text = std::move(value);
    return result;
}

FSceneObjectFieldValue FSceneObjectFieldValue::Vector(Vector2 value)
{
    FSceneObjectFieldValue result;
    result.m_Vector = value;
    return result;
}

FSceneObjectFieldValue FSceneObjectFieldValue::Float(float value)
{
    FSceneObjectFieldValue result;
    result.m_Float = value;
    return result;
}

FSceneObjectFieldValue FSceneObjectFieldValue::Int(int value)
{
    FSceneObjectFieldValue result;
    result.m_Int = value;
    return result;
}

FSceneObjectFieldValue FSceneObjectFieldValue::Bool(bool value)
{
    FSceneObjectFieldValue result;
    result.m_bFlag = value;
    return result;
}

FSceneObjectFieldValue ReadField(const t_SceneObject& object, ESceneObjectField field)
{
    switch (field)
    {
        case ESceneObjectField::Name:
            return FSceneObjectFieldValue::Text(object.m_Name);
        case ESceneObjectField::Type:
            return FSceneObjectFieldValue::Text(object.m_Type);
        case ESceneObjectField::TexturePath:
            return FSceneObjectFieldValue::Text(object.m_TexturePath);
        case ESceneObjectField::Position:
            return FSceneObjectFieldValue::Vector(object.m_Position);
        case ESceneObjectField::Rotation:
            return FSceneObjectFieldValue::Float(object.m_Rotation);
        case ESceneObjectField::Scale:
            return FSceneObjectFieldValue::Vector(object.m_Scale);
        case ESceneObjectField::Layer:
            return FSceneObjectFieldValue::Int(object.m_Layer);
        case ESceneObjectField::Visible:
            return FSceneObjectFieldValue::Bool(object.m_bVisible);
    }
    return {};
}

bool b_Equal(const FSceneObjectFieldValue& left, const FSceneObjectFieldValue& right)
{
    return left.m_Text == right.m_Text && left.m_Vector.x == right.m_Vector.x && left.m_Vector.y == right.m_Vector.y &&
        left.m_Float == right.m_Float && left.m_Int == right.m_Int && left.m_bFlag == right.m_bFlag;
}

FSceneObjectTransform ReadTransform(const t_SceneObject& object)
{
    return { object.m_Position, object.m_Rotation, object.m_Scale };
}

SceneObjectAddCommand::SceneObjectAddCommand(Scene& scene, std::string name, Vector2 position, std::string texture_path)
    : m_Scene(scene)
    , m_ObjectName(name.empty() ? "Object" : std::move(name))
    , m_TexturePath(std::move(texture_path))
    , m_Position(position)
{
}

void SceneObjectAddCommand::Apply()
{
    if (m_Id != 0 && m_Scene.FindObject(m_Id) != nullptr)
    {
        return;
    }

    t_SceneObject object;
    object.m_Name = m_ObjectName;
    object.m_TexturePath = m_TexturePath;
    object.m_Position = m_Position;

    if (m_Id == 0)
    {
        t_SceneObject& inserted = m_Scene.InsertObject(std::move(object));
        m_Id = inserted.m_Id;
        m_Index = m_Scene.IndexOfObject(m_Id);
    }
    else
    {
        object.m_Id = m_Id;
        m_Scene.InsertObjectAt(m_Index, std::move(object));
    }

    if (!m_TexturePath.empty())
    {
        m_Scene.ReloadTexture(m_Id);
    }
    m_Scene.MarkDirty();
}

void SceneObjectAddCommand::Revert()
{
    m_Scene.b_RemoveObject(m_Id);
}

SceneObjectRemoveCommand::SceneObjectRemoveCommand(Scene& scene, uint32_t id)
    : SceneObjectRemoveCommand(scene, std::vector<uint32_t>{ id })
{
}

SceneObjectRemoveCommand::SceneObjectRemoveCommand(Scene& scene, std::vector<uint32_t> ids) : m_Scene(scene)
{
    for (const uint32_t id : ids)
    {
        if (id == 0)
        {
            continue;
        }

        const t_SceneObject* object = m_Scene.FindObject(id);
        if (object == nullptr)
        {
            continue;
        }

        FEntry entry;
        entry.m_Id = id;
        entry.m_Index = m_Scene.IndexOfObject(id);
        entry.m_Snapshot = s_MakeSnapshot(*object);
        m_Entries.push_back(std::move(entry));
    }

    std::ranges::sort(m_Entries, {}, &FEntry::m_Index);
}

void SceneObjectRemoveCommand::Apply()
{
    for (const FEntry& entry : m_Entries)
    {
        m_Scene.b_RemoveObject(entry.m_Id);
    }
}

void SceneObjectRemoveCommand::Revert()
{
    // Ascending index order keeps each snapshot's original slot correct as the
    // earlier entries are reinserted.
    for (const FEntry& entry : m_Entries)
    {
        if (m_Scene.FindObject(entry.m_Id) != nullptr)
        {
            continue;
        }
        m_Scene.InsertObjectAt(entry.m_Index, entry.m_Snapshot);
    }
}

SceneObjectDuplicateCommand::SceneObjectDuplicateCommand(Scene& scene, uint32_t id, Vector2 offset)
    : SceneObjectDuplicateCommand(scene, std::vector<uint32_t>{ id }, offset)
{
}

SceneObjectDuplicateCommand::SceneObjectDuplicateCommand(Scene& scene, std::vector<uint32_t> ids, Vector2 offset)
    : m_Scene(scene), m_Offset(offset)
{
    for (const uint32_t id : ids)
    {
        if (id == 0 || s_bContainsId(m_CreatedIds, id))
        {
            continue;
        }

        const t_SceneObject* source = m_Scene.FindObject(id);
        if (source == nullptr)
        {
            continue;
        }

        t_SceneObject snapshot = s_MakeSnapshot(*source);
        snapshot.m_Id = 0;
        snapshot.m_Position.x += m_Offset.x;
        snapshot.m_Position.y += m_Offset.y;
        m_Sources.push_back(std::move(snapshot));
        m_CreatedIds.push_back(0);
        m_Indices.push_back(0);
    }
}

void SceneObjectDuplicateCommand::Apply()
{
    for (std::size_t index = 0; index < m_Sources.size(); ++index)
    {
        if (m_CreatedIds[index] != 0 && m_Scene.FindObject(m_CreatedIds[index]) != nullptr)
        {
            continue;
        }

        t_SceneObject snapshot = m_Sources[index];
        if (m_CreatedIds[index] == 0)
        {
            t_SceneObject& inserted = m_Scene.InsertObject(std::move(snapshot));
            m_CreatedIds[index] = inserted.m_Id;
            m_Indices[index] = m_Scene.IndexOfObject(inserted.m_Id);
        }
        else
        {
            snapshot.m_Id = m_CreatedIds[index];
            m_Scene.InsertObjectAt(m_Indices[index], std::move(snapshot));
        }

        m_Scene.ReloadTexture(m_CreatedIds[index]);
    }
    m_Scene.MarkDirty();
}

void SceneObjectDuplicateCommand::Revert()
{
    for (const uint32_t id : m_CreatedIds)
    {
        m_Scene.b_RemoveObject(id);
    }
}

SceneObjectFieldCommand::SceneObjectFieldCommand(Scene& scene, uint32_t id, ESceneObjectField field,
    FSceneObjectFieldValue before, FSceneObjectFieldValue after, std::string_view command_name)
    : m_Scene(scene), m_Id(id), m_Field(field), m_Before(std::move(before)), m_After(std::move(after))
{
    if (!command_name.empty())
    {
        m_Name = std::string(command_name);
    }
    else
    {
        m_Name = std::string("Edit ") + std::string(FieldName(field));
    }
}

void SceneObjectFieldCommand::Apply()
{
    SetValue(m_After);
}

void SceneObjectFieldCommand::Revert()
{
    SetValue(m_Before);
}

void SceneObjectFieldCommand::SetValue(const FSceneObjectFieldValue& value)
{
    t_SceneObject* object = m_Scene.FindObject(m_Id);
    if (object == nullptr)
    {
        return;
    }

    switch (m_Field)
    {
        case ESceneObjectField::Name:
            object->m_Name = value.m_Text;
            break;
        case ESceneObjectField::Type:
            object->m_Type = value.m_Text;
            break;
        case ESceneObjectField::TexturePath:
            object->m_TexturePath = value.m_Text;
            m_Scene.ReloadTexture(m_Id);
            break;
        case ESceneObjectField::Position:
            object->m_Position = value.m_Vector;
            break;
        case ESceneObjectField::Rotation:
            object->m_Rotation = value.m_Float;
            break;
        case ESceneObjectField::Scale:
            object->m_Scale = value.m_Vector;
            break;
        case ESceneObjectField::Layer:
            object->m_Layer = value.m_Int;
            break;
        case ESceneObjectField::Visible:
            object->m_bVisible = value.m_bFlag;
            break;
    }

    m_Scene.MarkDirty();
}

SceneObjectsTransformCommand::SceneObjectsTransformCommand(
    Scene& scene, std::vector<FSceneObjectTransformEntry> entries, std::string_view command_name)
    : m_Scene(scene), m_Entries(std::move(entries)), m_Name(command_name)
{
}

void SceneObjectsTransformCommand::Apply()
{
    ApplyTransforms(true);
}

void SceneObjectsTransformCommand::Revert()
{
    ApplyTransforms(false);
}

void SceneObjectsTransformCommand::ApplyTransforms(bool b_After)
{
    for (const FSceneObjectTransformEntry& entry : m_Entries)
    {
        t_SceneObject* object = m_Scene.FindObject(entry.m_Id);
        if (object == nullptr)
        {
            continue;
        }

        const FSceneObjectTransform& transform = b_After ? entry.m_After : entry.m_Before;
        object->m_Position = transform.m_Position;
        object->m_Rotation = transform.m_Rotation;
        object->m_Scale = transform.m_Scale;
    }
    m_Scene.MarkDirty();
}

SceneObjectsVisibleCommand::SceneObjectsVisibleCommand(
    Scene& scene, std::vector<uint32_t> ids, bool b_Visible, std::string_view command_name)
    : m_Scene(scene), m_bAfter(b_Visible), m_Name(command_name)
{
    for (const uint32_t id : ids)
    {
        const t_SceneObject* object = m_Scene.FindObject(id);
        if (object == nullptr)
        {
            continue;
        }
        m_Objects.emplace_back(id, object->m_bVisible);
    }
}

void SceneObjectsVisibleCommand::Apply()
{
    ApplyVisibility(true);
}

void SceneObjectsVisibleCommand::Revert()
{
    ApplyVisibility(false);
}

void SceneObjectsVisibleCommand::ApplyVisibility(bool b_After)
{
    for (const auto& [id, b_Before] : m_Objects)
    {
        if (t_SceneObject* object = m_Scene.FindObject(id))
        {
            object->m_bVisible = b_After ? m_bAfter : b_Before;
        }
    }
    m_Scene.MarkDirty();
}

FEntityTransform ReadTransform(const FEntityTransformEntry& entry)
{
    FEntityTransform transform;
    if (entry.m_Position != nullptr)
    {
        transform.m_Position = *entry.m_Position;
    }
    if (entry.m_Position3D != nullptr)
    {
        transform.m_Position3D = *entry.m_Position3D;
    }
    if (entry.m_Size != nullptr)
    {
        transform.m_Size = *entry.m_Size;
    }
    if (entry.m_Size3D != nullptr)
    {
        transform.m_Size3D = *entry.m_Size3D;
    }
    if (entry.m_Rotation != nullptr)
    {
        transform.m_Rotation = *entry.m_Rotation;
    }
    if (entry.m_Visible != nullptr)
    {
        transform.m_bVisible = *entry.m_Visible;
    }
    return transform;
}

EntityTransformCommand::EntityTransformCommand(
    std::vector<FEntityTransformEntry> entries, std::string_view command_name)
    : m_Entries(std::move(entries)), m_Name(command_name)
{
}

void EntityTransformCommand::Apply()
{
    ApplyValues(true);
}

void EntityTransformCommand::Revert()
{
    ApplyValues(false);
}

void EntityTransformCommand::ApplyValues(bool b_After)
{
    for (const FEntityTransformEntry& entry : m_Entries)
    {
        const FEntityTransform& value = b_After ? entry.m_After : entry.m_Before;
        if (entry.m_Position != nullptr)
        {
            *entry.m_Position = value.m_Position;
        }
        if (entry.m_Position3D != nullptr)
        {
            *entry.m_Position3D = value.m_Position3D;
        }
        if (entry.m_Size != nullptr)
        {
            *entry.m_Size = value.m_Size;
        }
        if (entry.m_Size3D != nullptr)
        {
            *entry.m_Size3D = value.m_Size3D;
        }
        if (entry.m_Rotation != nullptr)
        {
            *entry.m_Rotation = value.m_Rotation;
        }
        if (entry.m_Visible != nullptr)
        {
            *entry.m_Visible = value.m_bVisible;
        }
    }
}

FEntityPropertyValue ReadPropertyValue(const t_EntityProperty& property)
{
    FEntityPropertyValue value;
    if (property.m_Data == nullptr)
    {
        return value;
    }

    switch (property.m_Type)
    {
        case EEntityPropertyType::Float:
            value.m_Float = *static_cast<const float*>(property.m_Data);
            break;
        case EEntityPropertyType::Int:
            value.m_Int = *static_cast<const int*>(property.m_Data);
            break;
        case EEntityPropertyType::Bool:
            value.m_bFlag = *static_cast<const bool*>(property.m_Data);
            break;
        case EEntityPropertyType::Vector2:
            value.m_Vector = *static_cast<const Vector2*>(property.m_Data);
            break;
    }
    return value;
}

void WritePropertyValue(const t_EntityProperty& property, const FEntityPropertyValue& value)
{
    if (property.m_Data == nullptr)
    {
        return;
    }

    switch (property.m_Type)
    {
        case EEntityPropertyType::Float:
            *static_cast<float*>(property.m_Data) = value.m_Float;
            break;
        case EEntityPropertyType::Int:
            *static_cast<int*>(property.m_Data) = value.m_Int;
            break;
        case EEntityPropertyType::Bool:
            *static_cast<bool*>(property.m_Data) = value.m_bFlag;
            break;
        case EEntityPropertyType::Vector2:
            *static_cast<Vector2*>(property.m_Data) = value.m_Vector;
            break;
    }
}

EntityPropertyCommand::EntityPropertyCommand(std::vector<FEntityPropertyEntry> entries, std::string_view command_name)
    : m_Entries(std::move(entries)), m_Name(command_name)
{
}

void EntityPropertyCommand::Apply()
{
    ApplyValues(true);
}

void EntityPropertyCommand::Revert()
{
    ApplyValues(false);
}

void EntityPropertyCommand::ApplyValues(bool b_After)
{
    for (const FEntityPropertyEntry& entry : m_Entries)
    {
        if (entry.m_Property == nullptr)
        {
            continue;
        }
        WritePropertyValue(*entry.m_Property, b_After ? entry.m_After : entry.m_Before);
    }
}
} // namespace SceneCommands
