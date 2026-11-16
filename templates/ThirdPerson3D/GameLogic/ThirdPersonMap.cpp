#include "ThirdPersonMap.h"
#include <algorithm>
#include <cmath>
#include <iostream>

ThirdPersonMap::ThirdPersonMap()
    : GameMap("Third Person 3D")
{
}

ThirdPersonMap::~ThirdPersonMap()
{
    m_SceneTarget.Unload();

    if (b_IsStandalone())
    {
        EnableCursor();
    }
}

void ThirdPersonMap::SetStandaloneMode(bool b_Standalone)
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

void ThirdPersonMap::Initialize()
{
    std::cout << "ThirdPersonMap initialized\n";

    BuildArena();
    m_Player.Reset({ 0.0f, 0.0f, 4.0f });
    m_Camera.Initialize(m_Player.GetHeadPosition());
}

void ThirdPersonMap::Update(float delta_time)
{
    m_Camera.UpdateInput(b_IsStandalone(), delta_time);

    Vector3 move_input = GetMoveInput();
    bool b_jump = IsKeyPressed(KEY_SPACE);
    m_Player.Update(m_Colliders, move_input, b_jump, delta_time);

    // Keep the character inside the rendered arena.
    Vector3 position = m_Player.GetPosition();
    position.x = std::clamp(position.x, -ARENA_HALF_SIZE + 0.5f, ARENA_HALF_SIZE - 0.5f);
    position.z = std::clamp(position.z, -ARENA_HALF_SIZE + 0.5f, ARENA_HALF_SIZE - 0.5f);
    m_Player.SetPosition(position);

    m_Camera.Follow(m_Player.GetHeadPosition(), delta_time);
}

void ThirdPersonMap::Draw()
{
    // Render the 3D scene at 2x and downscale for cheap antialiasing. HUD is
    // drawn after the resolve so text stays sharp.
    int width = GetRenderWidth();
    int height = GetRenderHeight();

    m_SceneTarget.Begin(width, height);
    ClearBackground({ 22, 25, 30, 255 });

    m_Camera.Begin();
    DrawArena();
    m_Player.Draw();
    m_Camera.End();

    m_SceneTarget.End();

    DrawHud(width, height);
}

void ThirdPersonMap::SaveState(StateBag& out) const
{
    m_Player.SaveState(out);
    out.SetFloat("cam_yaw", m_Camera.GetYaw());
    out.SetFloat("cam_pitch", m_Camera.GetPitch());
    out.SetFloat("cam_distance", m_Camera.GetDistance());
}

void ThirdPersonMap::LoadState(const StateBag& in)
{
    m_Player.LoadState(in);
    m_Camera.SetOrbit(in.GetFloat("cam_yaw", 0.0f), in.GetFloat("cam_pitch", 25.0f),
        in.GetFloat("cam_distance", 6.0f));
    m_Camera.Follow(m_Player.GetHeadPosition(), 1.0f);
}

void ThirdPersonMap::BuildArena()
{
    m_Colliders.clear();

    AddBox({ -4.0f, 1.0f, -3.0f }, { 4.0f, 2.0f, 4.0f });
    AddBox({ 5.0f, 0.5f, -6.0f }, { 3.0f, 1.0f, 3.0f });
    AddBox({ 0.0f, 1.5f, -11.0f }, { 7.0f, 3.0f, 2.0f });
    AddBox({ 8.0f, 1.0f, 3.0f }, { 2.0f, 2.0f, 2.0f });
    AddBox({ -9.0f, 0.75f, 7.0f }, { 2.0f, 1.5f, 6.0f });
}

void ThirdPersonMap::AddBox(Vector3 center, Vector3 size)
{
    Vector3 half = { size.x * 0.5f, size.y * 0.5f, size.z * 0.5f };
    m_Colliders.push_back({
        { center.x - half.x, center.y - half.y, center.z - half.z },
        { center.x + half.x, center.y + half.y, center.z + half.z },
    });
}

void ThirdPersonMap::DrawArena() const
{
    float side = ARENA_HALF_SIZE * 2.0f;

    // Plane sits slightly below the grid so the lines stay crisp.
    DrawPlane({ 0.0f, -0.01f, 0.0f }, { side, side }, { 38, 42, 50, 255 });
    DrawGrid(static_cast<int>(side), 1.0f);

    for (const BoundingBox& box : m_Colliders)
    {
        Vector3 center = {
            (box.min.x + box.max.x) * 0.5f,
            (box.min.y + box.max.y) * 0.5f,
            (box.min.z + box.max.z) * 0.5f,
        };
        Vector3 size = {
            box.max.x - box.min.x,
            box.max.y - box.min.y,
            box.max.z - box.min.z,
        };

        DrawCube(center, size.x, size.y, size.z, { 58, 64, 76, 255 });
        DrawCubeWires(center, size.x, size.y, size.z, { 110, 122, 145, 255 });
    }

    DrawCubeWires({ 0.0f, 0.5f, 4.0f }, 1.0f, 1.0f, 1.0f, { 70, 190, 120, 160 });
}

void ThirdPersonMap::DrawHud(int width, int height) const
{
    DrawText("ThirdPerson3D", 20, 18, 26, RAYWHITE);
    DrawText("WASD / Arrows  move    Space  jump    Shift  sprint", 20, height - 66, 20, LIGHTGRAY);

    const char* look_hint = b_IsStandalone() ? "Mouse  orbit    Mouse wheel  zoom"
                                             : "Right-drag  orbit    Mouse wheel  zoom";
    DrawText(look_hint, 20, height - 38, 20, LIGHTGRAY);

    const char* state = m_Player.b_IsGrounded() ? "grounded" : "airborne";
    DrawText(state, width - MeasureText(state, 18) - 20, height - 38, 18, GRAY);
}

Vector3 ThirdPersonMap::GetMoveInput() const
{
    float yaw = m_Camera.GetYaw() * DEG2RAD;
    Vector3 forward = { -std::sin(yaw), 0.0f, -std::cos(yaw) };
    Vector3 right = { std::cos(yaw), 0.0f, -std::sin(yaw) };

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
