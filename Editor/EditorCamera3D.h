#pragma once
#include <algorithm>
#include <cmath>
#include <raylib.h>
#include <raymath.h>

// Editor-owned fly camera for 3D maps.
//
// The gameplay camera belongs to the game; it is usually locked to a player and
// cannot be navigated. This camera is derived from it once per map, then owned
// by the editor. 3D maps render through GameMap::BeginView3D(), which swaps in
// this camera while the project is stopped.
//
// Navigation follows the Unreal editor: hold right mouse to look, WASD to fly,
// Q/E for down/up, Shift to move faster. Middle-drag pans, wheel dollies.
class EditorCamera3D
{
public:
    // Derive orbit values from a gameplay camera so opening a map starts from
    // the same view the game would show.
    void FromCamera(const Camera3D& gameplay)
    {
        const Vector3 direction = Vector3Subtract(gameplay.target, gameplay.position);
        const float length = Vector3Length(direction);
        m_Distance = std::clamp((length > 0.001f) ? length : c_DefaultDistance, c_MinDistance, c_MaxDistance);
        m_Fov = std::clamp(gameplay.fovy, c_MinFov, c_MaxFov);
        m_Pivot = gameplay.target;

        if (length > 0.001f)
        {
            const Vector3 offset = Vector3Negate(direction);
            m_Pitch = std::asin(std::clamp(offset.y / length, -1.0f, 1.0f)) * RAD2DEG;
            m_Yaw = std::atan2(offset.x, offset.z) * RAD2DEG;
        }

        Clamp();
        m_bValid = true;
    }

    void Invalidate() { m_bValid = false; }
    bool b_IsValid() const { return m_bValid; }

    // Rotates the view in place, like holding right mouse in Unreal. The camera
    // position does not move; the pivot is rebuilt so focus/orbit stay coherent.
    void Look(float delta_x, float delta_y)
    {
        const Vector3 position = CameraPosition();
        m_Yaw -= delta_x * c_LookSensitivity;
        m_Pitch += delta_y * c_LookSensitivity;
        Clamp();
        m_Pivot = Vector3Subtract(position, Offset());
    }

    // Flies the camera. `input` is local: x = right, y = world up, z = forward.
    void Move(Vector3 input, float delta_time, bool fast)
    {
        if ((input.x == 0.0f) && (input.y == 0.0f) && (input.z == 0.0f))
        {
            return;
        }

        const float distance_scale = std::clamp(m_Distance / 10.0f, 0.4f, 6.0f);
        const float amount = c_MoveSpeed * distance_scale * (fast ? c_FastMultiplier : 1.0f) * delta_time;

        const Vector3 forward = Forward();
        const Vector3 right = Vector3Normalize(Vector3CrossProduct(forward, { 0.0f, 1.0f, 0.0f }));

        Vector3 delta = Vector3Add(Vector3Scale(forward, input.z * amount), Vector3Scale(right, input.x * amount));
        delta.y += input.y * amount;

        m_Pivot = Vector3Add(m_Pivot, delta);
    }

    // Pan in screen pixels. `viewport_height` is the height of the displayed
    // viewport, so the world moves at the same rate regardless of zoom.
    void Pan(float delta_x, float delta_y, float viewport_height)
    {
        const float height = std::max(viewport_height, 1.0f);
        const float world_per_pixel = (2.0f * m_Distance * std::tan(DEG2RAD * m_Fov * 0.5f)) / height;
        const Vector3 forward = Forward();
        const Vector3 right = Vector3Normalize(Vector3CrossProduct(forward, { 0.0f, 1.0f, 0.0f }));
        const Vector3 up = Vector3Normalize(Vector3CrossProduct(right, forward));

        m_Pivot = Vector3Subtract(m_Pivot, Vector3Scale(right, delta_x * world_per_pixel));
        m_Pivot = Vector3Add(m_Pivot, Vector3Scale(up, delta_y * world_per_pixel));
    }

    void Dolly(float wheel)
    {
        if (wheel == 0.0f)
        {
            return;
        }
        m_Distance = std::clamp(m_Distance * std::pow(c_DollyFactor, wheel), c_MinDistance, c_MaxDistance);
    }

    // Frames a point using its radius (half the entity extent).
    void Focus(Vector3 point, float radius)
    {
        m_Pivot = point;
        const float safe_radius = std::max(radius, 0.1f);
        m_Distance = std::clamp(safe_radius / std::tan(DEG2RAD * m_Fov * 0.5f) * 1.6f, c_MinDistance, c_MaxDistance);
    }

    Camera3D GetCamera() const
    {
        Camera3D camera{};
        camera.position = CameraPosition();
        camera.target = m_Pivot;
        camera.up = { 0.0f, 1.0f, 0.0f };
        camera.fovy = m_Fov;
        camera.projection = CAMERA_PERSPECTIVE;
        return camera;
    }

    Vector3 GetPivot() const { return m_Pivot; }
    Vector3 GetPosition() const { return CameraPosition(); }
    float GetDistance() const { return m_Distance; }
    float GetYaw() const { return m_Yaw; }
    float GetPitch() const { return m_Pitch; }
    float GetFov() const { return m_Fov; }

private:
    Vector3 CameraPosition() const { return Vector3Add(m_Pivot, Offset()); }

    Vector3 Offset() const
    {
        const float yaw = DEG2RAD * m_Yaw;
        const float pitch = DEG2RAD * m_Pitch;
        const float horizontal = m_Distance * std::cos(pitch);
        return { horizontal * std::sin(yaw), m_Distance * std::sin(pitch), horizontal * std::cos(yaw) };
    }

    Vector3 Forward() const { return Vector3Normalize(Vector3Negate(Offset())); }

    void Clamp()
    {
        m_Pitch = std::clamp(m_Pitch, c_MinPitch, c_MaxPitch);
        m_Distance = std::clamp(m_Distance, c_MinDistance, c_MaxDistance);
        m_Yaw = std::fmod(m_Yaw, 360.0f);
    }

    static constexpr float c_DefaultDistance = 10.0f;
    static constexpr float c_MinDistance = 0.5f;
    static constexpr float c_MaxDistance = 500.0f;
    static constexpr float c_MinPitch = -85.0f;
    static constexpr float c_MaxPitch = 85.0f;
    static constexpr float c_MinFov = 30.0f;
    static constexpr float c_MaxFov = 110.0f;
    static constexpr float c_LookSensitivity = 0.35f;
    static constexpr float c_DollyFactor = 0.85f;
    static constexpr float c_MoveSpeed = 12.0f;
    static constexpr float c_FastMultiplier = 3.0f;

    Vector3 m_Pivot{ 0.0f, 0.0f, 0.0f };
    float m_Yaw = 0.0f;
    float m_Pitch = 25.0f;
    float m_Distance = c_DefaultDistance;
    float m_Fov = 60.0f;
    bool m_bValid = false;
};
