#pragma once
#include <cstdint>
#include <raylib.h>
#include <string>
#include <string_view>

// A live gameplay object exposed to the editor. The game owns the object and
// the storage behind these pointers; the editor must never keep a pointer past
// the current frame. Selection is tracked by m_Id only.
struct t_EditorEntity
{
    uint64_t m_Id = 0;
    std::string m_Name;
    std::string m_Type;
    Vector2* m_Position = nullptr;
    Vector2* m_Size = nullptr;
    // Offset from m_Position to the editor selection rectangle's top-left.
    // Use it for entities whose position is a center point.
    Vector2 m_Origin{ 0.0f, 0.0f };
    float* m_Rotation = nullptr;
    bool* m_Visible = nullptr;

    // 3D maps expose a world position instead of a 2D screen-space one.
    Vector3* m_Position3D = nullptr;
    Vector3* m_Size3D = nullptr;
    Vector3 m_Origin3D{ 0.0f, 0.0f, 0.0f };
};

// Optional fields for GameMap::AddEntity. Every member is defaulted, so the
// common case stays a single call:
//     AddEntity("Player", &m_Position, &m_Size, { .Origin = { -32.0f, -32.0f } });
struct t_EntityOptions
{
    Vector2 Origin{ 0.0f, 0.0f };
    Vector3 Origin3D{ 0.0f, 0.0f, 0.0f };
    std::string_view Type{};
    float* Rotation = nullptr;
    bool* Visible = nullptr;
    uint64_t Id = 0;
};
