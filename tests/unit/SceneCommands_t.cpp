#include "Editor/CommandStack.h"
#include "Editor/SceneCommands.h"
#include "Engine/Scene.h"
#include "doctest/doctest.h"

#include <memory>
#include <string>
#include <vector>

namespace
{
struct FFakeEntity
{
    Vector2 m_Position{ 0.0f, 0.0f };
    Vector2 m_Size{ 16.0f, 16.0f };
    float m_Rotation = 0.0f;
    bool m_bVisible = true;
};

struct FFakeProperties
{
    float m_Speed = 3.0f;
    int m_Health = 10;
    bool m_bAlive = true;
    Vector2 m_Offset{ 1.0f, 2.0f };
};

t_SceneObject s_MakeObject(uint32_t id, std::string name, Vector2 position)
{
    t_SceneObject object;
    object.m_Id = id;
    object.m_Name = std::move(name);
    object.m_Position = position;
    return object;
}
} // namespace

TEST_CASE("SceneCommands: InsertObject preserves ids and bumps the next id")
{
    Scene scene;
    scene.AddObject("First");

    t_SceneObject restored = s_MakeObject(7, "Restored", { 3.0f, 4.0f });
    t_SceneObject& inserted = scene.InsertObject(std::move(restored));
    CHECK(inserted.m_Id == 7);
    CHECK(inserted.m_Name == "Restored");
    CHECK(inserted.m_Texture.id == 0);

    t_SceneObject& next = scene.AddObject("Next");
    CHECK(next.m_Id == 8);
}

TEST_CASE("SceneCommands: add applies, reverts, and redoes with the same id and order")
{
    Scene scene;
    scene.AddObject("First");
    CommandStack stack;

    auto command = std::make_unique<SceneCommands::SceneObjectAddCommand>(scene, "Crate", Vector2{ 10.0f, 20.0f });
    auto* raw = command.get();
    stack.Push(std::move(command));

    REQUIRE(scene.GetObjects().size() == 2);
    CHECK(raw->Id() == 2);
    CHECK(scene.GetObjects()[1].m_Id == 2);
    CHECK(scene.GetObjects()[1].m_Name == "Crate");
    CHECK(scene.GetObjects()[1].m_Position.x == doctest::Approx(10.0f));
    CHECK(scene.GetObjects()[1].m_Position.y == doctest::Approx(20.0f));
    CHECK(stack.CanUndo());

    REQUIRE(stack.Undo());
    CHECK(scene.GetObjects().size() == 1);
    CHECK(scene.FindObject(2) == nullptr);

    REQUIRE(stack.Redo());
    REQUIRE(scene.GetObjects().size() == 2);
    CHECK(scene.GetObjects()[1].m_Id == 2);
    CHECK(scene.GetObjects()[1].m_Position.x == doctest::Approx(10.0f));
}

TEST_CASE("SceneCommands: add is idempotent when applied twice")
{
    Scene scene;
    SceneCommands::SceneObjectAddCommand command(scene, "Once", Vector2{ 1.0f, 2.0f });
    command.Apply();
    command.Apply();
    CHECK(scene.GetObjects().size() == 1);
    command.Revert();
    command.Revert();
    CHECK(scene.GetObjects().empty());
}

TEST_CASE("SceneCommands: remove restores id, values, and slot exactly")
{
    Scene scene;
    t_SceneObject& first = scene.AddObject("First");
    const uint32_t first_id = first.m_Id;
    t_SceneObject& middle = scene.AddObject("Middle");
    const uint32_t middle_id = middle.m_Id;
    t_SceneObject& last = scene.AddObject("Last");
    const uint32_t last_id = last.m_Id;

    // AddObject can reallocate the vector; reacquire the middle object before
    // mutating it so the test never writes through a dangling reference.
    t_SceneObject* middle_object = scene.FindObject(middle_id);
    REQUIRE(middle_object != nullptr);
    middle_object->m_Position = { 5.0f, 6.0f };
    middle_object->m_Layer = 9;
    middle_object->m_bVisible = false;

    CommandStack stack;
    stack.Push(std::make_unique<SceneCommands::SceneObjectRemoveCommand>(scene, middle_id));

    REQUIRE(scene.GetObjects().size() == 2);
    CHECK(scene.GetObjects()[0].m_Id == first_id);
    CHECK(scene.GetObjects()[1].m_Id == last_id);

    REQUIRE(stack.Undo());
    REQUIRE(scene.GetObjects().size() == 3);
    CHECK(scene.GetObjects()[0].m_Id == first_id);
    CHECK(scene.GetObjects()[1].m_Id == middle_id);
    CHECK(scene.GetObjects()[2].m_Id == last_id);
    CHECK(scene.GetObjects()[1].m_Name == "Middle");
    CHECK(scene.GetObjects()[1].m_Position.x == doctest::Approx(5.0f));
    CHECK(scene.GetObjects()[1].m_Layer == 9);
    CHECK_FALSE(scene.GetObjects()[1].m_bVisible);

    REQUIRE(stack.Redo());
    CHECK(scene.GetObjects().size() == 2);
}

