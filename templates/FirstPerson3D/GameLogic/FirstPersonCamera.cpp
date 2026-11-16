#include "FirstPersonCamera.h"
#include <algorithm>
#include <cmath>

FirstPersonCamera::FirstPersonCamera()
{
    m_Camera.up = { 0.0f, 1.0f, 0.0f };
    m_Camera.fovy = 70.0f;
    m_Camera.projection = CAMERA_PERSPECTIVE;
}

void FirstPersonCamera::Initialize(Vector3 position)
{
    m_Position = position;
    UpdateTarget();
}

void FirstPersonCamera::UpdateInput(bool b_CursorCaptured, float delta_time)
{
    // Standalone captures the cursor and looks continuously. The editor keeps
    // a visible cursor and only looks while right-dragging.
    bool b_focused = IsWindowFocused();
    if (b_CursorCaptured && b_focused && !m_bWindowFocused)
    {
        // Regaining focus warps the captured cursor; ignore those deltas.
        m_CursorSettleTime = 0.2f;
    }
    m_bWindowFocused = b_focused;

    if (b_CursorCaptured && !m_bCursorCaptured)
    {
        // Cursor warping reports bogus deltas for a few frames; ignore them.
        m_CursorSettleTime = 0.2f;
    }
    m_bCursorCaptured = b_CursorCaptured;

    bool b_look = b_CursorCaptured || IsMouseButtonDown(MOUSE_BUTTON_RIGHT);
    if (b_look)
    {
        Vector2 delta = GetMouseDelta();
        if (m_CursorSettleTime > 0.0f)
        {
            m_CursorSettleTime -= delta_time;
            delta = { 0.0f, 0.0f };
        }

        // Pointer lock re-centering can emit a single huge spike; normal
        // mouse movement per frame stays well below this.
        if ((std::fabs(delta.x) > MAX_MOUSE_DELTA) || (std::fabs(delta.y) > MAX_MOUSE_DELTA))
        {
            delta = { 0.0f, 0.0f };
        }

        m_Yaw += delta.x * SENSITIVITY;
        m_Pitch -= delta.y * SENSITIVITY;
        m_Pitch = std::clamp(m_Pitch, -MAX_PITCH, MAX_PITCH);
    }

    UpdateTarget();
}

void FirstPersonCamera::SetPosition(Vector3 position)
{
    m_Position = position;
    UpdateTarget();
}

void FirstPersonCamera::Begin() const
{
    BeginMode3D(m_Camera);
}

void FirstPersonCamera::End()
{
    EndMode3D();
}

Vector3 FirstPersonCamera::GetForward() const
{
    float yaw = m_Yaw * DEG2RAD;
    float pitch = m_Pitch * DEG2RAD;

    return {
        std::cos(pitch) * std::cos(yaw),
        std::sin(pitch),
        std::cos(pitch) * std::sin(yaw),
    };
}

Vector3 FirstPersonCamera::GetFlatForward() const
{
    float yaw = m_Yaw * DEG2RAD;
    return { std::cos(yaw), 0.0f, std::sin(yaw) };
}

Vector3 FirstPersonCamera::GetFlatRight() const
{
    float yaw = m_Yaw * DEG2RAD;
    return { -std::sin(yaw), 0.0f, std::cos(yaw) };
}

void FirstPersonCamera::SetLook(float yaw, float pitch)
{
    m_Yaw = yaw;
    m_Pitch = std::clamp(pitch, -MAX_PITCH, MAX_PITCH);
    UpdateTarget();
}

void FirstPersonCamera::UpdateTarget()
{
    m_Camera.position = m_Position;

    Vector3 forward = GetForward();
    m_Camera.target = {
        m_Position.x + forward.x,
        m_Position.y + forward.y,
        m_Position.z + forward.z,
    };
}
