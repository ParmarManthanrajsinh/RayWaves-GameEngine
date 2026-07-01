#pragma once
#include <raylib.h>
#include <cstdint>

class Slime
{
public:
    Slime();
    
    void Initialize(Texture2D Texture, Sound DeathSound, Vector2 StartPosition);
    void Update(float DeltaTime);
    void Draw();
    
    Vector2 GetPosition() const { return m_Position; }
    Rectangle GetHitbox() const;
    bool IsAlive() const { return m_bIsAlive; }
    bool IsDying() const { return m_bIsDying; }
    bool IsFullyDead() const { return !m_bIsAlive && !m_bIsDying; }
    
    void SetPatrolBounds(float Left, float Right);
    void TakeDamage();

private:
    Texture2D m_Texture;
    Sound m_DeathSound;
    Vector2 m_Position;
    Vector2 m_Velocity;
    
    bool m_bIsAlive;
    bool m_bIsDying;
    bool m_bFacingRight;
    
    float m_AnimTimer;
    int32_t m_CurrentFrame;
    
    float m_DeathTimer;
    int32_t m_DeathFrame;
    
    float m_PatrolLeft;
    float m_PatrolRight;
    
    static constexpr float SPEED = 50.0f;
    static constexpr float FRAME_WIDTH = 32.0f;
    static constexpr float FRAME_HEIGHT = 32.0f;
    static constexpr int32_t FRAME_COUNT = 4;
    static constexpr int32_t DEATH_FRAME_COUNT = 4;
    static constexpr float ANIM_SPEED = 8.0f;
    static constexpr float DEATH_ANIM_SPEED = 10.0f;
    static constexpr float RENDER_SIZE = 72.0f;
};
