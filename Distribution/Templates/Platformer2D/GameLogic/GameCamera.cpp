#include "GameCamera.h"

#include <algorithm>
#include "../Engine/GameConfig.h"

GameCamera::GameCamera()
    : m_BoundsLeft(0)
    , m_BoundsRight(0)
    , m_BoundsTop(0)
    , m_BoundsBottom(0)
    , m_MinZoom(1.0f)
    , m_bHasBounds(false)
    , m_ViewportWidth(1280.0f)
    , m_ViewportHeight(720.0f)
{
    m_Camera = { 0 };
}

void GameCamera::UpdateViewport(float width, float height)
{
    m_ViewportWidth = width;
    m_ViewportHeight = height;
}

void GameCamera::Initialize(Vector2 Target, float Zoom)
{
    m_Camera.target = Target;
    float screen_width = m_ViewportWidth;
    float screen_height = m_ViewportHeight;
    
    m_Camera.offset = { screen_width / 2.0f, screen_height / 2.0f };
    m_Camera.rotation = 0.0f;
    m_Camera.zoom = Zoom;
}

void GameCamera::Reset(Vector2 Target)
{
    m_Camera.target = Target;
    float screen_width = m_ViewportWidth;
    float screen_height = m_ViewportHeight;
    
    m_Camera.offset = { screen_width / 2.0f, screen_height / 2.0f };
}

void GameCamera::FollowTarget(Vector2 Target, float DeltaTime, float SmoothSpeed)
{
    float screen_width = m_ViewportWidth;
    float screen_height = m_ViewportHeight;
    
    m_Camera.offset = { screen_width / 2.0f, screen_height / 2.0f };
    
    m_Camera.target.x += (Target.x - m_Camera.target.x) * SmoothSpeed * DeltaTime;
    m_Camera.target.y += (Target.y - m_Camera.target.y) * SmoothSpeed * DeltaTime;
    
    m_Camera.zoom = std::max(m_Camera.zoom, m_MinZoom);
    
    if (m_bHasBounds)
    {
        ClampToBounds();
    }
}

void GameCamera::SetBounds(float Left, float Right, float Top, float Bottom)
{
    m_BoundsLeft = Left;
    m_BoundsRight = Right;
    m_BoundsTop = Top;
    m_BoundsBottom = Bottom;
    m_bHasBounds = true;
}

void GameCamera::ClampToBounds()
{
    float screen_width = m_ViewportWidth;
    float screen_height = m_ViewportHeight;

    float HalfVisibleWidth = (screen_width / m_Camera.zoom) / 2.0f;
    float HalfVisibleHeight = (screen_height / m_Camera.zoom) / 2.0f;
    
    // X clamping - stay within level bounds
    float MinCamX = m_BoundsLeft + HalfVisibleWidth;
    float MaxCamX = m_BoundsRight - HalfVisibleWidth;
    
    m_Camera.target.x = std::max(m_Camera.target.x, MinCamX);
    m_Camera.target.x = std::min(m_Camera.target.x, MaxCamX);
    
    // Y clamping - only clamp max to prevent going below ground (original behavior: max 350)
    m_Camera.target.y = std::min(m_Camera.target.y, m_BoundsBottom);
}

void GameCamera::SetZoom(float Zoom)
{
    m_Camera.zoom = Zoom;
}

void GameCamera::SetMinZoom(float MinZoom)
{
    m_MinZoom = MinZoom;
}

void GameCamera::Begin() const
{
    BeginMode2D(m_Camera);
}

void GameCamera::End() 
{
    EndMode2D();
}
