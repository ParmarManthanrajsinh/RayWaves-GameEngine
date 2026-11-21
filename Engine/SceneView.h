#pragma once
#include <cstdint>
#include <raylib.h>

// Host-owned, read-only view of one editor-authored scene object.
//
// This is the only scene shape that crosses the GameLogic module boundary.
// Strings point into host memory and are valid only for the duration of
// GameMap::SetSceneObjects(); maps copy what they need into their own storage.
struct t_SceneObjectView
{
    uint32_t m_Id = 0;
    const char* m_Name = nullptr;
    const char* m_Type = nullptr;
    const char* m_TexturePath = nullptr;
    Vector2 m_Position{ 0.0f, 0.0f };
    float m_Rotation = 0.0f;
    Vector2 m_Scale{ 1.0f, 1.0f };
    Rectangle m_Source{ 0.0f, 0.0f, 0.0f, 0.0f };
    Color m_Tint = WHITE;
    int m_Layer = 0;
    bool m_bVisible = true;
};
