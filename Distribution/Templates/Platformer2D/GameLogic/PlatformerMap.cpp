#include "PlatformerMap.h"
#include <algorithm>
#include <iostream>
#include <cmath>

PlatformerMap::PlatformerMap()
    : m_PlayerPos{ 400.0f, 300.0f }
    , m_Velocity{ 0.0f, 0.0f }
    , m_Speed(400.0f)
    , m_JumpForce(700.0f)
    , m_Gravity(1200.0f)
    , m_IsGrounded(false)
{
    // Make platforms larger and theme them darker
    m_Platforms[0] = { { -500, 600, 3000, 200 }, { 35, 35, 35, 255 } }; // Ground
    m_Platforms[1] = { { 300, 420, 300, 40 }, { 45, 45, 45, 255 } };
    m_Platforms[2] = { { 750, 250, 300, 40 }, { 45, 45, 45, 255 } };
}

void PlatformerMap::Initialize()
{
    std::cout << "PlatformerMap initialized\n";
    m_Camera.Initialize(m_PlayerPos, 1.0f);
    m_Camera.SetBounds(-500, 2500, -500, 1000);
}

void PlatformerMap::Update(float delta_time)
{
    // Horizontal Movement
    if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D))
    {
        m_Velocity.x = m_Speed;
    }
    else if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A))
    {
        m_Velocity.x = -m_Speed;
    }
    else
    {
        m_Velocity.x = 0;
    }

    // Jumping
    if ((IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W) || IsKeyPressed(KEY_SPACE)) && m_IsGrounded)
    {
        m_Velocity.y = -m_JumpForce;
        m_IsGrounded = false;
    }

    // Apply Gravity
    m_Velocity.y += m_Gravity * delta_time;

    // Update Position
    m_PlayerPos.x += m_Velocity.x * delta_time;
    m_PlayerPos.y += m_Velocity.y * delta_time;

    // Collision Detection (64x64 player size)
    Rectangle playerRect = { m_PlayerPos.x, m_PlayerPos.y, 64, 64 };
    m_IsGrounded = false;

    for (auto & m_Platform : m_Platforms)
    {
        if (CheckCollisionRecs(playerRect, m_Platform.rect))
        {
            // Simple top collision check (landing)
            if (m_Velocity.y > 0 && m_PlayerPos.y + 64 <= m_Platform.rect.y + (m_Velocity.y * delta_time) + 10)
            {
                m_PlayerPos.y = m_Platform.rect.y - 64;
                m_Velocity.y = 0;
                m_IsGrounded = true;
            }
        }
    }

    // Keep player in bounds roughly
    m_PlayerPos.x = std::max<float>(m_PlayerPos.x, -500);
    m_PlayerPos.x = std::min<float>(m_PlayerPos.x, 2436);

    // Update Camera
    m_Camera.UpdateViewport(GetScreenWidth(), GetScreenHeight());
    m_Camera.FollowTarget({ m_PlayerPos.x + 32, m_PlayerPos.y + 32 }, delta_time, 7.0f);
}

void PlatformerMap::Draw()
{
    ClearBackground({ 24, 24, 24, 255 }); // Dark theme background

    m_Camera.Begin();

    // Draw Grid Pattern for Engine Feel
    Color grid_color = { 40, 40, 40, 255 };
    for (int i = -1000; i < 3000; i += 64)
    {
        DrawLine(i, -1000, i, 1500, grid_color);
        DrawLine(-1000, i, 3000, i, grid_color);
    }

    // Draw Environment
    for (auto & m_Platform : m_Platforms)
    {
        DrawRectangleRec(m_Platform.rect, m_Platform.color);
        // Add neon red accent line on top of platforms
        DrawRectangle(static_cast<int>(m_Platform.rect.x), static_cast<int>(m_Platform.rect.y), static_cast<int>(m_Platform.rect.width), 4, { 50, 200, 100, 255 });
    }

    // Draw instructions in world
    DrawText("Use WASD or Arrow Keys to Move and Jump", 50, 500, 30, LIGHTGRAY);

    // Draw Player
    Rectangle player_rec = { m_PlayerPos.x, m_PlayerPos.y, 64, 64 };
    DrawRectangleRec(player_rec, { 50, 150, 255, 255 }); // Nice engine blue
    DrawRectangleLinesEx(player_rec, 3, WHITE);

    GameCamera::End();
}