TEST_CASE("SceneCommands: removing several objects restores original order")
{
    Scene scene;
    for (int index = 0; index < 4; ++index)
    {
        scene.AddObject("Object" + std::to_string(index));
    }

    CommandStack stack;
    stack.Push(std::make_unique<SceneCommands::SceneObjectRemoveCommand>(scene, std::vector<uint32_t>{ 1, 3 }));

    REQUIRE(scene.GetObjects().size() == 2);
    CHECK(scene.GetObjects()[0].m_Id == 2);
    CHECK(scene.GetObjects()[1].m_Id == 4);

    REQUIRE(stack.Undo());
    REQUIRE(scene.GetObjects().size() == 4);
    for (uint32_t index = 0; index < 4; ++index)
    {
        CHECK(scene.GetObjects()[index].m_Id == index + 1);
    }
}

TEST_CASE("SceneCommands: duplicate keeps values, offsets the copy, and reuses its id on redo")
{
    Scene scene;
    t_SceneObject& source = scene.AddObject("Crate");
    source.m_Type = "decor";
    source.m_Position = { 10.0f, 20.0f };
    source.m_Scale = { 2.0f, 3.0f };
    const uint32_t source_id = source.m_Id;

    CommandStack stack;
    auto command = std::make_unique<SceneCommands::SceneObjectDuplicateCommand>(scene, source_id);
    auto* raw = command.get();
    stack.Push(std::move(command));

    REQUIRE(scene.GetObjects().size() == 2);
    REQUIRE(raw->CreatedIds().size() == 1);
    const uint32_t copy_id = raw->CreatedIds().front();
    CHECK(copy_id != source_id);
    const t_SceneObject* copy = scene.FindObject(copy_id);
    REQUIRE(copy != nullptr);
    CHECK(copy->m_Name == "Crate");
    CHECK(copy->m_Type == "decor");
    CHECK(copy->m_Position.x == doctest::Approx(26.0f));
    CHECK(copy->m_Position.y == doctest::Approx(36.0f));
    CHECK(copy->m_Scale.x == doctest::Approx(2.0f));
    CHECK(copy->m_Texture.id == 0);

    REQUIRE(stack.Undo());
    CHECK(scene.GetObjects().size() == 1);
    CHECK(scene.FindObject(copy_id) == nullptr);

    REQUIRE(stack.Redo());
    REQUIRE(scene.GetObjects().size() == 2);
    CHECK(scene.GetObjects()[1].m_Id == copy_id);
    CHECK(scene.GetObjects()[1].m_Position.x == doctest::Approx(26.0f));
}

TEST_CASE("SceneCommands: field edits apply and revert")
{
    Scene scene;
    t_SceneObject& object = scene.AddObject("Object");
    const uint32_t id = object.m_Id;
    CommandStack stack;

    const SceneCommands::FSceneObjectFieldValue name_before =
        SceneCommands::ReadField(object, SceneCommands::ESceneObjectField::Name);
    stack.Push(
        std::make_unique<SceneCommands::SceneObjectFieldCommand>(scene, id, SceneCommands::ESceneObjectField::Name,
            name_before, SceneCommands::FSceneObjectFieldValue::Text("Renamed"), "Rename Object"));
    CHECK(scene.FindObject(id)->m_Name == "Renamed");
    REQUIRE(stack.Undo());
    CHECK(scene.FindObject(id)->m_Name == "Object");
    REQUIRE(stack.Redo());
    CHECK(scene.FindObject(id)->m_Name == "Renamed");

    stack.Push(std::make_unique<SceneCommands::SceneObjectFieldCommand>(scene, id,
        SceneCommands::ESceneObjectField::Position, SceneCommands::FSceneObjectFieldValue::Vector({ 0.0f, 0.0f }),
        SceneCommands::FSceneObjectFieldValue::Vector({ 42.0f, -7.0f }), "Move Object"));
    CHECK(scene.FindObject(id)->m_Position.x == doctest::Approx(42.0f));
    REQUIRE(stack.Undo());
    CHECK(scene.FindObject(id)->m_Position.x == doctest::Approx(0.0f));

    stack.Push(std::make_unique<SceneCommands::SceneObjectFieldCommand>(scene, id,
        SceneCommands::ESceneObjectField::Layer, SceneCommands::FSceneObjectFieldValue::Int(0),
        SceneCommands::FSceneObjectFieldValue::Int(5), "Change Layer"));
    CHECK(scene.FindObject(id)->m_Layer == 5);
    REQUIRE(stack.Undo());
    CHECK(scene.FindObject(id)->m_Layer == 0);

    stack.Push(std::make_unique<SceneCommands::SceneObjectFieldCommand>(scene, id,
        SceneCommands::ESceneObjectField::Visible, SceneCommands::FSceneObjectFieldValue::Bool(true),
        SceneCommands::FSceneObjectFieldValue::Bool(false), "Hide Object"));
    CHECK_FALSE(scene.FindObject(id)->m_bVisible);
    REQUIRE(stack.Undo());
    CHECK(scene.FindObject(id)->m_bVisible);
}

