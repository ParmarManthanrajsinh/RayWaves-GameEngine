#pragma once
#include "rw.h"
#include <algorithm>

// Starter level for the simple rw API.
//
// Everything the editor needs is set up by rw::Level: entities spawned here
// show up in the Scene hierarchy, the camera is panned/zoomed in the viewport,
// and drawing stays plain raylib.
class Simple2DLevel : public rw::Level
{
public:
    Simple2DLevel() : rw::Level("Simple2D") {}

protected:
    void OnStart() override
    {
        m_PlayerTexture.Load("player.png");

        m_Player = Spawn("Player", { 320.0f, 240.0f }, { 48.0f, 64.0f }, rw::Origin::Center);
        Spawn("Crate A", { 520.0f, 320.0f }, { 48.0f, 48.0f });
        Spawn("Crate B", { 620.0f, 320.0f }, { 48.0f, 48.0f });
        Spawn("Crate C", { 720.0f, 320.0f }, { 48.0f, 48.0f });

        Cam().SetZoom(1.0f);
        Cam().SetBounds(0.0f, 1280.0f, 0.0f, 720.0f);
        Cam().Snap(m_Player->position);
    }

    void OnUpdate(float delta_time) override
    {
        constexpr float Speed = 240.0f;

        if (rw::IsKeyDown(KEY_LEFT) || rw::IsKeyDown(KEY_A))
        {
            m_Player->position.x -= Speed * delta_time;
        }
        if (rw::IsKeyDown(KEY_RIGHT) || rw::IsKeyDown(KEY_D))
        {
            m_Player->position.x += Speed * delta_time;
        }
        if (rw::IsKeyDown(KEY_UP) || rw::IsKeyDown(KEY_W))
        {
            m_Player->position.y -= Speed * delta_time;
        }
        if (rw::IsKeyDown(KEY_DOWN) || rw::IsKeyDown(KEY_S))
        {
            m_Player->position.y += Speed * delta_time;
        }

        m_Player->position.x = std::clamp(m_Player->position.x, 0.0f, 1280.0f);
        m_Player->position.y = std::clamp(m_Player->position.y, 0.0f, 720.0f);

        Cam().Follow(m_Player->position, delta_time);
    }

    void OnDraw() override
    {
        const Color GridColor{ 255, 255, 255, 18 };
        for (int x = 0; x <= 1280; x += 64)
        {
            DrawLine(x, 0, x, 720, GridColor);
        }
        for (int y = 0; y <= 720; y += 64)
        {
            DrawLine(0, y, 1280, y, GridColor);
        }

        for (const auto& entity : Entities())
        {
            if (entity.get() != m_Player)
            {
                rw::DrawEntityBox(*entity, Color{ 120, 200, 255, 255 });
            }
        }

        rw::DrawSprite(m_PlayerTexture, *m_Player);
    }

    void OnDrawUI() override
    {
        DrawText("Simple2D - WASD / arrows to move", 20, 20, 20, LIGHTGRAY);
        DrawFPS(GetRenderWidth() - 90, 20);
    }

private:
    rw::Texture m_PlayerTexture;
    rw::Entity* m_Player = nullptr;
};
