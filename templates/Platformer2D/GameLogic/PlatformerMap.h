#pragma once
#include "../Engine/MapManager.h"
#include <raylib.h>
#include "GameCamera.h"

class PlatformerMap : public GameMap
{
private:
    Vector2 m_PlayerPos;
    Vector2 m_Velocity;
    
    float m_Speed;
    float m_JumpForce;
    float m_Gravity;
    bool m_IsGrounded;

    GameCamera m_Camera;

    struct Platform 
    {
        Rectangle rect;
        Color color;
    };
    
    Platform m_Platforms[3];

public:
    PlatformerMap();
    void Initialize() override;
    void Update(float delta_time) override;
    void Draw() override;
};
