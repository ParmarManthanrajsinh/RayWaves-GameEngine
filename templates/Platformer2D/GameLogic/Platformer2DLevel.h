#pragma once
#include "rw.h"
#include <algorithm>

// 2D platformer starter built on the simple rw API.
//
// Movement, jumping, and collision are the only "systems" here: the player is a
// plain rw::Entity, platforms are rectangles, and the editor sees the player
// automatically.
class Platformer2DLevel : public rw::Level
{
public:
    Platformer2DLevel() : rw::Level("Platformer2D")
    {
        SetBackground({ 24, 24, 24, 255 });

        // Larger, darker platforms with a green accent line drawn on top.
        m_Platforms[0] = { { -500.0f, 600.0f, 3000.0f, 200.0f }, { 35, 35, 35, 255 } };
        m_Platforms[1] = { { 300.0f, 420.0f, 300.0f, 40.0f }, { 45, 45, 45, 255 } };
        m_Platforms[2] = { { 750.0f, 250.0f, 300.0f, 40.0f }, { 45, 45, 45, 255 } };
    }

protected:
    void OnStart() override
    {
        m_Velocity = { 0.0f, 0.0f };
        m_bGrounded = false;

        m_Player = Spawn("Player", { 400.0f, 300.0f }, { 64.0f, 64.0f }, rw::Origin::TopLeft, "Player");

        Cam().SetZoom(1.0f);
        Cam().SetBounds(-500.0f, 2500.0f, -500.0f, 1000.0f);
        Cam().Snap(PlayerCenter());
    }

    void OnUpdate(float delta_time) override
    {
        HandleInput();
        ApplyPhysics(delta_time);
        ResolveCollisions(delta_time);

        constexpr float LevelLeft = -500.0f;
        constexpr float LevelRight = 2436.0f;
        m_Player->position.x = std::clamp(m_Player->position.x, LevelLeft, LevelRight);

        Cam().Follow(PlayerCenter(), delta_time, 7.0f);
    }

    void OnDraw() override
    {
        const Color GridColor{ 40, 40, 40, 255 };
        for (int grid = -1000; grid < 3000; grid += 64)
        {
            DrawLine(grid, -1000, grid, 1500, GridColor);
            DrawLine(-1000, grid, 3000, grid, GridColor);
        }

        for (const Platform& platform : m_Platforms)
        {
            DrawRectangleRec(platform.rect, platform.color);
            DrawRectangle(static_cast<int>(platform.rect.x), static_cast<int>(platform.rect.y),
                static_cast<int>(platform.rect.width), 4, { 50, 200, 100, 255 });
        }

        DrawText("Use WASD or Arrow Keys to Move and Jump", 50, 500, 30, LIGHTGRAY);

        const Rectangle player_rect = PlayerRect();
        DrawRectangleRec(player_rect, { 50, 150, 255, 255 });
        DrawRectangleLinesEx(player_rect, 3.0f, WHITE);
    }

    void OnDrawUI() override { DrawFPS(GetRenderWidth() - 90, 20); }

private:
    struct Platform
    {
        Rectangle rect;
        Color color;
    };

    Rectangle PlayerRect() const
    {
        return { m_Player->position.x, m_Player->position.y, m_Player->size.x, m_Player->size.y };
    }

    Vector2 PlayerCenter() const
    {
        return { m_Player->position.x + (m_Player->size.x * 0.5f), m_Player->position.y + (m_Player->size.y * 0.5f) };
    }

    void HandleInput()
    {
        if (rw::IsKeyDown(KEY_RIGHT) || rw::IsKeyDown(KEY_D))
        {
            m_Velocity.x = c_Speed;
        }
        else if (rw::IsKeyDown(KEY_LEFT) || rw::IsKeyDown(KEY_A))
        {
            m_Velocity.x = -c_Speed;
        }
        else
        {
            m_Velocity.x = 0.0f;
        }

        if ((rw::IsKeyPressed(KEY_UP) || rw::IsKeyPressed(KEY_W) || rw::IsKeyPressed(KEY_SPACE)) && m_bGrounded)
        {
            m_Velocity.y = -c_JumpForce;
            m_bGrounded = false;
        }
    }

    void ApplyPhysics(float delta_time)
    {
        m_Velocity.y += c_Gravity * delta_time;
        m_Player->position.x += m_Velocity.x * delta_time;
        m_Player->position.y += m_Velocity.y * delta_time;
    }

    void ResolveCollisions(float delta_time)
    {
        Rectangle player_rect = PlayerRect();
        m_bGrounded = false;

        for (const Platform& platform : m_Platforms)
        {
            if (!CheckCollisionRecs(player_rect, platform.rect))
            {
                continue;
            }

            // Landing only: ignore side and bottom hits so the player can jump
            // up through platforms.
            const float player_bottom = m_Player->position.y + m_Player->size.y;
            if (m_Velocity.y > 0.0f && player_bottom <= platform.rect.y + (m_Velocity.y * delta_time) + 10.0f)
            {
                m_Player->position.y = platform.rect.y - m_Player->size.y;
                m_Velocity.y = 0.0f;
                m_bGrounded = true;
                player_rect = PlayerRect();
            }
        }
    }

    static constexpr float c_Speed = 400.0f;
    static constexpr float c_JumpForce = 700.0f;
    static constexpr float c_Gravity = 1200.0f;

    Platform m_Platforms[3]{};
    rw::Entity* m_Player = nullptr;
    Vector2 m_Velocity{ 0.0f, 0.0f };
    bool m_bGrounded = false;
};
