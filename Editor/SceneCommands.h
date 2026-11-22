#pragma once

#include "../Engine/EditorEntity.h"
#include "../Engine/Scene.h"
#include "CommandStack.h"
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// Undoable scene-authoring edits. These commands are deliberately independent
// of ImGui and GameEditor so they can be exercised by unit tests. They never own
// GPU handles: texture reloads are delegated to Scene.
namespace SceneCommands
{
enum class ESceneObjectField
{
    Name,
    Type,
    TexturePath,
    Position,
    Rotation,
    Scale,
    Layer,
    Visible
};

std::string_view FieldName(ESceneObjectField field);

// One field's value, tagged by which member the field uses. Unused members keep
// their default value so equality is a plain member comparison.
struct FSceneObjectFieldValue
{
    std::string m_Text;
    Vector2 m_Vector{ 0.0f, 0.0f };
    float m_Float = 0.0f;
    int m_Int = 0;
    bool m_bFlag = false;

    static FSceneObjectFieldValue Text(std::string value);
    static FSceneObjectFieldValue Vector(Vector2 value);
    static FSceneObjectFieldValue Float(float value);
    static FSceneObjectFieldValue Int(int value);
    static FSceneObjectFieldValue Bool(bool value);
};

FSceneObjectFieldValue ReadField(const t_SceneObject& object, ESceneObjectField field);
bool b_Equal(const FSceneObjectFieldValue& left, const FSceneObjectFieldValue& right);

struct FSceneObjectTransform
{
    Vector2 m_Position{ 0.0f, 0.0f };
    float m_Rotation = 0.0f;
    Vector2 m_Scale{ 1.0f, 1.0f };
};

struct FSceneObjectTransformEntry
{
    uint32_t m_Id = 0;
    FSceneObjectTransform m_Before{};
    FSceneObjectTransform m_After{};
};

FSceneObjectTransform ReadTransform(const t_SceneObject& object);

// Adds one object. The id is assigned on first Apply and reused on redo.
class SceneObjectAddCommand : public IEditorCommand
{
public:
    SceneObjectAddCommand(Scene& scene, std::string name, Vector2 position, std::string texture_path = std::string());

    std::string_view Name() const override { return "Add Object"; }
    void Apply() override;
    void Revert() override;

    // Valid after Apply; 0 before the first apply.
    uint32_t Id() const { return m_Id; }

private:
    Scene& m_Scene;
    std::string m_ObjectName;
    std::string m_TexturePath;
    Vector2 m_Position{ 0.0f, 0.0f };
    uint32_t m_Id = 0;
    std::size_t m_Index = 0;
};

// Removes one or more objects and restores their exact ids, values, and order.
class SceneObjectRemoveCommand : public IEditorCommand
{
public:
    explicit SceneObjectRemoveCommand(Scene& scene, uint32_t id);
    SceneObjectRemoveCommand(Scene& scene, std::vector<uint32_t> ids);

    std::string_view Name() const override { return m_Entries.size() > 1 ? "Remove Objects" : "Remove Object"; }
    void Apply() override;
    void Revert() override;

private:
    struct FEntry
    {
        uint32_t m_Id = 0;
        std::size_t m_Index = 0;
        t_SceneObject m_Snapshot;
    };

    Scene& m_Scene;
    std::vector<FEntry> m_Entries;
};

// Copies one or more objects and keeps the generated ids stable across redo.
class SceneObjectDuplicateCommand : public IEditorCommand
{
public:
    explicit SceneObjectDuplicateCommand(Scene& scene, uint32_t id, Vector2 offset = { 16.0f, 16.0f });
    SceneObjectDuplicateCommand(Scene& scene, std::vector<uint32_t> ids, Vector2 offset = { 16.0f, 16.0f });

    std::string_view Name() const override { return m_Sources.size() > 1 ? "Duplicate Objects" : "Duplicate Object"; }
    void Apply() override;
    void Revert() override;

    const std::vector<uint32_t>& CreatedIds() const { return m_CreatedIds; }

private:
    Scene& m_Scene;
    Vector2 m_Offset{ 16.0f, 16.0f };
    std::vector<t_SceneObject> m_Sources;
    std::vector<uint32_t> m_CreatedIds;
    std::vector<std::size_t> m_Indices;
};

// Sets a single authored field. Texture path changes request a reload.
class SceneObjectFieldCommand : public IEditorCommand
{
public:
    SceneObjectFieldCommand(Scene& scene, uint32_t id, ESceneObjectField field, FSceneObjectFieldValue before,
        FSceneObjectFieldValue after, std::string_view command_name = std::string_view());

