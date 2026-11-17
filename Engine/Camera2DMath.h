#pragma once
#include <algorithm>
#include <cmath>
#include <raylib.h>

namespace Camera2DMath
{
inline void Pan(Camera2D& camera, Vector2 previous_world, Vector2 current_world)
{
    camera.target.x -= current_world.x - previous_world.x;
    camera.target.y -= current_world.y - previous_world.y;
}

inline bool b_ZoomAt(
    Camera2D& camera, Vector2 screen_point, float wheel_delta, float min_zoom = 0.1f, float max_zoom = 10.0f)
{
    if (wheel_delta == 0.0f)
    {
        return false;
    }

    const Vector2 before = GetScreenToWorld2D(screen_point, camera);
    const float new_zoom = std::clamp(camera.zoom * std::pow(1.12f, wheel_delta), min_zoom, max_zoom);
    if (new_zoom == camera.zoom)
    {
        return false;
    }

    camera.zoom = new_zoom;
    const Vector2 after = GetScreenToWorld2D(screen_point, camera);
    camera.target.x += before.x - after.x;
    camera.target.y += before.y - after.y;
    return true;
}

inline void Frame(Camera2D& camera, Vector2 center)
{
    camera.target = center;
}
} // namespace Camera2DMath
