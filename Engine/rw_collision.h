#pragma once
#include <algorithm>
#include <raylib.h>
#include <vector>

namespace rw
{
// Outcome of one MoveAndCollide() step.
//
// MoveAndCollide() takes velocity by value, so callers are expected to zero the
// component themselves when the matching hit flag comes back set. b_Grounded is
// only reported for downward hits that land on top of a solid.
struct MoveResult
{
    bool b_HitX = false;
    bool b_HitY = false;
    bool b_Grounded = false;
};

// True when body intersects any solid. Solids are never modified.
inline bool b_Overlaps(const Rectangle& body, const std::vector<Rectangle>& solids)
{
    return std::any_of(
        solids.begin(), solids.end(), [&body](const Rectangle& solid) { return CheckCollisionRecs(body, solid); });
}

// Moves body by velocity * delta_time and resolves it against axis-aligned
// solids. X is resolved before Y so the body settles on a surface it hit and
// does not snag on diagonal corners. body is mutated in place; solids and
// velocity are treated as read-only.
//
// A body that already overlaps a solid before an axis step is allowed to keep
// moving on that axis: snapping it to the closest edge would look like a
// reversed collision and could trap it inside the geometry.
inline MoveResult MoveAndCollide(Rectangle& body, Vector2 velocity, float delta_time,
    const std::vector<Rectangle>& solids, bool b_StopOnFirstHit = true)
{
    MoveResult result;
    if (delta_time <= 0.0f)
    {
        return result;
    }

    const float delta_x = velocity.x * delta_time;
    if (delta_x != 0.0f)
    {
        const Rectangle axis_start = body;
        body.x += delta_x;

        bool b_found_hit = false;
        float resolved_x = body.x;
        for (const Rectangle& solid : solids)
        {
            // A body that starts inside a solid may leave it without snapping
            // back to the near edge. Do not resolve that solid on this axis.
            if (CheckCollisionRecs(axis_start, solid))
            {
                continue;
            }

            const bool b_vertical_overlap =
                axis_start.y < (solid.y + solid.height) && (axis_start.y + axis_start.height) > solid.y;
            if (!b_vertical_overlap)
            {
                continue;
            }

            float candidate_x = 0.0f;
            if (delta_x > 0.0f)
            {
                // Swept crossing: the right edge was at or before the solid and
                // has moved past its left edge.
                if ((axis_start.x + axis_start.width) > solid.x || (body.x + body.width) <= solid.x)
                {
                    continue;
                }
                candidate_x = solid.x - body.width;
            }
            else
            {
                // Mirror of the rightward case for movement into a left edge.
                if (axis_start.x < (solid.x + solid.width) || body.x >= (solid.x + solid.width))
                {
                    continue;
                }
                candidate_x = solid.x + solid.width;
            }

            // Keep the closest edge when scanning all solids, or respect scene
            // order when the caller asks for the first hit.
            if (!b_found_hit || (delta_x > 0.0f ? candidate_x < resolved_x : candidate_x > resolved_x))
            {
                resolved_x = candidate_x;
            }
            b_found_hit = true;
            result.b_HitX = true;

            if (b_StopOnFirstHit)
            {
                break;
            }
        }

        if (b_found_hit)
        {
            body.x = resolved_x;
        }
    }

    const float delta_y = velocity.y * delta_time;
    if (delta_y != 0.0f)
    {
        const Rectangle axis_start = body;
        body.y += delta_y;

        bool b_found_hit = false;
        float resolved_y = body.y;
        bool b_grounded = false;
        for (const Rectangle& solid : solids)
        {
            if (CheckCollisionRecs(axis_start, solid))
            {
                continue;
            }

            const bool b_horizontal_overlap =
                axis_start.x < (solid.x + solid.width) && (axis_start.x + axis_start.width) > solid.x;
            if (!b_horizontal_overlap)
            {
                continue;
            }

            float candidate_y = 0.0f;
            if (delta_y > 0.0f)
            {
                // Swept crossing: the bottom edge was at or above the solid and
                // has moved past its top edge.
                if ((axis_start.y + axis_start.height) > solid.y || (body.y + body.height) <= solid.y)
                {
                    continue;
                }
                candidate_y = solid.y - body.height;
                b_grounded = true;
            }
            else
            {
                // Mirror of the downward case for movement into a bottom edge.
                if (axis_start.y < (solid.y + solid.height) || body.y >= (solid.y + solid.height))
                {
                    continue;
                }
                candidate_y = solid.y + solid.height;
            }

            if (!b_found_hit || (delta_y > 0.0f ? candidate_y < resolved_y : candidate_y > resolved_y))
            {
                resolved_y = candidate_y;
            }
            b_found_hit = true;
            result.b_HitY = true;

            if (b_StopOnFirstHit)
            {
                break;
            }
        }

        if (b_found_hit)
        {
            body.y = resolved_y;
            result.b_Grounded = b_grounded;
        }
    }

    return result;
}
} // namespace rw
