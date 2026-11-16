#pragma once
#include "../Engine/GameState.h"
#include <raylib.h>
#include <vector>

// Capsule-style character with axis-separated AABB collision. Position is the
// feet center, so a spawn at y = 0 stands on the floor.
class Player
{
public:
    Player();

    void Reset(Vector3 spawn);
    void Update(const std::vector<BoundingBox>& colliders, Vector3 move_input, bool b_JumpPressed,
                float delta_time);
    void Draw() const;

    void SaveState(StateBag& out) const;
    void LoadState(const StateBag& in);

    Vector3 GetPosition() const { return m_Position; }
    Vector3 GetHeadPosition() const;
    bool b_IsGrounded() const { return m_bGrounded; }
    void SetPosition(Vector3 position) { m_Position = position; }

private:
    BoundingBox GetBounds() const;
    void ResolveHorizontal(const std::vector<BoundingBox>& colliders, bool b_XAxis, float amount);

    static bool b_Overlaps(const BoundingBox& a, const BoundingBox& b);

    Vector3 m_Position{}; // Feet center
    float m_VerticalSpeed = 0.0f;
    float m_FacingYaw = 0.0f;
    bool m_bGrounded = false;

    static constexpr float SPEED = 6.0f;
    static constexpr float SPRINT_MULT = 1.6f;
    static constexpr float GRAVITY = 24.0f;
    static constexpr float JUMP_SPEED = 8.5f;
    static constexpr float WIDTH = 0.8f;
    static constexpr float HEIGHT = 1.8f;
    static constexpr float RADIUS = WIDTH * 0.5f;
};
