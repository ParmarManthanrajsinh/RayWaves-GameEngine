#include "Player.h"
#include "DemoLevel.h"
#include "Engine/AssetResolver.h"
#include <algorithm>
#include <cmath>

Player::Player()
    : m_Position{ 0, 0 }
    , m_Velocity{ 0, 0 }
    , m_bIsGrounded(false)
    , m_bFacingRight(true)
    , m_bIsAttacking(false)
    , m_AttackTimer(0.0f)
    , m_AttackFrame(0)
{
}

Player::~Player()
{
    UnloadTexture(m_Texture);
    UnloadSound(m_JumpSound);
    UnloadSound(m_AttackSound);
}

void Player::Initialize(const char* TexturePath)
{
    m_Texture = LoadTexture(TexturePath);
    LoadSounds();
}

void Player::LoadSounds()
{
    m_JumpSound = LoadSound(AssetResolver::Resolve("Sounds/jump.wav").c_str());
    m_AttackSound = LoadSound(AssetResolver::Resolve("Sounds/attack.wav").c_str());
    
    std::cout << "[Player] Audio Device Ready: " << IsAudioDeviceReady() << '\n';
    std::cout << "[Player] Jump Sound Loaded: " << (m_JumpSound.frameCount > 0) << '\n';
    std::cout << "[Player] Attack Sound Loaded: " << (m_AttackSound.frameCount > 0) << '\n';
}

void Player::Reset(Vector2 StartPosition)
{
    m_Position = StartPosition;
    m_Velocity = { 0, 0 };
    m_bIsGrounded = false;
    m_bFacingRight = true;
    m_bIsAttacking = false;
    m_AttackTimer = 0.0f;
    m_AttackFrame = 0;
}

void Player::SaveState(StateBag& out) const
{
    out.SetVector2("player_position", m_Position);
    out.SetVector2("player_velocity", m_Velocity);
    out.SetBool("player_grounded", m_bIsGrounded);
    out.SetBool("player_facing_right", m_bFacingRight);
}

void Player::LoadState(const StateBag& in)
{
    m_Position = in.GetVector2("player_position", m_Position);
    m_Velocity = in.GetVector2("player_velocity", m_Velocity);
    m_bIsGrounded = in.GetBool("player_grounded", m_bIsGrounded);
    m_bFacingRight = in.GetBool("player_facing_right", m_bFacingRight);
}

void Player::HandleInput(float DeltaTime)
{
    // Attack input - left mouse button
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !m_bIsAttacking)
    {
        m_bIsAttacking = true;
        m_AttackTimer = 0.0f;
        m_AttackFrame = 0;
        if (m_AttackSound.frameCount > 0) PlaySound(m_AttackSound);
    }
    
    // Movement speed modifier (slower while attacking)
    float MoveSpeed = m_bIsAttacking ? SPEED * 0.3f : SPEED;
    
    // Movement
    if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D))
    {
        m_Velocity.x = MoveSpeed;
        m_bFacingRight = true;
    }
    else if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A))
    {
        m_Velocity.x = -MoveSpeed;
        m_bFacingRight = false;
    }
    else
    {
        m_Velocity.x = 0;
    }

    // Jump (always allowed)
    if ((IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) && m_bIsGrounded)
    {
        m_Velocity.y = JUMP_FORCE;
        m_bIsGrounded = false;
        if (m_JumpSound.frameCount > 0) PlaySound(m_JumpSound);
    }
}

void Player::Update(float DeltaTime)
{
    // Update attack timer
    if (m_bIsAttacking)
    {
        m_AttackTimer += DeltaTime;
        
        float FrameDuration = ATTACK_DURATION / ATTACK_FRAMES;
        m_AttackFrame = static_cast<int32_t>(m_AttackTimer / FrameDuration);
        
        if (m_AttackTimer >= ATTACK_DURATION)
        {
            m_bIsAttacking = false;
            m_AttackTimer = 0.0f;
            m_AttackFrame = 0;
        }
    }
}

void Player::ApplyGravity(float DeltaTime, float Gravity)
{
    m_Velocity.y += Gravity * DeltaTime;
}

