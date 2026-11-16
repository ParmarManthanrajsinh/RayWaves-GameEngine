#pragma once
#include <raylib.h>

// First-person look camera. Right-drag is used instead of DisableCursor so the
// editor cursor remains usable while editing and playing in the same window.
class FirstPersonCamera
{
public:
    FirstPersonCamera();

    void Initialize(Vector3 position);
    void UpdateInput(bool b_CursorCaptured, float delta_time);
    void SetPosition(Vector3 position);

    void Begin() const;
    static void End();

    Camera3D GetRaylibCamera() const { return m_Camera; }
    Vector3 GetPosition() const { return m_Position; }
    Vector3 GetForward() const;
    Vector3 GetFlatForward() const;
    Vector3 GetFlatRight() const;
    float GetYaw() const { return m_Yaw; }
    float GetPitch() const { return m_Pitch; }

    void SetLook(float yaw, float pitch);

private:
    void UpdateTarget();

    Camera3D m_Camera{};
    Vector3 m_Position{};
    float m_Yaw = -90.0f;
    float m_Pitch = 0.0f;
    bool m_bCursorCaptured = false;
    bool m_bWindowFocused = true;
    float m_CursorSettleTime = 0.0f;

    static constexpr float SENSITIVITY = 0.15f;
    static constexpr float MAX_PITCH = 85.0f;
    static constexpr float MAX_MOUSE_DELTA = 120.0f;
};
