#include "FirstPersonMap.h"
#include <algorithm>
#include <cmath>
#include <iostream>

FirstPersonMap::FirstPersonMap()
    : GameMap("First Person 3D")
{
}

FirstPersonMap::~FirstPersonMap()
{
    m_SceneTarget.Unload();

    if (b_IsStandalone())
    {
        EnableCursor();
    }
}

void FirstPersonMap::SetStandaloneMode(bool b_Standalone)
{
    GameMap::SetStandaloneMode(b_Standalone);

    // Editor keeps a usable cursor; the standalone game captures it.
    if (b_Standalone)
    {
        DisableCursor();
    }
    else
    {
        EnableCursor();
    }
}

void FirstPersonMap::Initialize()
{
    std::cout << "FirstPersonMap initialized\n";
    m_Camera.Initialize({ 0.0f, EYE_HEIGHT, 8.0f });
}

void FirstPersonMap::Update(float delta_time)
{
    m_Camera.UpdateInput(b_IsStandalone(), delta_time);

    float speed = WALK_SPEED * (IsKeyDown(KEY_LEFT_SHIFT) ? SPRINT_MULT : 1.0f);
    Vector3 move = GetMoveInput();

    Vector3 position = m_Camera.GetPosition();
    position.x += move.x * speed * delta_time;
    position.z += move.z * speed * delta_time;

    position.x = std::clamp(position.x, -ARENA_HALF_SIZE + 1.0f, ARENA_HALF_SIZE - 1.0f);
    position.z = std::clamp(position.z, -ARENA_HALF_SIZE + 1.0f, ARENA_HALF_SIZE - 1.0f);
    position.y = EYE_HEIGHT;

    m_Camera.SetPosition(position);
}

void FirstPersonMap::Draw()
{
    // Render the 3D scene at 2x and downscale for cheap antialiasing. HUD is
    // drawn after the resolve so text and the crosshair stay sharp.
    int width = GetRenderWidth();
    int height = GetRenderHeight();

    m_SceneTarget.Begin(width, height);
    ClearBackground({ 16, 18, 22, 255 });

    m_Camera.Begin();
    DrawPlane({ 0.0f, -0.01f, 0.0f }, { ARENA_HALF_SIZE * 2.0f, ARENA_HALF_SIZE * 2.0f },
        { 36, 40, 48, 255 });
    DrawGrid(static_cast<int>(ARENA_HALF_SIZE * 2.0f), 1.0f);
    DrawMarkers();
    m_Camera.End();

    m_SceneTarget.End();

    DrawHud(width, height);
}

void FirstPersonMap::SaveState(StateBag& out) const
{
    Vector3 position = m_Camera.GetPosition();
    out.SetFloat("camera_x", position.x);
    out.SetFloat("camera_y", position.y);
    out.SetFloat("camera_z", position.z);
    out.SetFloat("camera_yaw", m_Camera.GetYaw());
    out.SetFloat("camera_pitch", m_Camera.GetPitch());
}

void FirstPersonMap::LoadState(const StateBag& in)
{
    Vector3 position = {
        in.GetFloat("camera_x", 0.0f),
        in.GetFloat("camera_y", EYE_HEIGHT),
        in.GetFloat("camera_z", 8.0f),
    };

    m_Camera.SetPosition(position);
    m_Camera.SetLook(in.GetFloat("camera_yaw", -90.0f), in.GetFloat("camera_pitch", 0.0f));
}

void FirstPersonMap::DrawHud(int width, int height) const
{
    int center_x = width / 2;
    int center_y = height / 2;
    Color crosshair = { 255, 255, 255, 170 };
    DrawCircleLines(center_x, center_y, 8.0f, crosshair);
    DrawLine(center_x - 14, center_y, center_x - 5, center_y, crosshair);
    DrawLine(center_x + 5, center_y, center_x + 14, center_y, crosshair);
    DrawLine(center_x, center_y - 14, center_x, center_y - 5, crosshair);
    DrawLine(center_x, center_y + 5, center_x, center_y + 14, crosshair);

    DrawText("FirstPerson3D", 20, 18, 26, RAYWHITE);
    DrawText("WASD  move    Shift  sprint", 20, height - 66, 20, LIGHTGRAY);

    const char* look_hint = b_IsStandalone() ? "Mouse  look" : "Hold right mouse  look";
    DrawText(look_hint, 20, height - 38, 20, LIGHTGRAY);
}

void FirstPersonMap::DrawMarkers() const
{
    // Simple depth cues: colored pillars at known world positions.
    DrawCube({ -8.0f, 1.5f, -10.0f }, 2.0f, 3.0f, 2.0f, { 210, 90, 90, 255 });
    DrawCube({ 6.0f, 1.0f, -16.0f }, 1.5f, 2.0f, 1.5f, { 90, 190, 120, 255 });
    DrawCube({ 12.0f, 2.0f, 4.0f }, 3.0f, 4.0f, 3.0f, { 90, 140, 220, 255 });
    DrawCube({ -14.0f, 0.75f, 12.0f }, 4.0f, 1.5f, 4.0f, { 220, 190, 90, 255 });

    for (float x = -ARENA_HALF_SIZE; x <= ARENA_HALF_SIZE; x += 8.0f)
    {
        DrawCube({ x, 0.15f, -ARENA_HALF_SIZE }, 0.3f, 0.3f, 0.3f, { 120, 130, 150, 255 });
        DrawCube({ x, 0.15f, ARENA_HALF_SIZE }, 0.3f, 0.3f, 0.3f, { 120, 130, 150, 255 });
    }
}

Vector3 FirstPersonMap::GetMoveInput() const
{
    Vector3 forward = m_Camera.GetFlatForward();
    Vector3 right = m_Camera.GetFlatRight();

    float forward_amount = 0.0f;
    float right_amount = 0.0f;

    if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP))
    {
        forward_amount += 1.0f;
    }
    if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN))
    {
        forward_amount -= 1.0f;
    }
    if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT))
    {
        right_amount += 1.0f;
    }
    if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT))
    {
        right_amount -= 1.0f;
    }

    Vector3 move = {
        forward.x * forward_amount + right.x * right_amount,
        0.0f,
        forward.z * forward_amount + right.z * right_amount,
    };

    float length = std::sqrt(move.x * move.x + move.z * move.z);
    if (length > 0.0f)
    {
        move.x /= length;
        move.z /= length;
    }

    return move;
}
