#pragma once
#include "rw.h"

// Blank starter level. Spawn entities in OnStart and draw them with plain
// raylib in OnDraw. The editor sees every Spawn() automatically.
class EmptyLevel : public rw::Level
{
public:
    EmptyLevel() : rw::Level("EmptyLevel") {}

protected:
    void OnStart() override
    {
        Cam().SetZoom(1.0f);
        Cam().SetBounds(0.0f, 1280.0f, 0.0f, 720.0f);
    }

    void OnUpdate(float delta_time) override { (void)delta_time; }

    void OnDraw() override
    {
        const Color grid_color{ 255, 255, 255, 18 };
        for (int x = 0; x <= 1280; x += 64)
        {
            DrawLine(x, 0, x, 720, grid_color);
        }
        for (int y = 0; y <= 720; y += 64)
        {
            DrawLine(0, y, 1280, y, grid_color);
        }
    }

    void OnDrawUI() override
    {
        DrawText("Empty2D - scaffold a level with: rwcli new-level . MyLevel", 20, 20, 20, LIGHTGRAY);
        DrawFPS(GetRenderWidth() - 90, 20);
    }
};
