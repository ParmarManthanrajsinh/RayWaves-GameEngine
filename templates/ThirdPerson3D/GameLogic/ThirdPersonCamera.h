#pragma once
#include <raylib.h>

// Orbit camera that follows a target from a yaw/pitch/distance offset.
// Mouse look uses right-drag instead of DisableCursor, so the editor cursor
// stays usable while the project is open.
class ThirdPersonCamera
{
public:
    ThirdPersonCamera();

    void Initialize(Vector3 target);
    void UpdateInput(bool b_CursorCaptured, float delta_time);
    void Follow(Vector3 target, float delta_time);

    void Begin() const;
    static void End();

    Camera3D GetRaylibCamera() const { return m_Camera; }
    float GetYaw() const { return m_Yaw; }
    float GetPitch() const { return m_Pitch; }
    float GetDistance() const { return m_Distance; }

    void SetOrbit(float yaw, float pitch, float distance);

private:
    Vector3 ComputeOffset() const;

    Camera3D m_Camera{};
    Vector3 m_SmoothTarget{};
    float m_Yaw = 0.0f;
    float m_Pitch = 25.0f;
    float m_Distance = 6.0f;
    bool m_bCursorCaptured = false;
    bool m_bWindowFocused = true;
    float m_CursorSettleTime = 0.0f;

    static constexpr float SENSITIVITY = 0.2f;
    static constexpr float MAX_MOUSE_DELTA = 120.0f;
    static constexpr float MIN_DISTANCE = 2.5f;
    static constexpr float MAX_DISTANCE = 12.0f;
    static constexpr float MIN_PITCH = 5.0f;
    static constexpr float MAX_PITCH = 80.0f;
    static constexpr float FOLLOW_SPEED = 10.0f;
};