void Player::ResolveCollisions(float DeltaTime, const std::vector<GroundTile>& Tiles)
{
    // Horizontal collision
    float NextX = m_Position.x + (m_Velocity.x * DeltaTime);
    Rectangle NextHitboxX = { NextX - HITBOX_OFFSET_X, m_Position.y - HITBOX_OFFSET_Y, HITBOX_WIDTH, HITBOX_HEIGHT };
    
    bool bHitX = false;
    for (const auto& Tile : Tiles)
    {
        if (CheckCollisionRecs(NextHitboxX, Tile.Rect))
        {
            if (m_Velocity.x > 0)
            {
                m_Position.x = Tile.Rect.x - 48;
            }
            else if (m_Velocity.x < 0)
            {
                m_Position.x = Tile.Rect.x + Tile.Rect.width + 16;
            }
            m_Velocity.x = 0;
            bHitX = true;
            break;
        }
    }
    
    if (!bHitX)
    {
        m_Position.x = NextX;
    }

    // Vertical collision
    float NextY = m_Position.y + (m_Velocity.y * DeltaTime);
    Rectangle NextHitboxY = { m_Position.x - HITBOX_OFFSET_X, NextY - HITBOX_OFFSET_Y, HITBOX_WIDTH, HITBOX_HEIGHT };
    
    m_bIsGrounded = false;
    bool bHitY = false;
    
    for (const auto& Tile : Tiles)
    {
        if (CheckCollisionRecs(NextHitboxY, Tile.Rect))
        {
            if (m_Velocity.y > 0)
            {
                m_Position.y = Tile.Rect.y - 48;
                m_bIsGrounded = true;
            }
            else if (m_Velocity.y < 0)
            {
                m_Position.y = Tile.Rect.y + Tile.Rect.height + 16;
            }
            m_Velocity.y = 0;
            bHitY = true;
            break; 
        }
    }
    
    if (!bHitY)
    {
        m_Position.y = NextY;
    }
}

void Player::ClampToLevel(float LevelLeft, float LevelRight)
{
    m_Position.x = std::max(m_Position.x, LevelLeft);
    m_Position.x = std::min(m_Position.x, LevelRight);
}

void Player::Draw()
{
    int32_t Row = 0;
    int32_t MaxFrames = 1;
    float AnimSpeed = 6.0f;
    
    // Attack animation takes priority
    if (m_bIsAttacking)
    {
        Row = 8;  // Attack animation row
        MaxFrames = ATTACK_FRAMES;
        int32_t Frame = (m_AttackFrame < MaxFrames) ? m_AttackFrame : MaxFrames - 1;
        
        float PlayerFrameW = 32.0f;
        float PlayerFrameH = 32.0f;
        
        Rectangle Source = { Frame * PlayerFrameW, Row * PlayerFrameH, PlayerFrameW, PlayerFrameH };
        
        if (!m_bFacingRight)
        {
            Source.width *= -1;
        }
        
        DrawTexturePro(
            m_Texture,
            Source,
            { m_Position.x - 16, m_Position.y - 16 + 32, 64, 64 },
            { 0, 0 },
            0,
            WHITE
        );
        return;
    }
    
    // Normal animations
    if (!m_bIsGrounded)
    {
        Row = 5;
        MaxFrames = 4;
    }
    else if (fabs(m_Velocity.x) > 10.0f)
    {
        Row = 3;
        MaxFrames = 8;
        AnimSpeed = 12.0f;
    }
    
    float PlayerFrameW = 32.0f;
    float PlayerFrameH = 32.0f;
    int32_t Frame = static_cast<int32_t>(GetTime() * AnimSpeed) % MaxFrames;
    
    Rectangle Source = { Frame * PlayerFrameW, Row * PlayerFrameH, PlayerFrameW, PlayerFrameH };
    
    if (!m_bFacingRight)
    {
        Source.width *= -1;
    }
    
    DrawTexturePro(
        m_Texture,
        Source,
        { m_Position.x - 16, m_Position.y - 16 + 32, 64, 64 },
        { 0, 0 },
        0,
        WHITE
    );
}

Rectangle Player::GetAttackHitbox() const
{
    if (!m_bIsAttacking)
    {
        return { 0, 0, 0, 0 };
    }
    
    // Attack hitbox extends in front of player
    float HitboxWidth = 30.0f;
    float HitboxHeight = 60.0f;
    float OffsetX = m_bFacingRight ? 25.0f : -25.0f - HitboxWidth;
    
    return 
    {
        m_Position.x + OffsetX,
        m_Position.y,
        HitboxWidth,
        HitboxHeight
    };
}
