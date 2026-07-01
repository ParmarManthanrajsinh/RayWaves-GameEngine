#include "Slime.h"
#include <cmath>

Slime::Slime()
    : m_Position{ 0, 0 }
    , m_Velocity{ 0, 0 }
    , m_bIsAlive(true)
    , m_bIsDying(false)
    , m_bFacingRight(true)
    , m_AnimTimer(0.0f)
    , m_CurrentFrame(0)
    , m_DeathTimer(0.0f)
    , m_DeathFrame(0)
    , m_PatrolLeft(0.0f)
    , m_PatrolRight(1000.0f)
{
}

void Slime::Initialize(Texture2D Texture, Sound DeathSound, Vector2 StartPosition)
{
    m_Texture = Texture;
    m_DeathSound = DeathSound;
    m_Position = StartPosition;
    m_Velocity = { SPEED, 0 };
    m_bFacingRight = true;
}

void Slime::Update(float DeltaTime)
{
    // Handle death animation
    if (m_bIsDying)
    {
        m_DeathTimer += DeltaTime * DEATH_ANIM_SPEED;
        m_DeathFrame = static_cast<int32_t>(m_DeathTimer);
        
        if (m_DeathFrame >= DEATH_FRAME_COUNT)
        {
            m_bIsDying = false;
            m_bIsAlive = false;
        }
        return;
    }
    
    if (!m_bIsAlive)
    {
        return;
    }
    
    // Update animation
    m_AnimTimer += DeltaTime * ANIM_SPEED;
    m_CurrentFrame = static_cast<int32_t>(m_AnimTimer) % FRAME_COUNT;
    
    // Patrol movement
    m_Position.x += m_Velocity.x * DeltaTime;
    
    // Turn around at patrol bounds
    if (m_Position.x <= m_PatrolLeft)
    {
        m_Position.x = m_PatrolLeft;
        m_Velocity.x = SPEED;
        m_bFacingRight = true;
    }
    else if (m_Position.x >= m_PatrolRight)
    {
        m_Position.x = m_PatrolRight;
        m_Velocity.x = -SPEED;
        m_bFacingRight = false;
    }
}

void Slime::Draw()
{
    if (!m_bIsAlive && !m_bIsDying)
    {
        return;
    }
    
    float FrameX;
    float FrameY;
    
    if (m_bIsDying)
    {
        // Death animation - row 4 (5th row, 0-indexed)
        FrameX = (m_DeathFrame < DEATH_FRAME_COUNT ? m_DeathFrame : DEATH_FRAME_COUNT - 1) * FRAME_WIDTH;
        FrameY = 4 * FRAME_HEIGHT;  // 5th row for death
    }
    else
    {
        // Idle animation - row 0 (first row)
        FrameX = m_CurrentFrame * FRAME_WIDTH;
        FrameY = 0;
    }
    
    Rectangle Source = { FrameX, FrameY, FRAME_WIDTH, FRAME_HEIGHT };
    
    // Flip sprite based on direction
    if (!m_bFacingRight)
    {
        Source.width *= -1;
    }
    
    // Position is center of slime, draw centered
    Rectangle Dest = 
    { 
        m_Position.x - RENDER_SIZE / 2.0f, 
        m_Position.y - RENDER_SIZE / 2.0f,
        RENDER_SIZE, 
        RENDER_SIZE 
    };
    
    // Fade out during death
    Color TintColor = WHITE;
    if (m_bIsDying)
    {
        float Alpha = 1.0f - (static_cast<float>(m_DeathFrame) / DEATH_FRAME_COUNT);
        TintColor.a = static_cast<unsigned char>(Alpha * 255);
    }
    
    DrawTexturePro(
        m_Texture,
        Source,
        Dest,
        { 0, 0 },
        0,
        TintColor
    );
}

Rectangle Slime::GetHitbox() const
{
    // No hitbox if dying or dead
    if (m_bIsDying || !m_bIsAlive)
    {
        return { 0, 0, 0, 0 };
    }
    
    // Hitbox centered on slime position
    float HitboxWidth = RENDER_SIZE * 0.7f;
    float HitboxHeight = RENDER_SIZE * 0.7f;
    return 
    { 
        m_Position.x - HitboxWidth / 2.0f, 
        m_Position.y - HitboxHeight / 2.0f, 
        HitboxWidth, 
        HitboxHeight 
    };
}

void Slime::SetPatrolBounds(float Left, float Right)
{
    m_PatrolLeft = Left;
    m_PatrolRight = Right;
}

void Slime::TakeDamage()
{
    if (m_bIsAlive && !m_bIsDying)
    {
        m_bIsDying = true;
        m_DeathTimer = 0.0f;
        m_DeathFrame = 0;
        m_Velocity = { 0, 0 };
        if (m_DeathSound.frameCount > 0) PlaySound(m_DeathSound);
    }
}