    std::string_view Name() const override { return m_Name; }
    void Apply() override;
    void Revert() override;

private:
    void SetValue(const FSceneObjectFieldValue& value);

    Scene& m_Scene;
    uint32_t m_Id = 0;
    ESceneObjectField m_Field = ESceneObjectField::Name;
    FSceneObjectFieldValue m_Before;
    FSceneObjectFieldValue m_After;
    std::string m_Name;
};

// One command for a whole viewport drag: every entry carries its own before and
// after transform so the delta never compounds across frames.
class SceneObjectsTransformCommand : public IEditorCommand
{
public:
    SceneObjectsTransformCommand(Scene& scene, std::vector<FSceneObjectTransformEntry> entries,
        std::string_view command_name = "Transform Objects");

    std::string_view Name() const override { return m_Name; }
    void Apply() override;
    void Revert() override;

private:
    void ApplyTransforms(bool b_After);

    Scene& m_Scene;
    std::vector<FSceneObjectTransformEntry> m_Entries;
    std::string m_Name;
};

// Sets visibility for a selection as one undo step.
class SceneObjectsVisibleCommand : public IEditorCommand
{
public:
    SceneObjectsVisibleCommand(
        Scene& scene, std::vector<uint32_t> ids, bool b_Visible, std::string_view command_name = "Change Visibility");

    std::string_view Name() const override { return m_Name; }
    void Apply() override;
    void Revert() override;

private:
    void ApplyVisibility(bool b_After);

    Scene& m_Scene;
    std::vector<std::pair<uint32_t, bool>> m_Objects;
    bool m_bAfter = true;
    std::string m_Name;
};

// Gameplay entity transform fields. Only non-null pointers are written, so the
// same command type can represent a position edit, a size edit, or a whole
// drag.
struct FEntityTransform
{
    Vector2 m_Position{ 0.0f, 0.0f };
    Vector3 m_Position3D{ 0.0f, 0.0f, 0.0f };
    Vector2 m_Size{ 0.0f, 0.0f };
    Vector3 m_Size3D{ 0.0f, 0.0f, 0.0f };
    float m_Rotation = 0.0f;
    bool m_bVisible = true;
};

struct FEntityTransformEntry
{
    Vector2* m_Position = nullptr;
    Vector3* m_Position3D = nullptr;
    Vector2* m_Size = nullptr;
    Vector3* m_Size3D = nullptr;
    float* m_Rotation = nullptr;
    bool* m_Visible = nullptr;
    FEntityTransform m_Before{};
    FEntityTransform m_After{};
};

FEntityTransform ReadTransform(const FEntityTransformEntry& entry);

class EntityTransformCommand : public IEditorCommand
{
public:
    explicit EntityTransformCommand(
        std::vector<FEntityTransformEntry> entries, std::string_view command_name = "Move Entity");

    std::string_view Name() const override { return m_Name; }
    void Apply() override;
    void Revert() override;

private:
    void ApplyValues(bool b_After);

    std::vector<FEntityTransformEntry> m_Entries;
    std::string m_Name;
};

struct FEntityPropertyValue
{
    float m_Float = 0.0f;
    int m_Int = 0;
    bool m_bFlag = false;
    Vector2 m_Vector{ 0.0f, 0.0f };
};

FEntityPropertyValue ReadPropertyValue(const t_EntityProperty& property);
void WritePropertyValue(const t_EntityProperty& property, const FEntityPropertyValue& value);

struct FEntityPropertyEntry
{
    t_EntityProperty* m_Property = nullptr;
    FEntityPropertyValue m_Before{};
    FEntityPropertyValue m_After{};
};

class EntityPropertyCommand : public IEditorCommand
{
public:
    explicit EntityPropertyCommand(
        std::vector<FEntityPropertyEntry> entries, std::string_view command_name = "Edit Property");

    std::string_view Name() const override { return m_Name; }
    void Apply() override;
    void Revert() override;

private:
    void ApplyValues(bool b_After);

    std::vector<FEntityPropertyEntry> m_Entries;
    std::string m_Name;
};
} // namespace SceneCommands
