#include "Player.h"
#include "rw.h"
#include <cmath>

namespace
{
constexpr float EPSILON = 0.01f;
}

Player::Player() = default;

void Player::Reset(Vector3 spawn)
{
    m_Position = spawn;
    m_VerticalSpeed = 0.0f;
    m_FacingYaw = 0.0f;
    m_bGrounded = true;
}

void Player::Update(const std::vector<BoundingBox>& colliders, Vector3 move_input, bool b_JumpPressed, float delta_time)
{
    // Horizontal movement is resolved per axis so sliding along walls works.
    float speed = SPEED * (rw::IsKeyDown(KEY_LEFT_SHIFT) ? SPRINT_MULT : 1.0f);
    ResolveHorizontal(colliders, true, move_input.x * speed * delta_time);
    ResolveHorizontal(colliders, false, move_input.z * speed * delta_time);

    if ((move_input.x != 0.0f) || (move_input.z != 0.0f))
    {
        m_FacingYaw = std::atan2(move_input.x, move_input.z);
    }

    if (b_JumpPressed && m_bGrounded)
    {
        m_VerticalSpeed = JUMP_SPEED;
        m_bGrounded = false;
    }

    // Vertical pass: gravity, floor plane, then block tops and undersides.
    m_VerticalSpeed -= GRAVITY * delta_time;
    m_Position.y += m_VerticalSpeed * delta_time;
    m_bGrounded = false;

    if (m_Position.y <= 0.0f)
    {
        m_Position.y = 0.0f;
        m_VerticalSpeed = 0.0f;
        m_bGrounded = true;
    }

    for (const BoundingBox& box : colliders)
    {
        if (!b_Overlaps(GetBounds(), box))
        {
            continue;
        }

        if (m_VerticalSpeed <= 0.0f)
        {
            m_Position.y = box.max.y;
            m_VerticalSpeed = 0.0f;
            m_bGrounded = true;
        }
        else
        {
            m_Position.y = box.min.y - HEIGHT;
            m_VerticalSpeed = 0.0f;
        }
    }
}

void Player::Draw() const
{
    Vector3 feet = { m_Position.x, m_Position.y + RADIUS, m_Position.z };
    Vector3 top = { m_Position.x, m_Position.y + HEIGHT - RADIUS, m_Position.z };

    DrawCapsule(feet, top, RADIUS, 8, 8, { 60, 150, 255, 255 });
    DrawCapsuleWires(feet, top, RADIUS, 8, 8, RAYWHITE);

    // Facing marker makes movement direction readable without a model.
    Vector3 look = { std::sin(m_FacingYaw), 0.0f, std::cos(m_FacingYaw) };
    Vector3 head = { m_Position.x, m_Position.y + HEIGHT - 0.25f, m_Position.z };
    Vector3 marker = { head.x + look.x * 0.35f, head.y, head.z + look.z * 0.35f };
    DrawCube(marker, 0.14f, 0.14f, 0.14f, { 255, 210, 80, 255 });
}

void Player::SaveState(StateBag& out) const
{
    out.SetFloat("player_x", m_Position.x);
    out.SetFloat("player_y", m_Position.y);
    out.SetFloat("player_z", m_Position.z);
    out.SetFloat("player_facing", m_FacingYaw);
}

void Player::LoadState(const StateBag& in)
{
    m_Position.x = in.GetFloat("player_x", 0.0f);
    m_Position.y = in.GetFloat("player_y", 0.0f);
    m_Position.z = in.GetFloat("player_z", 4.0f);
    m_FacingYaw = in.GetFloat("player_facing", 0.0f);
    m_VerticalSpeed = 0.0f;
    m_bGrounded = false;
}

Vector3 Player::GetHeadPosition() const
{
    return { m_Position.x, m_Position.y + HEIGHT - 0.3f, m_Position.z };
}

BoundingBox Player::GetBounds() const
{
    return {
        { m_Position.x - RADIUS, m_Position.y, m_Position.z - RADIUS },
        { m_Position.x + RADIUS, m_Position.y + HEIGHT, m_Position.z + RADIUS },
    };
}

void Player::ResolveHorizontal(const std::vector<BoundingBox>& colliders, bool b_XAxis, float amount)
{
    if (amount == 0.0f)
    {
        return;
    }

    if (b_XAxis)
    {
        m_Position.x += amount;
    }
    else
    {
        m_Position.z += amount;
    }

    for (const BoundingBox& box : colliders)
    {
        if (!b_Overlaps(GetBounds(), box))
        {
            continue;
        }

        if (b_XAxis)
        {
            m_Position.x = (amount > 0.0f) ? box.min.x - RADIUS : box.max.x + RADIUS;
        }
        else
        {
            m_Position.z = (amount > 0.0f) ? box.min.z - RADIUS : box.max.z + RADIUS;
        }
    }
}

bool Player::b_Overlaps(const BoundingBox& a, const BoundingBox& b)
{
    // Touching faces are not an overlap. Without this, standing on a box would
    // block horizontal movement every frame.
    return (a.min.x < b.max.x - EPSILON) && (a.max.x > b.min.x + EPSILON) && (a.min.y < b.max.y - EPSILON) &&
        (a.max.y > b.min.y + EPSILON) && (a.min.z < b.max.z - EPSILON) && (a.max.z > b.min.z + EPSILON);
}
