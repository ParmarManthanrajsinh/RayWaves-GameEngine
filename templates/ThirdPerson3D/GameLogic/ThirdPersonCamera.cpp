#include "ThirdPersonCamera.h"
#include <algorithm>
#include <cmath>

ThirdPersonCamera::ThirdPersonCamera()
{
    m_Camera.up = { 0.0f, 1.0f, 0.0f };
    m_Camera.fovy = 60.0f;
    m_Camera.projection = CAMERA_PERSPECTIVE;
}

void ThirdPersonCamera::Initialize(Vector3 target)
{
    m_SmoothTarget = target;
    m_Camera.target = target;
    Vector3 offset = ComputeOffset();
    m_Camera.position = { target.x + offset.x, target.y + offset.y, target.z + offset.z };
}

void ThirdPersonCamera::UpdateInput(bool b_CursorCaptured, float delta_time)
{
    // Standalone captures the cursor and looks continuously. The editor keeps
    // a visible cursor and only orbits while right-dragging.
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

        m_Yaw -= delta.x * SENSITIVITY;
        m_Pitch -= delta.y * SENSITIVITY;
        m_Pitch = std::clamp(m_Pitch, MIN_PITCH, MAX_PITCH);
    }

    // Wheel zoom stays global so the camera can be adjusted without dragging.
    float wheel = GetMouseWheelMove();
    if (wheel != 0.0f)
    {
        m_Distance = std::clamp(m_Distance - wheel * 1.5f, MIN_DISTANCE, MAX_DISTANCE);
    }
}

void ThirdPersonCamera::Follow(Vector3 target, float delta_time)
{
    // Exponential smoothing keeps the follow speed frame-rate independent.
    float blend = 1.0f - std::exp(-FOLLOW_SPEED * delta_time);
    m_SmoothTarget.x += (target.x - m_SmoothTarget.x) * blend;
    m_SmoothTarget.y += (target.y - m_SmoothTarget.y) * blend;
    m_SmoothTarget.z += (target.z - m_SmoothTarget.z) * blend;

    m_Camera.target = m_SmoothTarget;
    Vector3 offset = ComputeOffset();
    m_Camera.position = {
        m_SmoothTarget.x + offset.x,
        m_SmoothTarget.y + offset.y,
        m_SmoothTarget.z + offset.z,
    };
}

void ThirdPersonCamera::Begin() const
{
    BeginMode3D(m_Camera);
}

void ThirdPersonCamera::End()
{
    EndMode3D();
}

void ThirdPersonCamera::SetOrbit(float yaw, float pitch, float distance)
{
    m_Yaw = yaw;
    m_Pitch = std::clamp(pitch, MIN_PITCH, MAX_PITCH);
    m_Distance = std::clamp(distance, MIN_DISTANCE, MAX_DISTANCE);
}

Vector3 ThirdPersonCamera::ComputeOffset() const
{
    float yaw = m_Yaw * DEG2RAD;
    float pitch = m_Pitch * DEG2RAD;

    return {
        m_Distance * std::cos(pitch) * std::sin(yaw),
        m_Distance * std::sin(pitch),
        m_Distance * std::cos(pitch) * std::cos(yaw),
    };
}
