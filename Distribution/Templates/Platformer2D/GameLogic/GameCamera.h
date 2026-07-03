#pragma once
#include <raylib.h>

class GameCamera
{
public:
    GameCamera();
    
    void Initialize(Vector2 Target, float Zoom = 2.5f);
    void Reset(Vector2 Target);
    void FollowTarget(Vector2 Target, float DeltaTime, float SmoothSpeed = 5.0f);
    void SetBounds(float Left, float Right, float Top, float Bottom);
    void ClampToBounds();
    void SetZoom(float Zoom);
    void SetMinZoom(float MinZoom);
    
    void Begin() const;
    void End() const;
    
    Camera2D GetRaylibCamera() const { return m_Camera; }
    Vector2 GetTarget() const { return m_Camera.target; }
    float GetZoom() const { return m_Camera.zoom; }
    
private:
    Camera2D m_Camera;
    
    float m_BoundsLeft;
    float m_BoundsRight;
    float m_BoundsTop;
    float m_BoundsBottom;
    float m_MinZoom;
    bool m_bHasBounds;
};