TEST_CASE("SceneCommands: multi-object transform is one reversible command")
{
    Scene scene;
    t_SceneObject& first = scene.AddObject("First");
    t_SceneObject& second = scene.AddObject("Second");
    first.m_Position = { 1.0f, 2.0f };
    second.m_Position = { 3.0f, 4.0f };

    SceneCommands::FSceneObjectTransformEntry first_entry;
    first_entry.m_Id = first.m_Id;
    first_entry.m_Before = SceneCommands::ReadTransform(first);
    first_entry.m_After = first_entry.m_Before;
    first_entry.m_After.m_Position = { 11.0f, 12.0f };

    SceneCommands::FSceneObjectTransformEntry second_entry;
    second_entry.m_Id = second.m_Id;
    second_entry.m_Before = SceneCommands::ReadTransform(second);
    second_entry.m_After = second_entry.m_Before;
    second_entry.m_After.m_Position = { 13.0f, 14.0f };

    CommandStack stack;
    stack.Push(std::make_unique<SceneCommands::SceneObjectsTransformCommand>(
        scene, std::vector{ first_entry, second_entry }, "Move Objects"));

    CHECK(scene.FindObject(first.m_Id)->m_Position.x == doctest::Approx(11.0f));
    CHECK(scene.FindObject(second.m_Id)->m_Position.y == doctest::Approx(14.0f));

    REQUIRE(stack.Undo());
    CHECK(scene.FindObject(first.m_Id)->m_Position.x == doctest::Approx(1.0f));
    CHECK(scene.FindObject(second.m_Id)->m_Position.y == doctest::Approx(4.0f));

    REQUIRE(stack.Redo());
    CHECK(scene.FindObject(first.m_Id)->m_Position.x == doctest::Approx(11.0f));
}

TEST_CASE("SceneCommands: visibility command applies to a set and reverts each value")
{
    Scene scene;
    t_SceneObject& first = scene.AddObject("First");
    t_SceneObject& second = scene.AddObject("Second");
    second.m_bVisible = false;

    CommandStack stack;
    stack.Push(std::make_unique<SceneCommands::SceneObjectsVisibleCommand>(
        scene, std::vector<uint32_t>{ first.m_Id, second.m_Id }, true));

    CHECK(scene.FindObject(first.m_Id)->m_bVisible);
    CHECK(scene.FindObject(second.m_Id)->m_bVisible);

    REQUIRE(stack.Undo());
    CHECK(scene.FindObject(first.m_Id)->m_bVisible);
    CHECK_FALSE(scene.FindObject(second.m_Id)->m_bVisible);
}

TEST_CASE("SceneCommands: entity transform command writes only its pointers and reverts")
{
    FFakeEntity entity;
    entity.m_Position = { 5.0f, 6.0f };
    entity.m_Size = { 32.0f, 48.0f };
    entity.m_Rotation = 15.0f;
    entity.m_bVisible = true;

    SceneCommands::FEntityTransformEntry entry;
    entry.m_Position = &entity.m_Position;
    entry.m_Size = &entity.m_Size;
    entry.m_Rotation = &entity.m_Rotation;
    entry.m_Visible = &entity.m_bVisible;
    entry.m_Before = SceneCommands::ReadTransform(entry);
    entry.m_After = entry.m_Before;
    entry.m_After.m_Position = { 50.0f, 60.0f };
    entry.m_After.m_Size = { 64.0f, 96.0f };
    entry.m_After.m_Rotation = 90.0f;
    entry.m_After.m_bVisible = false;

    CommandStack stack;
    stack.Push(std::make_unique<SceneCommands::EntityTransformCommand>(std::vector{ entry }, "Move Entity"));

    CHECK(entity.m_Position.x == doctest::Approx(50.0f));
    CHECK(entity.m_Size.y == doctest::Approx(96.0f));
    CHECK(entity.m_Rotation == doctest::Approx(90.0f));
    CHECK_FALSE(entity.m_bVisible);

    REQUIRE(stack.Undo());
    CHECK(entity.m_Position.x == doctest::Approx(5.0f));
    CHECK(entity.m_Size.y == doctest::Approx(48.0f));
    CHECK(entity.m_Rotation == doctest::Approx(15.0f));
    CHECK(entity.m_bVisible);

    REQUIRE(stack.Redo());
    CHECK(entity.m_Position.x == doctest::Approx(50.0f));
}

TEST_CASE("SceneCommands: entity property command covers every property type")
{
    FFakeProperties properties;
    t_EntityProperty views[4];
    views[0].m_Name = "Speed";
    views[0].m_Type = EEntityPropertyType::Float;
    views[0].m_Data = &properties.m_Speed;
    views[1].m_Name = "Health";
    views[1].m_Type = EEntityPropertyType::Int;
    views[1].m_Data = &properties.m_Health;
    views[2].m_Name = "Alive";
    views[2].m_Type = EEntityPropertyType::Bool;
    views[2].m_Data = &properties.m_bAlive;
    views[3].m_Name = "Offset";
    views[3].m_Type = EEntityPropertyType::Vector2;
    views[3].m_Data = &properties.m_Offset;

    std::vector<SceneCommands::FEntityPropertyEntry> entries;
    for (t_EntityProperty& view : views)
    {
        SceneCommands::FEntityPropertyEntry entry;
        entry.m_Property = &view;
        entry.m_Before = SceneCommands::ReadPropertyValue(view);
        entry.m_After = entry.m_Before;
        switch (view.m_Type)
        {
            case EEntityPropertyType::Float:
                entry.m_After.m_Float = 9.5f;
                break;
            case EEntityPropertyType::Int:
                entry.m_After.m_Int = 42;
                break;
            case EEntityPropertyType::Bool:
                entry.m_After.m_bFlag = false;
                break;
            case EEntityPropertyType::Vector2:
                entry.m_After.m_Vector = { 7.0f, 8.0f };
                break;
        }
        entries.push_back(entry);
    }

    CommandStack stack;
    stack.Push(std::make_unique<SceneCommands::EntityPropertyCommand>(std::move(entries), "Edit Properties"));

    CHECK(properties.m_Speed == doctest::Approx(9.5f));
    CHECK(properties.m_Health == 42);
    CHECK_FALSE(properties.m_bAlive);
    CHECK(properties.m_Offset.x == doctest::Approx(7.0f));

    REQUIRE(stack.Undo());
    CHECK(properties.m_Speed == doctest::Approx(3.0f));
    CHECK(properties.m_Health == 10);
    CHECK(properties.m_bAlive);
    CHECK(properties.m_Offset.x == doctest::Approx(1.0f));

    REQUIRE(stack.Redo());
    CHECK(properties.m_Speed == doctest::Approx(9.5f));
}

TEST_CASE("SceneCommands: commands compose with CommandStack undo and redo order")
{
    Scene scene;
    CommandStack stack;

    auto add = std::make_unique<SceneCommands::SceneObjectAddCommand>(scene, "Object", Vector2{ 0.0f, 0.0f });
    auto* add_raw = add.get();
    stack.Push(std::move(add));
    const uint32_t id = add_raw->Id();

    stack.Push(std::make_unique<SceneCommands::SceneObjectFieldCommand>(scene, id,
        SceneCommands::ESceneObjectField::Name, SceneCommands::FSceneObjectFieldValue::Text("Object"),
        SceneCommands::FSceneObjectFieldValue::Text("Renamed"), "Rename Object"));
    stack.Push(std::make_unique<SceneCommands::SceneObjectFieldCommand>(scene, id,
        SceneCommands::ESceneObjectField::Position, SceneCommands::FSceneObjectFieldValue::Vector({ 0.0f, 0.0f }),
        SceneCommands::FSceneObjectFieldValue::Vector({ 100.0f, 100.0f }), "Move Object"));

    CHECK(stack.UndoCount() == 3);
    REQUIRE(scene.FindObject(id) != nullptr);
    CHECK(scene.FindObject(id)->m_Name == "Renamed");

    REQUIRE(stack.Undo());
    CHECK(scene.FindObject(id)->m_Position.x == doctest::Approx(0.0f));
    REQUIRE(stack.Undo());
    CHECK(scene.FindObject(id)->m_Name == "Object");
    REQUIRE(stack.Undo());
    CHECK(scene.FindObject(id) == nullptr);

    REQUIRE(stack.Redo());
    REQUIRE(scene.FindObject(id) != nullptr);
    CHECK(scene.FindObject(id)->m_Name == "Object");
    REQUIRE(stack.Redo());
    CHECK(scene.FindObject(id)->m_Name == "Renamed");
    REQUIRE(stack.Redo());
    CHECK(scene.FindObject(id)->m_Position.x == doctest::Approx(100.0f));
    CHECK_FALSE(stack.CanRedo());
}
